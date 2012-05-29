//===--- ExprCharLiteral.h - Character constants for AST  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the CharacterLiteral.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_CHARACTER_LITERAL_H_
#define MLANG_AST_EXPR_CHARACTER_LITERAL_H_

#include "mlang/AST/Expr.h"

namespace mlang {
class CharacterLiteral : public Expr {
  unsigned Value;
  SourceLocation Loc;

public:
  // type should be IntTy
  CharacterLiteral(unsigned value, Type type, SourceLocation l)
    : Expr(CharacterLiteralClass, type, VK_RValue),
      Value(value), Loc(l) {
  }

  /// \brief Construct an empty character literal.
  CharacterLiteral(EmptyShell Empty) : Expr(CharacterLiteralClass, Empty) { }

  SourceLocation getLocation() const { return Loc; }

  SourceRange getSourceRange() const {
	  return SourceRange(getLocation());
  }

  unsigned getValue() const { return Value; }

  void setLocation(SourceLocation Location) { Loc = Location; }

  void setValue(unsigned Val) { Value = Val; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CharacterLiteralClass;
  }
  static bool classof(const CharacterLiteral *) { return true; }

  // Iterators
  child_range children() {
      return child_range();
    }
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_CHARACTER_LITERAL_H_ */
