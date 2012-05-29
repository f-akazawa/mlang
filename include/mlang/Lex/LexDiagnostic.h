//===--- LexDiagnostic.h - Diagnostics for libMlangLex  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the enum const Diagnostics for libMlangLex.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_LEX_LEXDIAGNOSTIC_H_
#define MLANG_LEX_LEXDIAGNOSTIC_H_

#include "mlang/Diag/Diagnostic.h"

namespace mlang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,CATEGORY,BRIEF,FULL) ENUM,
#define LEXSTART
#include "mlang/Diag/DiagnosticLexKinds.inc"
#undef DIAG
      NUM_BUILTIN_LEX_DIAGNOSTICS
    };
  }  // end namespace diag
} // end namespace mlang

#endif /* LEXDIAGNOSTIC_H_ */
