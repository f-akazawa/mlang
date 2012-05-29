//===--- ExprColonExpr.h - Colon expression ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the ColonExpr class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_COLON_EXPRESSION_H_
#define MLANG_AST_EXPR_COLON_EXPRESSION_H_

#include "mlang/AST/Expr.h"
#include "mlang/AST/ExprNumericLiteral.h"

namespace mlang {
/// ColonExpr: we use this to represent a vector.
class ColonExpr: public Expr {
	Stmt* SubExprs[3];
	SourceLocation firstLoc, secondLoc; // first and second colon location
	bool magic_end;
	bool magic_colon;
	bool BinaryOp;

public:
	/// Construct a single ':' expression, denote a whole column or row
	/// of an array, e.g. A(:,1:5)
	ColonExpr(Type Ty, SourceLocation Loc) :
		Expr(ColonExprClass, Ty, VK_RValue),
		firstLoc(Loc), secondLoc(Loc) {
		for(int i=0; i<3; ++i) {
			SubExprs[i] = NULL;
		}
		BinaryOp = false;
		magic_colon = true;
	}

	/// Construct a colon expression, which denotes a sequence of numbers
	/// if inc == NULL and firstloc == secondloc, it means it is a binary
	/// operator
	ColonExpr(ASTContext &ctx, Expr* base, Expr* inc, Expr* limit, Type Ty,
			SourceLocation firstloc, SourceLocation secondloc) :
		Expr(ColonExprClass, Ty, VK_RValue),
				firstLoc(firstloc), secondLoc(secondloc), magic_colon(false) {
		SubExprs[0] = base;
		SubExprs[2] = limit;
		if(inc == NULL)
		{
			BinaryOp = true;
			SubExprs[1] = IntegerLiteral::Create(ctx,llvm::APInt(8,1),
					ctx.Int8Ty,firstLoc);
		} else {
			SubExprs[1] = inc;
			BinaryOp = false;
		}
	}

	void setBase(Expr *E) {
		SubExprs[0] = E;
	}
	Expr *getBase() const {
		return cast<Expr> (SubExprs[0]);
	}

	void setLimit(Expr *E) {
		SubExprs[2] = E;
	}
	Expr *getLimit() const {
		return cast<Expr> (SubExprs[2]);
	}

	void setInc(Expr *E) {
		SubExprs[1] = E;
	}
	Expr *getInc() const {
		return cast<Expr> (SubExprs[1]);
	}

	bool isSingleColon() const {
		return SubExprs[0] == NULL && SubExprs[1] == NULL
				&& SubExprs[2] == NULL;
	}

	bool hasIncExpr() const {
		return SubExprs[0] != NULL && SubExprs[1] != NULL
				&& SubExprs[2] != NULL;
	}

	bool IsBinaryOp() const {
		return BinaryOp;
	}

	bool IsMagicColon() const {
		return magic_colon;
	}

	bool hasMagicEnd() const {
		return magic_end;
	}
	void setMagicEnd() {
		magic_end = true;
	}

	unsigned getRange() const {
		assert(SubExprs[0] != NULL && SubExprs[2] != NULL);

	}

	SourceLocation get1stConlonLoc() {
		return firstLoc;
	}
	const SourceLocation get1stConlonLoc() const {
		return firstLoc;
	}
	void set1stConlonLoc(SourceLocation Location) {
		firstLoc = Location;
	}
	SourceLocation get2ndConlonLoc() {
		return secondLoc;
	}
	const SourceLocation get2ndConlonLoc() const {
		return secondLoc;
	}
	void set2ndConlonLoc(SourceLocation Location) {
		secondLoc = Location;
	}

	virtual SourceRange getSourceRange() const {
		if (!isSingleColon()) {
			SourceLocation EndLoc = SubExprs[2]->getLocEnd();
			SourceLocation StartLoc = SubExprs[0]->getLocStart();
			return SourceRange(StartLoc, EndLoc);
		}

		return SourceRange(get1stConlonLoc());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ColonExprClass;
	}
	static bool classof(const ColonExpr *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubExprs[0], &SubExprs[0]+3);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_COLON_EXPRESSION_H_ */
