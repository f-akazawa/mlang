//===--- StmtVisitor.h - Visitor for Stmt subclasses ------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the StmtVisitor interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_STMT_VISITOR_H_
#define MLANG_AST_STMT_VISITOR_H_

#include "mlang/AST/CmdAll.h"
#include "mlang/AST/ExprAll.h"

namespace mlang {
#define DISPATCH(NAME, CLASS) \
  return static_cast<ImplClass*>(this)->Visit ## NAME(static_cast<CLASS*>(S))

/// StmtVisitor - This class implements a simple visitor for Stmt subclasses.
/// Since Expr derives from Stmt, this also includes support for visiting Exprs.
template<typename ImplClass, typename RetTy = void>
class StmtVisitor {
public:
	RetTy Visit(Stmt *S) {

		// If we have a binary expr, dispatch to the subcode of the binop.  A smart
		// optimizer (e.g. LLVM) will fold this comparison into the switch stmt
		// below.
		if (BinaryOperator *BinOp = dyn_cast<BinaryOperator>(S)) {
			switch (BinOp->getOpcode()) {
			default:
				assert(0 && "Unknown binary operator!");
			case BO_Power:
				DISPATCH(BinPower, BinaryOperator);
			case BO_MatrixPower:
				DISPATCH(BinMatrixPower, BinaryOperator);
			case BO_Mul:
				DISPATCH(BinMul, BinaryOperator);
			case BO_RDiv:
				DISPATCH(BinRDiv, BinaryOperator);
			case BO_LDiv:
				DISPATCH(BinLDiv, BinaryOperator);
			case BO_MatrixMul:
				DISPATCH(BinMatrixMul, BinaryOperator);
			case BO_MatrixRDiv:
				DISPATCH(BinMatrixRDiv, BinaryOperator);
			case BO_MatrixLDiv:
				DISPATCH(BinMatrixLDiv, BinaryOperator);
			case BO_Add:
				DISPATCH(BinAdd, BinaryOperator);
			case BO_Sub:
				DISPATCH(BinSub, BinaryOperator);
			case BO_Shl:
				DISPATCH(BinShl, BinaryOperator);
			case BO_Shr:
				DISPATCH(BinShr, BinaryOperator);
			case BO_LT:
				DISPATCH(BinLT, BinaryOperator);
			case BO_GT:
				DISPATCH(BinGT, BinaryOperator);
			case BO_LE:
				DISPATCH(BinLE, BinaryOperator);
			case BO_GE:
				DISPATCH(BinGE, BinaryOperator);
			case BO_EQ:
				DISPATCH(BinEQ, BinaryOperator);
			case BO_NE:
				DISPATCH(BinNE, BinaryOperator);

			case BO_And:
				DISPATCH(BinAnd, BinaryOperator);
			case BO_Or:
				DISPATCH(BinOr, BinaryOperator);
			case BO_LAnd:
				DISPATCH(BinLAnd, BinaryOperator);
			case BO_LOr:
				DISPATCH(BinLOr, BinaryOperator);
			case BO_Assign:
				DISPATCH(BinAssign, BinaryOperator);
			case BO_MatPowerAssign:
				DISPATCH(BinMatPowerAssign, CompoundAssignOperator);
			case BO_MatMulAssign:
				DISPATCH(BinMatMulAssign, CompoundAssignOperator);
			case BO_MatRDivAssign:
				DISPATCH(BinMatRDivAssign, CompoundAssignOperator);
			case BO_MatLDivAssign:
				DISPATCH(BinMatLDivAssign, CompoundAssignOperator);
			case BO_PowerAssign:
				DISPATCH(BinPowerAssign, CompoundAssignOperator);
			case BO_MulAssign:
				DISPATCH(BinMulAssign, CompoundAssignOperator);
			case BO_RDivAssign:
				DISPATCH(BinRDivAssign, CompoundAssignOperator);
			case BO_LDivAssign:
				DISPATCH(BinLDivAssign, CompoundAssignOperator);
			case BO_AddAssign:
				DISPATCH(BinAddAssign, CompoundAssignOperator);
			case BO_SubAssign:
				DISPATCH(BinSubAssign, CompoundAssignOperator);
			case BO_ShlAssign:
				DISPATCH(BinShlAssign, CompoundAssignOperator);
			case BO_ShrAssign:
				DISPATCH(BinShrAssign, CompoundAssignOperator);
			case BO_AndAssign:
				DISPATCH(BinAndAssign, CompoundAssignOperator);
			case BO_OrAssign:
				DISPATCH(BinOrAssign, CompoundAssignOperator);
			case BO_Comma:
				DISPATCH(BinComma, BinaryOperator);
			}
		} else if (UnaryOperator *UnOp = dyn_cast<UnaryOperator>(S)) {
			switch (UnOp->getOpcode()) {
			default:
				assert(0 && "Unknown unary operator!");
			case UO_Transpose:
				DISPATCH(UnaryTranspose, UnaryOperator);
			case UO_Quote:
				DISPATCH(UnaryTranspose, UnaryOperator);
			case UO_PostInc:
				DISPATCH(UnaryPostInc, UnaryOperator);
			case UO_PostDec:
				DISPATCH(UnaryPostDec, UnaryOperator);
			case UO_PreInc:
				DISPATCH(UnaryPreInc, UnaryOperator);
			case UO_PreDec:
				DISPATCH(UnaryPreDec, UnaryOperator);
			case UO_Plus:
				DISPATCH(UnaryPlus, UnaryOperator);
			case UO_Minus:
				DISPATCH(UnaryMinus, UnaryOperator);
			case UO_LNot:
				DISPATCH(UnaryLNot, UnaryOperator);
			case UO_Handle:
				DISPATCH(UnaryHandle, UnaryOperator);
			}
		}

		// Top switch stmt: dispatch to VisitFooStmt for each FooStmt.
		switch (S->getStmtClass()) {
		default:
			assert(0 && "Unknown stmt kind!");
#define ABSTRACT_STMT(STMT)
#define STMT(CLASS, PARENT)                              \
    case Stmt::CLASS ## Class: DISPATCH(CLASS, CLASS);
#include "mlang/AST/StmtNodes.inc"
		}
	}

