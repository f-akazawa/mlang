//===--- TargetBuiltins.h - Target specific builtin IDs -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_TARGET_BUILTINS_H
#define MLANG_BASIC_TARGET_BUILTINS_H

#include "mlang/Basic/Builtins.h"
#undef PPC

namespace mlang {

  /// ARM builtins
  namespace ARM {
    enum {
        LastTIBuiltin = mlang::Builtin::FirstTSBuiltin-1,
#define BUILTIN(ID, TYPE, ATTRS) BI##ID,
#include "mlang/Basic/BuiltinsARM.def"
        LastTSBuiltin
    };
  }

  /// NVPTX builtins
  namespace NVPTX {
    enum {
        LastTIBuiltin = mlang::Builtin::FirstTSBuiltin-1,
#define BUILTIN(ID, TYPE, ATTRS) BI##ID,
#include "mlang/Basic/BuiltinsNVPTX.def"
        LastTSBuiltin
    };
  }


  /// X86 builtins
  namespace X86 {
    enum {
        LastTIBuiltin = mlang::Builtin::FirstTSBuiltin-1,
#define BUILTIN(ID, TYPE, ATTRS) BI##ID,
#include "mlang/Basic/BuiltinsX86.def"
        LastTSBuiltin
    };
  }
} // end namespace mlang.

#endif
