//===--- Expr.h - Expression Base Class of AST Node -------------*- C++ -*-===//
//
// Copyright (C) 2010 Yabin Hu @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file defines the Expr class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPRESSION_H_
#define MLANG_AST_EXPRESSION_H_

#include "mlang/AST/APValue.h"
#include "mlang/AST/Stmt.h"
#include "mlang/AST/Type.h"
#include "mlang/AST/OperationKinds.h"
#include "mlang/Basic/Specifiers.h"
#include "mlang/Basic/IdentifierTable.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <string>
#include <list>

namespace mlang {
class NestedNameSpecifier;

// A base class for expressions.
class Expr: public Stmt {
	Type TR;
	virtual void ANCHOR(); // key function.

protected:
	// A flag that says whether this expression has an index associated
	// with it.  See the code in tree_identifier::rvalue for the rationale.
	bool PostfixIndexed;

	// Print result of rvalue for this expression?
	bool PrintFlag;

	Expr(StmtClass SC, Type T, ExprValueKind VK) :
		Stmt(SC), PostfixIndexed(false), PrintFlag(false) {
		ExprBits.ValueKind = VK;
		setType(T);
	}

	/// \brief Construct an empty expression.
	explicit Expr(StmtClass SC, EmptyShell) :
		Stmt(SC) {
	}

public:
	Type getType() const {
		return TR;
	}
	void setType(Type t) {
		TR = t;
	}

	/// SourceLocation tokens are not useful in isolation - they are low level
	/// value objects created/interpreted by SourceManager. We assume AST
	/// clients will have a pointer to the respective SourceManager.
	virtual SourceRange getSourceRange() const = 0;

	/// getExprLoc - Return the preferred location for the arrow when diagnosing
	/// a problem with a generic expression.
	virtual SourceLocation getExprLoc() const {
		return getLocStart();
	}

	/// isUnusedResultAWarning - Return true if this immediate expression should
	/// be warned about if the result is unused.  If so, fill in Loc and Ranges
	/// with location to warn on and the source range[s] to report with the
	/// warning.
	bool isUnusedResultAWarning(SourceLocation &Loc, SourceRange &R1,
			SourceRange &R2, ASTContext &Ctx) const;

	/// isLValue - True if this expression is an "l-value" according to
	/// the rules of the current language.
	///
	bool isLValue() const {
		return getValueKind() == VK_LValue;
	}

	/// isRValue - True if this expression is an "r-value" according to
	/// the rules of the current language.
	///
	bool isRValue() const {
		return getValueKind() == VK_RValue;
	}

	/// classification of expressions which can be an "L-value" :
	/// 1. variable(identifier) of numeric types and string
	/// 2. multi output assignment by function call
	/// 3. index expression
	/// 4. function handle name
	/// 5. struct or classdef member expression
	/// 6. classdef object
//	enum LValueClassification {
//		LV_Valid,
//		LV_ID,
//		LV_MultiOutput,
//		LV_IndexExpression,
//		LV_InvalidExpression,
//		LV_FunctionHandle,
//		LV_MemberExpression,
//		LV_ClassdefObject
//	};
	/// Reasons why an expression might not be an l-value.
//	LValueClassification ClassifyLValue(ASTContext &Ctx) const;

	/// getValueKindForType - Given a formal return or parameter type,
	/// give its value kind.
	static ExprValueKind getValueKindForType(Type T) {
		return VK_RValue;
	}

	/// getValueKind - The value kind that this expression produces.
	ExprValueKind getValueKind() const {
		return static_cast<ExprValueKind> (ExprBits.ValueKind);
	}

	/// setValueKind - Set the value kind produced by this expression.
	void setValueKind(ExprValueKind Cat) {
		ExprBits.ValueKind = Cat;
	}

	/// \brief Returns whether this expression refers to a vector element.
	bool refersToVectorElement() const;

	/// isKnownToHaveBooleanValue - Return true if this is an integer expression
	/// that is known to return 0 or 1.  This happens for _Bool/bool expressions
	/// but also int expressions which are produced by things like comparisons in
	/// C.
	bool isKnownToHaveBooleanValue() const;

	/// isIntegerConstantExpr - Return true if this expression is a valid integer
	/// constant expression, and, if so, return its value in Result.  If not a
	/// valid i-c-e, return false and fill in Loc (if specified) with the location
	/// of the invalid expression.
	bool isIntegerConstantExpr(llvm::APSInt &Result, ASTContext &Ctx,
			SourceLocation *Loc = 0, bool isEvaluated = true) const;
	bool isIntegerConstantExpr(ASTContext &Ctx, SourceLocation *Loc = 0) const {
		llvm::APSInt X;
		return isIntegerConstantExpr(X, Ctx, Loc);
	}
	/// isConstantInitializer - Returns true if this expression is a constant
	/// initializer, which can be emitted at compile-time.
	bool isConstantInitializer(ASTContext &Ctx, bool ForRef) const;

	/// EvalResult is a struct with detailed info about an evaluated expression.
	struct EvalResult {
		/// Val - This is the value the expression can be folded to.
		APValue Val;

		/// HasSideEffects - Whether the evaluated expression has side effects.
		/// For example, (f() && 0) can be folded, but it still has side effects.
		bool HasSideEffects;

