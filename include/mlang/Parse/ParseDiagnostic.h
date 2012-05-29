//===--- DiagnosticParse.h - Diagnostics for libparse -----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the enum const Diagnostics for libMlangParse.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_DIAG_DIAGNOSTIC_PARSE_H_
#define MLANG_DIAG_DIAGNOSTIC_PARSE_H_

#include "mlang/Diag/Diagnostic.h"

namespace mlang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,CATEGORY,BRIEF,FULL) ENUM,
#define PARSESTART
#include "mlang/Diag/DiagnosticParseKinds.inc"
#undef DIAG
      NUM_BUILTIN_PARSE_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace mlang

#endif /* MLANG_DIAG_DIAGNOSTIC_PARSE_H_ */
