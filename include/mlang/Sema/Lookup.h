//===--- Lookup.h - Classes for name lookup ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines defines the LookupResult class, which is integral to
// Sema's name-lookup subsystem.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SEMA_LOOKUP_H_
#define MLANG_SEMA_LOOKUP_H_

#include "mlang/Sema/Sema.h"
#include "mlang/AST/DefnOOP.h"

namespace mlang {
//class UserClassDefn;

/// @brief Represents the results of name lookup.
///
/// An instance of the LookupResult class captures the results of a
/// single name lookup, which can return no result (nothing found),
/// a single definition, a set of overloaded functions (OOP), or an
/// ambiguity. Use the getKind() method to determine which of these
/// results occurred for a given lookup.
class LookupResult {
public:
  enum LookupResultKind {
    /// @brief No entity found met the criteria.
    NotFound = 0,

    /// @brief No entity found met the criteria within the current
    /// instantiation, but there were dependent base classes of the
    /// current instantiation that could not be searched.
    NotFoundInCurrentInstantiation,

    /// @brief Name lookup found a single definition that met the
    /// criteria.  getFoundDefn() will return this definition.
    Found,

    /// @brief Name lookup found a set of overloaded functions that
    /// met the criteria.
    FoundOverloaded,

    /// @brief Name lookup results in an ambiguity; use
    /// getAmbiguityKind to figure out what kind of ambiguity
    /// we have.
    Ambiguous
  };

  enum AmbiguityKind {
    /// Name lookup results in an ambiguity because multiple
    /// entities that meet the lookup criteria were found in
    /// subobjects of different types. For example:
    /// @code
    /// struct A { void f(int); }
    /// struct B { void f(double); }
    /// struct C : A, B { };
    /// void test(C c) {
    ///   c.f(0); // error: A::f and B::f come from subobjects of different
    ///           // types. overload resolution is not performed.
    /// }
    /// @endcode
    AmbiguousBaseSubobjectTypes,

    /// Name lookup results in an ambiguity because multiple
    /// nonstatic entities that meet the lookup criteria were found
    /// in different subobjects of the same type. For example:
    /// @code
    /// struct A { int x; };
    /// struct B : A { };
    /// struct C : A { };
    /// struct D : B, C { };
    /// int test(D d) {
    ///   return d.x; // error: 'x' is found in two A subobjects (of B and C)
    /// }
    /// @endcode
    AmbiguousBaseSubobjects,

    /// Name lookup results in an ambiguity because multiple definitions
    /// of entity that meet the lookup criteria were found in different
    /// definition contexts.
    /// @code
    /// namespace A {
    ///   int i;
    ///   namespace B { int i; }
    ///   int test() {
    ///     using namespace B;
    ///     return i; // error 'i' is found in namespace A and A::B
    ///    }
    /// }
    /// @endcode
    AmbiguousReference,

    /// Name lookup results in an ambiguity because an entity with a
    /// tag name was hidden by an entity with an ordinary name from
    /// a different context.
    /// @code
    /// namespace A { struct Foo {}; }
    /// namespace B { void Foo(); }
    /// namespace C {
    ///   using namespace A;
    ///   using namespace B;
    /// }
    /// void test() {
    ///   C::Foo(); // error: tag 'A::Foo' is hidden by an object in a
    ///             // different namespace
    /// }
    /// @endcode
    AmbiguousTagHiding
  };

  /// A little identifier for flagging temporary lookup results.
  enum TemporaryToken {
    Temporary
  };

  typedef UnresolvedSetImpl::iterator iterator;

  LookupResult(Sema &SemaRef, const DefinitionNameInfo &NameInfo,
               Sema::LookupNameKind LookupKind,
               Sema::RedefinitionKind Redefn = Sema::NotForRedefinition)
    : ResultKind(NotFound),
      //Paths(0),
      NamingClass(0),
      SemaRef(SemaRef),
      NameInfo(NameInfo),
      LookupKind(LookupKind),
      IDNS(0),
      Redefn(Redefn != Sema::NotForRedefinition),
      HideTags(true),
      Diagnose(Redefn == Sema::NotForRedefinition)
  {
    configure();
  }

  // TODO: consider whether this constructor should be restricted to take
  // as input a const IndentifierInfo* (instead of Name),
  // forcing other cases towards the constructor taking a DNInfo.
  LookupResult(Sema &SemaRef, DefinitionName Name,
               SourceLocation NameLoc, Sema::LookupNameKind LookupKind,
               Sema::RedefinitionKind Redefn = Sema::NotForRedefinition)
    : ResultKind(NotFound),
      //Paths(0),
      NamingClass(0),
      SemaRef(SemaRef),
      NameInfo(Name, NameLoc),
      LookupKind(LookupKind),
      IDNS(0),
      Redefn(Redefn != Sema::NotForRedefinition),
      HideTags(true),
      Diagnose(Redefn == Sema::NotForRedefinition)
  {
    configure();
  }

