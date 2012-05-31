//===--- DefinitionName.h - Defined Names ----------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines DefinitionName class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_DEFINITION_NAME_H_
#define MLANG_AST_DEFINITION_NAME_H_

#include "mlang/AST/Type.h"
#include "mlang/Basic/OperatorKinds.h"

namespace mlang {
class ClassSpecialName;
class DiagnosticsEngine;

class DefinitionName {
public:
  /// NameKind - The kind of name this object contains.
  enum NameKind {
    NK_Identifier = 0,
    NK_ConstructorName,
    NK_DestructorName,
    NK_OperatorName,
    NK_LiteralOperatorName,
//    NK_ImportDirective,
    NK_Mask = 0x03
  };

private:
  /// Ptr - The lowest two bits are used to express what kind of name
  /// we're actually storing, using the values of NameKind.
	uintptr_t Ptr;

	/// Construct a definition name from a raw pointer.
	DefinitionName(uintptr_t Ptr) : Ptr(Ptr) { }

	friend class DefinitionNameTable;
	friend class NamedDefn;

	/// getFETokenInfoAsVoid - Retrieves the front end-specified pointer
	/// for this name as a void pointer.
	void *getFETokenInfoAsVoid() const;

public:
  /// DefinitionName - Used to create an empty selector.
	DefinitionName() : Ptr(0) { }

  // Construct a definition name from an IdentifierInfo *.
	DefinitionName(const IdentifierInfo *II)
    : Ptr(reinterpret_cast<uintptr_t>(II)) {
    assert((Ptr & NK_Mask) == 0 && "Improperly aligned IdentifierInfo");
  }

	// Construct a definition name from the name of a Class constructor,
	// destructor.
	DefinitionName(ClassSpecialName *Name);

	/// getNameKind -   Determine what kind of name this is.
	NameKind getNameKind() const {
		return static_cast<NameKind>(Ptr & NK_Mask);
	}

	/// getAsClassSpecialName - If the stored pointer is actually a
	/// ClassSpecialName, returns a pointer to it. Otherwise, returns
	/// a NULL pointer.
	ClassSpecialName *getAsClassSpecialName() const {
		if (getNameKind() == NK_ConstructorName ||
				getNameKind() == NK_DestructorName)
			return reinterpret_cast<ClassSpecialName *>(Ptr & ~NK_Mask);
		return 0;
	}

  // operator bool() - Evaluates true when this definition name is
  // non-empty.
  operator bool() const {
    return ((Ptr & NK_Mask) != 0) ||
           (reinterpret_cast<IdentifierInfo *>(Ptr & ~NK_Mask));
  }

  /// Predicate functions for querying what type of name this is.
  bool isIdentifier() const { return getNameKind() == NK_Identifier; }

  /// getNameAsString - Retrieve the human-readable string for this name.
  std::string getAsString() const;

  /// printName - Print the human-readable name to a stream.
  void printName(llvm::raw_ostream &OS) const;

  /// getAsIdentifierInfo - Retrieve the IdentifierInfo * stored in
  /// this definition name, or NULL if this definition name isn't a
  /// simple identifier.
  IdentifierInfo *getAsIdentifierInfo() const {
    if (isIdentifier())
      return reinterpret_cast<IdentifierInfo *>(Ptr);
    return NULL;
  }

  /// getAsOpaqueInteger - Get the representation of this definition
  /// name as an opaque integer.
  uintptr_t getAsOpaqueInteger() const { return Ptr; }

  /// getAsOpaquePtr - Get the representation of this definition name as
  /// an opaque pointer.
  void *getAsOpaquePtr() const { return reinterpret_cast<void*>(Ptr); }

  static DefinitionName getFromOpaquePtr(void *P) {
	  DefinitionName N;
    N.Ptr = reinterpret_cast<uintptr_t> (P);
    return N;
  }

  static DefinitionName getFromOpaqueInteger(uintptr_t P) {
	  DefinitionName N;
    N.Ptr = P;
    return N;
  }

  /// getClassNameType - If this name is one of the class names (of a
  /// constructor, destructor, or conversion function), return the
  /// type associated with that name.
  Type getClassNameType() const;

  /// getClassOverloadedOperator - If this name is the name of an
  /// overloadable operator in OOP (e.g., @c operator+), retrieve the
  /// kind of overloaded operator.
  OverloadedOperatorKind getClassOverloadedOperator() const;

