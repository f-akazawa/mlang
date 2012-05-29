//===--- ExprMultiOutput.h - MultiOuput Assignment --------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the MultiOuput used as lvalue of function call.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_MULTI_OUTPUT_H_
#define MLANG_AST_EXPR_MULTI_OUTPUT_H_

#include "mlang/AST/Expr.h"

namespace mlang {
/// When declaring or calling a function that returns more than one output,
/// enclose each return value that you need in square brackets.
///
class MultiOutput : public Expr {
	Stmt **SubExprs;
	unsigned NumOutputs;

	SourceLocation LBracketLoc, RBracketLoc;

public:
	MultiOutput(ASTContext &C, Type type, Expr **vars, unsigned nargout, SourceLocation LLoc, SourceLocation RLoc) :
		Expr(MultiOutputClass, type, VK_RValue), NumOutputs(nargout) {
		SubExprs = new (C) Stmt*[nargout];

		for (unsigned i = 0; i < nargout; ++i)
			SubExprs[i] = vars[i];

		LBracketLoc = LLoc;
		RBracketLoc = RLoc;
	}

	/// \brief Construct an empty character literal.
	explicit MultiOutput(EmptyShell Empty) :
			Expr(MultiOutputClass, Empty) {
	}

	unsigned getNumberofOutputs() const {
		return NumOutputs;
	}

	typedef ExprIterator arg_iterator;
	typedef ConstExprIterator const_arg_iterator;

	arg_iterator arg_begin() { return SubExprs; }
	arg_iterator arg_end() { return SubExprs+getNumberofOutputs(); }
	const_arg_iterator arg_begin() const { return SubExprs; }
	const_arg_iterator arg_end() const { return SubExprs+getNumberofOutputs();}

	Expr* getOutputArg(unsigned nArg) {
		assert(nArg < NumOutputs && "Output argument access out of range!");
		return cast<Expr>(SubExprs[nArg]);
	}
	const Expr* getOutputArg(unsigned nArg) const {
		assert(nArg < NumOutputs && "Output argument access out of range!");
			return cast<Expr>(SubExprs[nArg]);
		}
	/// setArg - Set the specified argument.
	void setOutputArg(unsigned nArg, Expr *ArgExpr) {
		assert(nArg < NumOutputs && "Arg access out of range!");
		SubExprs[nArg] = ArgExpr;
	}

	void setLBracketLocation(SourceLocation LB) {
		LBracketLoc = LB;
	}
	SourceLocation getLBracketLocation() const {
		return LBracketLoc;
	}

	void setRBracketLocation(SourceLocation RB) {
		RBracketLoc = RB;
	}
	SourceLocation getRBracketLocation() const {
		return RBracketLoc;
	}

	virtual SourceRange getSourceRange() const {
		return SourceRange(getLBracketLocation(), getRBracketLocation());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == MultiOutputClass;
	}
	static bool classof(const MultiOutput *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubExprs[0], &SubExprs[0] + NumOutputs);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_MULTI_OUTPUT_H_ */
