//===--- DefnAll.h - Various Defn sub-nodes Definition ----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines Defn subclasses except OOP related.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_DEFN_TRANSLATION_UNIT_H_
#define MLANG_AST_DEFN_TRANSLATION_UNIT_H_

#include "mlang/AST/APValue.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnContext.h"
#include "mlang/AST/DefinitionName.h"
#include "mlang/AST/ExternalASTSource.h"
#include "mlang/Basic/Linkage.h"
#include "mlang/Basic/SourceLocation.h"
#include "mlang/Basic/Specifiers.h"
#include "mlang/Basic/Visibility.h"
#include <cassert>

namespace mlang {
class APValue;
class BlockCmd;
class Expr;
class Cmd;
class MemberDefn;
class UserClassDefn;

/// TranslationUnitDefn - The top definition context.
class TranslationUnitDefn : public Defn, public DefnContext{
	ASTContext &Ctx;

	Defn *External;

	explicit TranslationUnitDefn(ASTContext &ctx) :
		Defn(TranslationUnit, 0, SourceLocation()),
		DefnContext(TranslationUnit),
		Ctx(ctx), External(0) {
	}

public:
  ASTContext &getASTContext() const { return Ctx; }

  Defn *getExternalDefinition() const { return External; }
  void setExternalDefinition(Defn *D) { External = D; }

  static TranslationUnitDefn *Create(ASTContext &C);

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Defn *D) { return classofKind(D->getKind()); }
  static bool classof(const TranslationUnitDefn *D) { return true; }
  static bool classofKind(Kind K) { return K == TranslationUnit; }
  static DefnContext *castToDefnContext(const TranslationUnitDefn *D) {
    return static_cast<DefnContext *>(const_cast<TranslationUnitDefn*>(D));
  }
  static TranslationUnitDefn *castFromDefnContext(const DefnContext *DC) {
    return static_cast<TranslationUnitDefn *>(const_cast<DefnContext*>(DC));
  }
};

class NamedDefn : public Defn {
	/// Name - The name of this definition, which is typically a normal
	/// identifier but may also be a special kind of name ( constructor,
	/// etc.)
	DefinitionName Name;

protected:
  NamedDefn(Kind DK, DefnContext *DC, SourceLocation L, DefinitionName N)
    : Defn(DK, DC, L), Name(N) { }

public:
  /// getIdentifier - Get the identifier that names this definition,
  /// if there is one. This will return NULL if this definition has
  /// no name (e.g., for an unnamed class) or if the name is a special
  /// name (class constructor, destructor, etc.).
  IdentifierInfo *getIdentifier() const { return Name.getAsIdentifierInfo(); }

  /// getName - Get the name of identifier for this definition as a StringRef.
  /// This requires that the definition have a name and that it be a simple
  /// identifier.
  llvm::StringRef getName() const {
	  assert(Name.isIdentifier() && "Name is not a simple identifier");
	  return getIdentifier() ? getIdentifier()->getName() : "";
  }

  /// getNameAsString - Get a human-readable name for the definition, even if
  /// it is one of the special kinds of names (constructor etc).  Creating
  /// this name requires expensive string manipulation, so it should be called
  /// only when performance doesn't matter.
  /// For simple declarations, getNameAsCString() should suffice.
  //
  // FIXME: This function should be renamed to indicate that it is not just an
  // alternate form of getName(), and clients should move as appropriate.
  //
  // FIXME: Deprecated, move clients to getName().
  std::string getNameAsString() const { return Name.getAsString(); }

  void printName(llvm::raw_ostream &os) const { return Name.printName(os); }

  /// getDefnName - Get the actual, stored name of the definition,
  /// which may be a special name.
  DefinitionName getDefnName() const { return Name; }

  /// \brief Set the name of this definition.
  void setDefnName(DefinitionName N) { Name = N; }

  /// getQualifiedNameAsString - Returns human-readable qualified name for
  /// definition, like A.B.i, for i being member of namespace A.B.
  /// If definition is not member of context which can be named (record,
  /// namespace), it will return same result as getNameAsString().
  /// Creating this name is expensive, so it should be called only when
  /// performance doesn't matter.
  std::string getQualifiedNameAsString() const;
  std::string getQualifiedNameAsString(const PrintingPolicy &Policy) const;

