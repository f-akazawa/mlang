//===--- ExprUnaryOperator.h - Unary Operator --------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines UnaryOperator and PostUnaryOperator.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_UNARY_OPERATOR_H_
#define MLANG_AST_EXPR_UNARY_OPERATOR_H_

#include "mlang/AST/Expr.h"

namespace mlang {
/// prefix unary operator  : "+"  "-"  "!"  "~"  "++"  "--"
/// postfix unary operator : "'"  ".'"  "++"  "--"
class UnaryOperator: public Expr {
public:
	typedef UnaryOperatorKind Opcode;

private:
	unsigned Opc :5;
	SourceLocation Loc;
	Stmt *Val;
public:

	UnaryOperator(Expr *input, Opcode opc, Type type, ExprValueKind VK,
			SourceLocation l) :
		Expr(UnaryOperatorClass, type, VK), Opc(opc), Loc(l), Val(input) {
	}

	/// \brief Build an empty unary operator.
	explicit UnaryOperator(EmptyShell Empty) :
		Expr(UnaryOperatorClass, Empty), Opc(UO_UnaryOpNums) {
	}

	Opcode getOpcode() const {
		return static_cast<Opcode> (Opc);
	}
	void setOpcode(Opcode O) {
		Opc = O;
	}

	Expr *getSubExpr() const {
		return cast<Expr> (Val);
	}
	void setSubExpr(Expr *E) {
		Val = E;
	}

	/// getOperatorLoc - Return the location of the operator.
	SourceLocation getOperatorLoc() const {
		return Loc;
	}
	void setOperatorLoc(SourceLocation L) {
		Loc = L;
	}

	/// isPostfix - Return true if this is a postfix operation, like x++.
	static bool isPostfix(Opcode Op) {
		return Op >= UO_Transpose && Op <= UO_PostDec;
	}

	/// isPostfix - Return true if this is a prefix operation, like --x.
	static bool isPrefix(Opcode Op) {
		return Op >= UO_PreInc && Op <= UO_Handle;
	}

	static bool isSingleChar(Opcode Op) {
		return Op == UO_Quote || Op == UO_Plus || Op == UO_Minus || Op
				== UO_LNot || Op == UO_Handle;
	}

	static bool isDoubleChar(Opcode Op) {
		return Op == UO_Transpose || Op == UO_PostInc || Op == UO_PostDec || Op
				== UO_PreInc || Op == UO_PreDec;
	}

	bool isPrefix() const {
		return isPrefix(getOpcode());
	}
	bool isPostfix() const {
		return isPostfix(getOpcode());
	}
	bool isSingleCharOp() const {
		return isSingleChar(getOpcode());
	}
	bool isDoubleCharOp() const {
		return isDoubleChar(getOpcode());
	}

	bool isTransposeOp() const {
		return Opc == UO_Transpose || Opc == UO_Quote;
	}
	bool isIncrementOp() const {
		return Opc == UO_PreInc || Opc == UO_PostInc;
	}
	bool isIncrementDecrementOp() const {
		return Opc >= UO_PostInc && Opc <= UO_PreDec;
	}
	static bool isArithmeticOp(Opcode Op) {
		return Op >= UO_Plus && Op <= UO_LNot;
	}
	bool isArithmeticOp() const {
		return isArithmeticOp(getOpcode());
	}

	/// getOpcodeStr - Turn an Opcode enum value into the punctuation char it
	/// corresponds to, e.g. "sizeof" or "[pre]++"
	static const char *getOpcodeStr(Opcode Op);

	SourceRange getSourceRange() const {
		if (isPostfix())
			return SourceRange(getSubExpr()->getLocStart(), getOperatorLoc());
		else
			return SourceRange(getOperatorLoc(), getSubExpr()->getLocEnd());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == UnaryOperatorClass;
	}
	static bool classof(const UnaryOperator *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&Val, &Val + 1);
	};
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_UNARY_OPERATOR_H_ */
