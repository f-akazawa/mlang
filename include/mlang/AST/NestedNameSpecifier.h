//===--- NestedNameSpecifier.h - C++ nested name specifiers -----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the NestedNameSpecifier class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_NESTED_NAMESPECIFIER_H_
#define MLANG_AST_NESTED_NAMESPECIFIER_H_

#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/PointerIntPair.h"

namespace llvm {
  class raw_ostream;
}

namespace mlang {
class ASTContext;
class NamespaceDefn;
class IdentifierInfo;
struct PrintingPolicy;
class RawType;
class LangOptions;

/// \brief Represents a C++ nested name specifier, such as
/// "::std::vector<int>::".
///
/// C++ nested name specifiers are the prefixes to qualified
/// namespaces. For example, "foo::" in "foo::x" is a nested name
/// specifier. Nested name specifiers are made up of a sequence of
/// specifiers, each of which can be a namespace, type, identifier
/// (for dependent names), or the global specifier ('::', must be the
/// first specifier).
class NestedNameSpecifier : public llvm::FoldingSetNode {
  /// \brief The nested name specifier that precedes this nested name
  /// specifier.
  ///
  /// The pointer is the nested-name-specifier that precedes this
  /// one. The integer stores one of the first four values of type
  /// SpecifierKind.
  llvm::PointerIntPair<NestedNameSpecifier *, 2> Prefix;

  /// \brief The last component in the nested name specifier, which
  /// can be an identifier, a declaration, or a type.
  ///
  /// When the pointer is NULL, this specifier represents the global
  /// specifier '::'. Otherwise, the pointer is one of
  /// IdentifierInfo*, Namespace*, or Type*, depending on the kind of
  /// specifier as encoded within the prefix.
  void* Specifier;

public:
  /// \brief The kind of specifier that completes this nested name
  /// specifier.
  enum SpecifierKind {
    /// \brief An identifier, stored as an IdentifierInfo*.
    Identifier = 0,
    /// \brief A namespace, stored as a Namespace*.
    Namespace = 1,
    /// \brief A type, stored as a Type*.
    TypeSpec = 2,
    /// \brief The global specifier '::'. There is no stored value.
    Global = 3
  };

private:
  /// \brief Builds the global specifier.
  NestedNameSpecifier() : Prefix(0, 0), Specifier(0) { }

  /// \brief Copy constructor used internally to clone nested name
  /// specifiers.
  NestedNameSpecifier(const NestedNameSpecifier &Other)
    : llvm::FoldingSetNode(Other), Prefix(Other.Prefix),
      Specifier(Other.Specifier) {
  }

  NestedNameSpecifier &operator=(const NestedNameSpecifier &); // do not implement

  /// \brief Either find or insert the given nested name specifier
  /// mockup in the given context.
  static NestedNameSpecifier *FindOrInsert(ASTContext &Context,
                                           const NestedNameSpecifier &Mockup);

public:
  /// \brief Builds a specifier combining a prefix and an identifier.
  ///
  /// The prefix must be dependent, since nested name specifiers
  /// referencing an identifier are only permitted when the identifier
  /// cannot be resolved.
  static NestedNameSpecifier *Create(ASTContext &Context,
                                     NestedNameSpecifier *Prefix,
                                     IdentifierInfo *II);

  /// \brief Builds a nested name specifier that names a namespace.
  static NestedNameSpecifier *Create(ASTContext &Context,
                                     NestedNameSpecifier *Prefix,
                                     NamespaceDefn *NS);

  /// \brief Builds a nested name specifier that names a type.
  /// This is user-defined class.
  static NestedNameSpecifier *Create(ASTContext &Context,
                                     NestedNameSpecifier *Prefix,
                                     RawType *T);

  /// \brief Builds a specifier that consists of just an identifier.
  ///
  /// The nested-name-specifier is assumed to be dependent, but has no
  /// prefix because the prefix is implied by something outside of the
  /// nested name specifier, e.g., in "x->Base::f", the "x" has a dependent
  /// type.
  static NestedNameSpecifier *Create(ASTContext &Context, IdentifierInfo *II);

  /// \brief Return the prefix of this nested name specifier.
  ///
  /// The prefix contains all of the parts of the nested name
  /// specifier that preced this current specifier. For example, for a
  /// nested name specifier that represents "foo::bar::", the current
  /// specifier will contain "bar::" and the prefix will contain
  /// "foo::".
  NestedNameSpecifier *getPrefix() const { return Prefix.getPointer(); }

  /// \brief Determine what kind of nested name specifier is stored.
  SpecifierKind getKind() const {
    if (Specifier == 0)
      return Global;
    return (SpecifierKind)Prefix.getInt();
  }

  /// \brief Retrieve the identifier stored in this nested name
  /// specifier.
  IdentifierInfo *getAsIdentifier() const {
    if (Prefix.getInt() == Identifier)
      return (IdentifierInfo *)Specifier;

    return 0;
  }

  /// \brief Retrieve the namespace stored in this nested name
  /// specifier.
  NamespaceDefn *getAsNamespace() const {
    if (Prefix.getInt() == Namespace)
      return (NamespaceDefn *)Specifier;

    return 0;
  }

  /// \brief Retrieve the type stored in this nested name specifier.
  RawType *getAsType() const {
    if (Prefix.getInt() == TypeSpec)
      return (RawType *)Specifier;

    return 0;
  }

  /// \brief Print this nested name specifier to the given output
  /// stream.
  void print(llvm::raw_ostream &OS, const PrintingPolicy &Policy) const;

  void Profile(llvm::FoldingSetNodeID &ID) const {
    ID.AddPointer(Prefix.getOpaqueValue());
    ID.AddPointer(Specifier);
  }

  /// \brief Dump the nested name specifier to standard output to aid
  /// in debugging.
  void dump(const LangOptions &LO);
};

/// Insertion operator for diagnostics. This allows sending NestedNameSpecifiers
/// into a diagnostic with <<.
inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           NestedNameSpecifier *NNS) {
  DB.AddTaggedVal(reinterpret_cast<intptr_t>(NNS),
                  DiagnosticsEngine::ak_nestednamespec);
  return DB;
}

} // end namespace mlang

#endif /* MLANG_AST_NESTED_NAMESPECIFIER_H_ */
