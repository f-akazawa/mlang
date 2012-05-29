//===--- UnresolvedSet.h - Unresolved sets of declarations ------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the UnresolvedSet class, which is used to store
//  collections of declarations in the AST.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_UNRESOLVEDSET_H_
#define MLANG_AST_UNRESOLVEDSET_H_

#include <iterator>
#include "llvm/ADT/SmallVector.h"
#include "mlang/AST/DefnAccessPair.h"

namespace mlang {

/// The iterator over UnresolvedSets.  Serves as both the const and
/// non-const iterator.
class UnresolvedSetIterator {
private:
  typedef llvm::SmallVectorImpl<DefnAccessPair> DefnsTy;
  typedef DefnsTy::iterator IteratorTy;

  IteratorTy ir;

  friend class UnresolvedSetImpl;
  friend class OverloadExpr;
  explicit UnresolvedSetIterator(DefnsTy::iterator ir) : ir(ir) {}
  explicit UnresolvedSetIterator(DefnsTy::const_iterator ir) :
    ir(const_cast<DefnsTy::iterator>(ir)) {}

  IteratorTy getIterator() const { return ir; }

public:
  UnresolvedSetIterator() {}

  typedef std::iterator_traits<IteratorTy>::difference_type difference_type;
  typedef NamedDefn *value_type;
  typedef NamedDefn **pointer;
  typedef NamedDefn *reference;
  typedef std::iterator_traits<IteratorTy>::iterator_category iterator_category;

  NamedDefn *getDefn() const { return ir->getDefn(); }
  AccessSpecifier getAccess() const { return ir->getAccess(); }
  void setAccess(AccessSpecifier AS) { ir->setAccess(AS); }
  DefnAccessPair getPair() const { return *ir; }

  NamedDefn *operator*() const { return getDefn(); }

  UnresolvedSetIterator &operator++() { ++ir; return *this; }
  UnresolvedSetIterator operator++(int) { return UnresolvedSetIterator(ir++); }
  UnresolvedSetIterator &operator--() { --ir; return *this; }
  UnresolvedSetIterator operator--(int) { return UnresolvedSetIterator(ir--); }

  UnresolvedSetIterator &operator+=(difference_type d) {
    ir += d; return *this;
  }
  UnresolvedSetIterator operator+(difference_type d) const {
    return UnresolvedSetIterator(ir + d);
  }
  UnresolvedSetIterator &operator-=(difference_type d) {
    ir -= d; return *this;
  }
  UnresolvedSetIterator operator-(difference_type d) const {
    return UnresolvedSetIterator(ir - d);
  }
  value_type operator[](difference_type d) const { return *(*this + d); }

  difference_type operator-(const UnresolvedSetIterator &o) const {
    return ir - o.ir;
  }

  bool operator==(const UnresolvedSetIterator &o) const { return ir == o.ir; }
  bool operator!=(const UnresolvedSetIterator &o) const { return ir != o.ir; }
  bool operator<(const UnresolvedSetIterator &o) const { return ir < o.ir; }
  bool operator<=(const UnresolvedSetIterator &o) const { return ir <= o.ir; }
  bool operator>=(const UnresolvedSetIterator &o) const { return ir >= o.ir; }
  bool operator>(const UnresolvedSetIterator &o) const { return ir > o.ir; }
};

/// UnresolvedSet - A set of unresolved declarations.
class UnresolvedSetImpl {
  typedef UnresolvedSetIterator::DefnsTy DefnsTy;

  // Don't allow direct construction, and only permit subclassing by
  // UnresolvedSet.
private:
  template <unsigned N> friend class UnresolvedSet;
  UnresolvedSetImpl() {}
  UnresolvedSetImpl(const UnresolvedSetImpl &) {}

public:
  // We don't currently support assignment through this iterator, so we might
  // as well use the same implementation twice.
  typedef UnresolvedSetIterator iterator;
  typedef UnresolvedSetIterator const_iterator;

  iterator begin() { return iterator(defns().begin()); }
  iterator end() { return iterator(defns().end()); }

  const_iterator begin() const { return const_iterator(defns().begin()); }
  const_iterator end() const { return const_iterator(defns().end()); }

  void addDefn(NamedDefn *D) {
    addDefn(D, AS_none);
  }

  void addDefn(NamedDefn *D, AccessSpecifier AS) {
    defns().push_back(DefnAccessPair::make(D, AS));
  }

  /// Replaces the given declaration with the new one, once.
  ///
  /// \return true if the set changed
  bool replace(const NamedDefn* Old, NamedDefn *New) {
    for (DefnsTy::iterator I = defns().begin(), E = defns().end(); I != E; ++I)
      if (I->getDefn() == Old)
        return (I->setDefn(New), true);
    return false;
  }

  /// Replaces the declaration at the given iterator with the new one,
  /// preserving the original access bits.
  void replace(iterator I, NamedDefn *New) {
    I.ir->setDefn(New);
  }

  void replace(iterator I, NamedDefn *New, AccessSpecifier AS) {
    I.ir->set(New, AS);
  }

  void erase(unsigned I) {
    defns()[I] = defns().back();
    defns().pop_back();
  }

  void erase(iterator I) {
    *I.ir = defns().back();
    defns().pop_back();
  }

  void setAccess(iterator I, AccessSpecifier AS) {
    I.ir->setAccess(AS);
  }

  void clear() { defns().clear(); }
  void set_size(unsigned N) { defns().set_size(N); }

  bool empty() const { return defns().empty(); }
  unsigned size() const { return defns().size(); }

  void append(iterator I, iterator E) {
    defns().append(I.ir, E.ir);
  }

  DefnAccessPair &operator[](unsigned I) { return defns()[I]; }
  const DefnAccessPair &operator[](unsigned I) const { return defns()[I]; }

private:
  // These work because the only permitted subclass is UnresolvedSetImpl

  DefnsTy &defns() {
    return *reinterpret_cast<DefnsTy*>(this);
  }
  const DefnsTy &defns() const {
    return *reinterpret_cast<const DefnsTy*>(this);
  }
};

/// A set of unresolved declarations
template <unsigned InlineCapacity> class UnresolvedSet :
    public UnresolvedSetImpl {
  llvm::SmallVector<DefnAccessPair, InlineCapacity> Defns;
};

} // end namespace mlang

#endif /* UNRESOLVEDSET_H_ */
