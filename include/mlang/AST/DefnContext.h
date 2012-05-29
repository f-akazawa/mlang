//===--- DefnContext.h - Context for Defns who have sub Defns ---*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines DefnContext.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_DEFINITION_CONTEXT_H_
#define MLANG_AST_DEFINITION_CONTEXT_H_

#include "mlang/AST/Defn.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/type_traits.h"

using llvm::isa;
using llvm::cast;

namespace mlang {
class DefinitionName;
class DefnContext;
class FunctionDefn;
class NamedDefn;
class NamespaceDefn;
class ScriptDefn;
class StoredDefnsMap;
class TypeDefn;
class UserClassDefn;
} // end namespace mlang

namespace llvm {
// DefnContext* is only 4-byte aligned on 32-bit systems.
template<>
class PointerLikeTypeTraits<mlang::DefnContext*> {
	typedef mlang::DefnContext* PT;
public:
	static inline void *getAsVoidPointer(PT P) {
		return P;
	}
	static inline PT getFromVoidPointer(void *P) {
		return static_cast<PT> (P);
	}
	enum {
		NumLowBitsAvailable = 2
	};
};
} // end namespace llvm

namespace mlang {

class DefnContextLookupResult
  : public std::pair<NamedDefn**,NamedDefn**> {
public:
  DefnContextLookupResult(NamedDefn **I, NamedDefn **E)
    : std::pair<NamedDefn**,NamedDefn**>(I, E) {}
  DefnContextLookupResult()
    : std::pair<NamedDefn**,NamedDefn**>() {}

  using std::pair<NamedDefn**,NamedDefn**>::operator=;
};

class DefnContextLookupConstResult
  : public std::pair<NamedDefn*const*, NamedDefn*const*> {
public:
  DefnContextLookupConstResult(std::pair<NamedDefn**,NamedDefn**> R)
    : std::pair<NamedDefn*const*, NamedDefn*const*>(R) {}
  DefnContextLookupConstResult(NamedDefn * const *I, NamedDefn * const *E)
    : std::pair<NamedDefn*const*, NamedDefn*const*>(I, E) {}
  DefnContextLookupConstResult()
    : std::pair<NamedDefn*const*, NamedDefn*const*>() {}

  using std::pair<NamedDefn*const*,NamedDefn*const*>::operator=;
};

/// DefnContext - This is used only as base class of specific defn types that
/// can act as definition contexts. These defns are (only the top classes
/// that directly derive from DefnContext are mentioned, not their subclasses):
///
///   TranslationUnitDefn
///   NamespaceDefn
///   FunctionDefn
///   UserClassDefn
///   ScriptDefn
///
class DefnContext {
  /// DefnKind - This indicates which class this is.
  unsigned DefnKind : 8;

  /// \brief Whether this definition context also has some external
  /// storage that contains additional definitions that are lexically
  /// part of this context.
  mutable unsigned ExternalLexicalStorage : 1;

  /// \brief Whether this definition context also has some external
  /// storage that contains additional definitions that are visible
  /// in this context.
  mutable unsigned ExternalVisibleStorage : 1;

  /// \brief Pointer to the data structure used to lookup definitions
  /// within this context.
  mutable StoredDefnsMap *LookupPtr;

protected:
  /// FirstDefn - The first definition stored within this definition
  /// context.
  mutable Defn *FirstDefn;

  /// LastDefn - The last definition stored within this definition
  /// context. FIXME: We could probably cache this value somewhere
  /// outside of the DefnContext, to reduce the size of DefnContext by
  /// another pointer.
  mutable Defn *LastDefn;

  friend class ExternalASTSource;

  /// \brief Build up a chain of definitions.
  ///
  /// \returns the first/last pair of definitions.
  static std::pair<Defn *, Defn *>
  BuildDefnChain(const llvm::SmallVectorImpl<Defn*> &Defns);

