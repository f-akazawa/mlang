//===--- SemaType.cpp - Semantic Analysis for Types -----------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements type-related semantic analysis.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/Expr.h"
#include "mlang/Diag/PartialDiagnostic.h"
#include "mlang/Basic/TargetInfo.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/ErrorHandling.h"

using namespace mlang;


bool Sema::RequireCompleteType(SourceLocation Loc, Type T,
		                     const PartialDiagnostic &PD,
		                     std::pair<SourceLocation, PartialDiagnostic> Note) {
  return false;
}

bool Sema::RequireCompleteType(SourceLocation Loc, Type T,
		const PartialDiagnostic &PD) {
	return false;
}

bool Sema::RequireCompleteType(SourceLocation Loc, Type T, unsigned DiagID) {
	return false;
}
