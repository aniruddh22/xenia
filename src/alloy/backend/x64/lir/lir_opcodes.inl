/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */


DEFINE_OPCODE(
    LIR_OPCODE_COMMENT,
    "comment",
    LIR_OPCODE_SIG_X_O,
    LIR_OPCODE_FLAG_IGNORE)

DEFINE_OPCODE(
    LIR_OPCODE_NOP,
    "nop",
    LIR_OPCODE_SIG_X,
    LIR_OPCODE_FLAG_IGNORE)

DEFINE_OPCODE(
    LIR_OPCODE_SOURCE_OFFSET,
    "source_offset",
    LIR_OPCODE_SIG_X_O,
    LIR_OPCODE_FLAG_IGNORE | LIR_OPCODE_FLAG_HIDE)
