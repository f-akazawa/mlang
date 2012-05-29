//===--- MemberExpr.h - Struct member or user-classdef property -*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the MemberExpr class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_MEMBER_EXPR_H_
#define MLANG_AST_EXPR_MEMBER_EXPR_H_

#include "mlang/AST/Expr.h"
#include "mlang/AST/DefnAccessPair.h"
#include "mlang/AST/DefinitionName.h"

namespace mlang {
class ValueDefn;
class Type;

/// struct property or class property (method) reference
///
class MemberExpr: public Expr {
	/// Extra data stored in some member expressions.
	struct MemberNameQualifier : public NameQualifier {
		DefnAccessPair FoundDefn;
	};

	/// Base - the expression for structure references.  In
	/// X.F, this is "X".
	Stmt *Base;

	/// Member - the expression for structure references.  In
	/// X.F, this is "F".
	ValueDefn *MemberDefn;

	/// MemberLoc - This is the location of the member name.
	SourceLocation MemberLoc;

	/// \brief True if this member expression used a nested-name-specifier to
	/// refer to the member, e.g., "x->Base::f", or found its member via a using
	/// declaration.  When true, a MemberNameQualifier
  /// structure is allocated immediately after the MemberExpr.
	bool HasQualifierOrFoundDefn : 1;

	/// \brief Retrieve the qualifier that preceded the member name, if any.
	MemberNameQualifier *getMemberQualifier() {
		assert(HasQualifierOrFoundDefn);
		return reinterpret_cast<MemberNameQualifier *> (this + 1);
	}

	/// \brief Retrieve the qualifier that preceded the member name, if any.
	const MemberNameQualifier *getMemberQualifier() const {
		return const_cast<MemberExpr *>(this)->getMemberQualifier();
	}

public:
	MemberExpr(Expr *base, ValueDefn *memberdefn,
	           const DefinitionNameInfo &NameInfo, Type ty,
	           ExprValueKind VK);

	MemberExpr(Expr *base, ValueDefn *member, Type ty, SourceLocation l,
	           ExprValueKind VK) :
		Expr(MemberExprClass, ty, VK), Base(base), MemberDefn(member),
		MemberLoc(l), HasQualifierOrFoundDefn(false) {
	}

	static MemberExpr *Create(ASTContext &C, Expr *base,
			                      NestedNameSpecifier *qual, SourceRange qualrange,
                            ValueDefn *member, DefnAccessPair founddefn,
                            DefinitionNameInfo MemberNameInfo,
                            Type ty, ExprValueKind VK);

	void setBase(Expr *E) {
		Base = E;
	}
	Expr *getBase() const {
		return cast<Expr> (Base);
	}

	/// \brief Retrieve the member declaration to which this expression refers.
	///
	/// The returned declaration will either be a MemberDefn or (in C++)
	/// a ClassMethodDefn.
	ValueDefn *getMemberDefn() const { return MemberDefn; }
	void setMemberDefn(ValueDefn *D) { MemberDefn = D; }

	/// \brief Retrieves the declaration found by lookup.
	DefnAccessPair getFoundDefn() const;

	/// \brief Determines whether this member expression actually had
	/// a C++ nested-name-specifier prior to the name of the member, e.g.,
	/// x->Base::foo.
	bool hasQualifier() const { return getQualifier() != 0; }

	/// \brief If the member name was qualified, retrieves the source range of
	/// the nested-name-specifier that precedes the member name. Otherwise,
	/// returns an empty source range.
	SourceRange getQualifierRange() const {
		if (!HasQualifierOrFoundDefn)
			return SourceRange();
		return getMemberQualifier()->Range;
	}

	/// \brief If the member name was qualified, retrieves the
	/// nested-name-specifier that precedes the member name. Otherwise, returns
	/// NULL.
	NestedNameSpecifier *getQualifier() const {
		if (!HasQualifierOrFoundDefn)
			return 0;

		return getMemberQualifier()->NNS;
	}

	/// getMemberLoc - Return the location of the "member", in X->F, it is the
	/// location of 'F'.
	SourceLocation getMemberLoc() const {
		return MemberLoc;
	}
	void setMemberLoc(SourceLocation L) {
		MemberLoc = L;
	}

	SourceRange getSourceRange() const {
		// If we have an implicit base (like a C++ implicit this),
		// make sure not to return its location
		SourceLocation EndLoc = getMemberLoc();

		SourceLocation BaseLoc = getBase()->getLocStart();

		return SourceRange(BaseLoc, EndLoc);
	}

	virtual SourceLocation getExprLoc() const {
		return MemberLoc;
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == MemberExprClass;
	}
	static bool classof(const MemberExpr *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&Base, &Base + 1);
	};

	friend class ASTReader;
	friend class ASTStmtWriter;
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_MEMBER_EXPR_H_ */
