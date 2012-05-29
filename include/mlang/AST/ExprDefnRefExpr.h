//===--- ExprDefnRef.h - DefnRefExpr for Mlang  -----------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines DefnRefExpr.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_DEFN_REF_H_
#define MLANG_AST_EXPR_DEFN_REF_H_

#include "mlang/AST/Expr.h"
#include "mlang/AST/DefnSub.h"
#include "mlang/AST/DefinitionName.h"
#include "llvm/ADT/PointerIntPair.h"

namespace mlang {
class ValueDefn;

/// DefnRefExpr - A reference to a defined variable, function handle, struct,
/// cell array, etc.
class DefnRefExpr : public Expr {
  enum {
    // Flag on DecoratedD that specifies when this declaration reference
    // expression has a class or container nested-name-specifier.
    HasQualifierFlag = 0x01
  };

  // DecoratedD - The declaration that we are referencing, plus two bits to
  // indicate whether (1) the declaration's name was explicitly qualified and
  // (2) the declaration's name was followed by an explicit template
  // argument list.
  llvm::PointerIntPair<ValueDefn *, 1> DecoratedD;

  // Loc - The location of the declaration name itself.
  SourceLocation Loc;

  /// \brief Retrieve the qualifier that preceded the declaration name, if any.
  NameQualifier *getNameQualifier() {
    if ((DecoratedD.getInt() & HasQualifierFlag) == 0)
      return 0;

    return reinterpret_cast<NameQualifier *> (this + 1);
  }

  /// \brief Retrieve the qualifier that preceded the member name, if any.
  const NameQualifier *getNameQualifier() const {
    return const_cast<DefnRefExpr *>(this)->getNameQualifier();
  }

  DefnRefExpr(NestedNameSpecifier *Qualifier, SourceRange QualifierRange,
              ValueDefn *D, SourceLocation NameLoc,
              Type T, ExprValueKind VK);

  DefnRefExpr(NestedNameSpecifier *Qualifier, SourceRange QualifierRange,
              ValueDefn *D, const DefinitionNameInfo &NameInfo,
              Type T, ExprValueKind VK);

  /// \brief Construct an empty declaration reference expression.
  explicit DefnRefExpr(EmptyShell Empty)
    : Expr(DefnRefExprClass, Empty) { }

  /// \brief Computes the type- and value-dependence flags for this
  /// declaration reference expression.
  // void computeDependence();

public:
  DefnRefExpr(ValueDefn *d, Type t, ExprValueKind VK, SourceLocation l) :
    Expr(DefnRefExprClass, t, VK),
    DecoratedD(d, 0), Loc(l) {
    // computeDependence();
  }

  static DefnRefExpr *Create(ASTContext &Context,
                             NestedNameSpecifier *Qualifier,
                             SourceRange QualifierRange,
                             ValueDefn *D,
                             SourceLocation NameLoc,
                             Type T, ExprValueKind VK);

  static DefnRefExpr *Create(ASTContext &Context,
                             NestedNameSpecifier *Qualifier,
                             SourceRange QualifierRange,
                             ValueDefn *D,
                             const DefinitionNameInfo &NameInfo,
                             Type T, ExprValueKind VK);

  /// \brief Construct an empty declaration reference expression.
  static DefnRefExpr *CreateEmpty(ASTContext &Context,
                                  bool HasQualifier);

  ValueDefn *getDefn() { return DecoratedD.getPointer(); }
  const ValueDefn *getDefn() const { return DecoratedD.getPointer(); }
  void setDefn(ValueDefn *NewD) { DecoratedD.setPointer(NewD); }

  DefinitionNameInfo getNameInfo() const {
    return DefinitionNameInfo(getDefn()->getDefnName(), Loc);
  }

  SourceLocation getLocation() const { return Loc; }
  void setLocation(SourceLocation L) { Loc = L; }
  SourceRange getSourceRange() const;

  /// \brief Determine whether this declaration reference was preceded by a
  /// C++ nested-name-specifier, e.g., \c N::foo.
  bool hasQualifier() const { return DecoratedD.getInt() & HasQualifierFlag; }

  /// \brief If the name was qualified, retrieves the source range of
  /// the nested-name-specifier that precedes the name. Otherwise,
  /// returns an empty source range.
  SourceRange getQualifierRange() const {
    if (!hasQualifier())
      return SourceRange();

    return getNameQualifier()->Range;
  }

  /// \brief If the name was qualified, retrieves the nested-name-specifier
  /// that precedes the name. Otherwise, returns NULL.
  NestedNameSpecifier *getQualifier() const {
    if (!hasQualifier())
      return 0;

    return getNameQualifier()->NNS;
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == DefnRefExprClass;
  }
  static bool classof(const DefnRefExpr *) { return true; }

  // Iterators
  child_range children() {
	  return child_range();
  }

  friend class ASTStmtReader;
  friend class ASTStmtWriter;
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_DEFN_REF_H_ */
