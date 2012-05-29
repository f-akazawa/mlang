//===--- DiagnosticSema.h - Diagnostics for libsema -------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the enum const Diagnostics for libMlangSema.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_DIAG_DIAGNOSTICSEMA_H_
#define MLANG_DIAG_DIAGNOSTICSEMA_H_

#include "mlang/Diag/Diagnostic.h"

namespace mlang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,CATEGORY,BRIEF,FULL) ENUM,
#define SEMASTART
#include "mlang/Diag/DiagnosticSemaKinds.inc"
#undef DIAG
      NUM_BUILTIN_SEMA_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace mlang

#endif /* MLANG_DIAG_DIAGNOSTICSEMA_H_ */
