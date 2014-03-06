/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

%name Select
%namespace_begin { NAMESPACE_BEGIN }
%namespace_end { NAMESPACE_END }
%extra_argument { X64Emitter* e }
%start_symbol function
%token_prefix SEL_

%include {
#include <alloy/backend/x64/x64_selector_util.h>
#include <alloy/hir/instr.h>
#include <alloy/hir/value.h>

using namespace alloy::hir;
using namespace alloy::runtime;
}

function ::= function_begin block_list function_end .

%type function_begin {HIRBuilder*}
function_begin ::= FUNCTION_BEGIN . {
  //
}
%type function_end {HIRBuilder*}
function_end ::= FUNCTION_END . {
  //
}

block_list ::= block_list block .
block_list ::= .

block ::= block_begin instr_list block_end .

%type block_begin {Block*}
block_begin ::= BLOCK_BEGIN . {
  //
}
%type block_end {Block*}
block_end ::= BLOCK_END . {
  //
}

instr_list ::= instr_list instr .
instr_list ::= .

%type flags {uint32_t}
flags(D) ::= FLAGS(S) .                     { D = (uint32_t)S; }

%type label {Label*}
label(D) ::= LABEL(S) .                     { D = (Label*)S; }

%type symbol_info {FunctionInfo*}
symbol_info(D) ::= SYMBOL_INFO(S) .         { D = (FunctionInfo*)S; }

%type offset {uint64_t}
offset(D) ::= OFFSET(S) .                   { D = (uint64_t)S; }

%type const_i8 {Value*}
const_i8_0(D) ::= CONST_I8_0(S) .           { D = (Value*)S; }
const_i8_1(D) ::= CONST_I8_1(S) .           { D = (Value*)S; }
const_i8_F(D) ::= CONST_I8_F(S) .           { D = (Value*)S; }
const_i8(D) ::= CONST_I8(S) .               { D = (Value*)S; }
const_i8(D) ::= const_i8_0(S) .             { D = (Value*)S; }
const_i8(D) ::= const_i8_1(S) .             { D = (Value*)S; }
const_i8(D) ::= const_i8_F(S) .             { D = (Value*)S; }

%type const_i16 {Value*}
const_i16_0(D) ::= CONST_I16_0(S) .         { D = (Value*)S; }
const_i16_1(D) ::= CONST_I16_1(S) .         { D = (Value*)S; }
const_i16_F(D) ::= CONST_I16_F(S) .         { D = (Value*)S; }
const_i16(D) ::= CONST_I16(S) .             { D = (Value*)S; }
const_i16(D) ::= const_i16_0(S) .           { D = (Value*)S; }
const_i16(D) ::= const_i16_1(S) .           { D = (Value*)S; }
const_i16(D) ::= const_i16_F(S) .           { D = (Value*)S; }

%type const_i32 {Value*}
const_i32_0(D) ::= CONST_I32_0(S) .         { D = (Value*)S; }
const_i32_1(D) ::= CONST_I32_1(S) .         { D = (Value*)S; }
const_i32_F(D) ::= CONST_I32_F(S) .         { D = (Value*)S; }
const_i32(D) ::= CONST_I32(S) .             { D = (Value*)S; }
const_i32(D) ::= const_i32_0(S) .           { D = (Value*)S; }
const_i32(D) ::= const_i32_1(S) .           { D = (Value*)S; }
const_i32(D) ::= const_i32_F(S) .           { D = (Value*)S; }

%type const_i64 {Value*}
const_i64_0(D) ::= CONST_I64_0(S) .         { D = (Value*)S; }
const_i64_1(D) ::= CONST_I64_1(S) .         { D = (Value*)S; }
const_i64_F(D) ::= CONST_I64_F(S) .         { D = (Value*)S; }
const_i64(D) ::= CONST_I64(S) .             { D = (Value*)S; }
const_i64(D) ::= const_i64_0(S) .           { D = (Value*)S; }
const_i64(D) ::= const_i64_1(S) .           { D = (Value*)S; }
const_i64(D) ::= const_i64_F(S) .           { D = (Value*)S; }

%type const_f32 {Value*}
const_f32_0(D) ::= CONST_F32_0(S) .         { D = (Value*)S; }
const_f32_1(D) ::= CONST_F32_1(S) .         { D = (Value*)S; }
const_f32_F(D) ::= CONST_F32_F(S) .         { D = (Value*)S; }
const_f32_N1(D) ::= CONST_F32_N1(S) .       { D = (Value*)S; }
const_f32(D) ::= CONST_F32(S) .             { D = (Value*)S; }
const_f32(D) ::= const_f32_0(S) .           { D = (Value*)S; }
const_f32(D) ::= const_f32_1(S) .           { D = (Value*)S; }
const_f32(D) ::= const_f32_F(S) .           { D = (Value*)S; }
const_f32(D) ::= const_f32_N1(S) .          { D = (Value*)S; }

