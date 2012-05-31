//===--- ASTDiagnostic.h - Diagnostics for the AST library ------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the enum const Diagnostics for libMlangAST.
//
//===----------------------------------------------------------------------===/
#ifndef MLANG_DIAG_DIAGNOSTICAST_H_
#define MLANG_DIAG_DIAGNOSTICAST_H_

#include "mlang/Diag/Diagnostic.h"

namespace mlang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define ASTSTART
#include "mlang/Diag/DiagnosticASTKinds.inc"
#undef DIAG
      NUM_BUILTIN_AST_DIAGNOSTICS
    };
  }  // end namespace diag

  /// \brief Diagnostic argument formatting function for diagnostics that
  /// involve AST nodes.
  ///
  /// This function formats diagnostic arguments for various AST nodes,
  /// including types, declaration names, nested name specifiers, and
  /// declaration contexts, into strings that can be printed as part of
  /// diagnostics. It is meant to be used as the argument to
  /// \c Diagnostic::SetArgToStringFn(), where the cookie is an \c ASTContext
  /// pointer.
  void FormatASTNodeDiagnosticArgument(
      DiagnosticsEngine::ArgumentKind Kind,
      intptr_t Val,
      const char *Modifier,
      unsigned ModLen,
      const char *Argument,
      unsigned ArgLen,
      const DiagnosticsEngine::ArgumentValue *PrevArgs,
      unsigned NumPrevArgs,
      llvm::SmallVectorImpl<char> &Output,
      void *Cookie);
}  // end namespace mlang

#endif /* MLANG_DIAG_DIAGNOSTICAST_H_ */