	// If the implementation chooses not to implement a certain visit method, fall
	// back on VisitExpr or whatever else is the superclass.
#define STMT(CLASS, PARENT)                                   \
  RetTy Visit ## CLASS(CLASS *S) { DISPATCH(PARENT, PARENT); }
#include "mlang/AST/StmtNodes.inc"

	// If the implementation doesn't implement binary operator methods, fall back
	// on VisitBinaryOperator.
#define BINOP_FALLBACK(NAME) \
  RetTy VisitBin ## NAME(BinaryOperator *S) { \
    DISPATCH(BinaryOperator, BinaryOperator); \
  }
	BINOP_FALLBACK(Power)
	BINOP_FALLBACK(MatrixPower)
	BINOP_FALLBACK(Mul)
	BINOP_FALLBACK(RDiv)
	BINOP_FALLBACK(LDiv)
	BINOP_FALLBACK(MatrixMul)
	BINOP_FALLBACK(MatrixRDiv)
	BINOP_FALLBACK(MatrixLDiv)
	BINOP_FALLBACK(Add)
	BINOP_FALLBACK(Sub)
	BINOP_FALLBACK(Shl)
	BINOP_FALLBACK(Shr)

	BINOP_FALLBACK(LT)
	BINOP_FALLBACK(GT)
	BINOP_FALLBACK(LE)
	BINOP_FALLBACK(GE)
	BINOP_FALLBACK(EQ)
	BINOP_FALLBACK(NE)
	BINOP_FALLBACK(And)
	BINOP_FALLBACK(Or)
	BINOP_FALLBACK(LAnd)
	BINOP_FALLBACK(LOr)

	BINOP_FALLBACK(Assign)
	BINOP_FALLBACK(Comma)
#undef BINOP_FALLBACK

	// If the implementation doesn't implement compound assignment operator
	// methods, fall back on VisitCompoundAssignOperator.
#define CAO_FALLBACK(NAME) \
  RetTy VisitBin ## NAME(CompoundAssignOperator *S) { \
    DISPATCH(CompoundAssignOperator, CompoundAssignOperator); \
  }
	CAO_FALLBACK(MatPowerAssign)
	CAO_FALLBACK(MatMulAssign)
	CAO_FALLBACK(MatRDivAssign)
	CAO_FALLBACK(MatLDivAssign)
	CAO_FALLBACK(PowerAssign)
	CAO_FALLBACK(MulAssign)
	CAO_FALLBACK(RDivAssign)
	CAO_FALLBACK(LDivAssign)
	CAO_FALLBACK(AddAssign)
	CAO_FALLBACK(SubAssign)
	CAO_FALLBACK(ShlAssign)
	CAO_FALLBACK(ShrAssign)
	CAO_FALLBACK(AndAssign)
	CAO_FALLBACK(OrAssign)
#undef CAO_FALLBACK

	// If the implementation doesn't implement unary operator methods, fall back
	// on VisitUnaryOperator.
#define UNARYOP_FALLBACK(NAME) \
  RetTy VisitUnary ## NAME(UnaryOperator *S) { \
    DISPATCH(UnaryOperator, UnaryOperator);    \
  }
	UNARYOP_FALLBACK(Transpose)
	UNARYOP_FALLBACK(Quote)
	UNARYOP_FALLBACK(PostInc)
	UNARYOP_FALLBACK(PostDec)
	UNARYOP_FALLBACK(PreInc)
	UNARYOP_FALLBACK(PreDec)
	UNARYOP_FALLBACK(Plus)
	UNARYOP_FALLBACK(Minus)
	UNARYOP_FALLBACK(LNot)
	// UNARYOP_FALLBACK(Not)
	UNARYOP_FALLBACK(Handle)
#undef UNARYOP_FALLBACK

	// Base case, ignore it. :)
	RetTy VisitStmt(Stmt *Node) {
		return RetTy();
	}
};

#undef DISPATCH
} // end namespace mlang

#endif /* MLANG_AST_STMT_VISITOR_H_ */
