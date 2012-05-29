//===--- DefnOOP.h - Defn Subclasses related to OOP -------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines Defn Subclasses for OOP.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_DEFINITION_OOP_H_
#define MLANG_AST_DEFINITION_OOP_H_

#include "mlang/AST/DefnSub.h"
#include "mlang/AST/UnresolvedSet.h"

namespace mlang {
/// NamespaceDefn - It is external source package introduece by 'import'
/// command.
class NamespaceDefn : public NamedDefn, public DefnContext {

public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const NamespaceDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == Namespace;}
};

/// ClassBaseSpecifier - A base class of a C++ class.
///
/// Each ClassBaseSpecifier represents a single, direct base class (or
/// struct) of a C++ class (or struct). It specifies the type of that
/// base class, whether it is a virtual or non-virtual base, and what
/// level of access (public, protected, private) is used for the
/// derivation. For example:
///
/// @code
///   class A { };
///   class B { };
///   class C : public virtual A, protected B { };
/// @endcode
///
/// In this code, C will have two CXXBaseSpecifiers, one for "public
/// virtual A" and the other for "protected B".
class ClassBaseSpecifier {
  /// Range - The source code range that covers the full base
  /// specifier, including the "virtual" (if present) and access
  /// specifier (if present).
  SourceRange Range;

  /// \brief The source location of the ellipsis, if this is a pack
  /// expansion.
  SourceLocation EllipsisLoc;

  /// Virtual - Whether this is a virtual base class or not.
  bool Virtual : 1;

  /// BaseOfClass - Whether this is the base of a class (true) or of a
  /// struct (false). This determines the mapping from the access
  /// specifier as written in the source code to the access specifier
  /// used for semantic analysis.
  bool BaseOfClass : 1;

  /// Access - Access specifier as written in the source code (which
  /// may be AS_none). The actual type of data stored here is an
  /// AccessSpecifier, but we use "unsigned" here to work around a
  /// VC++ bug.
  unsigned Access : 2;

  /// BaseTypeInfo - The type of the base class. This will be a class or struct
  /// (or a typedef of such). The source code range does not include the
  /// "virtual" or access specifier.
  Type BaseTypeInfo;

public:
  ClassBaseSpecifier() { }

  ClassBaseSpecifier(SourceRange R, bool V, bool BC, AccessSpecifier A,
                     Type TInfo, SourceLocation EllipsisLoc)
    : Range(R), EllipsisLoc(EllipsisLoc), Virtual(V), BaseOfClass(BC),
      Access(A), BaseTypeInfo(TInfo) { }

  /// getSourceRange - Retrieves the source range that contains the
  /// entire base specifier.
  SourceRange getSourceRange() const { return Range; }

  /// isVirtual - Determines whether the base class is a virtual base
  /// class (or not).
  bool isVirtual() const { return Virtual; }

  /// \brief Determine whether this base class is a base of a class declared
  /// with the 'class' keyword (vs. one declared with the 'struct' keyword).
  bool isBaseOfClass() const { return BaseOfClass; }

  /// \brief Determine whether this base specifier is a pack expansion.
  bool isPackExpansion() const { return EllipsisLoc.isValid(); }

  /// \brief For a pack expansion, determine the location of the ellipsis.
  SourceLocation getEllipsisLoc() const {
    return EllipsisLoc;
  }

  /// getAccessSpecifier - Returns the access specifier for this base
  /// specifier. This is the actual base specifier as used for
  /// semantic analysis, so the result can never be AS_none. To
  /// retrieve the access specifier as written in the source code, use
  /// getAccessSpecifierAsWritten().
  AccessSpecifier getAccessSpecifier() const {
    if ((AccessSpecifier)Access == AS_none)
      return BaseOfClass? AS_private : AS_public;
    else
      return (AccessSpecifier)Access;
  }

  /// getAccessSpecifierAsWritten - Retrieves the access specifier as
  /// written in the source code (which may mean that no access
  /// specifier was explicitly written). Use getAccessSpecifier() to
  /// retrieve the access specifier for use in semantic analysis.
  AccessSpecifier getAccessSpecifierAsWritten() const {
    return (AccessSpecifier)Access;
  }

  /// getType - Retrieves the type of the base class. This type will
  /// always be an unqualified class type.
  Type getType() const { return BaseTypeInfo; }
};

