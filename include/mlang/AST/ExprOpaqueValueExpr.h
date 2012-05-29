//===--- OpaqueValueExpr.h - OpaqueValueExpr for Mlang  ---------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines OpaqueValueExpr.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_OPAQUEVALUEEXPR_H_
#define MLANG_AST_OPAQUEVALUEEXPR_H_

#include "mlang/AST/Expr.h"

namespace mlang {
/// OpaqueValueExpr - An expression referring to an opaque object of a
/// fixed type and value class.  These don't correspond to concrete
/// syntax; instead they're used to express operations (usually copy
/// operations) on values whose source is generally obvious from
/// context.
class OpaqueValueExpr : public Expr {
  friend class ASTStmtReader;
  Expr *SourceExpr;
  SourceLocation Loc;

public:
  OpaqueValueExpr(SourceLocation Loc, Type T, ExprValueKind VK)
    : Expr(OpaqueValueExprClass, T, VK),
      SourceExpr(0), Loc(Loc) {
  }

  /// Given an expression which invokes a copy constructor --- i.e.  a
  /// CXXConstructExpr, possibly wrapped in an ExprWithCleanups ---
  /// find the OpaqueValueExpr that's the source of the construction.
  static const OpaqueValueExpr *findInCopyConstruct(const Expr *expr);

  explicit OpaqueValueExpr(EmptyShell Empty)
    : Expr(OpaqueValueExprClass, Empty) { }

  /// \brief Retrieve the location of this expression.
  SourceLocation getLocation() const { return Loc; }

  SourceRange getSourceRange() const {
    if (SourceExpr) return SourceExpr->getSourceRange();
    return Loc;
  }
  SourceLocation getExprLoc() const {
    if (SourceExpr) return SourceExpr->getExprLoc();
    return Loc;
  }

  child_range children() { return child_range(); }

  /// The source expression of an opaque value expression is the
  /// expression which originally generated the value.  This is
  /// provided as a convenience for analyses that don't wish to
  /// precisely model the execution behavior of the program.
  ///
  /// The source expression is typically set when building the
  /// expression which binds the opaque value expression in the first
  /// place.
  Expr *getSourceExpr() const { return SourceExpr; }
  void setSourceExpr(Expr *e) { SourceExpr = e; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == OpaqueValueExprClass;
  }
  static bool classof(const OpaqueValueExpr *) { return true; }
};

} // end namespace mlang

#endif /* MLANG_AST_OPAQUEVALUEEXPR_H_ */
