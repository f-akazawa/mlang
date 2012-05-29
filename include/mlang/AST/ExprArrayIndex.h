//===--- ExprIndexExpr.h - Array index and Cell index -----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the ArrayIndex and CellArrayIndex.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_ARRAY_INDEX_H_
#define MLANG_AST_EXPR_ARRAY_INDEX_H_

#include "mlang/AST/Expr.h"

namespace mlang {
/// Array subscript must either be real positive integers or logicals
class ArrayIndex: public Expr {
	enum {
		BASE = 0, INDEX_START = 1
	};
	Stmt **SubExprs;
	unsigned NumArgs;
	SourceLocation RParenLoc;
	bool CellIndexed;

protected:
	// This version of the constructor is for derived classes.
	ArrayIndex(ASTContext& C, StmtClass SC, Expr *base, Expr **args, unsigned numargs,
		Type t, ExprValueKind VK, SourceLocation rparenloc, bool isCell);

public:
	ArrayIndex(ASTContext& C, Expr *base, Expr **args, unsigned numargs, Type t,
		ExprValueKind VK, SourceLocation rparenloc, bool isCell);

	/// \brief Build an empty call expression.
	ArrayIndex(ASTContext &C, StmtClass SC, EmptyShell Empty);

	const Expr *getBase() const {
		return cast<Expr> (SubExprs[BASE]);
	}
	Expr *getBase() {
		return cast<Expr> (SubExprs[BASE]);
	}
	void setBase(Expr *base) {
		SubExprs[BASE] = base;
	}

	/// getNumArgs - Return the number of actual arguments to this call.
	///
	unsigned getNumArgs() const {
		return NumArgs;
	}

	/// getIdx - Return the specified argument.
	Expr *getIdx(unsigned Arg) {
		assert(Arg < NumArgs && "Arg access out of range!");
		return cast<Expr> (SubExprs[Arg + INDEX_START]);
	}
	const Expr *getIdx(unsigned Arg) const {
		assert(Arg < NumArgs && "Arg access out of range!");
		return cast<Expr> (SubExprs[Arg + INDEX_START]);
	}

	/// setArg - Set the specified argument.
	void setArg(unsigned Arg, Expr *ArgExpr) {
		assert(Arg < NumArgs && "Arg access out of range!");
		SubExprs[Arg + INDEX_START] = ArgExpr;
	}

	/// setNumArgs - This changes the number of arguments present in this call.
	/// Any orphaned expressions are deleted by this, and any new operands are set
	/// to null.
	void setNumArgs(ASTContext& C, unsigned NumArgs);

	typedef ExprIterator arg_iterator;
	typedef ConstExprIterator const_arg_iterator;

	arg_iterator arg_begin() {
		return SubExprs + INDEX_START;
	}
	arg_iterator arg_end() {
		return SubExprs + INDEX_START + getNumArgs();
	}
	const_arg_iterator arg_begin() const {
		return SubExprs + INDEX_START;
	}
	const_arg_iterator arg_end() const {
		return SubExprs + INDEX_START + getNumArgs();
	}

	/// getNumCommas - Return the number of commas that must have been present in
	/// this array index.
	unsigned getNumCommas() const {
		return NumArgs ? NumArgs - 1 : 0;
	}

	/// getTypeofBase - Get the type of the indexed expr.
	///
	Type getTypeofBase() const {
		return getBase()->getType();
	}

	/// isCellIndexed - if this is a cell array indexing expression, C{1,2}
	bool isCellIndexed() const { return CellIndexed; }
	void setCellIndexed() { CellIndexed = true; }

	SourceLocation getRParenLoc() const {
		return RParenLoc;
	}
	void setRParenLoc(SourceLocation Loc) {
		RParenLoc = Loc;
	}

	SourceRange getSourceRange() const {
		return SourceRange(getBase()->getLocStart(), getRParenLoc());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ArrayIndexClass;
	}
	static bool classof(const ArrayIndex *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubExprs[0], &SubExprs[0]+ NumArgs + INDEX_START);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_ARRAY_INDEX_H_ */
