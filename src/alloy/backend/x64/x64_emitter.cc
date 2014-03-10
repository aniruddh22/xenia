/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <alloy/backend/x64/x64_emitter.h>

#include <alloy/backend/x64/x64_backend.h>
#include <alloy/backend/x64/x64_code_cache.h>
#include <alloy/backend/x64/x64_selector_util.h>
#include <alloy/backend/x64/x64_thunk_emitter.h>
#include <alloy/hir/hir_builder.h>
#include <alloy/runtime/debug_info.h>

// TODO(benvanik): fix paths.
#include <x64_selector.y.h>

using namespace alloy;
using namespace alloy::backend;
using namespace alloy::backend::x64;
using namespace alloy::hir;
using namespace alloy::runtime;

using namespace Xbyak;


namespace alloy {
namespace backend {
namespace x64 {

static const size_t MAX_CODE_SIZE = 1 * 1024 * 1024;

void SelectTrace(FILE *stream, char *zPrefix);
void *SelectAlloc(void *(*mallocProc)(size_t));
void SelectFree(void *p, void (*freeProc)(void*));
void Select(void *yyp, int yymajor, void* yyminor, X64Emitter* e);

void SelectValue(void* yyp, Value* value, X64Emitter* e) {
  int yymajor = 0;
  if (value->IsConstant()) {
    // TODO(benvanik): other desired constants (1, 1111, FFFF, etc).
    if (value->IsConstantZero()) {
      static const int __map[] = {
        SEL_CONST_I8_0, SEL_CONST_I16_0, SEL_CONST_I32_0, SEL_CONST_I64_0, SEL_CONST_F32_0, SEL_CONST_F64_0, SEL_CONST_V128_0000,
      };
      yymajor = __map[value->type];
    } else {
      static const int __map[] = {
        SEL_CONST_I8, SEL_CONST_I16, SEL_CONST_I32, SEL_CONST_I64, SEL_CONST_F32, SEL_CONST_F64, SEL_CONST_V128,
      };
      yymajor = __map[value->type];
    }
  } else {
    static const int __map[] = {
      SEL_VALUE_I8, SEL_VALUE_I16, SEL_VALUE_I32, SEL_VALUE_I64, SEL_VALUE_F32, SEL_VALUE_F64, SEL_VALUE_V128,
    };
    yymajor = __map[value->type];
  }
  XEASSERTNOTZERO(yymajor);
  Select(yyp, yymajor, value, e);
}

void SelectOp(void* yyp, OpcodeSignatureType sig_type, Instr::Op* op, X64Emitter* e) {
  switch (sig_type) {
  case OPCODE_SIG_TYPE_X:
    break;
  case OPCODE_SIG_TYPE_L:
    Select(yyp, SEL_LABEL, op->label, e);
    break;
  case OPCODE_SIG_TYPE_O:
    Select(yyp, SEL_OFFSET, (void*)op->offset, e);
    break;
  case OPCODE_SIG_TYPE_S:
    Select(yyp, SEL_SYMBOL_INFO, op->symbol_info, e);
    break;
  case OPCODE_SIG_TYPE_V:
    SelectValue(yyp, op->value, e);
    break;
  }
}

int MapOpcodeToToken(Opcode op) {
  static const int opcode_map[__OPCODE_MAX_VALUE] = {
#define DEFINE_OPCODE(num, name, sig, flags) \
    SEL_##num,
#include <alloy/hir/opcodes.inl>
#undef DEFINE_OPCODE
  };
  return opcode_map[op];
}

}  // namespace x64
}  // namespace backend
}  // namespace alloy


const uint32_t X64Emitter::gpr_reg_map_[X64Emitter::GPR_COUNT] = {
  Operand::RBX,
  Operand::R12, Operand::R13, Operand::R14, Operand::R15,
};

const uint32_t X64Emitter::xmm_reg_map_[X64Emitter::XMM_COUNT] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};


X64Emitter::X64Emitter(X64Backend* backend, XbyakAllocator* allocator) :
    runtime_(backend->runtime()),
    backend_(backend),
    code_cache_(backend->code_cache()),
    allocator_(allocator),
    current_instr_(0),
    CodeGenerator(MAX_CODE_SIZE, AutoGrow, allocator) {
}

X64Emitter::~X64Emitter() {
}

int X64Emitter::Initialize() {
  return 0;
}

int X64Emitter::Emit(
    HIRBuilder* builder,
    uint32_t debug_info_flags, runtime::DebugInfo* debug_info,
    void*& out_code_address, size_t& out_code_size) {
  // Reset.
  if (debug_info_flags & DEBUG_INFO_SOURCE_MAP) {
    source_map_count_ = 0;
    source_map_arena_.Reset();
  }

  // Fill the generator with code.
  size_t stack_size = 0;
  int result = Emit(builder, stack_size);
  if (result) {
    return result;
  }

  // Copy the final code to the cache and relocate it.
  out_code_size = getSize();
  out_code_address = Emplace(stack_size);

  // Stash source map.
  if (debug_info_flags & DEBUG_INFO_SOURCE_MAP) {
    debug_info->InitializeSourceMap(
        source_map_count_,
        (SourceMapEntry*)source_map_arena_.CloneContents());
  }

  return 0;
}

