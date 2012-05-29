//===--- SemaInit.cpp - Semantic Analysis for Initializers ----------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements semantic analysis for initializers. The main entry
// point is Sema::CheckInitList(), but all of the work is performed
// within the InitListChecker class.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
#include "mlang/Sema/Lookup.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/ExprAll.h"
#include "llvm/Support/ErrorHandling.h"
#include <map>
using namespace mlang;

//===----------------------------------------------------------------------===//
// Sema Initialization Checking
//===----------------------------------------------------------------------===//
static Expr *IsStringInit(Expr *Init, Type DefnType, ASTContext &Context) {
  return NULL;
}

static void CheckStringInit(Expr *Str, Type &DefnT, Sema &S) {

}

//===----------------------------------------------------------------------===//
// Semantic checking for initializer lists.
//===----------------------------------------------------------------------===//
namespace {
class InitListChecker {
	Sema &SemaRef;
	bool hadError;
public:
  InitListChecker(Sema &S, /*const InitializedEntity &Entity,
                  InitListExpr *IL,*/ Type &T);
  bool HadError() { return hadError; }
};
}



//===----------------------------------------------------------------------===//
// Initialization entity
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
// Initialization sequence
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
// Attempt initialization
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
// Perform initialization
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
// Diagnose initialization failures
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Initialization helper functions
//===----------------------------------------------------------------------===//
//ExprResult
//Sema::PerformCopyInitialization(const InitializedEntity &Entity,
//                                SourceLocation EqualLoc,
//                                ExprResult Init) {
//  if (Init.isInvalid())
//    return ExprError();
//
//  Expr *InitE = Init.get();
//  assert(InitE && "No initialization expression?");
//
//  if (EqualLoc.isInvalid())
//    EqualLoc = InitE->getLocStart();
//
//  InitializationKind Kind = InitializationKind::CreateCopy(InitE->getLocStart(),
//                                                           EqualLoc);
//  InitializationSequence Seq(*this, Entity, Kind, &InitE, 1);
//  Init.release();
//  return Seq.Perform(*this, Entity, Kind, MultiExprArg(&InitE, 1));
//}