  /// getNameForDiagnostic - Appends a human-readable name for this
  /// definition into the given string.
  ///
  /// This is the method invoked by Sema when displaying a NamedDefn
  /// in a diagnostic.  It does not necessarily produce the same
  /// result as getNameAsString(); for example, class template
  /// specializations are printed with their template arguments.
  ///
  /// TODO: use an API that doesn't require so many temporary strings
  virtual void getNameForDiagnostic(std::string &S,
                                    const PrintingPolicy &Policy,
                                    bool Qualified) const {
    if (Qualified)
      S += getQualifiedNameAsString(Policy);
    else
      S += getNameAsString();
  }

  /// definitionReplaces - Determine whether this definition, if
  /// known to be well-formed within its context, will replace the
  /// definition OldD if introduced into scope. A definition will
  /// replace another definition if, for example, it is a
  /// redefinition of the same variable or function, but not if it is
  /// a definition of a different kind (function vs. class) or an
  /// overloaded function.
  bool definitionReplaces(NamedDefn *OldD) const;

  /// \brief Determine whether this definition has linkage.
  bool hasLinkage() const;

  /// \brief Determine whether this definition is a class member.
  bool isClassMember() const {
    const DefnContext *DC = getDefnContext();

    return DC->isRecord();
  }

  /// \brief Given that this definition is a C++ class member,
  /// determine whether it's an instance member of its class.
  bool isClassInstanceMember() const;

  class LinkageInfo {
	Linkage linkage_;
	Visibility visibility_;
	bool explicit_;

  public:
	  LinkageInfo() : linkage_(ExternalLinkage), visibility_(DefaultVisibility),
                    explicit_(false) {}
    LinkageInfo(Linkage L, Visibility V, bool E) :
    	linkage_(L), visibility_(V), explicit_(E) {}

    static LinkageInfo external() {
	  return LinkageInfo();
    }
    static LinkageInfo internal() {
	  return LinkageInfo(InternalLinkage, DefaultVisibility, false);
    }
	static LinkageInfo uniqueExternal() {
	  return LinkageInfo(UniqueExternalLinkage, DefaultVisibility, false);
	}
	static LinkageInfo none() {
	  return LinkageInfo(NoLinkage, DefaultVisibility, false);
	}

	Linkage linkage() const {
	  return linkage_;
	}
	Visibility visibility() const {
	  return visibility_;
	}
	bool visibilityExplicit() const {
	  return explicit_;
	}

	void setLinkage(Linkage L) {
      linkage_ = L;
	}
	void setVisibility(Visibility V) {
	  visibility_ = V;
	}
	void setVisibility(Visibility V, bool E) {
	  visibility_ = V;
	  explicit_ = E;
	}
	void setVisibility(LinkageInfo Other) {
	  setVisibility(Other.visibility(), Other.visibilityExplicit());
	}

    void mergeLinkage(Linkage L) {
      setLinkage(minLinkage(linkage(), L));
    }
    void mergeLinkage(LinkageInfo Other) {
      setLinkage(minLinkage(linkage(), Other.linkage()));
    }

    void mergeVisibility(Visibility V) {
      setVisibility(minVisibility(visibility(), V));
    }
    void mergeVisibility(Visibility V, bool E) {
      setVisibility(minVisibility(visibility(), V), visibilityExplicit() || E);
    }
    void mergeVisibility(LinkageInfo Other) {
      mergeVisibility(Other.visibility(), Other.visibilityExplicit());
    }
    void merge(LinkageInfo Other) {
      mergeLinkage(Other);
      mergeVisibility(Other);
    }
    void merge(std::pair<Linkage,Visibility> LV) {
      mergeLinkage(LV.first);
      mergeVisibility(LV.second);
    }

    friend LinkageInfo merge(LinkageInfo L, LinkageInfo R) {
      L.merge(R);
      return L;
    }
  };

  /// \brief Determine what kind of linkage this entity has.
  Linkage getLinkage() const;

  /// \brief Determines the visibility of this entity.
  Visibility getVisibility() const { return getLinkageAndVisibility().visibility(); }

  /// \brief Determines the linkage and visibility of this entity.
  LinkageInfo getLinkageAndVisibility() const;

  /// \brief Clear the linkage cache in response to a change
  /// to the definition.
  void ClearLinkageCache() { HasCachedLinkage = 0; }

  /// \brief Looks through UsingDefns for the underlying named defn
  NamedDefn *getUnderlyingDefn();
  const NamedDefn *getUnderlyingDefn() const {
    return const_cast<NamedDefn*>(this)->getUnderlyingDefn();
  }

