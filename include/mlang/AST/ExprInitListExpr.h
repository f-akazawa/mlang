//===--- ExprInitListExpr.h - Initialization List Expr ----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines Initialization List Expr.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_INITLISTEXPR_H_
#define MLANG_AST_EXPR_INITLISTEXPR_H_

#include "mlang/AST/ASTVector.h"

namespace mlang {
class MemberDefn;

/// @brief Describes an C or C++ initializer list.
///
/// InitListExpr describes an initializer list, which can be used to
/// initialize objects of different types, including
/// struct/class/union types, arrays, and vectors. For example:
///
/// @code
/// struct foo x = { 1, { 2, 3 } };
/// @endcode
///
/// Prior to semantic analysis, an initializer list will represent the
/// initializer list as written by the user, but will have the
/// placeholder type "void". This initializer list is called the
/// syntactic form of the initializer, and may contain C99 designated
/// initializers (represented as DesignatedInitExprs), initializations
/// of subobject members without explicit braces, and so on. Clients
/// interested in the original syntax of the initializer list should
/// use the syntactic form of the initializer list.
///
/// After semantic analysis, the initializer list will represent the
/// semantic form of the initializer, where the initializations of all
/// subobjects are made explicit with nested InitListExpr nodes and
/// C99 designators have been eliminated by placing the designated
/// initializations into the subobject they initialize. Additionally,
/// any "holes" in the initialization, where no initializer has been
/// specified for a particular subobject, will be replaced with
/// implicitly-generated ImplicitValueInitExpr expressions that
/// value-initialize the subobjects. Note, however, that the
/// initializer lists may still have fewer initializers than there are
/// elements to initialize within the object.
///
/// Given the semantic form of the initializer list, one can retrieve
/// the original syntactic form of that initializer list (if it
/// exists) using getSyntacticForm(). Since many initializer lists
/// have the same syntactic and semantic forms, getSyntacticForm() may
/// return NULL, indicating that the current initializer list also
/// serves as its syntactic form.
class InitListExpr : public Expr {
  // FIXME: Eliminate this vector in favor of ASTContext allocation
  typedef ASTVector<Stmt *> InitExprsTy;
  InitExprsTy InitExprs;
  SourceLocation LBraceLoc, RBraceLoc;

  /// Contains the initializer list that describes the syntactic form
  /// written in the source code.
  InitListExpr *SyntacticForm;

  /// Whether this initializer list originally had a GNU array-range
  /// designator in it. This is a temporary marker used by CodeGen.
  bool HadArrayRangeDesignator;

public:
  InitListExpr(ASTContext &C, SourceLocation lbraceloc,
               Expr **initexprs, unsigned numinits,
               SourceLocation rbraceloc);

  /// \brief Build an empty initializer list.
  explicit InitListExpr(ASTContext &C, EmptyShell Empty)
    : Expr(InitListExprClass, Empty), InitExprs(C) { }

  unsigned getNumInits() const { return InitExprs.size(); }

  /// \brief Retrieve the set of initializers.
  Expr **getInits() { return reinterpret_cast<Expr **>(InitExprs.data()); }

  const Expr *getInit(unsigned Init) const {
    assert(Init < getNumInits() && "Initializer access out of range!");
    return cast_or_null<Expr>(InitExprs[Init]);
  }

  Expr *getInit(unsigned Init) {
    assert(Init < getNumInits() && "Initializer access out of range!");
    return cast_or_null<Expr>(InitExprs[Init]);
  }

  void setInit(unsigned Init, Expr *expr) {
    assert(Init < getNumInits() && "Initializer access out of range!");
    InitExprs[Init] = expr;
  }

  /// \brief Reserve space for some number of initializers.
  void reserveInits(ASTContext &C, unsigned NumInits);

  /// @brief Specify the number of initializers
  ///
  /// If there are more than @p NumInits initializers, the remaining
  /// initializers will be destroyed. If there are fewer than @p
  /// NumInits initializers, NULL expressions will be added for the
  /// unknown initializers.
  void resizeInits(ASTContext &Context, unsigned NumInits);

  /// @brief Updates the initializer at index @p Init with the new
  /// expression @p expr, and returns the old expression at that
  /// location.
  ///
  /// When @p Init is out of range for this initializer list, the
  /// initializer list will be extended with NULL expressions to
  /// accomodate the new entry.
  Expr *updateInit(ASTContext &C, unsigned Init, Expr *expr);

  // Explicit InitListExpr's originate from source code (and have valid source
  // locations). Implicit InitListExpr's are created by the semantic analyzer.
  bool isExplicit() {
    return LBraceLoc.isValid() && RBraceLoc.isValid();
  }

  SourceLocation getLBraceLoc() const { return LBraceLoc; }
  void setLBraceLoc(SourceLocation Loc) { LBraceLoc = Loc; }
  SourceLocation getRBraceLoc() const { return RBraceLoc; }
  void setRBraceLoc(SourceLocation Loc) { RBraceLoc = Loc; }

  /// @brief Retrieve the initializer list that describes the
  /// syntactic form of the initializer.
  ///
  ///
  InitListExpr *getSyntacticForm() const { return SyntacticForm; }
  void setSyntacticForm(InitListExpr *Init) { SyntacticForm = Init; }

  bool hadArrayRangeDesignator() const { return HadArrayRangeDesignator; }
  void sawArrayRangeDesignator(bool ARD = true) {
    HadArrayRangeDesignator = ARD;
  }

  SourceRange getSourceRange() const;

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == InitListExprClass;
  }
  static bool classof(const InitListExpr *) { return true; }

  // Iterators
  child_range children() {
	  if(InitExprs.size() != 0)
		  return child_range(&InitExprs[0], &InitExprs[0] + InitExprs.size());
	  return child_range();
  }

  typedef InitExprsTy::iterator iterator;
  typedef InitExprsTy::const_iterator const_iterator;
  typedef InitExprsTy::reverse_iterator reverse_iterator;
  typedef InitExprsTy::const_reverse_iterator const_reverse_iterator;

  iterator begin() { return InitExprs.begin(); }
  const_iterator begin() const { return InitExprs.begin(); }
  iterator end() { return InitExprs.end(); }
  const_iterator end() const { return InitExprs.end(); }
  reverse_iterator rbegin() { return InitExprs.rbegin(); }
  const_reverse_iterator rbegin() const { return InitExprs.rbegin(); }
  reverse_iterator rend() { return InitExprs.rend(); }
  const_reverse_iterator rend() const { return InitExprs.rend(); }
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_INITLISTEXPR_H_ */