  DefnContext(Defn::Kind K)
     : DefnKind(K), ExternalLexicalStorage(false),
       ExternalVisibleStorage(false), LookupPtr(0), FirstDefn(0),
       LastDefn(0) { }

public:
  ~DefnContext();

  Defn::Kind getDefnKind() const {
    return static_cast<Defn::Kind>(DefnKind);
  }
  const char *getDefnKindName() const;

  /// getParent - Returns the containing DefnContext.
  DefnContext *getParent() {
    return cast<Defn>(this)->getDefnContext();
  }
  const DefnContext *getParent() const {
    return const_cast<DefnContext*>(this)->getParent();
  }

  ASTContext &getParentASTContext() const {
    return cast<Defn>(this)->getASTContext();
  }

  bool isFunctionOrMethod() const {
	  return DefnKind >= Defn::firstFunction && DefnKind <= Defn::lastFunction;
  }

  //FIXME is this method needed?
  bool isFileContext() const {
	  //FIXME Should UserClassDefn and FunctionDefn list here?
    return DefnKind == Defn::TranslationUnit ||
    		   DefnKind == Defn::UserClass ||
    		   DefnKind == Defn::Function;
  }

  bool isTranslationUnit() const {
    return DefnKind == Defn::TranslationUnit;
  }

  bool isRecord() const {
    return DefnKind == Defn::UserClass ||
    		   DefnKind == Defn::Struct ||
    		   DefnKind == Defn::Cell;
  }

  bool isNamespace() const {
    return false;
  }

  /// \brief Determine whether this definition context encloses the
  /// definition context DC.
  bool Encloses(const DefnContext *DC) const;

  /// defn_iterator - Iterates through the definitions stored
  /// within this context.
  class defn_iterator {
    /// Current - The current definition.
    Defn *Current;

  public:
    typedef Defn*                     value_type;
    typedef Defn*                     reference;
    typedef Defn*                     pointer;
    typedef std::forward_iterator_tag iterator_category;
    typedef std::ptrdiff_t            difference_type;

    defn_iterator() : Current(0) { }
    explicit defn_iterator(Defn *C) : Current(C) { }

    reference operator*() const { return Current; }
    pointer operator->() const { return Current; }

    defn_iterator& operator++() {
      Current = Current->getNextDefnInContext();
      return *this;
    }

    defn_iterator operator++(int) {
      defn_iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    friend bool operator==(defn_iterator x, defn_iterator y) {
      return x.Current == y.Current;
    }
    friend bool operator!=(defn_iterator x, defn_iterator y) {
      return x.Current != y.Current;
    }
  };

  /// defns_begin/defns_end - Iterate over the definitions stored in
  /// this context.
  defn_iterator defns_begin() const;
  defn_iterator defns_end() const;
  bool defns_empty() const;

  /// noload_defns_begin/end - Iterate over the definitions stored in this
  /// context that are currently loaded; don't attempt to retrieve anything
  /// from an external source.
  defn_iterator noload_defns_begin() const;
  defn_iterator noload_defns_end() const;

  /// specific_defn_iterator - Iterates over a subrange of
  /// definitions stored in a DefnContext, providing only those that
  /// are of type SpecificDefn (or a class derived from it). This
  /// iterator is used, for example, to provide iteration over just
  /// the fields within a RecordDefn (with SpecificDefn = FieldDefn).
  template<typename SpecificDefn>
  class specific_defn_iterator {
    /// Current - The current, underlying definition iterator, which
    /// will either be NULL or will point to a definition of
    /// type SpecificDefn.
    DefnContext::defn_iterator Current;

    /// SkipToNextDefn - Advances the current position up to the next
    /// definition of type SpecificDefn that also meets the criteria
    /// required by Acceptable.
    void SkipToNextDefn() {
      while (*Current && !isa<SpecificDefn>(*Current))
        ++Current;
    }

  public:
    typedef SpecificDefn* value_type;
    typedef SpecificDefn* reference;
    typedef SpecificDefn* pointer;
    typedef std::iterator_traits<DefnContext::defn_iterator>::difference_type
      difference_type;
    typedef std::forward_iterator_tag iterator_category;

