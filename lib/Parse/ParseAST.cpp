//===--- ParseAST.cpp - Provide the mlang::ParseAST method ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the mlang::ParseAST method.
//
//===----------------------------------------------------------------------===//

#include "mlang/Parse/ParseAST.h"
#include "mlang/Sema/Sema.h"
#include "mlang/Sema/SemaConsumer.h"
// #include "mlang/Sema/ExternalSemaSource.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/ExternalASTSource.h"
#include "mlang/AST/Stmt.h"
#include "mlang/AST/Type.h"
#include "mlang/Parse/Parser.h"
#include <cstdio>

using namespace mlang;

static void DumpRecordLayouts(ASTContext &C) {
  for (ASTContext::type_iterator I = C.types_begin(), E = C.types_end();
       I != E; ++I) {
    const ClassdefType *RT = dyn_cast<ClassdefType>(*I);
    if (!RT)
      continue;

    const UserClassDefn *RD = dyn_cast<UserClassDefn>(RT->getDefn());
    if (!RD || RD->isImplicit())
      continue;

    // FIXME: Do we really need to hard code this?
    if (RD->getNameAsString() == "__va_list_tag")
      continue;

    C.DumpRecordLayout(RD, llvm::errs());
  }
}

//===----------------------------------------------------------------------===//
// Public interface to the file
//===----------------------------------------------------------------------===//

/// ParseAST - Parse the entire file specified, notifying the ASTConsumer as
/// the file is parsed.  This inserts the parsed defns into the translation unit
/// held by Ctx.
///
void mlang::ParseAST(Preprocessor &PP, ASTConsumer *Consumer,
                     ASTContext &Ctx, bool PrintStats,
                     bool CompleteTranslationUnit) {
  Sema S(PP, Ctx, *Consumer, CompleteTranslationUnit);
  ParseAST(S, PrintStats);
}

void mlang::ParseAST(Sema &S, bool PrintStats) {
  // Collect global stats on Defns/Stmts (until we have a module streamer).
  if (PrintStats) {
    Defn::CollectingStats(true);
    Stmt::CollectingStats(true);
  }

  ASTConsumer *Consumer = &S.getASTConsumer();

  Parser P(S.getPreprocessor(), S);
  S.getPreprocessor().EnterMainSourceFile();
  P.Initialize();
  S.Initialize();
  
  if (ExternalASTSource *External = S.getASTContext().getExternalSource())
    External->StartTranslationUnit(Consumer);
  
  Parser::DefnGroupPtrTy ADefn;
  
  while (!P.ParseTopLevelDefn(ADefn)) {  // Not end of file.
    // If we got a null return and something *was* parsed, ignore it.  This
    // is due to a top-level semicolon, an action override, or a parse error
    // skipping something.
    if (ADefn)
      Consumer->HandleTopLevelDefn(ADefn.get());
  };

  // Dump record layouts, if requested.
  if (S.getLangOptions().DumpRecordLayouts)
    DumpRecordLayouts(S.getASTContext());
  
  Consumer->HandleTranslationUnit(S.getASTContext());
  
  if (PrintStats) {
    fprintf(stderr, "\nSTATISTICS:\n");
    P.getActions().PrintStats();
    S.getASTContext().PrintStats();
    Defn::PrintStats();
    Stmt::PrintStats();
    Consumer->PrintStats();
  }
}