  /// getClassLiteralIdentifier - If this name is the name of a literal
  /// operator, retrieve the identifier associated with it.
  IdentifierInfo *getClassLiteralIdentifier() const;

  /// getFETokenInfo/setFETokenInfo - The language front-end is
  /// allowed to associate arbitrary metadata with some kinds of
  /// definition names, including normal identifiers and C++
  /// constructors, destructors, and conversion functions.
  template<typename T>
  T *getFETokenInfo() const { return static_cast<T*>(getFETokenInfoAsVoid()); }

  void setFETokenInfo(void *T);

  /// operator== - Determine whether the specified names are identical..
  friend bool operator==(DefinitionName LHS, DefinitionName RHS) {
    return LHS.Ptr == RHS.Ptr;
  }

  /// operator!= - Determine whether the specified names are different.
  friend bool operator!=(DefinitionName LHS, DefinitionName RHS) {
    return LHS.Ptr != RHS.Ptr;
  }

  static DefinitionName getEmptyMarker() {
    return DefinitionName(uintptr_t(-1));
  }

  static DefinitionName getTombstoneMarker() {
    return DefinitionName(uintptr_t(-2));
  }

  static int compare(DefinitionName LHS, DefinitionName RHS);

  void dump() const;
};

/// Ordering on two definition names. If both names are identifiers,
/// this provides a lexicographical ordering.
inline bool operator<(DefinitionName LHS, DefinitionName RHS) {
  return DefinitionName::compare(LHS, RHS) < 0;
}

/// Ordering on two definition names. If both names are identifiers,
/// this provides a lexicographical ordering.
inline bool operator>(DefinitionName LHS, DefinitionName RHS) {
  return DefinitionName::compare(LHS, RHS) > 0;
}

/// Ordering on two definition names. If both names are identifiers,
/// this provides a lexicographical ordering.
inline bool operator<=(DefinitionName LHS, DefinitionName RHS) {
  return DefinitionName::compare(LHS, RHS) <= 0;
}

/// Ordering on two definition names. If both names are identifiers,
/// this provides a lexicographical ordering.
inline bool operator>=(DefinitionName LHS, DefinitionName RHS) {
  return DefinitionName::compare(LHS, RHS) >= 0;
}

/// DefinitionNameTable - Used to store and retrieve DefinitionName
/// instances for the various kinds of definition names, e.g., normal
/// identifiers, class constructor names, etc. This class contains
/// uniqued versions of each of the OOP special names, which can be
/// retrieved using its member functions (e.g.,
/// getClassConstructorName).
class DefinitionNameTable {
  ASTContext &Ctx;
  void *ClassSpecialNamesImpl; // Actually a FoldingSet<ClassSpecialName> *

  DefinitionNameTable(const DefinitionNameTable&);            // NONCOPYABLE
  DefinitionNameTable& operator=(const DefinitionNameTable&); // NONCOPYABLE

public:
  DefinitionNameTable(ASTContext &C);
  ~DefinitionNameTable();

  /// getIdentifier - Create a definition name that is a simple
  /// identifier.
  DefinitionName getIdentifier(const IdentifierInfo *ID) {
    return DefinitionName(ID);
  }

  /// getClassConstructorName - Returns the name of a constructor
  /// for the given Type.
  DefinitionName getClassConstructorName(Type Ty) {
    return getClassSpecialName(DefinitionName::NK_ConstructorName, Ty);
  }

  /// getClassDestructorName - Returns the name of a destructor
  /// for the given Type.
  DefinitionName getClassDestructorName(Type Ty) {
    return getClassSpecialName(DefinitionName::NK_DestructorName,  Ty);
  }

  /// getOperatorName - Returns the name of a destructor
	/// for the given Type.
	DefinitionName getOperatorName(OverloadedOperatorKind Op);

	/// getLiteralOperatorName - Returns the name of a OOP
	/// for the given Type.
	DefinitionName getLiteralOperatorName(IdentifierInfo *II);

