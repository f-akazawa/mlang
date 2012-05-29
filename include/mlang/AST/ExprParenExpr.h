//===--- ExprParenExpr.h - Parenthesized Expression  ------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the ParenExpr.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_PAREN_EXPRESSION_H_
#define MLANG_AST_EXPR_PAREN_EXPRESSION_H_

#include "mlang/AST/Expr.h"

namespace mlang {
/// ParenExpr - This represents a parenthesized expression, e.g. "(1)".  This
/// AST node is only formed if full location information is requested.
class ParenExpr: public Expr {
	SourceLocation L, R;
	Stmt *Val;
public:
	ParenExpr(SourceLocation l, SourceLocation r, Expr *val, Type type,
			ExprValueKind VK) :
		Expr(ParenExprClass, type, VK), L(l), R(r), Val(val) {
	}

	/// \brief Construct an empty parenthesized expression.
	explicit ParenExpr(EmptyShell Empty) :
		Expr(ParenExprClass, Empty) {
	}

	const Expr *getSubExpr() const {
		return cast<Expr> (Val);
	}
	Expr *getSubExpr() {
		return cast<Expr> (Val);
	}
	void setSubExpr(Expr *E) {
		Val = E;
	}

	/// \brief Get the location of the left parentheses '('.
	SourceLocation getLParen() const {
		return L;
	}
	void setLParen(SourceLocation Loc) {
		L = Loc;
	}

	/// \brief Get the location of the right parentheses ')'.
	SourceLocation getRParen() const {
		return R;
	}
	void setRParen(SourceLocation Loc) {
		R = Loc;
	}

	SourceRange getSourceRange() const {
		return SourceRange(getLParen(), getRParen());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ParenExprClass;
	}
	static bool classof(const ParenExpr *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&Val, &Val + 1);
	};
};

class ParenListExpr: public Expr {
	Stmt **SubExprs;
	unsigned NumExprs;
	SourceLocation LParenLoc, RParenLoc;

public:
	ParenListExpr(ASTContext& C, SourceLocation lparenloc, Expr **exprs,
			unsigned numexprs, SourceLocation rparenloc);

	/// \brief Build an empty paren list.
	explicit ParenListExpr(EmptyShell Empty) :
		Expr(ParenListExprClass, Empty) {
	}

	unsigned getNumExprs() const {
		return NumExprs;
	}

	const Expr* getExpr(unsigned Init) const {
		assert(Init < getNumExprs() && "Initializer access out of range!");
		return cast_or_null<Expr> (SubExprs[Init]);
	}

	Expr* getExpr(unsigned Init) {
		assert(Init < getNumExprs() && "Initializer access out of range!");
		return cast_or_null<Expr> (SubExprs[Init]);
	}

	Expr **getExprs() {
		return reinterpret_cast<Expr **> (SubExprs);
	}

	SourceLocation getLParenLoc() const {
		return LParenLoc;
	}
	SourceLocation getRParenLoc() const {
		return RParenLoc;
	}

	SourceRange getSourceRange() const {
		return SourceRange(getLParenLoc(), getRParenLoc());
	}
	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ParenListExprClass;
	}
	static bool classof(const ParenListExpr *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubExprs[0], &SubExprs[0] + NumExprs);
	};

	friend class ASTStmtReader;
	friend class ASTStmtWriter;
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_PAREN_EXPRESSION_H_ */