class ClassMethodDefn;
class ClassConstructorDefn;

/// UserClassDefn - Represents a user class type definition.
class UserClassDefn : public TypeDefn {
	ASTContext &Context;

	SourceLocation ClassBeginLoc; // location of classdef keyword

	// note that a class definition file can be ended without end keyword
	// but with a EoF token.
	SourceLocation ClassEndLoc; // location of end keyword

	/// NumBases - The number of base class specifiers in Bases.
	unsigned NumBases;

	/// Bases - Base classes of this class.
	/// FIXME: This is wasted space for a union.
	LazyClassBaseSpecifiersPtr Bases;

	friend class DefnContext;
	friend class ASTReader;
	friend class ASTWriter;

public:
	UserClassDefn(ASTContext &Ctx, DefnContext *DC,
			          SourceLocation LName,
	              IdentifierInfo *Id,
	              SourceLocation LClassdef,
	              SourceLocation LEnd) :
		TypeDefn(UserClass, TK_UserClassdef, DC, LName, Id),
		Context(Ctx),
		ClassBeginLoc(LClassdef),
		ClassEndLoc(LEnd) {
	}

	/// base_class_iterator - Iterator that traverses the base classes
	/// of a class.
	typedef ClassBaseSpecifier*       base_class_iterator;

	/// base_class_const_iterator - Iterator that traverses the base
	/// classes of a class.
	typedef const ClassBaseSpecifier* base_class_const_iterator;

	/// reverse_base_class_iterator = Iterator that traverses the base classes
	/// of a class in reverse order.
	typedef std::reverse_iterator<base_class_iterator>
	    reverse_base_class_iterator;

	/// reverse_base_class_iterator = Iterator that traverses the base classes
	/// of a class in reverse order.
	typedef std::reverse_iterator<base_class_const_iterator>
	    reverse_base_class_const_iterator;

	/// \brief Retrieve the set of direct base classes.
	ClassBaseSpecifier *getBases() const {
		return Bases.get(Context.getExternalSource());
	}
	/// setBases - Sets the base classes of this struct or class.
	void setBases(ClassBaseSpecifier const * const *Bases, unsigned NumBases);

	/// getNumBases - Retrieves the number of base classes of this
	/// class.
	unsigned getNumBases() const {
		return NumBases;
	}

	base_class_iterator bases_begin() {
		return getBases();
	}
	base_class_const_iterator bases_begin() const {
		return getBases();
	}
	base_class_iterator bases_end() {
		return bases_begin() + NumBases;
	}
	base_class_const_iterator bases_end() const {
		return bases_begin() + NumBases;
	}
	reverse_base_class_iterator bases_rbegin() {
		return reverse_base_class_iterator(bases_end());
	}
	reverse_base_class_const_iterator bases_rbegin() const {
		return reverse_base_class_const_iterator(bases_end());
	}
	reverse_base_class_iterator bases_rend() {
		return reverse_base_class_iterator(bases_begin());
	}
	reverse_base_class_const_iterator bases_rend() const {
		return reverse_base_class_const_iterator(bases_begin());
	}


	/// Iterator access to method members.  The method iterator visits
	/// all method members of the class, including non-instance methods,
	/// special methods, etc.
	typedef specific_defn_iterator<ClassMethodDefn> method_iterator;

	/// method_begin - Method begin iterator.  Iterates in the order the methods
	/// were declared.
	method_iterator method_begin() const {
		return method_iterator(defns_begin());
	}
	/// method_end - Method end iterator.
	method_iterator method_end() const {
		return method_iterator(defns_end());
	}

	/// Iterator access to constructor members.
	typedef specific_defn_iterator<ClassConstructorDefn> ctor_iterator;

	ctor_iterator ctor_begin() const {
		return ctor_iterator(defns_begin());
	}
	ctor_iterator ctor_end() const {
		return ctor_iterator(defns_end());
	}

  SourceLocation getClassdefKeywordLoc() const {
    return ClassBeginLoc;
	}
	void setClassdefKeywordLoc(SourceLocation L) {
		ClassBeginLoc = L;
	}
	SourceLocation getClassEndLoc() const {
		return ClassEndLoc;
	}
	void setClassEndLoc(SourceLocation L) {
		ClassEndLoc = L;
	}

