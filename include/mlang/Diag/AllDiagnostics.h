//===--- AllDiagnostics.h - Aggregate Diagnostic headers --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file includes all the separate Diagnostic headers & some related
//  helpers.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_ALL_DIAGNOSTICS_H
#define MLANG_ALL_DIAGNOSTICS_H

#include "mlang/AST/ASTDiagnostic.h"
#include "mlang/Analysis/AnalysisDiagnostic.h"
#include "mlang/Driver/DriverDiagnostic.h"
#include "mlang/Frontend/FrontendDiagnostic.h"
#include "mlang/Lex/LexDiagnostic.h"
#include "mlang/Parse/ParseDiagnostic.h"
#include "mlang/Sema/SemaDiagnostic.h"
#include "mlang/Serialization/SerializationDiagnostic.h"

namespace mlang {
template <size_t SizeOfStr, typename FieldType>
class StringSizerHelper {
  char FIELD_TOO_SMALL[SizeOfStr <= FieldType(~0U) ? 1 : -1];
public:
  enum { Size = SizeOfStr };
};
} // end namespace mlang

#define STR_SIZE(str, fieldTy) mlang::StringSizerHelper<sizeof(str)-1, \
                                                        fieldTy>::Size

#endif