  static bool classof(const Defn *D) { return classofKind(D->getKind()); }
  static bool classof(const NamedDefn *D) { return true; }
  static bool classofKind(Kind K) { return K >= firstNamed && K <= lastNamed; }
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                     const NamedDefn *ND) {
  ND->getDefnName().printName(OS);
  return OS;
}

class TypeDefn : public NamedDefn, public DefnContext {
public:
	enum TypeKind {
		TK_Struct,
		TK_CellArray,
		TK_ContainerMap,
		TK_UserClassdef
	};

private:
	unsigned TagKind;

	/// TypeForDefn - This indicates the Type object that represents
	/// this TypeDefn.  It is a cache maintained by
	/// ASTContext::getClassdefType.
	mutable RawType *TypeForDefn;

	/// IsBeingDefined - True if this is currently being defined.
	bool IsBeingDefined;

	friend class ASTContext;
	//friend class UserClassDefn;
	//friend class ClassdefType;

protected:
  TypeDefn(Kind DK, TypeKind TK, DefnContext *DC, SourceLocation L,
           IdentifierInfo *Id)
    : NamedDefn(DK, DC, L, Id), DefnContext(DK),
		  TagKind(TK), TypeForDefn(0), IsBeingDefined(true) {}

public:

  unsigned getTypeKind() const {
	  return TagKind;
  }
  // Low-level accessor
  RawType *getTypeForDefn() const { return TypeForDefn; }
  void setTypeForDefn(RawType *TD) { TypeForDefn = TD; }

	/// isBeingDefined - Return true if this defn is currently being defined.
	bool isBeingDefined() const { return IsBeingDefined; }

	std::string getKindName() const {
		switch(TagKind) {
		case TK_Struct:
			return "struct";
		case TK_CellArray:
			return "cell";
		case TK_ContainerMap:
			return "map";
		case TK_UserClassdef:
			return "user class";
		default:
			assert(0 && "not a valid type defn.");
		}
	}

	bool isStruct() const {
		return getTypeKind() == TK_Struct;
	}

	bool isCell() const {
		return getTypeKind() == TK_CellArray;
	}

	bool isClassdef() const {
		return getTypeKind() == TK_UserClassdef;
	}

	bool isMap() const {
		return getTypeKind() == TK_ContainerMap;
	}

  // Iterator access to field members. The field iterator only visits
  // the non-static data members of this class, ignoring any static
  // data members, functions, constructors, destructors, etc.
  typedef specific_defn_iterator<MemberDefn> member_iterator;

  member_iterator member_begin() const;
  member_iterator member_end() const {
	  return member_iterator(defn_iterator());
  }
  // member_empty - Whether there are any fields (non-static data
  // members) in this record.
  bool member_empty() const {
	  return member_begin() == member_end();
  }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Defn *D) { return classofKind(D->getKind()); }
  static bool classof(const TypeDefn *D) { return true; }
  static bool classofKind(Kind K) { return K >= firstType && K <= lastType; }
  static DefnContext *castToDefnContext(const TypeDefn *D) {
  		return static_cast<DefnContext *>(const_cast<TypeDefn*>(D));
  	}
  static TypeDefn *castFromDefnContext(const DefnContext *DC) {
	  return static_cast<TypeDefn *>(const_cast<DefnContext*>(DC));
  }
};

/// CellDefn - A cell object can be constructed by:
///  1. C = {A B D E} - by a pair of braces
///  2. C3 = {C1 C2}  - by concatenating existing cell arrays
///  3. C = cell(N)   - by cell function call to preallocate memory
///                   - equal to C{N,N} = []
///
class CellDefn : public TypeDefn {
	Expr *CellInit;

public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const CellDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == Cell; }
};

/// StructDefn - A struct object can be constructed by:
///  1. s = struct('field1', values1, 'field2', values2, ...)
///  2. s = struct('field1', {}, 'field2', {}, ...) -- nested struct
///  3. s = struct
///  4. s = struct([])
///  5. s = struct(obj) - copy constructor
///
class StructDefn : public TypeDefn {
	Expr *StructInit;

public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const StructDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == Struct; }
};

class ValueDefn : public NamedDefn {
	Type DefnType;

protected:
  ValueDefn(Kind DK, DefnContext *DC, SourceLocation L,
            DefinitionName N, Type T)
    : NamedDefn(DK, DC, L, N), DefnType(T) {}

public:
  Type getType() const { return DefnType; }
  void setType(Type newType) { DefnType = newType; }

	// Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ValueDefn *D) { return true; }
	static bool classofKind(Kind K) { return K >= firstValue && K<= lastValue; }
};

/// \brief Structure used to store a statement, the constant value to
/// which it was evaluated (if any), and whether or not the statement
/// is an integral constant expression (if known).
struct EvaluatedStmt {
  EvaluatedStmt() : WasEvaluated(false), IsEvaluating(false), CheckedICE(false),
                    CheckingICE(false), IsICE(false) { }

  /// \brief Whether this statement was already evaluated.
  bool WasEvaluated : 1;

  /// \brief Whether this statement is being evaluated.
  bool IsEvaluating : 1;

  /// \brief Whether we already checked whether this statement was an
  /// integral constant expression.
  bool CheckedICE : 1;

  /// \brief Whether we are checking whether this statement is an
  /// integral constant expression.
  bool CheckingICE : 1;

  /// \brief Whether this statement is an integral constant
  /// expression. Only valid if CheckedICE is true.
  bool IsICE : 1;

  Stmt *Value;
  APValue Evaluated;
};

/// VarDefn - A named variable that defined by an assignment expression.
class VarDefn : public ValueDefn {
public:
  typedef mlang::StorageClass StorageClass;

  /// getStorageClassSpecifierString - Return the string used to
  /// specify the storage class \arg SC.
  ///
  /// It is illegal to call this function with SC == None.
  static const char *getStorageClassSpecifierString(StorageClass SC);

protected:
  typedef llvm::PointerUnion<Stmt *, EvaluatedStmt *> InitType;

  /// \brief The initializer for this variable or, for a ParamVarDefn, the
  /// default argument.
  mutable InitType Init;

private:
  // FIXME: This can be packed into the bitfields in Defn.
  unsigned SClass : 3;
  unsigned SClassAsWritten : 3;
	bool PrintResult : 1;

	/// \brief Whether this variable is the exception variable in a catch
	/// statement.
	bool ExceptionVar : 1;

	friend class StmtIteratorBase;
	friend class ASTDefnReader;

protected:
  VarDefn(Kind DK, DefnContext *DC, SourceLocation L, DefinitionName Id,
		      Type T, StorageClass SC, StorageClass SCAsWritten)
    : ValueDefn(DK, DC, L, Id, T),
      PrintResult(false),
      ExceptionVar(false) {
    SClass = SC;
    SClassAsWritten = SCAsWritten;
  }

public:
  static VarDefn *Create(ASTContext &C, DefnContext *DC,
		                     SourceLocation L, IdentifierInfo *Id,
		                     Type T, StorageClass S,
		                     StorageClass SCAsWritten);

  virtual SourceLocation getInnerLocStart() const;
  virtual SourceRange getSourceRange() const;

  StorageClass getStorageClass() const { return (StorageClass)SClass; }
  StorageClass getStorageClassAsWritten() const {
	  return (StorageClass) SClassAsWritten;
  }
  void setStorageClass(StorageClass SC);
  void setStorageClassAsWritten(StorageClass SC) {
    assert(isLegalForVariable(SC));
    SClassAsWritten = SC;
  }
  bool hasGlobalStorage() const {
	  return getStorageClass() == mlang::SC_Global;
  }

  /// \brief Determine whether this variable is the exception variable in a
  /// catch statememt.
  bool isExceptionVariable() const {
	  return ExceptionVar;
  }
  void setExceptionVariable(bool EV) { ExceptionVar = EV; }

  /// getAnyInitializer - Get the initializer for this variable, no matter which
  /// definition it is attached to.
  const Expr *getAnyInitializer() const {
	  const VarDefn *D;
	  return getAnyInitializer(D);
  }

  /// getAnyInitializer - Get the initializer for this variable, no matter which
  /// definition it is attached to. Also get that definition.
  const Expr *getAnyInitializer(const VarDefn *&D) const;

  bool hasInit() const {
	  return !Init.isNull() && (Init.is<Stmt *>() || Init.is<EvaluatedStmt *>());
  }

  const Expr *getInit() const {
	  if (Init.isNull())
		  return 0;

	  const Stmt *S = Init.dyn_cast<Stmt *>();
	  if (!S) {
		  if (EvaluatedStmt *ES = Init.dyn_cast<EvaluatedStmt*>())
			  S = ES->Value;
	  }
	  return (const Expr*) S;
  }