	//FIXME yabin we should remove this later
	bool isEmpty() const {
		return false;
	}
	// Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const UserClassDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == UserClass; }
};

/// ClassAttributesDefn - The base class of class and its components
///  attributes.
class ClassAttributesDefn: public Defn {
	/// The location of the '(' and ')', between which is the attributes
	/// definition expression.
	SourceLocation LParenLoc, RParenLoc;

	/// Attrs - the list of attributes expression
	Expr **Attrs;

	ClassAttributesDefn(AccessSpecifier AS, DefnContext *DC, SourceLocation CALoc,
			SourceLocation LParenLoc,	SourceLocation RParenLoc) :
		Defn(ClassAttributes, DC, CALoc), LParenLoc(LParenLoc), RParenLoc(RParenLoc) {
		setAccess(AS);
	}
	ClassAttributesDefn(EmptyShell Empty) :
		Defn(ClassAttributes, Empty) {
	}

public:
	/// getLParenLoc - The location of the colon following the access specifier.
	SourceLocation getLParenLoc() const {
		return LParenLoc;
	}
	/// setLParenLoc - Sets the location of the left paren.
	void setLParenLoc(SourceLocation LPLoc) {
		LParenLoc = LPLoc;
	}

	SourceLocation getRParenLoc() const {
		return RParenLoc;
	}
	/// setColonLoc - Sets the location of the colon.
	void setRParenLoc(SourceLocation RPLoc) {
		RParenLoc = RPLoc;
	}

	SourceRange getSourceRange() const {
		return SourceRange(getLParenLoc(), getRParenLoc());
	}

	static ClassAttributesDefn *Create(ASTContext &C, AccessSpecifier AS,
			DefnContext *DC, SourceLocation CALoc,
			SourceLocation LPLoc, SourceLocation RPLoc) {
		return new (C, 16) ClassAttributesDefn(AS, DC, CALoc, LPLoc, RPLoc);
	}
	static ClassAttributesDefn *Create(ASTContext &C, EmptyShell Empty) {
		return new (C, 16) ClassAttributesDefn(Empty);
	}

	// Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) {
		return classofKind(D->getKind());
	}
	static bool classof(const ClassAttributesDefn *D) {
		return true;
	}
	static bool classofKind(Kind K) {
		return K == ClassAttributes;
	}
};

/// UserObjectDefn - A user object can be defined by:
///  1. obj = [packagename.]classname  - the default ctor
///  2. obj = [packagename.]class_CTOR(arg1,arg2)
class UserObjectDefn : public VarDefn {
	SourceLocation UserClassCTORLoc;
	Type *theClass;

public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const UserObjectDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == UserObject; }
};

/// ClassEventDefn - Represent a definition in the Events section of
/// a classdef.
class ClassEventDefn : public VarDefn {
public:
	// Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ClassEventDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ClassEvent; }
};

/// ClassEventDefn - Represent a definition in the Properties section
/// of a classdef.
class ClassPropertyDefn : public VarDefn {
public:
	// Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ClassPropertyDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ClassProperty; }
};

class ClassMethodDefn : public FunctionDefn {
public:

	bool isInstance() const { return true;}

  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ClassMethodDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ClassMethod;}
};

class ClassConstructorDefn : public ClassMethodDefn {
public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ClassConstructorDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ClassConstructor;}
};

class ClassDestructorDefn : public ClassMethodDefn {
public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ClassDestructorDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ClassDestructor;}
};

class ClassPropertiesAttrsDefn : public ClassAttributesDefn {
public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ClassPropertiesAttrsDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ClassPropertiesAttrs;}
};

class ClassEventsAttrsDefn : public ClassAttributesDefn {
public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ClassEventsAttrsDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ClassEventsAttrs;}
};

class ClassMethodsAttrsDefn: public ClassAttributesDefn {
public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ClassMethodsAttrsDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ClassMethodsAttrs;}
};

class ImplicitParamDefn : public VarDefn {
public:
  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *D) { return classofKind(D->getKind()); }
	static bool classof(const ClassDestructorDefn *D) { return true; }
	static bool classofKind(Kind K) { return K == ImplicitParam;}
};

} // end namespace mlang

#endif /* MLANG_AST_DEFINITION_OOP_H_ */
