//===--- DiagnosticFrontend.h - Diagnostics for frontend --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_DIAGNOSTIC_H_
#define MLANG_FRONTEND_DIAGNOSTIC_H_

#include "mlang/Diag/Diagnostic.h"

namespace mlang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,CATEGORY,BRIEF,FULL) ENUM,
#define FRONTENDSTART
#include "mlang/Diag/DiagnosticFrontendKinds.inc"
#undef DIAG
      NUM_BUILTIN_FRONTEND_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace mlang

#endif /* MLANG_FRONTEND_DIAGNOSTIC_H_ */
