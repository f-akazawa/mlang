//===--- SerializationDiagnostic.h - Serialization Diagnostics -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SERIALIZATIONDIAGNOSTIC_H
#define MLANG_SERIALIZATIONDIAGNOSTIC_H

#include "mlang/Diag/Diagnostic.h"

namespace mlang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define SERIALIZATIONSTART
#include "mlang/Diag/DiagnosticSerializationKinds.inc"
#undef DIAG
      NUM_BUILTIN_SERIALIZATION_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace mlang

#endif