  /// Creates a temporary lookup result, initializing its core data
  /// using the information from another result.  Diagnostics are always
  /// disabled.
  LookupResult(TemporaryToken _, const LookupResult &Other)
    : ResultKind(NotFound),
      //Paths(0),
      NamingClass(0),
      SemaRef(Other.SemaRef),
      NameInfo(Other.NameInfo),
      LookupKind(Other.LookupKind),
      IDNS(Other.IDNS),
      Redefn(Other.Redefn),
      HideTags(Other.HideTags),
      Diagnose(false)
  {}

  ~LookupResult() {
    if (Diagnose) diagnose();
    //if (Paths) deletePaths(Paths);
  }

  /// Gets the name info to look up.
  const DefinitionNameInfo &getLookupNameInfo() const {
    return NameInfo;
  }

  /// \brief Sets the name info to look up.
  void setLookupNameInfo(const DefinitionNameInfo &NameInfo) {
    this->NameInfo = NameInfo;
  }

  /// Gets the name to look up.
  DefinitionName getLookupName() const {
    return NameInfo.getName();
  }

  /// \brief Sets the name to look up.
  void setLookupName(DefinitionName Name) {
    NameInfo.setName(Name);
  }

  /// Gets the kind of lookup to perform.
  Sema::LookupNameKind getLookupKind() const {
    return LookupKind;
  }

  /// True if this lookup is just looking for an existing definition.
  bool isForRedefinition() const {
    return Redefn;
  }

  /// Sets whether tag declarations should be hidden by non-tag
  /// declarations during resolution.  The default is true.
  void setHideTags(bool Hide) {
    HideTags = Hide;
  }

  bool isAmbiguous() const {
    return getResultKind() == Ambiguous;
  }

  /// Determines if this names a single result which is not an
  /// unresolved value using decl.  If so, it is safe to call
  /// getFoundDefn().
  bool isSingleResult() const {
    return getResultKind() == Found;
  }

  /// Determines if the results are overloaded.
  bool isOverloadedResult() const {
    return getResultKind() == FoundOverloaded;
  }

  LookupResultKind getResultKind() const {
    sanity();
    return ResultKind;
  }

  AmbiguityKind getAmbiguityKind() const {
    assert(isAmbiguous());
    return Ambiguity;
  }

  const UnresolvedSetImpl &asUnresolvedSet() const {
	  return Defns;
  }

  iterator begin() const { return iterator(Defns.begin()); }
  iterator end() const { return iterator(Defns.end()); }

  /// \brief Return true if no defns were found
  bool empty() const { return Defns.empty(); }

  /// \brief Return the base paths structure that's associated with
  /// these results, or null if none is.
//  CXXBasePaths *getBasePaths() const {
//    return Paths;
//  }

  /// \brief Tests whether the given definition is acceptable.
  bool isAcceptableDefn(NamedDefn *D) const {
    return D->isInIdentifierNamespace(IDNS);
  }

  /// \brief Returns the identifier namespace mask for this lookup.
  unsigned getIdentifierNamespace() const {
    return IDNS;
  }

  /// \brief Returns whether these results arose from performing a
  /// lookup into a class.
  bool isClassLookup() const {
    return NamingClass != 0;
  }

  /// \brief Returns the 'naming class' for this lookup, i.e. the
  /// class which was looked into to find these results.
  ///
  /// C++0x [class.access.base]p5:
  ///   The access to a member is affected by the class in which the
  ///   member is named. This naming class is the class in which the
  ///   member name was looked up and found. [Note: this class can be
  ///   explicit, e.g., when a qualified-id is used, or implicit,
  ///   e.g., when a class member access operator (5.2.5) is used
  ///   (including cases where an implicit "this->" is added). If both
  ///   a class member access operator and a qualified-id are used to
  ///   name the member (as in p->T::m), the class naming the member
  ///   is the class named by the nested-name-specifier of the
  ///   qualified-id (that is, T). -- end note ]
  ///
  /// This is set by the lookup routines when they find results in a class.
  UserClassDefn *getNamingClass() const {
    return NamingClass;
  }

  /// \brief Sets the 'naming class' for this lookup.
  void setNamingClass(UserClassDefn *Record) {
    NamingClass = Record;
  }

  /// \brief Returns the base object type associated with this lookup;
  /// important for [class.protected].  Most lookups do not have an
  /// associated base object.
  Type getBaseObjectType() const {
    return BaseObjectType;
  }

