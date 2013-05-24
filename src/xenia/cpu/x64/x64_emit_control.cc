/*
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <xenia/cpu/x64/x64_emit.h>

#include <xenia/cpu/cpu-private.h>


using namespace xe::cpu;
using namespace xe::cpu::ppc;
using namespace xe::cpu::sdb;


namespace xe {
namespace cpu {
namespace x64 {


int XeEmitIndirectBranchTo(
    X64Emitter& e, jit_function_t f, const char* src, uint32_t cia,
    bool lk, uint32_t reg) {
  // TODO(benvanik): run a DFA pass to see if we can detect whether this is
  //     a normal function return that is pulling the LR from the stack that
  //     it set in the prolog. If so, we can omit the dynamic check!

  // NOTE: we avoid spilling registers until we know that the target is not
  // a basic block within this function.

  jit_value_t target;
  switch (reg) {
    case kXEPPCRegLR:
      target = e.lr_value();
      break;
    case kXEPPCRegCTR:
      target = e.ctr_value();
      break;
    default:
      XEASSERTALWAYS();
      return 1;
  }

  // Dynamic test when branching to LR, which is usually used for the return.
  // We only do this if LK=0 as returns wouldn't set LR.
  // Ideally it's a return and we can just do a simple ret and be done.
  // If it's not, we fall through to the full indirection logic.
  if (!lk && reg == kXEPPCRegLR) {
    // The return block will spill registers for us.
    // TODO(benvanik): 'lr_mismatch' debug info.
    jit_value_t lr_cmp = jit_insn_eq(f, target, jit_value_get_param(f, 1));
    e.branch_to_return_if(lr_cmp);
  }

  // Defer to the generator, which will do fancy things.
  bool likely_local = !lk && reg == kXEPPCRegCTR;
  return e.GenerateIndirectionBranch(cia, target, lk, likely_local);
}

int XeEmitBranchTo(
    X64Emitter& e, jit_function_t f, const char* src, uint32_t cia,
    bool lk, jit_value_t condition) {
  FunctionBlock* fn_block = e.fn_block();

  // Fast-path for branches to other blocks.
  // Only valid when not tracing branches.
  if (!FLAGS_trace_branches &&
      fn_block->outgoing_type == FunctionBlock::kTargetBlock) {
    e.branch_to_block_if(fn_block->outgoing_address, condition);
    return 0;
  }

  // Only branch of conditionals when we have one.
  jit_label_t post_jump_label = jit_label_undefined;
  if (condition) {
    // TODO(benvanik): add debug info for this?
    // char name[32];
    // xesnprintfa(name, XECOUNT(name), "loc_%.8X_bcx", i.address);
    jit_insn_branch_if_not(f, condition, &post_jump_label);
  }

  if (FLAGS_trace_branches) {
    e.TraceBranch(cia);
  }

  // Get the basic block and switch behavior based on outgoing type.
  int result = 0;
  switch (fn_block->outgoing_type) {
    case FunctionBlock::kTargetBlock:
      // Taken care of above usually.
      e.branch_to_block(fn_block->outgoing_address);
      break;
    case FunctionBlock::kTargetFunction:
    {
      // Spill all registers to memory.
      // TODO(benvanik): only spill ones used by the target function? Use
      //     calling convention flags on the function to not spill temp
      //     registers?
      e.SpillRegisters();

      XEASSERTNOTNULL(fn_block->outgoing_function);
      // TODO(benvanik): check to see if this is the last block in the function.
      //     This would enable tail calls/etc.
      bool is_end = false;
      if (!lk || is_end) {
        // Tail. No need to refill the local register values, just return.
        // We optimize this by passing in the LR from our parent instead of the
        // next instruction. This allows the return from our callee to pop
        // all the way up.
        e.call_function(fn_block->outgoing_function,
                        jit_value_get_param(f, 1), true);
        jit_insn_return(f, NULL);
      } else {
        // Will return here eventually.
        // Refill registers from state.
        e.call_function(fn_block->outgoing_function,
                        e.get_uint64(cia + 4), false);
        e.FillRegisters();
      }
      break;
    }
    case FunctionBlock::kTargetLR:
    {
      // An indirect jump.
      printf("INDIRECT JUMP VIA LR: %.8X\n", cia);
      result = XeEmitIndirectBranchTo(e, f, src, cia, lk, kXEPPCRegLR);
      break;
    }
    case FunctionBlock::kTargetCTR:
    {
      // An indirect jump.
      printf("INDIRECT JUMP VIA CTR: %.8X\n", cia);
      result = XeEmitIndirectBranchTo(e, f, src, cia, lk, kXEPPCRegCTR);
      break;
    }
    default:
    case FunctionBlock::kTargetNone:
      XEASSERTALWAYS();
      result = 1;
      break;
  }

  if (condition) {
    jit_insn_label(f, &post_jump_label);
  }

  return result;
}


XEEMITTER(bx,           0x48000000, I  )(X64Emitter& e, jit_function_t f, InstrData& i) {
  // if AA then
  //   NIA <- EXTS(LI || 0b00)
  // else
  //   NIA <- CIA + EXTS(LI || 0b00)
  // if LK then
  //   LR <- CIA + 4

  uint32_t nia;
  if (i.I.AA) {
    nia = XEEXTS26(i.I.LI << 2);
  } else {
    nia = i.address + XEEXTS26(i.I.LI << 2);
  }
  if (i.I.LK) {
    e.update_lr_value(e.get_uint64(i.address + 4));
  }

  return XeEmitBranchTo(e, f, "bx", i.address, i.I.LK, NULL);
}

XEEMITTER(bcx,          0x40000000, B  )(X64Emitter& e, jit_function_t f, InstrData& i) {
  // if ¬BO[2] then
  //   CTR <- CTR - 1
  // ctr_ok <- BO[2] | ((CTR[0:63] != 0) XOR BO[3])
  // cond_ok <- BO[0] | (CR[BI+32] ≡ BO[1])
  // if ctr_ok & cond_ok then
  //   if AA then
  //     NIA <- EXTS(BD || 0b00)
  //   else
  //     NIA <- CIA + EXTS(BD || 0b00)
  // if LK then
  //   LR <- CIA + 4

  // NOTE: the condition bits are reversed!
  // 01234 (docs)
  // 43210 (real)

  // TODO(benvanik): this may be wrong and overwrite LRs when not desired!
  // The docs say always, though...
  if (i.B.LK) {
    e.update_lr_value(e.get_uint64(i.address + 4));
  }

  jit_value_t ctr_ok = NULL;
  if (XESELECTBITS(i.B.BO, 2, 2)) {
    // Ignore ctr.
  } else {
    // Decrement counter.
    jit_value_t ctr = e.ctr_value();
    ctr = jit_insn_sub(f, ctr, e.get_int64(1));
    e.update_ctr_value(ctr);

    // Ctr check.
    if (XESELECTBITS(i.B.BO, 1, 1)) {
      ctr_ok = jit_insn_eq(f, ctr, e.get_int64(0));
    } else {
      ctr_ok = jit_insn_ne(f, ctr, e.get_int64(0));
    }
  }

  jit_value_t cond_ok = NULL;
  if (XESELECTBITS(i.B.BO, 4, 4)) {
    // Ignore cond.
  } else {
    jit_value_t cr = e.cr_value(i.B.BI >> 2);
    cr = jit_insn_and(f, cr, e.get_uint32(1 << (i.B.BI & 3)));
    if (XESELECTBITS(i.B.BO, 3, 3)) {
      cond_ok = jit_insn_ne(f, cr, e.get_int64(0));
    } else {
      cond_ok = jit_insn_eq(f, cr, e.get_int64(0));
    }
  }

  // We do a bit of optimization here to make the llvm assembly easier to read.
  jit_value_t ok = NULL;
  if (ctr_ok && cond_ok) {
    ok = jit_insn_and(f, ctr_ok, cond_ok);
  } else if (ctr_ok) {
    ok = ctr_ok;
  } else if (cond_ok) {
    ok = cond_ok;
  }

  uint32_t nia;
  if (i.B.AA) {
    nia = XEEXTS26(i.B.BD << 2);
  } else {
    nia = i.address + XEEXTS26(i.B.BD << 2);
  }
  if (XeEmitBranchTo(e, f, "bcx", i.address, i.B.LK, ok)) {
    return 1;
  }

  return 0;
}

XEEMITTER(bcctrx,       0x4C000420, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  // cond_ok <- BO[0] | (CR[BI+32] ≡ BO[1])
  // if cond_ok then
  //   NIA <- CTR[0:61] || 0b00
  // if LK then
  //   LR <- CIA + 4

  // NOTE: the condition bits are reversed!
  // 01234 (docs)
  // 43210 (real)

  // TODO(benvanik): this may be wrong and overwrite LRs when not desired!
  // The docs say always, though...
  if (i.XL.LK) {
    e.update_lr_value(e.get_uint64(i.address + 4));
  }

  jit_value_t cond_ok = NULL;
  if (XESELECTBITS(i.XL.BO, 4, 4)) {
    // Ignore cond.
  } else {
    jit_value_t cr = e.cr_value(i.XL.BI >> 2);
    cr = jit_insn_and(f, cr, e.get_uint64(1 << (i.XL.BI & 3)));
    if (XESELECTBITS(i.XL.BO, 3, 3)) {
      cond_ok = jit_insn_ne(f, cr, e.get_int64(0));
    } else {
      cond_ok = jit_insn_eq(f, cr, e.get_int64(0));
    }
  }

  // We do a bit of optimization here to make the llvm assembly easier to read.
  jit_value_t ok = NULL;
  if (cond_ok) {
    ok = cond_ok;
  }

  if (XeEmitBranchTo(e, f, "bcctrx", i.address, i.XL.LK, ok)) {
    return 1;
  }

  return 0;
}

XEEMITTER(bclrx,        0x4C000020, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  // if ¬BO[2] then
  //   CTR <- CTR - 1
  // ctr_ok <- BO[2] | ((CTR[0:63] != 0) XOR BO[3]
  // cond_ok <- BO[0] | (CR[BI+32] ≡ BO[1])
  // if ctr_ok & cond_ok then
  //   NIA <- LR[0:61] || 0b00
  // if LK then
  //   LR <- CIA + 4

  // NOTE: the condition bits are reversed!
  // 01234 (docs)
  // 43210 (real)

  // TODO(benvanik): this may be wrong and overwrite LRs when not desired!
  // The docs say always, though...
  if (i.XL.LK) {
    e.update_lr_value(e.get_uint64(i.address + 4));
  }

  jit_value_t ctr_ok = NULL;
  if (XESELECTBITS(i.XL.BO, 2, 2)) {
    // Ignore ctr.
  } else {
    // Decrement counter.
    jit_value_t ctr = e.ctr_value();
    ctr = jit_insn_sub(f, ctr, e.get_int64(1));

    // Ctr check.
    if (XESELECTBITS(i.XL.BO, 1, 1)) {
      ctr_ok = jit_insn_eq(f, ctr, e.get_int64(0));
    } else {
      ctr_ok = jit_insn_ne(f, ctr, e.get_int64(0));
    }
  }

  jit_value_t cond_ok = NULL;
  if (XESELECTBITS(i.XL.BO, 4, 4)) {
    // Ignore cond.
  } else {
    jit_value_t cr = e.cr_value(i.XL.BI >> 2);
    cr = jit_insn_and(f, cr, e.get_uint32(1 << (i.XL.BI & 3)));
    if (XESELECTBITS(i.XL.BO, 3, 3)) {
      cond_ok = jit_insn_ne(f, cr, e.get_int64(0));
    } else {
      cond_ok = jit_insn_eq(f, cr, e.get_int64(0));
    }
  }

  // We do a bit of optimization here to make the llvm assembly easier to read.
  jit_value_t ok = NULL;
  if (ctr_ok && cond_ok) {
    ok = jit_insn_and(f, ctr_ok, cond_ok);
  } else if (ctr_ok) {
    ok = ctr_ok;
  } else if (cond_ok) {
    ok = cond_ok;
  }

  if (XeEmitBranchTo(e, f, "bclrx", i.address, i.XL.LK, ok)) {
    return 1;
  }

  return 0;
}


// Condition register logical (A-23)

XEEMITTER(crand,        0x4C000202, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(crandc,       0x4C000102, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(creqv,        0x4C000242, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(crnand,       0x4C0001C2, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(crnor,        0x4C000042, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(cror,         0x4C000382, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(crorc,        0x4C000342, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(crxor,        0x4C000182, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(mcrf,         0x4C000000, XL )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}


// System linkage (A-24)

XEEMITTER(sc,           0x44000002, SC )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}


// Trap (A-25)

int XeEmitTrap(X64Emitter& e, jit_function_t f, InstrData& i,
                jit_value_t va, jit_value_t vb, uint32_t TO) {
  // if (a < b) & TO[0] then TRAP
  // if (a > b) & TO[1] then TRAP
  // if (a = b) & TO[2] then TRAP
  // if (a <u b) & TO[3] then TRAP
  // if (a >u b) & TO[4] then TRAP
  // Bits swapped:
  // 01234
  // 43210

  if (!TO) {
    return 0;
  }

  // TODO(benvanik): port from LLVM
  XEASSERTALWAYS();

  // BasicBlock* after_bb = BasicBlock::Create(*e.context(), "", e.fn(),
  //                                           e.GetNextBasicBlock());
  // BasicBlock* trap_bb = BasicBlock::Create(*e.context(), "", e.fn(),
  //                                          after_bb);

  // // Create the basic blocks (so we can chain).
  // std::vector<BasicBlock*> bbs;
  // if (TO & (1 << 4)) {
  //   bbs.push_back(BasicBlock::Create(*e.context(), "", e.fn(), trap_bb));
  // }
  // if (TO & (1 << 3)) {
  //   bbs.push_back(BasicBlock::Create(*e.context(), "", e.fn(), trap_bb));
  // }
  // if (TO & (1 << 2)) {
  //   bbs.push_back(BasicBlock::Create(*e.context(), "", e.fn(), trap_bb));
  // }
  // if (TO & (1 << 1)) {
  //   bbs.push_back(BasicBlock::Create(*e.context(), "", e.fn(), trap_bb));
  // }
  // if (TO & (1 << 0)) {
  //   bbs.push_back(BasicBlock::Create(*e.context(), "", e.fn(), trap_bb));
  // }
  // bbs.push_back(after_bb);

  // // Jump to the first bb.
  // b.CreateBr(bbs.front());

  // // Setup each basic block.
  // std::vector<BasicBlock*>::iterator it = bbs.begin();
  // if (TO & (1 << 4)) {
  //   // a < b
  //   BasicBlock* bb = *(it++);
  //   b.SetInsertPoint(bb);
  //   jit_value_t cmp = b.CreateICmpSLT(va, vb);
  //   b.CreateCondBr(cmp, trap_bb, *it);
  // }
  // if (TO & (1 << 3)) {
  //   // a > b
  //   BasicBlock* bb = *(it++);
  //   b.SetInsertPoint(bb);
  //   jit_value_t cmp = b.CreateICmpSGT(va, vb);
  //   b.CreateCondBr(cmp, trap_bb, *it);
  // }
  // if (TO & (1 << 2)) {
  //   // a = b
  //   BasicBlock* bb = *(it++);
  //   b.SetInsertPoint(bb);
  //   jit_value_t cmp = b.CreateICmpEQ(va, vb);
  //   b.CreateCondBr(cmp, trap_bb, *it);
  // }
  // if (TO & (1 << 1)) {
  //   // a <u b
  //   BasicBlock* bb = *(it++);
  //   b.SetInsertPoint(bb);
  //   jit_value_t cmp = b.CreateICmpULT(va, vb);
  //   b.CreateCondBr(cmp, trap_bb, *it);
  // }
  // if (TO & (1 << 0)) {
  //   // a >u b
  //   BasicBlock* bb = *(it++);
  //   b.SetInsertPoint(bb);
  //   jit_value_t cmp = b.CreateICmpUGT(va, vb);
  //   b.CreateCondBr(cmp, trap_bb, *it);
  // }

  // // Create trap BB.
  // b.SetInsertPoint(trap_bb);
  // e.SpillRegisters();
  // // TODO(benvanik): use @llvm.debugtrap? could make debugging better
  // b.CreateCall2(e.gen_module()->getFunction("XeTrap"),
  //               e.fn()->arg_begin(),
  //               e.get_uint64(i.address));
  // b.CreateBr(after_bb);

  // // Resume.
  // b.SetInsertPoint(after_bb);

  return 0;
}

XEEMITTER(td,           0x7C000088, X  )(X64Emitter& e, jit_function_t f, InstrData& i) {
  // a <- (RA)
  // b <- (RB)
  // if (a < b) & TO[0] then TRAP
  // if (a > b) & TO[1] then TRAP
  // if (a = b) & TO[2] then TRAP
  // if (a <u b) & TO[3] then TRAP
  // if (a >u b) & TO[4] then TRAP
  return XeEmitTrap(e, f, i,
                    e.gpr_value(i.X.RA),
                    e.gpr_value(i.X.RB),
                    i.X.RT);
}

XEEMITTER(tdi,          0x08000000, D  )(X64Emitter& e, jit_function_t f, InstrData& i) {
  // a <- (RA)
  // if (a < EXTS(SI)) & TO[0] then TRAP
  // if (a > EXTS(SI)) & TO[1] then TRAP
  // if (a = EXTS(SI)) & TO[2] then TRAP
  // if (a <u EXTS(SI)) & TO[3] then TRAP
  // if (a >u EXTS(SI)) & TO[4] then TRAP
  return XeEmitTrap(e, f, i,
                    e.gpr_value(i.D.RA),
                    e.get_int64(XEEXTS16(i.D.DS)),
                    i.D.RT);
}

XEEMITTER(tw,           0x7C000008, X  )(X64Emitter& e, jit_function_t f, InstrData& i) {
  // a <- EXTS((RA)[32:63])
  // b <- EXTS((RB)[32:63])
  // if (a < b) & TO[0] then TRAP
  // if (a > b) & TO[1] then TRAP
  // if (a = b) & TO[2] then TRAP
  // if (a <u b) & TO[3] then TRAP
  // if (a >u b) & TO[4] then TRAP
  return XeEmitTrap(e, f, i,
                    e.sign_extend(e.trunc_to_int(e.gpr_value(i.X.RA)),
                                  jit_type_nint),
                    e.sign_extend(e.trunc_to_int(e.gpr_value(i.X.RB)),
                                  jit_type_nint),
                    i.X.RT);
}

XEEMITTER(twi,          0x0C000000, D  )(X64Emitter& e, jit_function_t f, InstrData& i) {
  // a <- EXTS((RA)[32:63])
  // if (a < EXTS(SI)) & TO[0] then TRAP
  // if (a > EXTS(SI)) & TO[1] then TRAP
  // if (a = EXTS(SI)) & TO[2] then TRAP
  // if (a <u EXTS(SI)) & TO[3] then TRAP
  // if (a >u EXTS(SI)) & TO[4] then TRAP
  return XeEmitTrap(e, f, i,
                    e.sign_extend(e.trunc_to_int(e.gpr_value(i.D.RA)),
                                  jit_type_nint),
                    e.get_int64(XEEXTS16(i.D.DS)),
                    i.D.RT);
}


// Processor control (A-26)

XEEMITTER(mfcr,         0x7C000026, X  )(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(mfspr,        0x7C0002A6, XFX)(X64Emitter& e, jit_function_t f, InstrData& i) {
  // n <- spr[5:9] || spr[0:4]
  // if length(SPR(n)) = 64 then
  //   RT <- SPR(n)
  // else
  //   RT <- i32.0 || SPR(n)

  const uint32_t n = ((i.XFX.spr & 0x1F) << 5) | ((i.XFX.spr >> 5) & 0x1F);
  jit_value_t v = NULL;
  switch (n) {
  case 1:
    // XER
    v = e.xer_value();
    break;
  case 8:
    // LR
    v = e.lr_value();
    break;
  case 9:
    // CTR
    v = e.ctr_value();
    break;
  default:
    XEINSTRNOTIMPLEMENTED();
    return 1;
  }

  e.update_gpr_value(i.XFX.RT, v);

  return 0;
}

XEEMITTER(mftb,         0x7C0002E6, XFX)(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(mtcrf,        0x7C000120, XFX)(X64Emitter& e, jit_function_t f, InstrData& i) {
  XEINSTRNOTIMPLEMENTED();
  return 1;
}

XEEMITTER(mtspr,        0x7C0003A6, XFX)(X64Emitter& e, jit_function_t f, InstrData& i) {
  // n <- spr[5:9] || spr[0:4]
  // if length(SPR(n)) = 64 then
  //   SPR(n) <- (RS)
  // else
  //   SPR(n) <- (RS)[32:63]

  jit_value_t v = e.gpr_value(i.XFX.RT);

  const uint32_t n = ((i.XFX.spr & 0x1F) << 5) | ((i.XFX.spr >> 5) & 0x1F);
  switch (n) {
  case 1:
    // XER
    e.update_xer_value(v);
    break;
  case 8:
    // LR
    e.update_lr_value(v);
    break;
  case 9:
    // CTR
    e.update_ctr_value(v);
    break;
  default:
    XEINSTRNOTIMPLEMENTED();
    return 1;
  }

  return 0;
}


void X64RegisterEmitCategoryControl() {
  XEREGISTERINSTR(bx,           0x48000000);
  XEREGISTERINSTR(bcx,          0x40000000);
  XEREGISTERINSTR(bcctrx,       0x4C000420);
  XEREGISTERINSTR(bclrx,        0x4C000020);
  XEREGISTERINSTR(crand,        0x4C000202);
  XEREGISTERINSTR(crandc,       0x4C000102);
  XEREGISTERINSTR(creqv,        0x4C000242);
  XEREGISTERINSTR(crnand,       0x4C0001C2);
  XEREGISTERINSTR(crnor,        0x4C000042);
  XEREGISTERINSTR(cror,         0x4C000382);
  XEREGISTERINSTR(crorc,        0x4C000342);
  XEREGISTERINSTR(crxor,        0x4C000182);
  XEREGISTERINSTR(mcrf,         0x4C000000);
  XEREGISTERINSTR(sc,           0x44000002);
  XEREGISTERINSTR(td,           0x7C000088);
  XEREGISTERINSTR(tdi,          0x08000000);
  XEREGISTERINSTR(tw,           0x7C000008);
  XEREGISTERINSTR(twi,          0x0C000000);
  XEREGISTERINSTR(mfcr,         0x7C000026);
  XEREGISTERINSTR(mfspr,        0x7C0002A6);
  XEREGISTERINSTR(mftb,         0x7C0002E6);
  XEREGISTERINSTR(mtcrf,        0x7C000120);
  XEREGISTERINSTR(mtspr,        0x7C0003A6);
}


}  // namespace x64
}  // namespace cpu
}  // namespace xe