void* X64Emitter::Emplace(size_t stack_size) {
  // To avoid changing xbyak, we do a switcharoo here.
  // top_ points to the Xbyak buffer, and since we are in AutoGrow mode
  // it has pending relocations. We copy the top_ to our buffer, swap the
  // pointer, relocate, then return the original scratch pointer for use.
  uint8_t* old_address = top_;
  void* new_address = code_cache_->PlaceCode(top_, size_, stack_size);
  top_ = (uint8_t*)new_address;
  ready();
  top_ = old_address;
  reset();
  return new_address;
}

int X64Emitter::Emit(HIRBuilder* builder, size_t& out_stack_size) {
  // Calculate stack size. We need to align things to their natural sizes.
  // This could be much better (sort by type/etc).
  auto locals = builder->locals();
  size_t stack_offset = StackLayout::GUEST_STACK_SIZE;
  for (auto it = locals.begin(); it != locals.end(); ++it) {
    auto slot = *it;
    size_t type_size = GetTypeSize(slot->type);
    // Align to natural size.
    stack_offset = XEALIGN(stack_offset, type_size);
    slot->set_constant(stack_offset);
    stack_offset += type_size;
  }
  // Ensure 16b alignment.
  stack_offset -= StackLayout::GUEST_STACK_SIZE;
  stack_offset = XEALIGN(stack_offset, 16);

  // Function prolog.
  // Must be 16b aligned.
  // Windows is very strict about the form of this and the epilog:
  // http://msdn.microsoft.com/en-us/library/tawsa7cb.aspx
  // TODO(benvanik): save off non-volatile registers so we can use them:
  //     RBX, RBP, RDI, RSI, RSP, R12, R13, R14, R15
  //     Only want to do this if we actually use them, though, otherwise
  //     it just adds overhead.
  // IMPORTANT: any changes to the prolog must be kept in sync with
  //     X64CodeCache, which dynamically generates exception information.
  //     Adding or changing anything here must be matched!
  const bool emit_prolog = true;
  const size_t stack_size = StackLayout::GUEST_STACK_SIZE + stack_offset;
  XEASSERT((stack_size + 8) % 16 == 0);
  out_stack_size = stack_size;
  stack_size_ = stack_size;
  if (emit_prolog) {
    sub(rsp, (uint32_t)stack_size);
    mov(qword[rsp + StackLayout::GUEST_RCX_HOME], rcx);
    mov(qword[rsp + StackLayout::GUEST_RET_ADDR], rdx);
    mov(qword[rsp + StackLayout::GUEST_CALL_RET_ADDR], 0);
    // ReloadRDX:
    mov(rdx, qword[rcx + 8]); // membase
  }

  void* selector = SelectAlloc(malloc);
  SelectTrace(stdout, "sel ");

  Select(selector, SEL_FUNCTION_BEGIN, builder, this);

  // Body.
  auto block = builder->first_block();
  while (block) {
    // Mark block labels.
    auto label = block->label_head;
    while (label) {
      L(label->name);
      label = label->next;
    }

    Select(selector, SEL_BLOCK_BEGIN, block, this);

    // Process instructions.
    auto instr = block->instr_head;
    current_instr_ = instr;
    while (instr) {
      bool processed = true;

      const OpcodeInfo* info = instr->opcode;

      OpcodeSignatureType dest_type = GET_OPCODE_SIG_TYPE_DEST(info->signature);
      if (dest_type) {
        SelectValue(selector, instr->dest, this);
      }

      int yymajor = MapOpcodeToToken(info->num);
      Select(selector, yymajor, instr, this);

      Select(selector, SEL_FLAGS, (void*)instr->flags, this);

      OpcodeSignatureType src1_type = GET_OPCODE_SIG_TYPE_SRC1(info->signature);
      if (src1_type) {
        SelectOp(selector, src1_type, &instr->src1, this);
      }
      OpcodeSignatureType src2_type = GET_OPCODE_SIG_TYPE_SRC2(info->signature);
      if (src2_type) {
        SelectOp(selector, src2_type, &instr->src2, this);
      }
      OpcodeSignatureType src3_type = GET_OPCODE_SIG_TYPE_SRC3(info->signature);
      if (src3_type) {
        SelectOp(selector, src3_type, &instr->src3, this);
      }

      if (!processed) {
        // No sequence found!
        XELOGE("Unable to process HIR opcode %s", instr->opcode->name);
        break;
      }

      instr = Advance(instr);
    }

    Select(selector, SEL_BLOCK_END, block, this);

    block = block->next;
  }

  Select(selector, SEL_FUNCTION_END, builder, this);
  SelectFree(selector, free);

  // Function epilog.
  L("epilog");
  if (emit_prolog) {
    mov(rcx, qword[rsp + StackLayout::GUEST_RCX_HOME]);
    add(rsp, (uint32_t)stack_size);
  }
  ret();

#if XE_DEBUG
  nop();
  nop();
  nop();
  nop();
  nop();
#endif  // XE_DEBUG

  return 0;
}

Instr* X64Emitter::Advance(Instr* i) {
  auto next = i->next;
  current_instr_ = next;
  return next;
}

void X64Emitter::MarkSourceOffset(Instr* i) {
  auto entry = source_map_arena_.Alloc<SourceMapEntry>();
  entry->source_offset  = i->src1.offset;
  entry->hir_offset     = uint32_t(i->block->ordinal << 16) | i->ordinal;
  entry->code_offset    = getSize();
  source_map_count_++;
}
