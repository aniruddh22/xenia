/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef ALLOY_BACKEND_X64_LIR_LIR_BUILDER_H_
#define ALLOY_BACKEND_X64_LIR_LIR_BUILDER_H_

#include <alloy/core.h>
#include <alloy/backend/x64/lir/lir_block.h>
#include <alloy/backend/x64/lir/lir_instr.h>
#include <alloy/backend/x64/lir/lir_label.h>


namespace alloy {
namespace backend {
namespace x64 {
namespace lir {


enum LIRFunctionAttributes {
  //LIR_FUNCTION_ATTRIB_INLINE    = (1 << 1),
};


class LIRBuilder {
public:
  LIRBuilder();
  ~LIRBuilder();

  void Reset();
  int Finalize();

  void Dump(StringBuffer* str);

  Arena* arena() const { return arena_; }

  uint32_t attributes() const { return attributes_; }
  void set_attributes(uint32_t value) { attributes_ = value; }

  LIRBlock* first_block() const { return block_head_; }
  LIRBlock* last_block() const { return block_tail_; }
  LIRBlock* current_block() const;
  LIRInstr* last_instr() const;

  LIRLabel* NewLabel();
  void MarkLabel(LIRLabel* label, LIRBlock* block = 0);

private:
  LIRBlock* AppendBlock();
  void EndBlock();

private:
  Arena*    arena_;

  uint32_t  attributes_;

  uint32_t  next_label_id_;

  LIRBlock*	block_head_;
  LIRBlock* block_tail_;
  LIRBlock* current_block_;
};


}  // namespace lir
}  // namespace x64
}  // namespace backend
}  // namespace alloy


#endif  // ALLOY_BACKEND_X64_LIR_LIR_BUILDER_H_
