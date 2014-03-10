/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef ALLOY_BACKEND_X64_LOWERING_X64_SELECTOR_UTIL_H_
#define ALLOY_BACKEND_X64_LOWERING_X64_SELECTOR_UTIL_H_

#include <alloy/core.h>
#include <alloy/backend/x64/x64_emitter.h>


XEDECLARECLASS2(alloy, hir, Block);
XEDECLARECLASS2(alloy, hir, HIRBuilder);
XEDECLARECLASS2(alloy, hir, Instr);
XEDECLARECLASS2(alloy, hir, Label);
XEDECLARECLASS2(alloy, hir, Value);
XEDECLARECLASS2(alloy, runtime, FunctionInfo);


#define NAMESPACE_BEGIN \
    namespace alloy { \
    namespace backend { \
    namespace x64 {
#define NAMESPACE_END \
    } \
    } \
    }


#if XE_COMPILER_MSVC
// warning C4065: switch statement contains 'default' but no 'case' labels.
#pragma warning(disable: 4065)
#endif  // XE_COMPILER_MSVC


NAMESPACE_BEGIN

//

NAMESPACE_END


#endif  // ALLOY_BACKEND_X64_LOWERING_X64_SELECTOR_UTIL_H_