    specific_defn_iterator() : Current() { }

    /// specific_defn_iterator - Construct a new iterator over a
    /// subset of the definitions the range [C,
    /// end-of-definitions). If A is non-NULL, it is a pointer to a
    /// member function of SpecificDefn that should return true for
    /// all of the SpecificDefn instances that will be in the subset
    /// of iterators. For example, if you want Objective-C instance
    /// methods, SpecificDefn will be ObjCMethodDefn and A will be
    /// &ObjCMethodDefn::isInstanceMethod.
    explicit specific_defn_iterator(DefnContext::defn_iterator C) : Current(C) {
      SkipToNextDefn();
    }

    reference operator*() const { return cast<SpecificDefn>(*Current); }
    pointer operator->() const { return cast<SpecificDefn>(*Current); }

    specific_defn_iterator& operator++() {
      ++Current;
      SkipToNextDefn();
      return *this;
    }

    specific_defn_iterator operator++(int) {
      specific_defn_iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    friend bool
    operator==(const specific_defn_iterator& x, const specific_defn_iterator& y) {
      return x.Current == y.Current;
    }

    friend bool
    operator!=(const specific_defn_iterator& x, const specific_defn_iterator& y) {
      return x.Current != y.Current;
    }
  };

  /// \brief Iterates over a filtered subrange of definitions stored
  /// in a DefnContext.
  ///
  /// This iterator visits only those definitions that are of type
  /// SpecificDefn (or a class derived from it) and that meet some
  /// additional run-time criteria. This iterator is used, for
  /// example, to provide access to the instance methods within an
  /// Objective-C interface (with SpecificDefn = ObjCMethodDefn and
  /// Acceptable = ObjCMethodDefn::isInstanceMethod).
  template<typename SpecificDefn, bool (SpecificDefn::*Acceptable)() const>
  class filtered_defn_iterator {
    /// Current - The current, underlying definition iterator, which
    /// will either be NULL or will point to a definition of
    /// type SpecificDefn.
    DefnContext::defn_iterator Current;

    /// SkipToNextDefn - Advances the current position up to the next
    /// definition of type SpecificDefn that also meets the criteria
    /// required by Acceptable.
    void SkipToNextDefn() {
      while (*Current &&
             (!isa<SpecificDefn>(*Current) ||
              (Acceptable && !(cast<SpecificDefn>(*Current)->*Acceptable)())))
        ++Current;
    }

  public:
    typedef SpecificDefn* value_type;
    typedef SpecificDefn* reference;
    typedef SpecificDefn* pointer;
    typedef std::iterator_traits<DefnContext::defn_iterator>::difference_type
      difference_type;
    typedef std::forward_iterator_tag iterator_category;

    filtered_defn_iterator() : Current() { }

    /// specific_defn_iterator - Construct a new iterator over a
    /// subset of the definitions the range [C,
    /// end-of-definitions). If A is non-NULL, it is a pointer to a
    /// member function of SpecificDefn that should return true for
    /// all of the SpecificDefn instances that will be in the subset
    /// of iterators. For example, if you want Objective-C instance
    /// methods, SpecificDefn will be ObjCMethodDefn and A will be
    /// &ObjCMethodDefn::isInstanceMethod.
    explicit filtered_defn_iterator(DefnContext::defn_iterator C) : Current(C) {
      SkipToNextDefn();
    }

    reference operator*() const { return cast<SpecificDefn>(*Current); }
    pointer operator->() const { return cast<SpecificDefn>(*Current); }

    filtered_defn_iterator& operator++() {
      ++Current;
      SkipToNextDefn();
      return *this;
    }

    filtered_defn_iterator operator++(int) {
      filtered_defn_iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    friend bool
    operator==(const filtered_defn_iterator& x, const filtered_defn_iterator& y) {
      return x.Current == y.Current;
    }

    friend bool
    operator!=(const filtered_defn_iterator& x, const filtered_defn_iterator& y) {
      return x.Current != y.Current;
    }
  };

