//===--- SemaStmt.cpp - Semantic Analysis for Statements ------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for statements.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
#include "mlang/Sema/Scope.h"
#include "mlang/Sema/ScopeInfo.h"
#include "mlang/AST/APValue.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/CmdAll.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Basic/TargetInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlang;
using namespace sema;

StmtResult Sema::ActOnExprStmt(Expr* E, bool isShowResult){
	if (!E) // FIXME: FullExprArg has no error state?
		return StmtError();

	if(isShowResult) {
		E->setPrintFlag(true);
	}

  // Same thing in for stmt first clause (when expr) and third clause.
	return Owned(static_cast<Stmt*>(E));
}

StmtResult Sema::ActOnNullCmd(SourceLocation SemiLoc) {
	return Owned(new (Context) NullCmd(SemiLoc));
}

StmtResult Sema::ActOnBlockCmd(SourceLocation L, SourceLocation R, MultiStmtArg elts,
		bool isStmtExpr){
	unsigned NumElts = elts.size();
	Stmt **Elts = reinterpret_cast<Stmt**> (elts.release());

	// Warn about unused expressions in statements.
	for (unsigned i = 0; i != NumElts; ++i) {
		// Ignore statements that are last in a statement expression.
		if (isStmtExpr && i == NumElts - 1)
			continue;

		DiagnoseUnusedExprResult(Elts[i]);
	}

	return Owned(new (Context) BlockCmd(Context, Elts, NumElts, L, R));
}

StmtResult Sema::ActOnDefnCmd(DefnGroupPtrTy Defn, SourceLocation StartLoc,
		SourceLocation EndLoc){
	return StmtError();
}

void Sema::ActOnForEachDefnCmd(DefnGroupPtrTy Defn){
	return ;
}

StmtResult Sema::ActOnCaseCmd(SourceLocation CaseLoc, Expr *LHSVal,
		Stmt *RHSVal) {
	return StmtError();
}

void Sema::ActOnCaseCmdBody(Cmd *CaseCmd, Stmt *SubStmt){
	return ;
}

StmtResult Sema::ActOnOtherwiseCmd(SourceLocation OtherwiseLoc, Stmt *SubStmt,
		Scope *CurScope){
	return StmtError();
}

StmtResult Sema::ActOnThenCmd(SourceLocation TLoc, MultiStmtArg ThenStmts) {
	ThenCmd *T = new (getASTContext()) ThenCmd(getASTContext(), TLoc,
			ThenStmts.release(), ThenStmts.size());
	return Owned(T);
}

StmtResult Sema::ActOnElseCmd(SourceLocation ElseLoc, MultiStmtArg ElseStmts) {
	ElseCmd *E = new (getASTContext()) ElseCmd(getASTContext(), ElseLoc,
			ElseStmts.release(), ElseStmts.size());
	return Owned(E);
}

StmtResult Sema::ActOnElseIfCmd(SourceLocation ElseIfLoc, Expr *Cond,
		MultiStmtArg ElseIfStmts) {
	ElseIfCmd *C = new (getASTContext()) ElseIfCmd(getASTContext(), ElseIfLoc,
			Cond, ElseIfStmts.release(), ElseIfStmts.size());
	return Owned(C);
}

StmtResult Sema::ActOnIfCmd(SourceLocation IfLoc, SourceLocation EndLoc,
		Expr *CondVal, Stmt *ThenVal,	SourceLocation ElseLoc, Stmt *ElseVal){
	IfCmd *C = new (getASTContext()) IfCmd(getASTContext(), IfLoc, EndLoc,
			CondVal, ThenVal, 0, 0, ElseLoc, ElseVal);
	return Owned(C);
}

StmtResult Sema::ActOnIfCmd(SourceLocation IfLoc, SourceLocation EndLoc,
		Expr *CondVal, Stmt *ThenVal,	MultiStmtArg ElseIfClauses,
		SourceLocation ElseLoc,	Stmt *ElseVal) {
	IfCmd *C = new (getASTContext()) IfCmd(getASTContext(), IfLoc, EndLoc,
				CondVal, ThenVal, ElseIfClauses.release(), ElseIfClauses.size(),
				ElseLoc, ElseVal);
	return Owned(C);
}

StmtResult Sema::ActOnCaseCmd(SourceLocation Cloc, Expr *Pat,
		MultiStmtArg Stmts) {
	CaseCmd *C = new (getASTContext()) CaseCmd(getASTContext(), Cloc, Pat,
			Stmts.release(), Stmts.size());
	return Owned(C);
}

StmtResult Sema::ActOnOtherwiseCmd(SourceLocation Oloc, MultiStmtArg Stmts) {
	OtherwiseCmd *O = new (getASTContext())OtherwiseCmd(getASTContext(), Oloc, Stmts.release(),
			Stmts.size());
	return Owned(O);
}

StmtResult Sema::ActOnSwitchCmd(SourceLocation SwitchLoc,
		SourceLocation EndLoc, Expr *Var, Expr *Cond,
		MultiStmtArg CaseClauses, Stmt *OtherwiseCmd){

// FIXME yabin CondVar should be a Defn*, we check the condition variable first
//  ExprResult CondResult;

//	VarDefn *ConditionVar = 0;
//	if (CondVar) {
//		ConditionVar = cast<VarDefn>(CondVar);
//		CondResult = CheckConditionVariable(ConditionVar, SourceLocation(), false);
//		if (CondResult.isInvalid())
//			return StmtError();
//		Cond = CondResult.release();
//	}
	SwitchCmd *S = new (Context)SwitchCmd(Context, SwitchLoc, EndLoc,
			Var, Cond, CaseClauses.release(), CaseClauses.size(), OtherwiseCmd);
	return Owned(S);
}

StmtResult Sema::ActOnWhileCmd(SourceLocation WhileLoc, SourceLocation EndLoc,
		Expr *Cond, Stmt *Body){
	WhileCmd *W = new (getASTContext()) WhileCmd(getASTContext(), Cond, Body,
			WhileLoc, EndLoc);
	return Owned(W);
}

StmtResult Sema::ActOnDoCmd(SourceLocation DoLoc, Stmt *Body, SourceLocation WhileLoc,
		SourceLocation CondLParen, Expr *Cond, SourceLocation CondRParen){
	return StmtError();
}

StmtResult Sema::ActOnForCmd(SourceLocation ForLoc, SourceLocation EndLoc,
		Expr *LoopVar, Expr *LoopCond, Stmt *Body){
	ForCmd *F = new (getASTContext()) ForCmd(getASTContext(), LoopVar, LoopCond,
			Body, ForLoc, EndLoc);
	return Owned(static_cast<Cmd *>(F));
}

StmtResult Sema::ActOnContinueCmd(SourceLocation ContinueLoc, Scope *CurScope){
	return Owned(new (Context) ContinueCmd(ContinueLoc));
}

StmtResult Sema::ActOnBreakCmd(SourceLocation BreakLoc, Scope *CurScope){
	return Owned(new (Context) BreakCmd(BreakLoc));
}

StmtResult Sema::ActOnReturnCmd(SourceLocation ReturnLoc, Expr *RetValExp){
	return Owned(new (Context) BreakCmd(ReturnLoc));
}

StmtResult Sema::ActOnCatchBlock(SourceLocation CatchLoc, Defn *ExDefn,
		Stmt *HandlerBlock){
	return StmtError();
}

StmtResult Sema::ActOnTryBlock(SourceLocation TryLoc, Stmt *TryBlock,
		MultiStmtArg Handlers){
	return StmtError();
}

StmtResult Sema::ActOnDefnStmt(Defn * D){
  return StmtError();
}