  /// getSpecialName - Returns a definition name for special kind
  /// of name, e.g., for a constructor, destructor.
  DefinitionName getClassSpecialName(DefinitionName::NameKind Kind,
		  Type Ty);
};

/// DefinitionNameInfo - A collector data type for bundling together
/// a DefinitionName and the correspnding source location info.
struct DefinitionNameInfo {
  /// Name - The definition name, also encoding name kind.
  DefinitionName Name;
  /// Loc - The main source location for the definition name.
  SourceLocation NameLoc;

public:
  // FIXME: remove it.
  DefinitionNameInfo() {}

  DefinitionNameInfo(DefinitionName Name, SourceLocation NameLoc)
    : Name(Name), NameLoc(NameLoc) {}

  /// getName - Returns the embedded definition name.
  DefinitionName getName() const { return Name; }
  /// setName - Sets the embedded definition name.
  void setName(DefinitionName N) { Name = N; }

  /// getLoc - Returns the main location of the definition name.
  SourceLocation getLoc() const { return NameLoc; }
  /// setLoc - Sets the main location of the definition name.
  void setLoc(SourceLocation L) { NameLoc = L; }

  /// getAsString - Retrieve the human-readable string for this name.
  std::string getAsString() const;

  /// printName - Print the human-readable name to a stream.
  void printName(llvm::raw_ostream &OS) const;

  /// getBeginLoc - Retrieve the location of the first token.
  SourceLocation getBeginLoc() const { return NameLoc; }
  /// getEndLoc - Retrieve the location of the last end token.
  SourceLocation getEndLoc() const;
  /// getSourceRange - The range of the definition name.
  SourceRange getSourceRange() const {
    return SourceRange(getBeginLoc(), getEndLoc());
  }
};

/// ClassSpecialName - Records the type associated with one of the
/// "special" kinds of definition names, e.g., constructors,
/// destructors.
// FIXME yabin
// the original base of ClassSpecialName is DefinitionNameExtra,
// which is defined IdentifierTable.h
class ClassSpecialName : public llvm::FoldingSetNode {
  Type Ty;
  DefinitionName::NameKind Kind;
  void *FETokenInfo;

public:
  void *getFETokenInfo() const {
    return FETokenInfo;
  }

  DefinitionName::NameKind getKind() const {
    return Kind;
  }

  Type getType() const {
	  return Ty;
  }

  void setFETokenInfo(void *FETokenInfo) {
	  this->FETokenInfo = FETokenInfo;
  }

  void setKind(DefinitionName::NameKind Kind) {
	  this->Kind = Kind;
  }

  void setType(Type Ty) {
	  this->Ty = Ty;
  }

  void Profile(llvm::FoldingSetNodeID &ID) {
	  ID.AddInteger((unsigned)Kind);
	  ID.AddPointer(Ty.getAsOpaquePtr());
  }
};


/// Insertion operator for diagnostics.  This allows sending DefinitionName's
/// into a diagnostic with <<.
inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           DefinitionName N) {
  DB.AddTaggedVal(N.getAsOpaqueInteger(),
                  DiagnosticsEngine::ak_declarationname);
  return DB;
}

/// Insertion operator for partial diagnostics.  This allows binding
/// DefinitionName's into a partial diagnostic with <<.
inline const PartialDiagnostic &operator<<(const PartialDiagnostic &PD,
                                           DefinitionName N) {
  PD.AddTaggedVal(N.getAsOpaqueInteger(),
                  DiagnosticsEngine::ak_declarationname);
  return PD;
}

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                     DefinitionNameInfo DNInfo) {
  DNInfo.printName(OS);
  return OS;
}

} // end namespace mlang

namespace llvm {
/// Define DenseMapInfo so that DefinitionNames can be used as keys
/// in DenseMap and DenseSets.
template<>
struct DenseMapInfo<mlang::DefinitionName> {
  static inline mlang::DefinitionName getEmptyKey() {
    return mlang::DefinitionName::getEmptyMarker();
  }

  static inline mlang::DefinitionName getTombstoneKey() {
    return mlang::DefinitionName::getTombstoneMarker();
  }

  static unsigned getHashValue(mlang::DefinitionName);

  static inline bool
  isEqual(mlang::DefinitionName LHS, mlang::DefinitionName RHS) {
    return LHS == RHS;
  }
};

template <>
struct isPodLike<mlang::DefinitionName> { static const bool value = true; };

}  // end namespace llvm

#endif /* MLANG_AST_DEFINITION_NAME_H_ */
