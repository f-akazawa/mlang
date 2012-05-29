//===--- ExprAnonFuncHandle.h - Function Handle Expression ------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the FuncHandleExpr and AnonFunctionHandle.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_ANON_FUNC_HANDLE_H_
#define MLANG_AST_EXPR_ANON_FUNC_HANDLE_H_

#include "mlang/AST/Expr.h"

namespace mlang {
class FunctionDefn;

class FuncHandle: public Expr {
	Stmt *RefFunc;
	SourceLocation AtSymbolLoc;

public:
	FuncHandle(Expr *func, Type Ty, SourceLocation atLoc) :
		Expr(FuncHandleClass, Ty, VK_RValue), RefFunc(func), AtSymbolLoc(atLoc) {
	}

	/// \brief Construct an empty binary operator.
	explicit FuncHandle(EmptyShell Empty) :
		Expr(FuncHandleClass, Empty) {
	}

	SourceLocation getAtSymbolLoc() {
		return AtSymbolLoc;
	}
	const SourceLocation getAtSymbolLoc() const{
		return AtSymbolLoc;
	}
	void setAtSymbolLoc(SourceLocation L) {
		AtSymbolLoc = L;
	}

	Expr *getRefFunc() {
		return cast<Expr>(RefFunc);
	}
	const Expr *getRefFunc() const{
		return cast<Expr>(RefFunc);
	}
	void setRefFunc(Expr * re) {
			RefFunc = re;
	}

	virtual SourceRange getSourceRange() const {
		return SourceRange(getAtSymbolLoc(), getRefFunc()->getLocEnd());
	}

	static bool classof(const Stmt *S) {
		return S->getStmtClass() == FuncHandleClass;
	}
	static bool classof(const FuncHandle *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&RefFunc, &RefFunc + 1);
	}
};

class AnonFuncHandle: public Expr {
	/// InputArgList is a ParenExpr or ParenListExpr
	enum {
		InputArgList = 0, FuncBody = 1
	};
	Stmt **SubExprs;
	SourceLocation AtSymbolLoc;

public:
	AnonFuncHandle(Expr *args, Expr *body, Type Ty, SourceLocation atLoc) :
		Expr(AnonFuncHandleClass, Ty, VK_RValue), AtSymbolLoc(atLoc) {
		SubExprs[InputArgList] = args;
		SubExprs[FuncBody] = body;
	}

	/// \brief Build an empty compound assignment operator expression.
	explicit AnonFuncHandle(EmptyShell Empty) :
		Expr(AnonFuncHandleClass, Empty) {
	}

	SourceLocation getAtSymbolLoc() {
		return AtSymbolLoc;
	}
	const SourceLocation getAtSymbolLoc() const{
		return AtSymbolLoc;
	}
	void setAtSymbolLoc(SourceLocation l) {
		AtSymbolLoc = l;
	}

	Expr *getArgs() {
		return cast<Expr> (SubExprs[InputArgList]);
	}
	const Expr *getArgs() const {
		return cast<Expr> (SubExprs[InputArgList]);
	}
	void setArgs(Expr *E) {
		SubExprs[InputArgList] = E;
	}

	Expr *getBody() {
		return cast<Expr> (SubExprs[FuncBody]);
	}
	const Expr *getBody() const {
		return cast<Expr> (SubExprs[FuncBody]);
	}
	void setBody(Expr *E) {
		SubExprs[FuncBody] = E;
	}

	virtual SourceRange getSourceRange() const{
		return	SourceRange(getAtSymbolLoc(), getBody()->getLocEnd());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == AnonFuncHandleClass;
	}
	static bool classof(const AnonFuncHandle *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubExprs[0], &SubExprs[0] + FuncBody);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_ANON_FUNC_HANDLE_H_ */
