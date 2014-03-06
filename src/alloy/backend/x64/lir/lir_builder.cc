/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <alloy/backend/x64/lir/lir_builder.h>

using namespace alloy;
using namespace alloy::backend::x64::lir;


LIRBuilder::LIRBuilder() {
  arena_ = new Arena();
  Reset();
}

LIRBuilder::~LIRBuilder() {
  Reset();
  delete arena_;
}

void LIRBuilder::Reset() {
  attributes_ = 0;
  next_label_id_ = 0;
  block_head_ = block_tail_ = NULL;
  current_block_ = NULL;
#if XE_DEBUG
  arena_->DebugFill();
#endif
  arena_->Reset();
}

int LIRBuilder::Finalize() {
  return 0;
}

void LIRBuilder::Dump(StringBuffer* str) {
  if (attributes_) {
    str->Append("; attributes = %.8X\n", attributes_);
  }

  uint32_t block_ordinal = 0;
  LIRBlock* block = block_head_;
  while (block) {
    if (block == block_head_) {
      str->Append("<entry>:\n");
    } else if (!block->label_head) {
      str->Append("<block%d>:\n", block_ordinal);
    }
    block_ordinal++;

    LIRLabel* label = block->label_head;
    while (label) {
      if (label->name) {
        str->Append("%s:\n", label->name);
      } else {
        str->Append("label%d:\n", label->id);
      }
      label = label->next;
    }

    LIRInstr* i = block->instr_head;
    while (i) {
      str->Append("\n");
      i = i->next;
    }

    block = block->next;
  }
}

LIRBlock* LIRBuilder::current_block() const {
  return current_block_;
}

LIRInstr* LIRBuilder::last_instr() const {
  if (current_block_ && current_block_->instr_tail) {
    return current_block_->instr_tail;
  } else if (block_tail_) {
    return block_tail_->instr_tail;
  }
  return NULL;
}

LIRLabel* LIRBuilder::NewLabel() {
  LIRLabel* label = arena_->Alloc<LIRLabel>();
  label->next = label->prev = NULL;
  label->block = NULL;
  label->id = next_label_id_++;
  label->name = NULL;
  return label;
}

void LIRBuilder::MarkLabel(LIRLabel* label, LIRBlock* block) {
  if (!block) {
    if (current_block_ && current_block_->instr_tail) {
      EndBlock();
    }
    if (!current_block_) {
      AppendBlock();
    }
    block = current_block_;
  }
  label->block = block;
  label->prev = block->label_tail;
  label->next = NULL;
  if (label->prev) {
    label->prev->next = label;
    block->label_tail = label;
  } else {
    block->label_head = block->label_tail = label;
  }
}

LIRBlock* LIRBuilder::AppendBlock() {
  LIRBlock* block = arena_->Alloc<LIRBlock>();
  block->arena = arena_;
  block->next = NULL;
  block->prev = block_tail_;
  if (block_tail_) {
    block_tail_->next = block;
  }
  block_tail_ = block;
  if (!block_head_) {
    block_head_ = block;
  }
  current_block_ = block;
  block->label_head = block->label_tail = NULL;
  block->instr_head = block->instr_tail = NULL;
  return block;
}

void LIRBuilder::EndBlock() {
  if (current_block_ && !current_block_->instr_tail) {
    // Block never had anything added to it. Since it likely has an
    // incoming edge, just keep it around.
    return;
  }
  current_block_ = NULL;
}