  /// \brief Sets the base object type for this lookup.
  void setBaseObjectType(Type T) {
    BaseObjectType = T;
  }

  /// \brief Add a declaration to these results with its natural access.
  /// Does not test the acceptance criteria.
  void addDefn(NamedDefn *D) {
	  addDefn(D, D->getAccess());
  }

  /// \brief Add a declaration to these results with the given access.
  /// Does not test the acceptance criteria.
  void addDefn(NamedDefn *D, AccessSpecifier AS) {
	  Defns.addDefn(D, AS);
	  ResultKind = Found;
  }

  /// \brief Add all the declarations from another set of lookup
  /// results.
  void addAllDefns(const LookupResult &Other) {
	  Defns.append(Other.Defns.begin(), Other.Defns.end());
	  ResultKind = Found;
  }

  /// \brief Determine whether no result was found because we could not
  /// search into dependent base classes of the current instantiation.
  bool wasNotFoundInCurrentInstantiation() const {
    return ResultKind == NotFoundInCurrentInstantiation;
  }

  /// \brief Note that while no result was found in the current instantiation,
  /// there were dependent base classes that could not be searched.
  void setNotFoundInCurrentInstantiation() {
    assert(ResultKind == NotFound );
    ResultKind = NotFoundInCurrentInstantiation;
  }

  /// \brief Resolves the result kind of the lookup, possibly hiding
  /// decls.
  ///
  /// This should be called in any environment where lookup might
  /// generate multiple lookup results.
  void resolveKind();

  /// \brief Re-resolves the result kind of the lookup after a set of
  /// removals has been performed.
  void resolveKindAfterFilter() {
	  if (Defns.empty()) {
			if (ResultKind != NotFoundInCurrentInstantiation)
				ResultKind = NotFound;

//			if (Paths) {
//				deletePaths( Paths);
//				Paths = 0;
//			}
		} else {
			AmbiguityKind SavedAK = Ambiguity;
			ResultKind = Found;
			resolveKind();

			// If we didn't make the lookup unambiguous, restore the old
			// ambiguity kind.
			if (ResultKind == Ambiguous) {
				Ambiguity = SavedAK;
			} /*else if (Paths) {
				deletePaths( Paths);
				Paths = 0;
			}*/
		}
  }

  template <class DefnClass>
  DefnClass *getAsSingle() const {
    if (getResultKind() != Found) return 0;
    return dyn_cast<DefnClass>(getFoundDefn());
  }

  /// \brief Fetch the unique defn found by this lookup.  Asserts
  /// that one was found.
  ///
  /// This is intended for users who have examined the result kind
  /// and are certain that there is only one result.
  NamedDefn *getFoundDefn() const {
	  assert(getResultKind() == Found
	             && "getFoundDefn called on non-unique result");
	  return (*begin())->getUnderlyingDefn();
  }

  /// Fetches a representative defn.  Useful for lazy diagnostics.
  NamedDefn *getRepresentativeDefn() const {
	  assert(!Defns.empty() && "cannot get representative of empty set");
	  return *begin();
  }

  /// \brief Asks if the result is a single tag defn.
  bool isSingleTagDefn() const {
    return getResultKind() == Found && isa<UserClassDefn>(getFoundDefn());
  }

  /// \brief Make these results show that the name was found in
  /// base classes of different types.
  ///
  /// The given paths object is copied and invalidated.
 // void setAmbiguousBaseSubobjectTypes(CXXBasePaths &P);

  /// \brief Make these results show that the name was found in
  /// distinct base classes of the same type.
  ///
  /// The given paths object is copied and invalidated.
 // void setAmbiguousBaseSubobjects(CXXBasePaths &P);

  /// \brief Make these results show that the name was found in
  /// different contexts and a tag decl was hidden by an ordinary
  /// decl in a different context.
  void setAmbiguousQualifiedTagHiding() {
    setAmbiguous(AmbiguousTagHiding);
  }

  /// \brief Clears out any current state.
  void clear() {
    ResultKind = NotFound;
//    if (Paths) deletePaths(Paths);
//    Paths = NULL;
  }

  /// \brief Clears out any current state and re-initializes for a
  /// different kind of lookup.
  void clear(Sema::LookupNameKind Kind) {
    clear();
    LookupKind = Kind;
    configure();
  }

  /// \brief Change this lookup's redefinition kind.
  void setRedefinitionKind(Sema::RedefinitionKind RK) {
    Redefn = RK;
    configure();
  }

  void print(llvm::raw_ostream &);

