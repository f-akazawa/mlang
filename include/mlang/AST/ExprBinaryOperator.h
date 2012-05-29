//===--- ExprBinaryOperator.h - Binary Operator -----------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines BinaryOperator and Compound Assignment Operator.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_BINARY_OPERATOR_H_
#define MLANG_AST_EXPR_BINARY_OPERATOR_H_

#include "mlang/AST/Expr.h"

namespace mlang {
class BinaryOperator : public Expr {
public:
  typedef BinaryOperatorKind Opcode;

private:
  unsigned Opc : 6;
  SourceLocation OpLoc;

  enum { LHS, RHS, END_EXPR };
  Stmt* SubExprs[END_EXPR];
public:

  BinaryOperator(Expr *lhs, Expr *rhs, Opcode opc, Type ResTy,
		  ExprValueKind VK, SourceLocation opLoc)
    : Expr(BinaryOperatorClass, ResTy, VK),
      Opc(opc), OpLoc(opLoc) {
    SubExprs[LHS] = lhs;
    SubExprs[RHS] = rhs;
    assert(!isCompoundAssignmentOp() &&
           "Use ArithAssignBinaryOperator for compound assignments");
  }

  /// \brief Construct an empty binary operator.
  explicit BinaryOperator(EmptyShell Empty)
    : Expr(BinaryOperatorClass, Empty), Opc(BO_Comma) { }

  SourceLocation getOperatorLoc() const { return OpLoc; }
  void setOperatorLoc(SourceLocation L) { OpLoc = L; }

  Opcode getOpcode() const { return static_cast<Opcode>(Opc); }
  void setOpcode(Opcode O) { Opc = O; }

  Expr *getLHS() const { return cast<Expr>(SubExprs[LHS]); }
  void setLHS(Expr *E) { SubExprs[LHS] = E; }
  Expr *getRHS() const { return cast<Expr>(SubExprs[RHS]); }
  void setRHS(Expr *E) { SubExprs[RHS] = E; }

  virtual SourceRange getSourceRange() const {
    return SourceRange(getLHS()->getLocStart(), getRHS()->getLocEnd());
  }

  /// getOpcodeStr - Turn an Opcode enum value into the punctuation char it
  /// corresponds to, e.g. "<<=".
  static const char *getOpcodeStr(Opcode Op);

  const char *getOpcodeStr() const { return getOpcodeStr(getOpcode()); }

  /// predicates to categorize the respective opcodes.
  bool isMultiplicativeOp() const { return Opc >= BO_Mul && Opc <= BO_LDiv; }
  bool isMatrixMultiplicativeOp() const { return Opc >= BO_MatrixMul && Opc <= BO_MatrixLDiv; }
  static bool isAdditiveOp(Opcode Opc) { return Opc == BO_Add || Opc==BO_Sub; }
  bool isAdditiveOp() const { return isAdditiveOp(getOpcode()); }
  static bool isShiftOp(Opcode Opc) { return Opc == BO_Shl || Opc == BO_Shr; }
  bool isShiftOp() const { return isShiftOp(getOpcode()); }

  static bool isBitwiseOp(Opcode Opc) { return Opc >= BO_And && Opc <= BO_Or; }
  bool isBitwiseOp() const { return isBitwiseOp(getOpcode()); }

  static bool isRelationalOp(Opcode Opc) { return Opc >= BO_LT && Opc<=BO_GE; }
  bool isRelationalOp() const { return isRelationalOp(getOpcode()); }

  static bool isEqualityOp(Opcode Opc) { return Opc == BO_EQ || Opc == BO_NE; }
  bool isEqualityOp() const { return isEqualityOp(getOpcode()); }

  static bool isComparisonOp(Opcode Opc) { return Opc >= BO_LT && Opc<=BO_NE; }
  bool isComparisonOp() const { return isComparisonOp(getOpcode()); }

  static bool isLogicalOp(Opcode Opc) { return Opc == BO_LAnd || Opc==BO_LOr; }
  bool isLogicalOp() const { return isLogicalOp(getOpcode()); }

  bool isAssignmentOp() const { return Opc >= BO_Assign && Opc <= BO_OrAssign; }
  bool isCompoundAssignmentOp() const {
    return Opc > BO_Assign && Opc <= BO_OrAssign;
  }
  bool isShiftAssignOp() const {
    return Opc == BO_ShlAssign || Opc == BO_ShrAssign;
  }

  static bool classof(const Stmt *S) {
    return S->getStmtClass() >= firstBinaryOperatorConstant &&
           S->getStmtClass() <= lastBinaryOperatorConstant;
  }
  static bool classof(const BinaryOperator *) { return true; }

  // Iterators
  child_range children() {
	  return child_range(&SubExprs[0], &SubExprs[0]+END_EXPR);
  }

protected:
  BinaryOperator(Expr *lhs, Expr *rhs, Opcode opc, Type ResTy,
		  ExprValueKind VK, SourceLocation opLoc, bool dead)
    : Expr(CompoundAssignOperatorClass, ResTy, VK),
      Opc(opc), OpLoc(opLoc) {
    SubExprs[LHS] = lhs;
    SubExprs[RHS] = rhs;
  }

  BinaryOperator(StmtClass SC, EmptyShell Empty)
    : Expr(SC, Empty), Opc(BO_MulAssign) { }
};

/// CompoundAssignOperator - For compound assignments (e.g. +=), we keep
/// track of the type the operation is performed in.  Due to the semantics of
/// these operators, the operands are promoted, the aritmetic performed, an
/// implicit conversion back to the result type done, then the assignment takes
/// place.  This captures the intermediate type which the computation is done
/// in.
/// This is supported in Octave only,
/// include : += -= *= /= \= ^= >>= <<= .*= ./= .\= .^= &= |=
///
class CompoundAssignOperator : public BinaryOperator {
  Type ComputationLHSType;
  Type ComputationResultType;
public:
  CompoundAssignOperator(Expr *lhs, Expr *rhs, Opcode opc,
                         Type ResType,
                         Type CompLHSType,
                         Type CompResultType,
                         SourceLocation OpLoc)
    : BinaryOperator(lhs, rhs, opc, ResType, VK_RValue, OpLoc, true),
      ComputationLHSType(CompLHSType),
      ComputationResultType(CompResultType) {
    assert(isCompoundAssignmentOp() &&
           "Only should be used for compound assignments");
  }

  /// \brief Build an empty compound assignment operator expression.
  explicit CompoundAssignOperator(EmptyShell Empty)
    : BinaryOperator(CompoundAssignOperatorClass, Empty) { }

  // The two computation types are the type the LHS is converted
  // to for the computation and the type of the result; the two are
  // distinct in a few cases (specifically, int+=ptr and ptr-=ptr).
  Type getComputationLHSType() const { return ComputationLHSType; }
  void setComputationLHSType(Type T) { ComputationLHSType = T; }

  Type getComputationResultType() const { return ComputationResultType; }
  void setComputationResultType(Type T) { ComputationResultType = T; }

  static bool classof(const CompoundAssignOperator *) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == CompoundAssignOperatorClass;
  }
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_BINARY_OPERATOR_H_ */