  /// @brief Add the definition D into this context.
  ///
  /// This routine should be invoked when the definition D has first
  /// been defined, to place D into the context where it was
  /// (lexically) defined. Every definition must be added to one
  /// (and only one!) context, where it can be visited via
  /// [defns_begin(), defns_end()). Once a definition has been added
  /// to its lexical context, the corresponding DefnContext owns the
  /// definition.
  ///
  /// If D is also a NamedDefn, it will be made visible within its
  /// semantic context via makeDefnVisibleInContext.
  void addDefn(Defn *D);

  /// @brief Add the definition D to this context without modifying
  /// any lookup tables.
  ///
  /// This is useful for some operations in dependent contexts where
  /// the semantic context might not be dependent;  this basically
  /// only happens with friends.
  void addHiddenDefn(Defn *D);

  /// @brief Removes a definition from this context.
  void removeDefn(Defn *D);

  /// lookup_iterator - An iterator that provides access to the results
  /// of looking up a name within this context.
  typedef NamedDefn **lookup_iterator;

  /// lookup_const_iterator - An iterator that provides non-mutable
  /// access to the results of lookup up a name within this context.
  typedef NamedDefn * const * lookup_const_iterator;

  typedef DefnContextLookupResult lookup_result;
  typedef DefnContextLookupConstResult lookup_const_result;

  /// lookup - Find the definitions (if any) with the given Name in
  /// this context. Returns a range of iterators that contains all of
  /// the definitions with this name, with object, function, member,
  /// and enumerator names preceding any tag name. Note that this
  /// routine will not look into parent contexts.
  lookup_result lookup(DefinitionName Name);
  lookup_const_result lookup(DefinitionName Name) const;

  /// @brief Makes a definition visible within this context.
  ///
  /// This routine makes the definition D visible to name lookup
  /// within this context and, if this is a transparent context,
  /// within its parent contexts up to the first enclosing
  /// non-transparent context. Making a definition visible within a
  /// context does not transfer ownership of a definition, and a
  /// definition can be visible in many contexts that aren't its
  /// lexical context.
  ///
  /// If D is a redefinition of an existing definition that is
  /// visible from this context, as determined by
  /// NamedDefn::definitionReplaces, the previous definition will be
  /// replaced with D.
  ///
  /// @param Recoverable true if it's okay to not add this defn to
  /// the lookup tables because it can be easily recovered by walking
  /// the definition chains.
  void makeDefnVisibleInContext(NamedDefn *D, bool Recoverable = true);

  /// \brief Deserialize all the visible definitions from external storage.
  ///
  /// Name lookup deserializes visible definitions lazily, thus a DefnContext
  /// may not have a complete name lookup table. This function deserializes
  /// the rest of visible definitions from the external storage and completes
  /// the name lookup table.
  void MaterializeVisibleDefnsFromExternalStorage();

  // Low-level accessors

  /// \brief Retrieve the internal representation of the lookup structure.
  StoredDefnsMap* getLookupPtr() const { return LookupPtr; }

  /// \brief Whether this DefnContext has external storage containing
  /// additional definitions that are lexically in this context.
  bool hasExternalLexicalStorage() const { return ExternalLexicalStorage; }

  /// \brief State whether this DefnContext has external storage for
  /// definitions lexically in this context.
  void setHasExternalLexicalStorage(bool ES = true) {
    ExternalLexicalStorage = ES;
  }

  /// \brief Whether this DefnContext has external storage containing
  /// additional definitions that are visible in this context.
  bool hasExternalVisibleStorage() const { return ExternalVisibleStorage; }

  /// \brief State whether this DefnContext has external storage for
  /// definitions visible in this context.
  void setHasExternalVisibleStorage(bool ES = true) {
    ExternalVisibleStorage = ES;
  }