  /// Suppress the diagnostics that would normally fire because of this
  /// lookup.  This happens during (e.g.) Redefinition lookups.
  void suppressDiagnostics() {
    Diagnose = false;
  }

  /// Determines whether this lookup is suppressing diagnostics.
  bool isSuppressingDiagnostics() const {
    return !Diagnose;
  }

  /// Sets a 'context' source range.
  void setContextRange(SourceRange SR) {
    NameContextRange = SR;
  }

  /// Gets the source range of the context of this name; for C++
  /// qualified lookups, this is the source range of the scope
  /// specifier.
  SourceRange getContextRange() const {
    return NameContextRange;
  }

  /// Gets the location of the identifier.  This isn't always defined:
  /// sometimes we're doing lookups on synthesized names.
  SourceLocation getNameLoc() const {
    return NameInfo.getLoc();
  }

  /// \brief Get the Sema object that this lookup result is searching
  /// with.
  Sema &getSema() const { return SemaRef; }

  /// A class for iterating through a result set and possibly
  /// filtering out results.  The results returned are possibly
  /// sugared.
  class Filter {
    LookupResult &Results;
    LookupResult::iterator I;
    bool Changed;
    bool CalledDone;

    friend class LookupResult;
    Filter(LookupResult &Results)
      : Results(Results), I(Results.begin()), Changed(false), CalledDone(false)
    {}

  public:
    ~Filter() {
      assert(CalledDone &&
             "LookupResult::Filter destroyed without done() call");
    }

    bool hasNext() const {
      return I != Results.end();
    }

    NamedDefn *next() {
      assert(I != Results.end() && "next() called on empty filter");
      return *I++;
    }

    /// Erase the last element returned from this iterator.
    void erase() {
      Results.Defns.erase(--I);
      Changed = true;
    }

    /// Replaces the current entry with the given one, preserving the
    /// access bits.
    void replace(NamedDefn *D) {
      Results.Defns.replace(I-1, D);
      Changed = true;
    }

    /// Replaces the current entry with the given one.
    void replace(NamedDefn *D, AccessSpecifier AS) {
      Results.Defns.replace(I-1, D, AS);
      Changed = true;
    }

    void done() {
      assert(!CalledDone && "done() called twice");
      CalledDone = true;

      if (Changed)
        Results.resolveKindAfterFilter();
    }
  };
//
//  /// Create a filter for this result set.
//  Filter makeFilter() {
//    return Filter(*this);
//  }

private:
  void diagnose() {
    if (isAmbiguous()) {
//      SemaRef.DiagnoseAmbiguousLookup(*this);
    }
    else if (isClassLookup() && SemaRef.getLangOptions().AccessControl) {
//      SemaRef.CheckLookupAccess(*this);
    	}
  }

  void setAmbiguous(AmbiguityKind AK) {
    ResultKind = Ambiguous;
    Ambiguity = AK;
  }

  //void addDefnsFromBasePaths(const CXXBasePaths &P);
  void configure();

  // Sanity checks.
  void sanity() const;

  // Results.
  LookupResultKind ResultKind;
  AmbiguityKind Ambiguity; // ill-defined unless ambiguous
  UnresolvedSet<8> Defns;
  // CXXBasePaths *Paths;
  UserClassDefn *NamingClass;
  Type BaseObjectType;

  // Parameters.
  Sema &SemaRef;
  DefinitionNameInfo NameInfo;
  SourceRange NameContextRange;
  Sema::LookupNameKind LookupKind;
  unsigned IDNS; // set by configure()

  bool Redefn;

  /// \brief True if tag declarations should be hidden if non-tags
  ///   are present
  bool HideTags;

  bool Diagnose;
};

/// \brief Consumes visible declarations found when searching for
/// all visible names within a given scope or context.
///
/// This abstract class is meant to be subclassed by clients of \c
/// Sema::LookupVisibleDefns(), each of which should override the \c
/// FoundDefn() function to process declarations as they are found.
class VisibleDefnConsumer {
  public:
    /// \brief Destroys the visible definition consumer.
    virtual ~VisibleDefnConsumer();

    /// \brief Invoked each time \p Sema::LookupVisibleDefns() finds a
    /// definition visible from the current scope or context.
    ///
    /// \param ND the definition found.
    ///
    /// \param Hiding a definition that hides the definition \p ND,
    /// or NULL if no such definition exists.
    ///
    /// \param InBaseClass whether this definition was found in base
    /// class of the context we searched.
    virtual void FoundDefn(NamedDefn *ND, NamedDefn *Hiding,
                           bool InBaseClass) = 0;
  };

} // end namespace mlang

#endif /* MLANG_SEMA_LOOKUP_H_ */
