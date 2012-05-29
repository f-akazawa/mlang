//===--- ParseAST.h - Define the ParseAST method ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the mlang::ParseAST method.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_PARSE_PARSEAST_H
#define MLANG_PARSE_PARSEAST_H

namespace mlang {
  class Preprocessor;
  class ASTConsumer;
  class ASTContext;
  class Sema;

  /// \brief Parse the entire file specified, notifying the ASTConsumer as
  /// the file is parsed.
  ///
  /// This operation inserts the parsed defns into the translation
  /// unit held by Ctx.
  ///
  /// \param CompleteTranslationUnit When true, the parsed file is
  /// considered to be a complete translation unit, and any
  /// end-of-translation-unit wrapup will be performed.
  ///
  /// \param CompletionConsumer If given, an object to consume code completion
  /// results.
  void ParseAST(Preprocessor &pp, ASTConsumer *C,
                ASTContext &Ctx, bool PrintStats = false,
                bool CompleteTranslationUnit = true);

  /// \brief Parse the main file known to the preprocessor, producing an 
  /// abstract syntax tree.
  void ParseAST(Sema &S, bool PrintStats = false);
  
}  // end namespace mlang

#endif