  static bool classof(const Defn *D);
  static bool classof(const DefnContext *D) { return true; }
#define DEFN(NAME, BASE)
#define DEFN_CONTEXT(NAME) \
  static bool classof(const NAME##Defn *D) { return true; }
#include "mlang/AST/DefnNodes.inc"

  void dumpDefnContext() const;

private:
  void LoadDefnsFromExternalStorage() const;

  StoredDefnsMap *CreateStoredDefnsMap(ASTContext &C) const;

  void makeDefnVisibleInContextImpl(NamedDefn *D);
};

// Specialization selected when ToTy is not a known subclass of DefnContext.
template <class ToTy,
          bool IsKnownSubtype = ::llvm::is_base_of< DefnContext, ToTy>::value>
struct cast_convert_defn_context {
  static const ToTy *doit(const DefnContext *Val) {
    return static_cast<const ToTy*>(Defn::castFromDefnContext(Val));
  }

  static ToTy *doit(DefnContext *Val) {
    return static_cast<ToTy*>(Defn::castFromDefnContext(Val));
  }
};

// Specialization selected when ToTy is a known subclass of DefnContext.
template <class ToTy>
struct cast_convert_defn_context<ToTy, true> {
  static const ToTy *doit(const DefnContext *Val) {
    return static_cast<const ToTy*>(Val);
  }

  static ToTy *doit(DefnContext *Val) {
    return static_cast<ToTy*>(Val);
  }
};

} // end namespace mlang

namespace llvm {

/// isa<T>(DefnContext*)
template <typename To>
struct isa_impl<To, ::mlang::DefnContext> {
  static bool doit(const ::mlang::DefnContext &Val) {
    return To::classofKind(Val.getDefnKind());
  }
};

/// cast<T>(DefnContext*)
template<class ToTy>
struct cast_convert_val<ToTy,
                        const ::mlang::DefnContext,const ::mlang::DefnContext> {
  static const ToTy &doit(const ::mlang::DefnContext &Val) {
    return *::mlang::cast_convert_defn_context<ToTy>::doit(&Val);
  }
};
template<class ToTy>
struct cast_convert_val<ToTy, ::mlang::DefnContext, ::mlang::DefnContext> {
  static ToTy &doit(::mlang::DefnContext &Val) {
    return *::mlang::cast_convert_defn_context<ToTy>::doit(&Val);
  }
};
template<class ToTy>
struct cast_convert_val<ToTy,
                     const ::mlang::DefnContext*, const ::mlang::DefnContext*> {
  static const ToTy *doit(const ::mlang::DefnContext *Val) {
    return ::mlang::cast_convert_defn_context<ToTy>::doit(Val);
  }
};
template<class ToTy>
struct cast_convert_val<ToTy, ::mlang::DefnContext*, ::mlang::DefnContext*> {
  static ToTy *doit(::mlang::DefnContext *Val) {
    return ::mlang::cast_convert_defn_context<ToTy>::doit(Val);
  }
};

/// Implement cast_convert_val for Defn -> DefnContext conversions.
template<class FromTy>
struct cast_convert_val< ::mlang::DefnContext, FromTy, FromTy> {
  static ::mlang::DefnContext &doit(const FromTy &Val) {
    return *FromTy::castToDefnContext(&Val);
  }
};

template<class FromTy>
struct cast_convert_val< ::mlang::DefnContext, FromTy*, FromTy*> {
  static ::mlang::DefnContext *doit(const FromTy *Val) {
    return FromTy::castToDefnContext(Val);
  }
};

template<class FromTy>
struct cast_convert_val< const ::mlang::DefnContext, FromTy, FromTy> {
  static const ::mlang::DefnContext &doit(const FromTy &Val) {
    return *FromTy::castToDefnContext(&Val);
  }
};

template<class FromTy>
struct cast_convert_val< const ::mlang::DefnContext, FromTy*, FromTy*> {
  static const ::mlang::DefnContext *doit(const FromTy *Val) {
    return FromTy::castToDefnContext(Val);
  }
};

} // end namespace llvm

#endif /* MLANG_AST_DEFINITION_CONTEXT_H_ */