		/// Diag - If the expression is unfoldable, then Diag contains a note
		/// diagnostic indicating why it's not foldable. DiagLoc indicates a caret
		/// position for the error, and DiagExpr is the expression that caused
		/// the error.
		/// If the expression is foldable, but not an integer constant expression,
		/// Diag contains a note diagnostic that describes why it isn't an integer
		/// constant expression. If the expression *is* an integer constant
		/// expression, then Diag will be zero.
		unsigned Diag;
		const Expr *DiagExpr;
		SourceLocation DiagLoc;

		EvalResult() :
			HasSideEffects(false), Diag(0), DiagExpr(0) {
		}

		// isGlobalLValue - Return true if the evaluated lvalue expression
		// is global.
		bool isGlobalLValue() const;

		// isPersistentLValue - Return true if the evaluated lvalue expression
		// is persistent.
		bool isPersistentLValue() const;

		// hasSideEffects - Return true if the evaluated expression has
		// side effects.
		bool hasSideEffects() const {
			return HasSideEffects;
		}
	};

	/// Evaluate - Return true if this is a constant which we can fold using
	/// any crazy technique (that has nothing to do with language standards) that
	/// we want to.  If this function returns true, it returns the folded constant
	/// in Result.
	bool Evaluate(EvalResult &Result, ASTContext &Ctx) const;

	/// EvaluateAsBooleanCondition - Return true if this is a constant
	/// which we we can fold and convert to a boolean condition using
	/// any crazy technique that we want to.
	bool EvaluateAsBooleanCondition(bool &Result, ASTContext &Ctx) const;

	/// isEvaluatable - Call Evaluate to see if this expression can be constant
	/// folded, but discard the result.
	bool isEvaluatable(ASTContext &Ctx) const;

	/// HasSideEffects - This routine returns true for all those expressions
	/// which must be evaluated each time and must not be optimized away
	/// or evaluated at compile time. Example is a function call, volatile
	/// variable read.
	bool HasSideEffects(ASTContext &Ctx) const;

	/// EvaluateAsInt - Call Evaluate and return the folded integer. This
	/// must be called on an expression that constant folds to an integer.
	llvm::APSInt EvaluateAsInt(ASTContext &Ctx) const;

	/// EvaluateAsLValue - Evaluate an expression to see if it's a lvalue
	/// with link time known address.
	bool EvaluateAsLValue(EvalResult &Result, ASTContext &Ctx) const;

	/// EvaluateAsLValue - Evaluate an expression to see if it's a lvalue.
	bool EvaluateAsAnyLValue(EvalResult &Result, ASTContext &Ctx) const;

	/// IgnoreParens - Ignore parentheses.  If this Expr is a ParenExpr, return
	///  its subexpression.  If that subexpression is also a ParenExpr,
	///  then this method recursively returns its subexpression, and so forth.
	///  Otherwise, the method returns the current Expr.
	Expr *IgnoreParens();

	/// IgnoreParenCasts - Ignore parentheses and casts.  Strip off any ParenExpr
	/// or CastExprs, returning their operand.
	Expr *IgnoreParenCasts();

	/// IgnoreParenImpCasts - Ignore parentheses and implicit casts.  Strip off any
	/// ParenExpr or ImplicitCastExprs, returning their operand.
	Expr *IgnoreParenImpCasts();

	const Expr *IgnoreParenImpCasts() const {
		return const_cast<Expr*> (this)->IgnoreParenImpCasts();
	}

	/// IgnoreParenNoopCasts - Ignore parentheses and casts that do not change the
	/// value (including ptr->int casts of the same size).  Strip off any
	/// ParenExpr or CastExprs, returning their operand.
	Expr *IgnoreParenNoopCasts(ASTContext &Ctx);

	const Expr *IgnoreParens() const {
		return const_cast<Expr*> (this)->IgnoreParens();
	}
	const Expr *IgnoreParenCasts() const {
		return const_cast<Expr*> (this)->IgnoreParenCasts();
	}
	const Expr *IgnoreParenNoopCasts(ASTContext &Ctx) const {
		return const_cast<Expr*> (this)->IgnoreParenNoopCasts(Ctx);
	}

	static bool hasAnyTypeDependentArguments(Expr** Exprs, unsigned NumExprs);
	static bool hasAnyValueDependentArguments(Expr** Exprs, unsigned NumExprs);

	static bool classof(const Stmt *T) {
		return T->getStmtClass() >= firstExprConstant && T->getStmtClass()
				<= lastExprConstant;
	}
	static bool classof(const Expr *) {
		return true;
	}

	virtual bool hasMagicEnd(void) const {
		return false;
	}

	bool isPostfixIndexed(void) const {
		return PostfixIndexed;
	}
	void setPostfixIndexed(bool pindex) {
		PostfixIndexed = pindex;
	}

	bool PrintResultNeeded(void) const {
		return PrintFlag;
	}
	void setPrintFlag(bool print) {
		PrintFlag = print;
	}
};

/// \brief Represents the qualifier that may precede a name, e.g., the
/// "space.classname." in "space.classname.methodname".
struct NameQualifier {
  /// \brief The nested name specifier.
  NestedNameSpecifier *NNS;

  /// \brief The source range covered by the nested name specifier.
  SourceRange Range;
};

} // end namespace mlang

#endif /* MLANG_AST_EXPRESSION_H_ */
