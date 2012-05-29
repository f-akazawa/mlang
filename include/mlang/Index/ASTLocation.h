//===--- ASTLocation.h - A <Decl, Stmt> pair --------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  ASTLocation is Defn or a Stmt and its immediate Defn parent.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_ASTLOCATION_H_
#define MLANG_FRONTEND_ASTLOCATION_H_

#include "mlang/AST/Type.h"
#include "llvm/ADT/PointerIntPair.h"

namespace llvm {
  class raw_ostream;
}

namespace mlang {
  class Defn;
  class Stmt;
  class NamedDefn;

namespace idx {
  class TranslationUnit;

/// \brief Represents a Defn or a Stmt and its immediate Decl parent. It's
/// immutable.
///
/// ASTLocation is intended to be used as a "pointer" into the AST. It is either
/// just a Decl, or a Stmt and its Decl parent. Since a single Stmt is devoid
/// of context, its parent Decl provides all the additional missing information
/// like the declaration context, ASTContext, etc.
///
class ASTLocation {
public:
  enum NodeKind {
    N_Decl, N_NamedRef, N_Stmt, N_Type
  };

  struct NamedRef {
    NamedDefn *ND;
    SourceLocation Loc;
    
    NamedRef() : ND(0) { }
    NamedRef(NamedDefn *nd, SourceLocation loc) : ND(nd), Loc(loc) { }
  };

private:
  llvm::PointerIntPair<Defn *, 2, NodeKind> ParentDecl;

  union {
    Defn *D;
    Stmt *Stm;
    struct {
      NamedDefn *ND;
      unsigned RawLoc;
    } NDRef;
    Type *Ty;
  };

public:
  ASTLocation() { }

  explicit ASTLocation(const Defn *d)
    : ParentDecl(const_cast<Defn*>(d), N_Decl), D(const_cast<Defn*>(d)) { }

  ASTLocation(const Defn *parentDecl, const Stmt *stm)
    : ParentDecl(const_cast<Defn*>(parentDecl), N_Stmt),
      Stm(const_cast<Stmt*>(stm)) {
    if (!stm) ParentDecl.setPointer(0);
  }

  ASTLocation(const Defn *parentDecl, NamedDefn *ndRef, SourceLocation loc)
    : ParentDecl(const_cast<Defn*>(parentDecl), N_NamedRef) {
    if (ndRef) {
      NDRef.ND = ndRef;
      NDRef.RawLoc = loc.getRawEncoding();
    } else
      ParentDecl.setPointer(0);
  }

  ASTLocation(const Defn *parentDecl, Type ty)
    : ParentDecl(const_cast<Defn*>(parentDecl), N_Type) {
    if (ty.getRawTypePtr()) {
      Ty = &ty;
    } else
      ParentDecl.setPointer(0);
  }

  bool isValid() const { return ParentDecl.getPointer() != 0; }
  bool isInvalid() const { return !isValid(); }
  
  NodeKind getKind() const {
    assert(isValid());
    return (NodeKind)ParentDecl.getInt();
  }
  
  Defn *getParentDecl() const { return ParentDecl.getPointer(); }
  
  Defn *AsDecl() const {
    assert(getKind() == N_Decl);
    return D;
  }
  Stmt *AsStmt() const {
    assert(getKind() == N_Stmt);
    return Stm;
  }
  NamedRef AsNamedRef() const {
    assert(getKind() == N_NamedRef);
    return NamedRef(NDRef.ND, SourceLocation::getFromRawEncoding(NDRef.RawLoc));
  }
  Type *AsType() const {
    assert(getKind() == N_Type);
    return Ty;
  }

  Defn *dyn_AsDecl() const { return isValid() && getKind() == N_Decl ? D : 0; }
  Stmt *dyn_AsStmt() const { return isValid() && getKind() == N_Stmt ? Stm : 0; }
  NamedRef dyn_AsNamedRef() const {
    return getKind() == N_Type ? AsNamedRef() : NamedRef();
  }
  Type *dyn_AsType() const {
    return getKind() == N_Type ? AsType() : 0;
  }
  
  bool isDecl() const { return isValid() && getKind() == N_Decl; }
  bool isStmt() const { return isValid() && getKind() == N_Stmt; }
  bool isNamedRef() const { return isValid() && getKind() == N_NamedRef; }
  bool isType() const { return isValid() && getKind() == N_Type; }

  /// \brief Returns the declaration that this ASTLocation references.
  ///
  /// If this points to a Decl, that Decl is returned.
  /// If this points to an Expr that references a Decl, that Decl is returned,
  /// otherwise it returns NULL.
  Defn *getReferencedDecl();
  const Defn *getReferencedDecl() const {
    return const_cast<ASTLocation*>(this)->getReferencedDecl();
  }

  SourceRange getSourceRange() const;

  void print(llvm::raw_ostream &OS) const;
};

/// \brief Like ASTLocation but also contains the TranslationUnit that the
/// ASTLocation originated from.
class TULocation : public ASTLocation {
  TranslationUnit *TU;

public:
  TULocation(TranslationUnit *tu, ASTLocation astLoc)
    : ASTLocation(astLoc), TU(tu) {
    assert(tu && "Passed null translation unit");
  }

  TranslationUnit *getTU() const { return TU; }
};

} // namespace idx

} // namespace mlang

#endif /* MLANG_FRONTEND_ASTLOCATION_H_ */
