/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef ALLOY_BACKEND_X64_LIR_LIR_INSTR_H_
#define ALLOY_BACKEND_X64_LIR_LIR_INSTR_H_

#include <alloy/core.h>


namespace alloy {
namespace backend {
namespace x64 {
namespace lir {

class LIRBlock;
class LIROpcodeInfo;


class LIRInstr {
public:
  LIRBlock*   block;
  LIRInstr*   next;
  LIRInstr*   prev;

  const LIROpcodeInfo* opcode;
  uint16_t  flags;
};


}  // namespace lir
}  // namespace x64
}  // namespace backend
}  // namespace alloy


#endif  // ALLOY_BACKEND_X64_LIR_LIR_INSTR_H_