  Expr *getInit() {
	  if (Init.isNull())
		  return 0;

	  Stmt *S = Init.dyn_cast<Stmt *>();
	  if (!S) {
		  if (EvaluatedStmt *ES = Init.dyn_cast<EvaluatedStmt*>())
			  S = ES->Value;
	  }

	  return (Expr*) S;
  }

  /// \brief Retrieve the address of the initializer expression.
  Stmt **getInitAddress() {
	  if (EvaluatedStmt *ES = Init.dyn_cast<EvaluatedStmt*>())
		  return &ES->Value;

	  // This union hack tip-toes around strict-aliasing rules.
	  union {
		  InitType *InitPtr;
		  Stmt **StmtPtr;
	  };

	  InitPtr = &Init;
	  return StmtPtr;
  }

  void setInit(Expr *I);

  EvaluatedStmt *EnsureEvaluatedStmt() const {
    EvaluatedStmt *Eval = Init.dyn_cast<EvaluatedStmt *>();
    if (!Eval) {
      Stmt *S = Init.get<Stmt *>();
      Eval = new (getASTContext(), 8) EvaluatedStmt;
      Eval->Value = S;
      Init = Eval;
    }
    return Eval;
  }

  /// \brief Check whether we are in the process of checking whether the
  /// initializer can be evaluated.
  bool isEvaluatingValue() const {
    if (EvaluatedStmt *Eval = Init.dyn_cast<EvaluatedStmt *>())
      return Eval->IsEvaluating;

    return false;
  }

  /// \brief Note that we now are checking whether the initializer can be
  /// evaluated.
  void setEvaluatingValue() const {
    EvaluatedStmt *Eval = EnsureEvaluatedStmt();
    Eval->IsEvaluating = true;
  }

  /// \brief Note that constant evaluation has computed the given
  /// value for this variable's initializer.
  void setEvaluatedValue(const APValue &Value) const {
    EvaluatedStmt *Eval = EnsureEvaluatedStmt();
    Eval->IsEvaluating = false;
    Eval->WasEvaluated = true;
    Eval->Evaluated = Value;
  }

  /// \brief Return the already-evaluated value of this variable's
  /// initializer, or NULL if the value is not yet known. Returns pointer
  /// to untyped APValue if the value could not be evaluated.
  APValue *getEvaluatedValue() const {
    if (EvaluatedStmt *Eval = Init.dyn_cast<EvaluatedStmt *>())
      if (Eval->WasEvaluated)
        return &Eval->Evaluated;

    return 0;
  }

  /// \brief Determines whether it is already known whether the
  /// initializer is an integral constant expression or not.
  bool isInitKnownICE() const {
    if (EvaluatedStmt *Eval = Init.dyn_cast<EvaluatedStmt *>())
      return Eval->CheckedICE;

    return false;
  }

  /// \brief Determines whether the initializer is an integral
  /// constant expression.
  ///
  /// \pre isInitKnownICE()
  bool isInitICE() const {
    assert(isInitKnownICE() &&
           "Check whether we already know that the initializer is an ICE");
    return Init.get<EvaluatedStmt *>()->IsICE;
  }

  /// \brief Check whether we are in the process of checking the initializer
  /// is an integral constant expression.
  bool isCheckingICE() const {
    if (EvaluatedStmt *Eval = Init.dyn_cast<EvaluatedStmt *>())
      return Eval->CheckingICE;

    return false;
  }

  /// \brief Note that we now are checking whether the initializer is an
  /// integral constant expression.
  void setCheckingICE() const {
    EvaluatedStmt *Eval = EnsureEvaluatedStmt();
    Eval->CheckingICE = true;
  }

  /// \brief Note that we now know whether the initializer is an
  /// integral constant expression.
  void setInitKnownICE(bool IsICE) const {
    EvaluatedStmt *Eval = EnsureEvaluatedStmt();
    Eval->CheckingICE = false;
    Eval->CheckedICE = true;
    Eval->IsICE = IsICE;
  }

  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const VarDefn *D) { return true; }
	static bool classofKind(Kind K) { return K >= firstVar && K<= lastVar; }
};

/// ParamVarDefn - Represent a parameter to a function.
class ParamVarDefn : public VarDefn {

protected:
  ParamVarDefn(Kind DK, DefnContext *DC, SourceLocation L,
               IdentifierInfo *Id, Type T,
               StorageClass S, StorageClass SCAsWritten, Expr *DefArg)
    : VarDefn(DK, DC, L, Id, T, S, SCAsWritten) {
    setDefaultArg(DefArg);
  }

public:
  static ParamVarDefn *Create(ASTContext &C, DefnContext *DC,
                              SourceLocation L,IdentifierInfo *Id,
                              Type T, StorageClass S,
                              StorageClass SCAsWritten, Expr *DefArg);

