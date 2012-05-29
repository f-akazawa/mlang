//===--- ExprConcatExpr.h - Concatenation Expression ------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the ConcatExpr.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_CONCATEXPR_H_
#define MLANG_AST_EXPR_CONCATEXPR_H_

#include "mlang/AST/Expr.h"

namespace mlang {

/// Concatenation of multiple expressions using a pair of "[]", e.g.
/// a = [12 34; 45 67]; b = [23 34 23;45 56 34];
/// c = [a b];
/// c is a 2X5 matrix.
/// To combine two or more arrays into a new array through concatenation,
/// enclose all array elements in square brackets. All arrays' dimensions
/// should be compatible.
class RowVectorExpr: public Expr {
	Stmt **SubExprs;
	unsigned NumExprs;

public:
	RowVectorExpr(ASTContext &C, Expr **args, unsigned nexpr, Type ty) :
		Expr(RowVectorExprClass, ty, VK_RValue), NumExprs(nexpr){
		SubExprs = new (C) Stmt*[nexpr];
		for (unsigned i = 0; i < nexpr; i++)
			SubExprs[i] = args[i];
	}

	explicit RowVectorExpr(EmptyShell empty) :
		Expr(RowVectorExprClass, empty), SubExprs(0) {
	}

	/// getNumSubExprs - Return the size of the SubExprs array.  This includes the
	/// constant expression, the actual arguments passed in, and the function
	/// pointers.
	unsigned getNumSubExprs() const {
		return NumExprs;
	}

	/// getExpr - Return the Expr at the specified index.
	Expr *getExpr(unsigned Index) {
		assert((Index < NumExprs) && "Arg access out of range!");
		return cast<Expr> (SubExprs[Index]);
	}
	const Expr *getExpr(unsigned Index) const {
		assert((Index < NumExprs) && "Arg access out of range!");
		return cast<Expr> (SubExprs[Index]);
	}

	void setExprs(ASTContext &C, Expr ** Exprs, unsigned NumExprs);

	SourceLocation getStartLoc() const {
		return SubExprs[0]->getLocStart();
	}

	SourceLocation getEndLoc() const {
		return SubExprs[NumExprs-1]->getLocEnd();
	}

	SourceRange getSourceRange() const {
		return SourceRange(getStartLoc(), getEndLoc());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == RowVectorExprClass;
	}
	static bool classof(const RowVectorExpr *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubExprs[0], &SubExprs[0] + NumExprs);
	}
};

/// Concatenation of multiple expressions using a pair of "[]", e.g.
/// a = [12 34; 45 67]; b = [23 34 23;45 56 34];
/// c = [a b];
/// c is a 2X5 matrix.
/// To combine two or more arrays into a new array through concatenation,
/// enclose all array elements in square brackets. All arrays' dimensions
/// should be compatible.
class ConcatExpr: public Expr {
	Stmt **SubExprs;
	unsigned NumExprs;

	SourceLocation LBracketLoc, RBracketLoc;

public:
	ConcatExpr(ASTContext &C, Expr **args, unsigned nexpr, Type ty,
			SourceLocation LBLoc, SourceLocation RBLoc) :
		Expr(ConcatExprClass, ty, VK_RValue), NumExprs(nexpr), LBracketLoc(
				LBLoc), RBracketLoc(RBLoc) {
		SubExprs = new (C) Stmt*[nexpr];
		for (unsigned i = 0; i < nexpr; i++)
			SubExprs[i] = args[i];
	}

	explicit ConcatExpr(EmptyShell empty) :
		Expr(ConcatExprClass, empty), SubExprs(0) {
	}

	/// getNumSubExprs - Return the size of the SubExprs array.  This includes the
	/// constant expression, the actual arguments passed in, and the function
	/// pointers.
	unsigned getNumSubExprs() const {
		return NumExprs;
	}

	/// getExpr - Return the Expr at the specified index.
	Expr *getExpr(unsigned Index) {
		assert((Index < NumExprs) && "Arg access out of range!");
		return cast<Expr> (SubExprs[Index]);
	}
	const Expr *getExpr(unsigned Index) const {
		assert((Index < NumExprs) && "Arg access out of range!");
		return cast<Expr> (SubExprs[Index]);
	}

	void setExprs(ASTContext &C, Expr ** Exprs, unsigned NumExprs);

	SourceLocation getLBracketLoc() {
		return LBracketLoc;
	}
	const SourceLocation getLBracketLoc() const {
		return LBracketLoc;
	}
	void setLBracketLoc(SourceLocation L) {
		LBracketLoc = L;
	}

	SourceLocation getRBracketLoc() {
		return RBracketLoc;
	}
	const SourceLocation getRBracketLoc() const {
		return RBracketLoc;
	}
	void setRBracketLoc(SourceLocation L) {
		RBracketLoc = L;
	}

	SourceRange getSourceRange() const {
		return SourceRange(getLBracketLoc(), getRBracketLoc());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ConcatExprClass;
	}
	static bool classof(const ConcatExpr *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubExprs[0], &SubExprs[0] + NumExprs);
	}
};

/// Concatenation of multiple cell expressions
/// using a pair of "{}", e.g.
/// C3 = {C1 C2};
/// Concatenates cell arrays C1 and C2 into a two-element cell
/// array C3 such that C3{1} = C1 and C3{2} = C2.
///
class CellConcatExpr: public Expr {
	Stmt **SubExprs;
	unsigned NumExprs;

	SourceLocation LBraceLoc, RBraceLoc;

public:
	CellConcatExpr(ASTContext &C, Expr **args, unsigned nexpr, Type ty,
			SourceLocation LBLoc, SourceLocation RBLoc) :
		Expr(CellConcatExprClass, ty, VK_RValue), NumExprs(nexpr), LBraceLoc(
				LBLoc), RBraceLoc(RBLoc) {
		SubExprs = new (C) Stmt*[nexpr];
		for (unsigned i = 0; i < nexpr; i++)
			SubExprs[i] = args[i];
	}

	explicit CellConcatExpr(EmptyShell empty) :
		Expr(CellConcatExprClass, empty), SubExprs(0) {
	}

	/// getNumSubExprs - Return the size of the SubExprs array.  This includes the
	/// constant expression, the actual arguments passed in, and the function
	/// pointers.
	unsigned getNumSubExprs() const {
		return NumExprs;
	}

	/// getExpr - Return the Expr at the specified index.
	Expr *getExpr(unsigned Index) {
		assert((Index < NumExprs) && "Arg access out of range!");
		return cast<Expr> (SubExprs[Index]);
	}
	const Expr *getExpr(unsigned Index) const {
		assert((Index < NumExprs) && "Arg access out of range!");
		return cast<Expr> (SubExprs[Index]);
	}

	void setExprs(ASTContext &C, Expr ** Exprs, unsigned NumExprs);

	SourceLocation getLBraceLoc() {
		return LBraceLoc;
	}
	const SourceLocation getLBraceLoc() const {
		return LBraceLoc;
	}
	void setLBracketLoc(SourceLocation L) {
		LBraceLoc = L;
	}

	SourceLocation getRBraceLoc() {
		return RBraceLoc;
	}
	const SourceLocation getRBraceLoc() const {
		return RBraceLoc;
	}
	void setRBraceLoc(SourceLocation L) {
		RBraceLoc = L;
	}

	SourceRange getSourceRange() const {
		return SourceRange(getLBraceLoc(), getRBraceLoc());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == CellConcatExprClass;
	}
	static bool classof(const CellConcatExpr *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubExprs[0], &SubExprs[0] + NumExprs);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_CONCATEXPR_H_ */
