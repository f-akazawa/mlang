//===--- IdentifierResolver.h - Lexical Scope Name lookup -------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
// This file defines the IdentifierResolver class, which is used for lexical
// scoped lookup, based on definition names.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SEMA_IDENTIFIER_RESOLVER_H_
#define MLANG_SEMA_IDENTIFIER_RESOLVER_H_

#include "mlang/Basic/IdentifierTable.h"

namespace mlang {
class ASTContext;
class Defn;
class DefnContext;
class DefinitionName;
class NamedDefn;
class Scope;

/// IdentifierResolver - Keeps track of shadowed defns on enclosing
/// scopes.  It manages the shadowing chains of definition names and
/// implements efficient defn lookup based on a definition name.
class IdentifierResolver {

  /// IdDefnInfo - Keeps track of information about defns associated
  /// to a particular definition name. IdDefnInfos are lazily
  /// constructed and assigned to a definition name the first time a
  /// defn with that definition name is shadowed in some scope.
  class IdDefnInfo {
  public:
    typedef llvm::SmallVector<NamedDefn*, 2> DefnsTy;

    inline DefnsTy::iterator defns_begin() { return Defns.begin(); }
    inline DefnsTy::iterator defns_end() { return Defns.end(); }

    void AddDefn(NamedDefn *D) { Defns.push_back(D); }

    /// RemoveDefn - Remove the defn from the scope chain.
    /// The defn must already be part of the defn chain.
    void RemoveDefn(NamedDefn *D);

    /// Replaces the Old definition with the New definition. If the
    /// replacement is successful, returns true. If the old
    /// definition was not found, returns false.
    bool ReplaceDefn(NamedDefn *Old, NamedDefn *New);

  private:
    DefnsTy Defns;
  };

public:

  /// iterator - Iterate over the defns of a specified definition name.
  /// It will walk or not the parent definition contexts depending on how
  /// it was instantiated.
  class iterator {
  public:
    typedef NamedDefn *             value_type;
    typedef NamedDefn *             reference;
    typedef NamedDefn *             pointer;
    typedef std::input_iterator_tag iterator_category;
    typedef std::ptrdiff_t          difference_type;

    /// Ptr - There are 3 forms that 'Ptr' represents:
    /// 1) A single NamedDefn. (Ptr & 0x1 == 0)
    /// 2) A IdDefnInfo::DefnsTy::iterator that traverses only the defns of the
    ///    same definition context. (Ptr & 0x3 == 0x1)
    /// 3) A IdDefnInfo::DefnsTy::iterator that traverses the defns of parent
    ///    definition contexts too. (Ptr & 0x3 == 0x3)
    uintptr_t Ptr;
    typedef IdDefnInfo::DefnsTy::iterator BaseIter;

    /// A single NamedDefn. (Ptr & 0x1 == 0)
    iterator(NamedDefn *D) {
      Ptr = reinterpret_cast<uintptr_t>(D);
      assert((Ptr & 0x1) == 0 && "Invalid Ptr!");
    }
    /// A IdDefnInfo::DefnsTy::iterator that walks or not the parent definition
    /// contexts depending on 'LookInParentCtx'.
    iterator(BaseIter I) {
      Ptr = reinterpret_cast<uintptr_t>(I) | 0x1;
    }

    bool isIterator() const { return (Ptr & 0x1); }

    BaseIter getIterator() const {
      assert(isIterator() && "Ptr not an iterator!");
      return reinterpret_cast<BaseIter>(Ptr & ~0x3);
    }

    friend class IdentifierResolver;

    void incrementSlowCase();
  public:
    iterator() : Ptr(0) {}

    NamedDefn *operator*() const {
      if (isIterator())
        return *getIterator();
      else
        return reinterpret_cast<NamedDefn*>(Ptr);
    }

    bool operator==(const iterator &RHS) const {
      return Ptr == RHS.Ptr;
    }
    bool operator!=(const iterator &RHS) const {
      return Ptr != RHS.Ptr;
    }

    // Preincrement.
    iterator& operator++() {
      if (!isIterator()) // common case.
        Ptr = 0;
      else
        incrementSlowCase();
      return *this;
    }

    uintptr_t getAsOpaqueValue() const { return Ptr; }

    static iterator getFromOpaqueValue(uintptr_t P) {
      iterator Result;
      Result.Ptr = P;
      return Result;
    }
  };

  /// begin - Returns an iterator for defns with the name 'Name'.
  static iterator begin(DefinitionName Name);

  /// end - Returns an iterator that has 'finished'.
  static iterator end() {
    return iterator();
  }

  /// isDefnInScope - If 'Ctx' is a function/method, isDefnInScope returns true
  /// if 'D' is in Scope 'S', otherwise 'S' is ignored and isDefnInScope returns
  /// true if 'D' belongs to the given definition context.
  bool isDefnInScope(Defn *D, DefnContext *Ctx, ASTContext &Context,
                     Scope *S = 0) const;

  /// AddDefn - Link the defn to its shadowed defn chain.
  void AddDefn(NamedDefn *D);

  /// RemoveDefn - Unlink the defn from its shadowed defn chain.
  /// The defn must already be part of the defn chain.
  void RemoveDefn(NamedDefn *D);

  /// Replace the defn Old with the new definition New on its
  /// identifier chain. Returns true if the old definition was found
  /// (and, therefore, replaced).
  bool ReplaceDefn(NamedDefn *Old, NamedDefn *New);

  /// \brief Link the definition into the chain of declarations for
  /// the given identifier.
  ///
  /// This is a lower-level routine used by the AST reader to link a
  /// definition into a specific IdentifierInfo before the
  /// definition actually has a name.
  void AddDefnToIdentifierChain(IdentifierInfo *II, NamedDefn *D);

  explicit IdentifierResolver(const LangOptions &LangOpt);
  ~IdentifierResolver();

private:
  const LangOptions &LangOpt;

  class IdDefnInfoMap;
  IdDefnInfoMap *IdDefnInfos;

  /// FETokenInfo contains a defn pointer if lower bit == 0.
  static inline bool isDefnPtr(void *Ptr) {
    return (reinterpret_cast<uintptr_t>(Ptr) & 0x1) == 0;
  }

  /// FETokenInfo contains a IdDefnInfo pointer if lower bit == 1.
  static inline IdDefnInfo *toIdDefnInfo(void *Ptr) {
    assert((reinterpret_cast<uintptr_t>(Ptr) & 0x1) == 1
          && "Ptr not a IdDefnInfo* !");
    return reinterpret_cast<IdDefnInfo*>(
                    reinterpret_cast<uintptr_t>(Ptr) & ~0x1);
  }
};

} // end namespace mlang

#endif /* MLANG_SEMA_IDENTIFIER_RESOLVER_H_ */