  Expr *getDefaultArg();
  const Expr *getDefaultArg() const {
    return const_cast<ParamVarDefn *>(this)->getDefaultArg();
    }

  void setDefaultArg(Expr *defarg) {
	  Init = reinterpret_cast<Stmt *>(defarg);
  }

	// Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ParamVarDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ParamVar; }
};

/// MemberDefn - This is usually introduced explicitly during s struct
/// construction by field value assignment or a user class object property
/// declaration and initiation;
/// Can be 2 forms:
///  1. s.field1 = value1, s.field2 = value2, ...
///  2. s(1).field1 = value1, s(1).field2 = value2, ...
///     s(2).field1 = value1, s(2).field2 = value2, ...
/// Form #2 is for nested struct creation.
///
/// Also it can be implicitly introduced by struct constructor innovation,
/// e.g.    s = struct('a',5,'b','key');
/// The above assignment introduces a VarDefn named 's', a StructDefn also
/// named 's', and two MemberDefn named 'a' and 'b' respectively.
class MemberDefn : public VarDefn {
	// Style about how this member is indexed (referred).
	// Can be by field name in a struct/class or by number in a cell array
	bool IndexedByName;

public:
  MemberDefn(Kind DK, DefnContext *DC, SourceLocation L, DefinitionName NField,
		         Type T, StorageClass SC, StorageClass SCAsWritten, bool IndexedByName)
    : VarDefn(DK, DC, L, NField, T, SC, SCAsWritten),
      IndexedByName(IndexedByName) {
	}

  bool isIndexedByName() const {
	  return IndexedByName;
  }
  bool isIndexedByNumuric() const {
	  return !IndexedByName;
  }

  /// getParent - Returns the parent of this field declaration, which
  /// is the struct in which this method is defined.
  const TypeDefn *getParent() const {
	  return cast<TypeDefn>(getDefnContext());
  }
  TypeDefn *getParent() {
      return cast<TypeDefn>(getDefnContext());
    }

 	// Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const MemberDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == Member; }
};

/// FunctionDefn - This represents a function definition in a seperate
/// file. NOTE that the function may be ended with 'end' keyword or a
/// tok::EoF.
class FunctionDefn : public ValueDefn, public DefnContext {
public:
	typedef mlang::StorageClass StorageClass;

private:
	ASTContext &Context;

	SourceLocation FunctionBeginLoc; // location of function keyword
	// note that a function file can be ended without end keyword
	// but with a EoF token.
	SourceLocation FunctionEndLoc; // location of end keyword

	/// ParamInfo - new[]'d array of pointers to VarDecls for the formal
	/// parameters of this function.  This is null if a prototype or if there are
	/// no formals.
	ParamVarDefn **ParamInfo;

	LazyDefnCmdPtr Body;

	// FIXME: This can be packed into the bitfields in Decl.
	// NOTE: VC++ treats enums as signed, avoid using the StorageClass enum
	unsigned SClass : 2;
	unsigned SClassAsWritten : 2;
	bool IsInline : 1;
	bool IsInlineSpecified : 1;
	bool HasInheritedPrototype : 1;
	bool HasWrittenPrototype : 1;
	bool IsDeleted : 1;
	bool IsTrivial : 1; // sunk from ClassMethodDefn
	bool HasImplicitReturnZero : 1;

  /// \brief End part of this FunctionDecl's source range.
	///
	/// We could compute the full range in getSourceRange(). However, when we're
	/// dealing with a function definition deserialized from a PCH/AST file,
	/// we can only compute the full range once the function body has been
	/// de-serialized, so it's far better to have the (sometimes-redundant)
	/// EndRangeLoc.
	SourceLocation EndRangeLoc;

