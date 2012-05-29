//===--- ExprImaginaryLiteral.h - Complex Constant Number -------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the ImaginaryLiteral class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_IMAGINARY_LITERAL_H_
#define MLANG_AST_EXPR_IMAGINARY_LITERAL_H_

namespace mlang {
/// ImaginaryLiteral - This is a complex number, e.g. 1-9i
///
class ImaginaryLiteral : public Expr {
	Stmt *Val;

public:
  ImaginaryLiteral(Expr *imag, Type Ty)
      : Expr(ImaginaryLiteralClass, Ty, VK_RValue),Val(imag) {
    }

  /// \brief Build an empty imaginary literal.
  explicit ImaginaryLiteral(EmptyShell Empty)
    : Expr(ImaginaryLiteralClass, Empty) { }

  const Expr *getSubExpr() const { return cast<Expr>(Val); }
  Expr *getSubExpr() { return cast<Expr>(Val); }
  void setSubExpr(Expr *E) { Val = E; }

  SourceRange getSourceRange() const {
	  return Val->getSourceRange();
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == ImaginaryLiteralClass;
  }
  static bool classof(const ImaginaryLiteral *) { return true; }

  // Iterators
  child_range children() {
	  return child_range(&Val, &Val+1);
  }
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_IMAGINARY_LITERAL_H_ */
