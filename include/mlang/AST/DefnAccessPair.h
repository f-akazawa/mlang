//===--- DeclAccessPair.h - A decl bundled with its path access -*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the DefnAccessPair class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_DEFN_ACCESS_PAIR_H_
#define MLANG_AST_DEFN_ACCESS_PAIR_H_

#include "mlang/Basic/Specifiers.h"

namespace mlang {
class NamedDefn;

/// A POD class for pairing a NamedDefn* with an access specifier.
/// Can be put into unions.
class DefnAccessPair {
  NamedDefn *Ptr; // we'd use llvm::PointerUnion, but it isn't trivial

  enum { Mask = 0x3 };

public:
  static DefnAccessPair make(NamedDefn *D, AccessSpecifier AS) {
    DefnAccessPair p;
    p.set(D, AS);
    return p;
  }

  NamedDefn *getDefn() const {
    return (NamedDefn*) (~Mask & (uintptr_t) Ptr);
  }
  AccessSpecifier getAccess() const {
    return AccessSpecifier(Mask & (uintptr_t) Ptr);
  }

  void setDefn(NamedDefn *D) {
    set(D, getAccess());
  }
  void setAccess(AccessSpecifier AS) {
    set(getDefn(), AS);
  }
  void set(NamedDefn *D, AccessSpecifier AS) {
    Ptr = reinterpret_cast<NamedDefn*>(uintptr_t(AS) |
                                       reinterpret_cast<uintptr_t>(D));
  }

  operator NamedDefn*() const { return getDefn(); }
  NamedDefn *operator->() const { return getDefn(); }
};
} // end namespace mlang

// Take a moment to tell SmallVector that DefnAccessPair is POD.
namespace llvm {
template<typename> struct isPodLike;
template<> struct isPodLike<mlang::DefnAccessPair> {
   static const bool value = true;
};
}

#endif /* MLANG_AST_DEFN_ACCESS_PAIR_H_ */