	void setParams(ASTContext &C, ParamVarDefn **NewParamInfo, unsigned NumParams);

protected:
  FunctionDefn(Kind DK, ASTContext &Ctx, DefnContext *DC,
		           const DefinitionNameInfo &NameInfo,
               Type T, StorageClass S, StorageClass SCAsWritten,
               bool isInlineSpecified, bool hasWrittenPrototype)
    : ValueDefn(DK, DC, NameInfo.getLoc(), NameInfo.getName(), T),
      DefnContext(DK), Context(Ctx),
      ParamInfo(0), Body(),
      SClass(S), SClassAsWritten(SCAsWritten),
      IsInline(isInlineSpecified), IsInlineSpecified(isInlineSpecified),
      HasInheritedPrototype(false), HasWrittenPrototype(hasWrittenPrototype),
      IsDeleted(false), IsTrivial(false), HasImplicitReturnZero(false),
      EndRangeLoc(NameInfo.getEndLoc()){}

public:
  static FunctionDefn *Create(ASTContext &C, DefnContext *DC, SourceLocation L,
                                DefinitionName N, Type T,
                                StorageClass S = SC_Local,
                                StorageClass SCAsWritten = SC_Local,
                                bool isInlineSpecified = false,
                                bool hasWrittenPrototype = true) {
      DefinitionNameInfo NameInfo(N, L);
      return FunctionDefn::Create(C, DC, NameInfo, T, S, SCAsWritten,
                                  isInlineSpecified, hasWrittenPrototype);
    }

    static FunctionDefn *Create(ASTContext &C, DefnContext *DC,
                                const DefinitionNameInfo &NameInfo,
                                Type T, StorageClass S = SC_Local,
                                StorageClass SCAsWritten = SC_Local,
                                bool isInlineSpecified = false,
                                bool hasWrittenPrototype = true);

  DefinitionNameInfo getNameInfo() const {
	  return DefinitionNameInfo(getDefnName(), getLocation());
  }

  virtual SourceRange getSourceRange() const {
	  return SourceRange(FunctionBeginLoc, EndRangeLoc);
    }
  void setLocEnd(SourceLocation E) {
	  EndRangeLoc = E;
  }

  /// getBody - Retrieve the body (definition) of the function.
  virtual Cmd *getBody() const {
	  return reinterpret_cast<Cmd *>(
			  Body.get(Context.getExternalSource()));
  }
  void setBody(BlockCmd *C);
  void setLazyBody(uint64_t Offset) { Body = Offset; }

  /// Whether this function is variadic.
  bool isVariadic() const;

  unsigned getBuiltinID() const;

  Type getResultType() const {
	  return cast<FunctionType>(getType().getRawTypePtr())->getResultType();
  }

  // Iterator access to formal parameters.
  unsigned param_size() const { return getNumParams(); }
  typedef ParamVarDefn **param_iterator;
  typedef ParamVarDefn * const *param_const_iterator;

  param_iterator param_begin() { return ParamInfo; }
  param_iterator param_end()   { return ParamInfo+param_size(); }

  param_const_iterator param_begin() const { return ParamInfo; }
  param_const_iterator param_end() const   { return ParamInfo+param_size(); }

  /// getNumParams - Return the number of parameters this function must have
  /// based on its FunctionType.  This is the length of the ParamInfo array
  /// after it has been created.
  unsigned getNumParams() const;

  const ParamVarDefn *getParamDefn(unsigned i) const {
	  assert(i < getNumParams() && "Illegal param #");
	  return ParamInfo[i];
  }

  ParamVarDefn *getParamDefn(unsigned i) {
	  assert(i < getNumParams() && "Illegal param #");
	  return ParamInfo[i];
  }

  void setParams(ParamVarDefn **NewParamInfo, unsigned NumParams) {
	  setParams(getASTContext(), NewParamInfo, NumParams);
  }

  StorageClass getStorageClass() const { return StorageClass(SClass); }
  void setStorageClass(StorageClass SC);

  StorageClass getStorageClassAsWritten() const {
	  return StorageClass(SClassAsWritten);
  }

  /// \brief Determine whether the "inline" keyword was specified for this
  /// function.
  bool isInlineSpecified() const { return IsInlineSpecified; }

  /// Set whether the "inline" keyword was specified for this function.
  void setInlineSpecified(bool I) {
	  IsInlineSpecified = I;
	  IsInline = I;
  }

  /// \brief Determine whether this function should be inlined, because it is
  /// either marked "inline" or is a member function of a user class that
  /// was defined in the class body.
  bool isInlined() const;

  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const FunctionDefn *D) { return true; }
	static bool classofKind(Kind K) { return K >= firstFunction && K<= lastFunction;}
	static DefnContext *castToDefnContext(const FunctionDefn *D) {
		return static_cast<DefnContext *>(const_cast<FunctionDefn*>(D));
	}
	static FunctionDefn *castFromDefnContext(const DefnContext *DC) {
		return static_cast<FunctionDefn *>(const_cast<DefnContext*>(DC));
	}

	friend class ASTDefnReader;
	friend class ASTDefnWriter;
};

