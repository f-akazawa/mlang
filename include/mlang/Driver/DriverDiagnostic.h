//===--- DiagnosticDriver.h - Diagnostics for libdriver ---------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the enum const Diagnostics for libMlangDriver.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_DIAG_DRIVER_DIAGNOSTIC_H_
#define MLANG_DIAG_DRIVER_DIAGNOSTIC_H_

#include "mlang/Diag/Diagnostic.h"

namespace mlang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,CATEGORY,BRIEF,FULL) ENUM,
#define DRIVERSTART
#include "mlang/Diag/DiagnosticDriverKinds.inc"
#undef DIAG
      NUM_BUILTIN_DRIVER_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace mlang

#endif /* MLANG_DIAG_DRIVER_DIAGNOSTIC_H_ */
