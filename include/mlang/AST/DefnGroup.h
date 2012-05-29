//===--- DefnGroup.h - Classes for representing groups of Defns -*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the DefnGroup, DefnGroupRef, and OwningDefnGroup classes.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_DEFNGROUP_H_
#define MLANG_AST_DEFNGROUP_H_

#include "llvm/Support/DataTypes.h"
#include <cassert>

namespace mlang {
class ASTContext;
class Defn;
class DefnGroup;
class DefnGroupIterator;

class DefnGroup {
  // FIXME: Include a TypeSpecifier object.
  unsigned NumDefns;

private:
  DefnGroup() : NumDefns(0) {}
  DefnGroup(unsigned numdecls, Defn** defns);

public:
  static DefnGroup *Create(ASTContext &C, Defn **Defns, unsigned NumDefns);

  unsigned size() const { return NumDefns; }

  Defn*& operator[](unsigned i) {
    assert (i < NumDefns && "Out-of-bounds access.");
    return *((Defn**) (this+1));
  }

  Defn* const& operator[](unsigned i) const {
    assert (i < NumDefns && "Out-of-bounds access.");
    return *((Defn* const*) (this+1));
  }
};

class DefnGroupRef {
  // Note this is not a PointerIntPair because we need the address of the
  // non-group case to be valid as a Defn** for iteration.
  enum Kind { SingleDefnKind=0x0, DefnGroupKind=0x1, Mask=0x1 };
  Defn* D;

  Kind getKind() const {
    return (Kind) (reinterpret_cast<uintptr_t>(D) & Mask);
  }

public:
  DefnGroupRef() : D(0) {}

  explicit DefnGroupRef(Defn* d) : D(d) {}
  explicit DefnGroupRef(DefnGroup* dg)
    : D((Defn*) (reinterpret_cast<uintptr_t>(dg) | DefnGroupKind)) {}

  static DefnGroupRef Create(ASTContext &C, Defn **Defns, unsigned NumDefns) {
    if (NumDefns == 0)
      return DefnGroupRef();
    if (NumDefns == 1)
      return DefnGroupRef(Defns[0]);
    return DefnGroupRef(DefnGroup::Create(C, Defns, NumDefns));
  }

  typedef Defn** iterator;
  typedef Defn* const * const_iterator;

  bool isNull() const { return D == 0; }
  bool isSingleDefn() const { return getKind() == SingleDefnKind; }
  bool isDefnGroup() const { return getKind() == DefnGroupKind; }

  Defn *getSingleDefn() {
    assert(isSingleDefn() && "Isn't a declgroup");
    return D;
  }
  const Defn *getSingleDefn() const {
    return const_cast<DefnGroupRef*>(this)->getSingleDefn();
  }

  DefnGroup &getDefnGroup() {
    assert(isDefnGroup() && "Isn't a defngroup");
    return *((DefnGroup*)(reinterpret_cast<uintptr_t>(D) & ~Mask));
  }
  const DefnGroup &getDefnGroup() const {
    return const_cast<DefnGroupRef*>(this)->getDefnGroup();
  }

  iterator begin() {
    if (isSingleDefn())
      return D ? &D : 0;
    return &getDefnGroup()[0];
  }

  iterator end() {
    if (isSingleDefn())
      return D ? &D+1 : 0;
    DefnGroup &G = getDefnGroup();
    return &G[0] + G.size();
  }

  const_iterator begin() const {
    if (isSingleDefn())
      return D ? &D : 0;
    return &getDefnGroup()[0];
  }

  const_iterator end() const {
    if (isSingleDefn())
      return D ? &D+1 : 0;
    const DefnGroup &G = getDefnGroup();
    return &G[0] + G.size();
  }

  void *getAsOpaquePtr() const { return D; }
  static DefnGroupRef getFromOpaquePtr(void *Ptr) {
    DefnGroupRef X;
    X.D = static_cast<Defn*>(Ptr);
    return X;
  }
};

} // end namespace mlang

namespace llvm {
  // DefnGroupRef is "like a pointer", implement PointerLikeTypeTraits.
  template <typename T>
  class PointerLikeTypeTraits;
  template <>
  class PointerLikeTypeTraits<mlang::DefnGroupRef> {
  public:
    static inline void *getAsVoidPointer(mlang::DefnGroupRef P) {
      return P.getAsOpaquePtr();
    }
    static inline mlang::DefnGroupRef getFromVoidPointer(void *P) {
      return mlang::DefnGroupRef::getFromOpaquePtr(P);
    }
    enum { NumLowBitsAvailable = 0 };
  };
} // end namespace llvm

#endif /* MLANG_AST_DEFNGROUP_H_ */
