//===--- DiagnosticAnalysis.h - Diagnostics for libanalysis -----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_ANALYSIS_DIAGNOSTICANALYSIS_H_
#define MLANG_ANALYSIS_DIAGNOSTICANALYSIS_H_

#include "mlang/Diag/Diagnostic.h"

namespace mlang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,CATEGORY,BRIEF,FULL) ENUM,
#define ANALYSISSTART
#include "mlang/Diag/DiagnosticAnalysisKinds.inc"
#undef DIAG
      NUM_BUILTIN_ANALYSIS_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace mlang

#endif /* MLANG_ANALYSIS_DIAGNOSTICANALYSIS_H_ */