/// AnonFunctionDefn - A anonymous function can reside in a function handle
/// definition or directly be used as a function handle reference (within a
/// function call which has a function handle input parameter).
class AnonFunctionDefn : public Defn {
	/// indicate which defn this resides in
	Defn *Parent;

public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const AnonFunctionDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == AnonFunction;}
};

/// ScriptDefn - A script definition's name is its file name
class ScriptDefn : public NamedDefn, public DefnContext {
public:
	/// A class which contains all the information about a particular
	/// captured value.
	class Capture {
		enum {
			flag_isByRef = 0x1, flag_isNested = 0x2
		};

		/// The variable being captured.
		llvm::PointerIntPair<VarDefn*, 2> VariableAndFlags;

		/// The copy expression, expressed in terms of a DefnRef (or
		/// BlockDeclRef) to the captured variable.  Only required if the
		/// variable has a C++ class type.
		Expr *CopyExpr;

	public:
		Capture(VarDefn *variable, bool byRef, bool nested, Expr *copy) :
			VariableAndFlags(variable, (byRef ? flag_isByRef : 0)
					| (nested ? flag_isNested : 0)), CopyExpr(copy) {
		}

		/// The variable being captured.
		VarDefn *getVariable() const {
			return VariableAndFlags.getPointer();
		}

		/// Whether this is a "by ref" capture, i.e. a capture of a __block
		/// variable.
		bool isByRef() const {
			return VariableAndFlags.getInt() & flag_isByRef;
		}

		/// Whether this is a nested capture, i.e. the variable captured
		/// is not from outside the immediately enclosing function/block.
		bool isNested() const {
			return VariableAndFlags.getInt() & flag_isNested;
		}

		bool hasCopyExpr() const {
			return CopyExpr != 0;
		}
		Expr *getCopyExpr() const {
			return CopyExpr;
		}
		void setCopyExpr(Expr *e) {
			CopyExpr = e;
		}
	};

private:
	Cmd *Body;

	// whether it is the entry script
	bool isEntry;

	Capture *Captures;
	unsigned NumCaptures;

protected:
	explicit ScriptDefn(DefnContext *DC)
    : NamedDefn(Script, DC, SourceLocation(), DefinitionName()),
      DefnContext(Script),
      Body(0), isEntry(false), Captures(0), NumCaptures(0) {}

public:
	static ScriptDefn *Create(ASTContext &C, DefnContext *DC);

	BlockCmd *getBlockBody() { return reinterpret_cast<BlockCmd*>(Body); }
	virtual Cmd *getBody() const { return (Cmd*) Body; }

	void setBody(BlockCmd *B) { Body = (Cmd*) B; }

	bool isEntryScript() const {
		return isEntry;
	}
	void setEntryScript(bool enable) {
		isEntry = enable;
	}
	void setEntryScript() {
		setEntryScript(true);
	}

	/// hasCaptures - True if this block (or its nested blocks) captures
	/// anything of local storage from its enclosing scopes.
	bool hasCaptures() const { return NumCaptures != 0; }

	/// getNumCaptures - Returns the number of captured variables.
	/// Does not include an entry for 'this'.
	unsigned getNumCaptures() const { return NumCaptures; }

	typedef const Capture *capture_iterator;
	typedef const Capture *capture_const_iterator;
	capture_iterator capture_begin() { return Captures; }
	capture_iterator capture_end() { return Captures + NumCaptures; }
	capture_const_iterator capture_begin() const { return Captures; }
	capture_const_iterator capture_end() const { return Captures + NumCaptures; }

	bool capturesVariable(const VarDefn *var) const;

	void setCaptures(ASTContext &Context,
	                 const Capture *begin,
	                 const Capture *end);

	virtual SourceRange getSourceRange() const;

	// Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ScriptDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == Script;}
	static DefnContext *castToDefnContext(const ScriptDefn *D) {
		return static_cast<DefnContext *>(const_cast<ScriptDefn*>(D));
	}
	static ScriptDefn *castFromDefnContext(const DefnContext *DC) {
		return static_cast<ScriptDefn *>(const_cast<DefnContext*>(DC));
	}
};

} // end namespace mlang
#endif /* MLANG_AST_DEFN_TRANSLATION_UNIT_H_ */