%type const_f64 {Value*}
const_f64_0(D) ::= CONST_F64_0(S) .         { D = (Value*)S; }
const_f64_1(D) ::= CONST_F64_1(S) .         { D = (Value*)S; }
const_f64_F(D) ::= CONST_F64_F(S) .         { D = (Value*)S; }
const_f64_N1(D) ::= CONST_F64_N1(S) .       { D = (Value*)S; }
const_f64(D) ::= CONST_F64(S) .             { D = (Value*)S; }
const_f64(D) ::= const_f64_0(S) .           { D = (Value*)S; }
const_f64(D) ::= const_f64_1(S) .           { D = (Value*)S; }
const_f64(D) ::= const_f64_F(S) .           { D = (Value*)S; }
const_f64(D) ::= const_f64_N1(S) .          { D = (Value*)S; }

%type const_v128 {Value*}
const_v128_0000(D) ::= CONST_V128_0000(S) . { D = (Value*)S; }
const_v128_1111(D) ::= CONST_V128_1111(S) . { D = (Value*)S; }
const_v128_FFFF(D) ::= CONST_v128_FFFF(S) . { D = (Value*)S; }
const_v128_0001(D) ::= CONST_V128_0001(S) . { D = (Value*)S; }
const_v128(D) ::= CONST_V128(S) .           { D = (Value*)S; }
const_v128(D) ::= const_v128_0000(S) .      { D = (Value*)S; }
const_v128(D) ::= const_v128_1111(S) .      { D = (Value*)S; }
const_v128(D) ::= const_v128_FFFF(S) .      { D = (Value*)S; }
const_v128(D) ::= const_v128_0001(S) .      { D = (Value*)S; }

%type const_i {Value*}
const_i(D) ::= const_i8(S) .                { D = (Value*)S; }
const_i(D) ::= const_i16(S) .               { D = (Value*)S; }
const_i(D) ::= const_i32(S) .               { D = (Value*)S; }
const_i(D) ::= const_i64(S) .               { D = (Value*)S; }
%type const_f {Value*}
const_f(D) ::= const_f32(S) .               { D = (Value*)S; }
const_f(D) ::= const_f64(S) .               { D = (Value*)S; }
%type const_v {Value*}
const_v(D) ::= const_v128(S) .              { D = (Value*)S; }
%type const {Value*}
const(D) ::= const_i(S) .                   { D = (Value*)S; }
const(D) ::= const_f(S) .                   { D = (Value*)S; }
const(D) ::= const_v(S) .                   { D = (Value*)S; }

%type value_i8 {Value*}
value_i8(D) ::= VALUE_I8(S) .               { D = (Value*)S; }
%type value_i16 {Value*}
value_i16(D) ::= VALUE_I16(S) .             { D = (Value*)S; }
%type value_i32 {Value*}
value_i32(D) ::= VALUE_I32(S) .             { D = (Value*)S; }
%type value_i64 {Value*}
value_i64(D) ::= VALUE_I64(S) .             { D = (Value*)S; }
%type value_f32 {Value*}
value_f32(D) ::= VALUE_F32(S) .             { D = (Value*)S; }
%type value_f64 {Value*}
value_f64(D) ::= VALUE_F64(S) .             { D = (Value*)S; }
%type value_v128 {Value*}
value_v128(D) ::= VALUE_V128(S) .           { D = (Value*)S; }

%type value_i {Value*}
value_i(D) ::= value_i8(S) .                { D = (Value*)S; }
value_i(D) ::= value_i16(S) .               { D = (Value*)S; }
value_i(D) ::= value_i32(S) .               { D = (Value*)S; }
value_i(D) ::= value_i64(S) .               { D = (Value*)S; }
%type value_f {Value*}
value_f(D) ::= value_f32(S) .               { D = (Value*)S; }
value_f(D) ::= value_f64(S) .               { D = (Value*)S; }
%type value_v {Value*}
value_v(D) ::= value_v128(S) .              { D = (Value*)S; }
%type value {Value*}
value(D) ::= value_i(S) .                   { D = (Value*)S; }
value(D) ::= value_f(S) .                   { D = (Value*)S; }
value(D) ::= value_v(S) .                   { D = (Value*)S; }


instr ::= OPCODE_COMMENT flags offset(O) . {
  fprintf(stdout, "comment: %s\n", (const char*)O);
  fflush(stdout);
}

instr ::= OPCODE_NOP .


instr ::= value_i8 OPCODE_ADD flags value_i8 value_i8 . {
  //
  printf("hi!");
}
instr ::= value_i8 OPCODE_ADD flags value_i8 const_i8 . {
  //
}
instr ::= value_i8 OPCODE_ADD flags const_i8 value_i8 . {
  //
}

instr ::= value_i64 OPCODE_ADD flags value_i64 value_i64 . {
  //
  printf("hi!");
  __debugbreak();
}
instr ::= value_i64 OPCODE_ADD flags value_i64 const_i64 . {
  //
  printf("hi!");
  __debugbreak();
}
instr ::= value_i64 OPCODE_ADD flags const_i64 value_i64 . {
  //
  printf("hi!");
  __debugbreak();
}







/* Sinks to get terminals into the headers and to prevent reduction errors. */
opcode_term ::=
  OPCODE_COMMENT
  OPCODE_NOP

  OPCODE_SOURCE_OFFSET

  OPCODE_DEBUG_BREAK
  OPCODE_DEBUG_BREAK_TRUE

  OPCODE_TRAP
  OPCODE_TRAP_TRUE

  OPCODE_CALL
  OPCODE_CALL_TRUE
  OPCODE_CALL_INDIRECT
  OPCODE_CALL_INDIRECT_TRUE
  OPCODE_CALL_EXTERN
  OPCODE_RETURN
  OPCODE_RETURN_TRUE
  OPCODE_SET_RETURN_ADDRESS

  OPCODE_BRANCH
  OPCODE_BRANCH_TRUE
  OPCODE_BRANCH_FALSE

  OPCODE_ASSIGN
  OPCODE_CAST
  OPCODE_ZERO_EXTEND
  OPCODE_SIGN_EXTEND
  OPCODE_TRUNCATE
  OPCODE_CONVERT
  OPCODE_ROUND
  OPCODE_VECTOR_CONVERT_I2F
  OPCODE_VECTOR_CONVERT_F2I

  OPCODE_LOAD_VECTOR_SHL
  OPCODE_LOAD_VECTOR_SHR

  OPCODE_LOAD_CLOCK

  OPCODE_LOAD_LOCAL
  OPCODE_STORE_LOCAL

  OPCODE_LOAD_CONTEXT
  OPCODE_STORE_CONTEXT

  OPCODE_LOAD
  OPCODE_STORE
  OPCODE_PREFETCH

  OPCODE_MAX
  OPCODE_MIN
  OPCODE_SELECT
  OPCODE_IS_TRUE
  OPCODE_IS_FALSE
  OPCODE_COMPARE_EQ
  OPCODE_COMPARE_NE
  OPCODE_COMPARE_SLT
  OPCODE_COMPARE_SLE
  OPCODE_COMPARE_SGT
  OPCODE_COMPARE_SGE
  OPCODE_COMPARE_ULT
  OPCODE_COMPARE_ULE
  OPCODE_COMPARE_UGT
  OPCODE_COMPARE_UGE
  OPCODE_DID_CARRY
  OPCODE_DID_OVERFLOW
  OPCODE_DID_SATURATE
  OPCODE_VECTOR_COMPARE_EQ
  OPCODE_VECTOR_COMPARE_SGT
  OPCODE_VECTOR_COMPARE_SGE
  OPCODE_VECTOR_COMPARE_UGT
  OPCODE_VECTOR_COMPARE_UGE

  OPCODE_ADD
  OPCODE_ADD_CARRY
  OPCODE_VECTOR_ADD
  OPCODE_SUB
  OPCODE_MUL
  OPCODE_MUL_HI
  OPCODE_DIV
  OPCODE_MUL_ADD
  OPCODE_MUL_SUB
  OPCODE_NEG
  OPCODE_ABS
  OPCODE_SQRT
  OPCODE_RSQRT
  OPCODE_POW2
  OPCODE_LOG2
  OPCODE_DOT_PRODUCT_3
  OPCODE_DOT_PRODUCT_4

  OPCODE_AND
  OPCODE_OR
  OPCODE_XOR
  OPCODE_NOT
  OPCODE_SHL
  OPCODE_VECTOR_SHL
  OPCODE_SHR
  OPCODE_VECTOR_SHR
  OPCODE_SHA
  OPCODE_VECTOR_SHA
  OPCODE_ROTATE_LEFT
  OPCODE_BYTE_SWAP
  OPCODE_CNTLZ
  OPCODE_INSERT
  OPCODE_EXTRACT
  OPCODE_SPLAT
  OPCODE_PERMUTE
  OPCODE_SWIZZLE
  OPCODE_PACK
  OPCODE_UNPACK

  OPCODE_COMPARE_EXCHANGE
  OPCODE_ATOMIC_EXCHANGE
  OPCODE_ATOMIC_ADD
  OPCODE_ATOMIC_SUB
  .
dummy ::= label symbol_info offset const value opcode_term .
instr ::= INVALID dummy .
