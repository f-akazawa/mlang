//===--- Sema.h - Semantic Analysis & AST Building ---------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the Sema class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SEMA_SEMA_H_
#define MLANG_SEMA_SEMA_H_

#include "mlang/Sema/AnalysisBasedWarnings.h"
#include "mlang/Sema/IdentifierResolver.h"
#include "mlang/Sema/Ownership.h"
#include "mlang/AST/OperationKinds.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/ExternalASTSource.h"
#include "mlang/AST/Type.h"
#include "mlang/Basic/Specifiers.h"
#include "mlang/Diag/PartialDiagnostic.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include <deque>
#include <string>

namespace llvm {
class APSInt;
template<typename ValueT> struct DenseMapInfo;
template<typename ValueT, typename ValueInfoT> class DenseSet;
}

namespace mlang {
class ASTConsumer;
class ASTContext;
class ArrayType;
// class AttributeList;
class ClassMethodDefn;
class DefinitionNameInfo;
class Defn;
class DefnAccessPair;
class DefnContext;
class DefnGroupRef;
class DefnRefExpr;
class Expr;
class VectorType;
class ExternalSemaSource;
class FunctionCall;
class FunctionDefn;
class FunctionProtoType;
class IdentifierInfo;
class IntegerLiteral;
class LangOptions;
// class LocalInstantiationScope;
class LookupResult;
class MemberDefn;
class NamedDefn;
//class NonNullAttr;
class ParenListExpr;
class ParamVarDefn;
class Preprocessor;
class ScriptDefn;
class Stmt;
class StringLiteral;
class SwitchCmd;
// class TargetAttributesSema;
class Type;
class Token;
class TryCmd;
class UserClassDefn;
class ValueDefn;
class VarDefn;
// class VisibilityAttr;
class VisibleDefnConsumer;

namespace sema {
// class AccessedEntity;
class BlockScopeInfo;
class FunctionScopeInfo;
}

/// Sema - This implements semantic analysis and AST building for Mlang.
class Sema {
	Sema(const Sema&); // DO NOT IMPLEMENT
	void operator=(const Sema&); // DO NOT IMPLEMENT

public:
	typedef OpaquePtr<DefnGroupRef> DefnGroupPtrTy;
	typedef OpaquePtr<Type> TypeTy; //is this needed and if yes where?
	typedef Expr ExprTy; //is this needed and if yes where?
	typedef Stmt StmtTy; //is this needed and if yes where?

	const LangOptions &LangOpts;
	Preprocessor &PP;
	ASTContext &Context;
	ASTConsumer &Consumer;
	Diagnostic &Diags;
	SourceManager &SourceMgr;

	/// \brief Source of additional semantic information.
	ExternalSemaSource *ExternalSource;

	/// CurContext - This is the current definition context of parsing.
	DefnContext *CurContext;

	/// \brief Stack containing information about each of the nested
	/// function, block, and method scopes that are currently active.
	///
	/// This array is never empty. Clients should ignore the first
	/// element, which is used to cache a single FunctionScopeInfo
	/// that's used to parse every top-level function.
	llvm::SmallVector<sema::FunctionScopeInfo *, 4> FunctionScopes;

	/// PureVirtualClassDiagSet - a set of class declarations which we have
	/// emitted a list of pure virtual functions. Used to prevent emitting the
	/// same list more than once.
	typedef llvm::SmallPtrSet<const UserClassDefn*, 8> RecordDefnSetTy;
	llvm::OwningPtr<RecordDefnSetTy> PureVirtualClassDiagSet;

	/// \brief A mapping from external names to the most recent
	/// locally-scoped external declaration with that name.
	///
	/// This map contains external declarations introduced in local
	/// scoped, e.g.,
	///
	/// \code
	/// void f() {
	///   void foo(int, int);
	/// }
	/// \endcode
	///
	/// Here, the name "foo" will be associated with the declaration on
	/// "foo" within f. This name is not visible outside of
	/// "f". However, we still find it in two cases:
	///
	///   - If we are declaring another external with the name "foo", we
	///     can find "foo" as a previous declaration, so that the types
	///     of this external declaration can be checked for
	///     compatibility.
	///
	///   - If we would implicitly declare "foo" (e.g., due to a call to
	///     "foo" in C when no prototype or definition is visible), then
	///     we find this declaration of "foo" and complain that it is
	///     not visible.
	llvm::DenseMap<DefinitionName, NamedDefn *> LocallyScopedExternalDefns;

	/// \brief All the tentative definitions encountered in the TU.
	llvm::SmallVector<VarDefn *, 2> TentativeDefinitions;

	/// \brief The depth of the current ParsingDeclaration stack.
	/// If nonzero, we are currently parsing a declaration (and
	/// hence should delay deprecation warnings).
	unsigned ParsingDefnDepth;

	IdentifierResolver IdResolver;

	/// BaseWorkspace - Top-level scope for current base workspace.
	Scope *BaseWorkspace;

	/// \brief The "std" namespace, where the standard library resides.
	LazyDefnPtr StdNamespace;

	/// \brief The C++ "std::bad_alloc" class, which is defined by the C++
	/// standard library.
	LazyDefnPtr StdBadAlloc;

	/// \brief The set of declarations that have been referenced within
	/// a potentially evaluated expression.
	typedef llvm::SmallVector<std::pair<SourceLocation, Defn *>, 10>
			PotentiallyReferencedDefns;

	/// \brief A set of diagnostics that may be emitted.
	typedef llvm::SmallVector<std::pair<SourceLocation, PartialDiagnostic>, 10>
			PotentiallyEmittedDiagnostics;

	/// \brief Describes how the expressions currently being parsed are
	/// evaluated at run-time, if at all.
	enum ExpressionEvaluationContext {
		/// \brief The current expression and its subexpressions occur within an
		/// unevaluated operand (C++0x [expr]p8), such as a constant expression
		/// or the subexpression of \c sizeof, where the type or the value of the
		/// expression may be significant but no code will be generated to evaluate
		/// the value of the expression at run time.
		Unevaluated,

		/// \brief The current expression is potentially evaluated at run time,
		/// which means that code may be generated to evaluate the value of the
		/// expression at run time.
		PotentiallyEvaluated,

		/// \brief The current expression may be potentially evaluated or it may
		/// be unevaluated, but it is impossible to tell from the lexical context.
		/// This evaluation context is used primary for the operand of the C++
		/// \c typeid expression, whose argument is potentially evaluated only when
		/// it is an lvalue of polymorphic class type (C++ [basic.def.odr]p2).
		PotentiallyPotentiallyEvaluated,

		/// \brief The current expression is potentially evaluated, but any
		/// declarations referenced inside that expression are only used if
		/// in fact the current expression is used.
		///
		/// This value is used when parsing default function arguments, for which
		/// we would like to provide diagnostics (e.g., passing non-POD arguments
		/// through varargs) but do not want to mark declarations as "referenced"
		/// until the default argument is used.
		PotentiallyEvaluatedIfUsed
	};

	/// \brief Data structure used to record current or nested
	/// expression evaluation contexts.
	struct ExpressionEvaluationContextRecord {
		/// \brief The expression evaluation context.
		ExpressionEvaluationContext Context;

		/// \brief The number of temporaries that were active when we
		/// entered this expression evaluation context.
		unsigned NumTemporaries;

		/// \brief The set of declarations referenced within a
		/// potentially potentially-evaluated context.
		///
		/// When leaving a potentially potentially-evaluated context, each
		/// of these elements will be as referenced if the corresponding
		/// potentially potentially evaluated expression is potentially
		/// evaluated.
		PotentiallyReferencedDefns *PotentiallyReferenced;

		/// \brief The set of diagnostics to emit should this potentially
		/// potentially-evaluated context become evaluated.
		PotentiallyEmittedDiagnostics *PotentiallyDiagnosed;

		ExpressionEvaluationContextRecord(ExpressionEvaluationContext Context,
				unsigned NumTemporaries) :
			Context(Context), NumTemporaries(NumTemporaries),
					PotentiallyReferenced(0), PotentiallyDiagnosed(0) {
		}

		void addReferencedDefn(SourceLocation Loc, Defn *Defn) {
			if (!PotentiallyReferenced)
				PotentiallyReferenced = new PotentiallyReferencedDefns;
			PotentiallyReferenced->push_back(std::make_pair(Loc, Defn));
		}

		void addDiagnostic(SourceLocation Loc, const PartialDiagnostic &PD) {
			if (!PotentiallyDiagnosed)
				PotentiallyDiagnosed = new PotentiallyEmittedDiagnostics;
			PotentiallyDiagnosed->push_back(std::make_pair(Loc, PD));
		}

		void Destroy() {
			delete PotentiallyReferenced;
			delete PotentiallyDiagnosed;
			PotentiallyReferenced = 0;
			PotentiallyDiagnosed = 0;
		}
	};

	/// A stack of expression evaluation contexts.
	llvm::SmallVector<ExpressionEvaluationContextRecord, 8> ExprEvalContexts;

	/// \brief Whether the code handled by Sema should be considered a
	/// complete translation unit or not.
	///
	/// When true (which is generally the case), Sema will perform
	/// end-of-translation-unit semantic tasks (such as creating
	/// initializers for tentative definitions in C) once parsing has
	/// completed. This flag will be false when building PCH files,
	/// since a PCH file is by definition not a complete translation
	/// unit.
	bool CompleteTranslationUnit;

	llvm::BumpPtrAllocator BumpAlloc;

	/// \brief The number of SFINAE diagnostics that have been trapped.
	unsigned NumSFINAEErrors;

	/// \brief Worker object for performing CFG-based warnings.
	// sema::AnalysisBasedWarnings AnalysisWarnings;

	/// Private Helper predicate to check for 'self'.
	bool isSelfExpr(Expr *RExpr);

public:
	Sema(Preprocessor &pp, ASTContext &ctxt, ASTConsumer &consumer,
			 bool CompleteTranslationUnit = true);
	~Sema();

	/// \brief Perform initialization that occurs after the parser has been
	/// initialized but before it parses anything.
	void Initialize();

	const LangOptions &getLangOptions() const {
		return LangOpts;
	}
	Diagnostic &getDiagnostics() const {
		return Diags;
	}
	SourceManager &getSourceManager() const {
		return SourceMgr;
	}

	// const TargetAttributesSema &getTargetAttributesSema() const;

	Preprocessor &getPreprocessor() const {
		return PP;
	}
	ASTContext &getASTContext() const {
		return Context;
	}
	ASTConsumer &getASTConsumer() const {
		return Consumer;
	}

	/// \brief Helper class that creates diagnostics with optional
	/// template instantiation stacks.
	///
	/// This class provides a wrapper around the basic DiagnosticBuilder
	/// class that emits diagnostics. SemaDiagnosticBuilder is
	/// responsible for emitting the diagnostic (as DiagnosticBuilder
	/// does) and, if the diagnostic comes from inside a template
	/// instantiation, printing the template instantiation stack as
	/// well.
	class SemaDiagnosticBuilder: public DiagnosticBuilder {
		Sema &SemaRef;
		unsigned DiagID;

	public:
		SemaDiagnosticBuilder(DiagnosticBuilder &DB, Sema &SemaRef,
				unsigned DiagID) :
			DiagnosticBuilder(DB), SemaRef(SemaRef), DiagID(DiagID) {
		}

		explicit SemaDiagnosticBuilder(Sema &SemaRef) :
			DiagnosticBuilder(DiagnosticBuilder::Suppress), SemaRef(SemaRef) {
		}

		~SemaDiagnosticBuilder();
	};

	/// \brief Emit a diagnostic.
	SemaDiagnosticBuilder Diag(SourceLocation Loc, unsigned DiagID);

	/// \brief Emit a partial diagnostic.
	SemaDiagnosticBuilder Diag(SourceLocation Loc, const PartialDiagnostic& PD);

	/// \brief Build a partial diagnostic.
	PartialDiagnostic PDiag(unsigned DiagID = 0);

	ExprResult Owned(Expr* E) {
		return E;
	}
	ExprResult Owned(ExprResult R) {
		return R;
	}
	StmtResult Owned(Cmd* C) {
		return C;
	}
	StmtResult Owned(Stmt* S) {
		return S;
	}

	void ActOnEndOfTranslationUnit();

	Scope *getScopeForContext(DefnContext *Ctx);

	void PushFunctionScope();
	void PushBlockScope(Scope *BlockScope, ScriptDefn *Block);
	void PopFunctionOrBlockScope();

	sema::FunctionScopeInfo *getCurFunction() const {
		return FunctionScopes.back();
	}

	bool hasAnyErrorsInThisFunction() const;

	/// \brief Retrieve the current block, if any.
	sema::BlockScopeInfo *getCurBlock();

	//===--------------------------------------------------------------------===//
	// Type Analysis / Processing: SemaType.cpp.
	//===--------------------------------------------------------------------===//
	Type adjustParameterType(Type T);
	Type BuildQualifiedType(Type T, SourceLocation Loc, TypeInfo Qs);
	Type BuildQualifiedType(Type T, SourceLocation Loc, unsigned CVR) {
		return BuildQualifiedType(T, Loc, TypeInfo::fromFastMask(CVR));
	}
	Type BuildPointerType(Type T, SourceLocation Loc, DefinitionName Entity);
	Type BuildReferenceType(Type T, bool LValueRef, SourceLocation Loc,
			DefinitionName Entity);
	Type BuildArrayType(Type T, Expr *ArraySize, unsigned Quals,
			SourceRange Brackets, DefinitionName Entity);
	Type BuildVectorType(Type T, Expr *VectorSize, SourceLocation AttrLoc);
	Type BuildFunctionType(Type T, Type *ParamTypes, unsigned NumParamTypes,
			bool Variadic, unsigned Quals, SourceLocation Loc,
			DefinitionName Entity);

	TypeResult ActOnTypeName(Scope *S, Type T);

	bool RequireCompleteType(SourceLocation Loc, Type T,
			const PartialDiagnostic &PD, std::pair<SourceLocation,
					PartialDiagnostic> Note);
	bool RequireCompleteType(SourceLocation Loc, Type T,
			const PartialDiagnostic &PD);
	bool RequireCompleteType(SourceLocation Loc, Type T, unsigned DiagID);

	Type BuildTypeofExprType(Expr *E);
	Type BuildDefntypeType(Expr *E);

	// AssignmentAction - This is used by all the assignment diagnostic functions
	// to represent what is actually causing the operation
	enum AssignmentAction {
		AA_Assigning,
		AA_Passing,
		AA_Returning,
		AA_Converting,
		AA_Initializing,
		AA_Sending,
		AA_Casting
	};

	//  bool TryImplicitConversion(InitializationSequence &Sequence,
	//                             const InitializedEntity &Entity,
	//                             Expr *From,
	//                             bool SuppressUserConversions,
	//                             bool AllowExplicit,
	//                             bool InOverloadResolution);

	bool IsIntegralPromotion(Expr *From, Type FromType, Type ToType);
	bool IsFloatingPointPromotion(Type FromType, Type ToType);
	bool IsComplexPromotion(Type FromType, Type ToType);
	bool FunctionArgTypesAreEqual(FunctionProtoType* OldType,
			FunctionProtoType* NewType);

	bool IsQualificationConversion(Type FromType, Type ToType);
	bool DiagnoseMultipleUserDefinedConversion(Expr *From, Type ToType);

	bool PerformContextuallyConvertToBool(Expr *&From);

	Expr *
	ConvertToIntegralOrEnumerationType(SourceLocation Loc, Expr *FromE,
			const PartialDiagnostic &NotIntDiag,
			const PartialDiagnostic &IncompleteDiag,
			const PartialDiagnostic &ExplicitConvDiag,
			const PartialDiagnostic &ExplicitConvNote,
			const PartialDiagnostic &AmbigDiag,
			const PartialDiagnostic &AmbigNote,
			const PartialDiagnostic &ConvDiag);

	bool PerformObjectMemberConversion(Expr *&From, NamedDefn *FoundDefn,
			NamedDefn *Member);

	/// CastCategory - Get the correct forwarded implicit cast result category
	/// from the inner expression.
	ExprValueKind CastCategory(Expr *E);

	/// ImpCastExprToType - If Expr is not of type 'Type', insert an implicit
	/// cast.  If there is already an implicit cast, merge into the existing one.
	/// If isLvalue, the result of the cast is an lvalue.
	void ImpCastExprToType(Expr *&Expr, Type Type, CastKind CK,
			ExprValueKind VK = VK_RValue);

	/// IgnoredValueConversions - Given that an expression's result is
	/// syntactically ignored, perform any conversions that are
	/// required.
	void IgnoredValueConversions(Expr *&expr);

	// UsualUnaryConversions - promotes integers (C99 6.3.1.1p2) and converts
	// functions and arrays to their respective pointers (C99 6.3.2.1).
	Expr *UsualUnaryConversions(Expr *&expr);

	// DefaultFunctionArrayConversion - converts functions and arrays
	// to their respective pointers (C99 6.3.2.1).
	void DefaultFunctionArrayConversion(Expr *&expr);

	// DefaultFunctionArrayLvalueConversion - converts functions and
	// arrays to their respective pointers and performs the
	// lvalue-to-rvalue conversion.
	void DefaultFunctionArrayLvalueConversion(Expr *&expr);

	// DefaultArgumentPromotion (C99 6.5.2.2p6). Used for function calls that
	// do not have a prototype. Integer promotions are performed on each
	// argument, and arguments that have type float are promoted to double.
	void DefaultArgumentPromotion(Expr *&Expr);

	// Used for emitting the right warning by DefaultVariadicArgumentPromotion
	enum VariadicCallType {
		VariadicFunction,
		VariadicBlock,
		VariadicMethod,
		VariadicConstructor,
		VariadicDoesNotApply
	};

	/// GatherArgumentsForCall - Collector argument expressions for various
	/// form of call prototypes.
	bool GatherArgumentsForCall(SourceLocation CallLoc, FunctionDefn *FDefn,
			const FunctionProtoType *Proto, unsigned FirstProtoArg,
			Expr **Args, unsigned NumArgs,
			llvm::SmallVector<Expr *, 8> &AllArgs, VariadicCallType CallType =
					VariadicDoesNotApply);

	// DefaultVariadicArgumentPromotion - Like DefaultArgumentPromotion, but
	// will warn if the resulting type is not a POD type.
	bool DefaultVariadicArgumentPromotion(Expr *&Expr, VariadicCallType CT,
			FunctionDefn *FDefn);

	// UsualArithmeticConversions - performs the UsualUnaryConversions on it's
	// operands and then handles various conversions that are common to binary
	// operators (C99 6.3.1.8). If both operands aren't arithmetic, this
	// routine returns the first non-arithmetic type found. The client is
	// responsible for emitting appropriate error diagnostics.
	Type UsualArithmeticConversions(Expr *&lExpr, Expr *&rExpr,
			bool isCompAssign = false);

	/// AssignConvertType - All of the 'assignment' semantic checks return this
	/// enum to indicate whether the assignment was allowed.  These checks are
	/// done for simple assignments, as well as initialization, return from
	/// function, argument passing, etc.  The query is phrased in terms of a
	/// source and destination type.
	enum AssignConvertType {
		/// Compatible - the types are compatible according to the standard.
		Compatible,

		/// PointerToInt - The assignment converts a pointer to an int, which we
		/// accept as an extension.
		PointerToInt,

		/// IntToPointer - The assignment converts an int to a pointer, which we
		/// accept as an extension.
		IntToPointer,

		/// FunctionVoidPointer - The assignment is between a function pointer and
		/// void*, which the standard doesn't allow, but we accept as an extension.
		FunctionVoidPointer,

		/// IncompatiblePointer - The assignment is between two pointers types that
		/// are not compatible, but we accept them as an extension.
		IncompatiblePointer,

		/// IncompatiblePointer - The assignment is between two pointers types which
		/// point to integers which have a different sign, but are otherwise identical.
		/// This is a subset of the above, but broken out because it's by far the most
		/// common case of incompatible pointers.
		IncompatiblePointerSign,

		/// CompatiblePointerDiscardsQualifiers - The assignment discards
		/// c/v/r qualifiers, which we accept as an extension.
		CompatiblePointerDiscardsQualifiers,

		/// IncompatibleNestedPointerQualifiers - The assignment is between two
		/// nested pointer types, and the qualifiers other than the first two
		/// levels differ e.g. char ** -> const char **, but we accept them as an
		/// extension.
		IncompatibleNestedPointerQualifiers,

		/// IncompatibleVectors - The assignment is between two vector types that
		/// have the same size, which we accept as an extension.
		IncompatibleVectors,

		/// IntToBlockPointer - The assignment converts an int to a block
		/// pointer. We disallow this.
		IntToBlockPointer,

		/// IncompatibleBlockPointer - The assignment is between two block
		/// pointers types that are not compatible.
		IncompatibleBlockPointer,

		/// IncompatibleObjCQualifiedId - The assignment is between a qualified
		/// id type and something else (that is incompatible with it). For example,
		/// "id <XXX>" = "Foo *", where "Foo *" doesn't implement the XXX protocol.
		IncompatibleObjCQualifiedId,

		/// Incompatible - We reject this conversion outright, it is invalid to
		/// represent it in the AST.
		Incompatible
	};

	/// DiagnoseAssignmentResult - Emit a diagnostic, if required, for the
	/// assignment conversion type specified by ConvTy.  This returns true if the
	/// conversion was invalid or false if the conversion was accepted.
	bool DiagnoseAssignmentResult(AssignConvertType ConvTy, SourceLocation Loc,
			Type DstType, Type SrcType, Expr *SrcExpr, AssignmentAction Action,
			bool *Complained = 0);

	/// CheckAssignmentConstraints - Perform type checking for assignment,
	/// argument passing, variable initialization, and function return values.
	/// This routine is only used by the following two methods. C99 6.5.16.
	AssignConvertType CheckAssignmentConstraints(Type lhs, Type rhs);

	// CheckSingleAssignmentConstraints - Currently used by
	// CheckAssignmentOperands, and ActOnReturnStmt. Prior to type checking,
	// this routine performs the default function/array converions.
	AssignConvertType CheckSingleAssignmentConstraints(Type lhs, Expr *&rExpr);

	// \brief If the lhs type is a transparent union, check whether we
	// can initialize the transparent union with the given expression.
	AssignConvertType CheckTransparentUnionArgumentConstraints(Type lhs,
			Expr *&rExpr);

	// Helper function for CheckAssignmentConstraints (C99 6.5.16.1p1)
	AssignConvertType
	CheckPointerTypesForAssignment(Type lhsType, Type rhsType);

	AssignConvertType CheckObjCPointerTypesForAssignment(Type lhsType,
			Type rhsType);

	// Helper function for CheckAssignmentConstraints involving two
	// block pointer types.
	AssignConvertType CheckBlockPointerTypesForAssignment(Type lhsType,
			Type rhsType);

	bool IsStringLiteralToNonConstPointerConversion(Expr *From, Type ToType);

	bool CheckExceptionSpecCompatibility(Expr *From, Type ToType);

	bool PerformImplicitConversion(Expr *&From, Type ToType,
			AssignmentAction Action, bool AllowExplicit = false);
	//  bool PerformImplicitConversion(Expr *&From, Type ToType,
	//                                 AssignmentAction Action,
	//                                 bool AllowExplicit,
	//                                 ImplicitConversionSequence& ICS);
	//  bool PerformImplicitConversion(Expr *&From, Type ToType,
	//                                 const ImplicitConversionSequence& ICS,
	//                                 AssignmentAction Action,
	//                                 bool IgnoreBaseAccess = false);
	//  bool PerformImplicitConversion(Expr *&From, Type ToType,
	//                                 const StandardConversionSequence& SCS,
	//                                 AssignmentAction Action,bool IgnoreBaseAccess);

	/// the following "Check" methods will return a valid/converted Type
	/// or a null Type (indicating an error diagnostic was issued).

	/// type checking binary operators (subroutines of CreateBuiltinBinOp).
	Type InvalidOperands(SourceLocation l, ExprResult &lex, ExprResult &rex);
	Type CheckPowerOperands(ExprResult &lex, ExprResult &rex,
			SourceLocation OpLoc, bool isIndirect);
	Type CheckMultiplyDivideOperands(ExprResult &lex, ExprResult &rex,
			SourceLocation OpLoc, bool isCompAssign, bool isDivide);
	Type CheckAdditionOperands(ExprResult &lex, ExprResult &rex,
			SourceLocation OpLoc, Type* CompLHSTy = 0);
	Type CheckSubtractionOperands(ExprResult &lex, ExprResult &rex,
			SourceLocation OpLoc, Type* CompLHSTy = 0);
	Type CheckShiftOperands(ExprResult &lex, ExprResult &rex,
			SourceLocation OpLoc, unsigned Opc,	bool isCompAssign =	false);
	Type CheckCompareOperands(ExprResult &lex, ExprResult &rex,
			SourceLocation OpLoc, unsigned Opc,	bool isRelational);
	Type CheckBitwiseOperands(ExprResult &lex, ExprResult &rex,
			SourceLocation OpLoc, bool isCompAssign =	false);
	Type CheckLogicalOperands(ExprResult &lex, ExprResult &rex,
			SourceLocation OpLoc, unsigned Opc);

	// CheckAssignmentOperands is used for both simple and compound assignment.
	// For simple assignment, pass both expressions and a null converted type.
	// For compound assignment, pass both expressions and the converted type.
	Type CheckAssignmentOperands(Expr *lex, ExprResult &rex,
			SourceLocation OpLoc, Type convertedType);

	Type CheckColonOperands(ExprResult &lex, ExprResult &rex,
			SourceLocation OpLoc);

	void ConvertPropertyAssignment(Expr *LHS, Expr *&RHS, Type& LHSTy);

	Type FindCompositePointerType(SourceLocation Loc, Expr *&E1, Expr *&E2,
			bool *NonStandardCompositeType = 0);

	/// type checking for vector binary operators.
	Type CheckVectorOperands(SourceLocation l, Expr *&lex, Expr *&rex);
	Type CheckVectorCompareOperands(Expr *&lex, Expr *&rx, SourceLocation l,
			bool isRel);

	/// type checking unary operators (subroutines of ActOnUnaryOp).
	Type CheckIncrementDecrementOperand(Expr *op, SourceLocation OpLoc,
			bool isInc, bool isPrefix);
	Type CheckAddressOfOperand(Expr *op, SourceLocation OpLoc);

	/// type checking primary expressions.
	Type CheckExtVectorComponent(Type baseType, SourceLocation OpLoc,
			const IdentifierInfo *Comp, SourceLocation CmpLoc);

	/// type checking declaration initializers
	//  bool CheckInitList(const InitializedEntity &Entity,
	//                     InitListExpr *&InitList, Type &DefnType);
	bool CheckForConstantInitializer(Expr *e, Type t);

	// type checking C++ declaration initializers (C++ [dcl.init]).

	/// ReferenceCompareResult - Expresses the result of comparing two
	/// types (cv1 T1 and cv2 T2) to determine their compatibility for the
	/// purposes of initialization by reference (C++ [dcl.init.ref]p4).
	enum ReferenceCompareResult {
		/// Ref_Incompatible - The two types are incompatible, so direct
		/// reference binding is not possible.
		Ref_Incompatible = 0,
		/// Ref_Related - The two types are reference-related, which means
		/// that their unqualified forms (T1 and T2) are either the same
		/// or T1 is a base class of T2.
		Ref_Related,
		/// Ref_Compatible_With_Added_Qualification - The two types are
		/// reference-compatible with added qualification, meaning that
		/// they are reference-compatible and the qualifiers on T1 (cv1)
		/// are greater than the qualifiers on T2 (cv2).
		Ref_Compatible_With_Added_Qualification,
		/// Ref_Compatible - The two types are reference-compatible and
		/// have equivalent qualifiers (cv1 == cv2).
		Ref_Compatible
	};

	ReferenceCompareResult CompareReferenceRelationship(SourceLocation Loc,
			Type T1, Type T2, bool &DerivedToBase, bool &ObjCConversion);

	/// ConvertIntegerToTypeWarnOnOverflow - Convert the specified APInt to have
	/// the specified width and sign.  If an overflow occurs, detect it and emit
	/// the specified diagnostic.
	void ConvertIntegerToTypeWarnOnOverflow(llvm::APSInt &OldVal,
			unsigned NewWidth, bool NewSign, SourceLocation Loc,
			unsigned DiagID);

	void InitBuiltinVaListType();

	//===--------------------------------------------------------------------===//
	// Name lookup: SemaLookup.cpp.
	//===--------------------------------------------------------------------===//

	/// \name Name lookup
	///
	/// These routines provide name lookup that is used during semantic
	/// analysis to resolve the various kinds of names (identifiers,
	/// overloaded operator names, constructor names, etc.) into zero or
	/// more declarations within a particular scope. The major entry
	/// points are LookupName, which performs unqualified name lookup,
	/// and LookupQualifiedName, which performs qualified name lookup.
	///
	/// All name lookup is performed based on some specific criteria,
	/// which specify what names will be visible to name lookup and how
	/// far name lookup should work. These criteria are important both
	/// for capturing language semantics (certain lookups will ignore
	/// certain names, for example) and for performance, since name
	/// lookup is often a bottleneck in the compilation of OOP. Name
	/// lookup criteria is specified via the LookupCriteria enumeration.
	///
	/// The results of name lookup can vary based on the kind of name
	/// lookup performed, the current language feature, and the translation
	/// unit. In non-OOP, for example, name lookup will either return
	/// nothing (no entity found) or a single declaration. In OOP, name
	/// lookup can additionally refer to a set of overloaded functions or
	/// result in an ambiguity. All of the possible results of name
	/// lookup are captured by the LookupResult class, which provides
	/// the ability to distinguish among them.
	//@{

	/// @brief Describes the kind of name lookup to perform.
	enum LookupNameKind {
		/// Ordinary name lookup, which finds ordinary names (functions,
		/// variables, types, etc.).
		LookupOrdinaryName = 0,

		/// Type name lookup, which finds the names of classes and structs.
		LookupTypeName,

		/// Member name lookup, which finds the names of class/struct
		/// members.
		LookupMemberName,

		/// Look up of a name that precedes the '.' scope resolution
		/// operator.
		LookupNestedNameSpecifierName,

		/// Look up a namespace name within an import directive.
		LookupExternalPackageName,

		/// \brief Look up any declaration with any name.
		LookupAnyName
	};

	/// \brief Specifies whether (or how) name lookup is being performed for a
	/// redefinition (vs. a reference).
	enum RedefinitionKind {
		/// \brief The lookup is a reference to this name that is not for the
		/// purpose of redefining the name.
		NotForRedefinition = 0,
		/// \brief The lookup results will be used for redefinition of a name,
		/// if an entity by that name already exists.
		ForRedefinition
	};

private:
	bool OOPLookupName(LookupResult &R, Scope *S);

public:
	/// \brief Look up a name, looking for a single definition.  Return
	/// null if the results were absent, ambiguous, or overloaded.
	///
	/// It is preferable to use the elaborated form and explicitly handle
	/// ambiguity and overloaded.
	NamedDefn *LookupSingleName(Scope *S, DefinitionName Name,
			SourceLocation Loc, LookupNameKind NameKind,
			RedefinitionKind Redefn = NotForRedefinition);
	bool LookupName(LookupResult &R, Scope *S, bool AllowBuiltinCreation =
			false);

	bool LookupQualifiedName(LookupResult &R, DefnContext *LookupCtx,
			bool InUnqualifiedLookup = false);
	bool LookupParsedName(LookupResult &R, Scope *S, bool AllowBuiltinCreation =
			false, bool EnteringContext = false);

	void LookupVisibleDefns(Scope *S, LookupNameKind Kind,
			VisibleDefnConsumer &Consumer, bool IncludeGlobalScope = true);
	void LookupVisibleDefns(DefnContext *Ctx, LookupNameKind Kind,
			VisibleDefnConsumer &Consumer, bool IncludeGlobalScope = true);

	/// \brief The context in which typo-correction occurs.
	///
	/// The typo-correction context affects which keywords (if any) are
	/// considered when trying to correct for typos.
	enum CorrectTypoContext {
		/// \brief An unknown context, where any keyword might be valid.
		CTC_Unknown,
		/// \brief A context where no keywords are used (e.g. we expect an actual
		/// name).
		CTC_NoKeywords,
		/// \brief A context where we're correcting a type name.
		CTC_Type,
		/// \brief An expression context.
		CTC_Expression,
		/// \brief A member lookup context.
		CTC_MemberLookup
	};

	DefinitionName CorrectTypo(LookupResult &R, Scope *S,
			DefnContext *MemberContext = 0, bool EnteringContext = false,
			CorrectTypoContext CTC = CTC_Unknown);

	// Members have to be NamespaceDefn* or TranslationUnitDefn*.
	// TODO: make this is a typesafe union.
	typedef llvm::SmallPtrSet<DefnContext *, 16> AssociatedNamespaceSet;
	typedef llvm::SmallPtrSet<UserClassDefn *, 16> AssociatedClassSet;
	void FindAssociatedClassesAndNamespaces(Expr **Args, unsigned NumArgs,
			AssociatedNamespaceSet &AssociatedNamespaces,
			AssociatedClassSet &AssociatedClasses);

	bool DiagnoseAmbiguousLookup(LookupResult &Result);
	//@}

	//===--------------------------------------------------------------------===//
	// Symbol table / Defn tracking callbacks: SemaDefn.cpp.
	//===--------------------------------------------------------------------===//
	DefnGroupPtrTy ConvertDefnToDefnGroup(Defn *Ptr);

	ParsedType getTypeName(IdentifierInfo &II, SourceLocation NameLoc,
			Scope *S, bool isClassName = false, ParsedType ObjectType =
					ParsedType());

	// TypeSpecifierType isTagName(IdentifierInfo &II, Scope *S);
	bool DiagnoseUnknownTypeName(const IdentifierInfo &II,
			SourceLocation IILoc, Scope *S, ParsedType &SuggestedType);

	void CheckShadow(Scope *S, VarDefn *D, const LookupResult& R);
	void CheckShadow(Scope *S, VarDefn *D);
	void CheckCastAlign(Expr *Op, Type T, SourceRange TRange);

	void CheckVariableDeclaration(VarDefn *NewVD, LookupResult &Previous,
			bool &Redefinition);
	NamedDefn* ActOnFunctionDeclarator(Scope* S, DefnContext* DC, Type R,
			LookupResult &Previous);
	void CheckFunctionDeclaration(Scope *S, FunctionDefn *NewFD,
			LookupResult &Previous, bool IsExplicitSpecialization,
			bool &Redefinition, bool &OverloadableAttrRequired);
	void CheckMain(FunctionDefn *FD);

	ParamVarDefn *BuildParmVarDefnForTypedef(DefnContext *DC,
			SourceLocation Loc, Type T);
	ParamVarDefn *CheckParameter(DefnContext *DC, Type T, IdentifierInfo *Name,
			SourceLocation NameLoc, StorageClass SC, StorageClass SCAsWritten);
	void ActOnParamDefaultArgument(Defn *param, SourceLocation EqualLoc,
			Expr *defarg);
	void ActOnParamUnparsedDefaultArgument(Defn *param,
			SourceLocation EqualLoc, SourceLocation ArgLoc);
	void ActOnParamDefaultArgumentError(Defn *param);
	bool SetParamDefaultArgument(ParamVarDefn *Param, Expr *DefaultArg,
			SourceLocation EqualLoc);

	// Contains the locations of the beginning of unparsed default
	// argument locations.
	llvm::DenseMap<ParamVarDefn *, SourceLocation> UnparsedDefaultArgLocs;

	void AddInitializerToDefn(Defn *dcl, Expr *init);
	void AddInitializerToDefn(Defn *dcl, Expr *init, bool DirectInit);
	void ActOnUninitializedDefn(Defn *dcl, bool TypeContainsUndeducedAuto);
	void ActOnInitializerError(Defn *Dcl);
	void SetDefnDeleted(Defn *dcl, SourceLocation DelLoc);
	//TODO yabin DefnGroupPtrTy FinalizeDeclaratorGroup(Scope *S, const DefnSpec &DS,
	//                                       Defn **Group,
	//                                       unsigned NumDecls);

	Defn *ActOnStartOfScriptDef(Scope *S, Defn *D = 0);
	Defn *ActOnFinishScriptBody(Defn *Defn, Stmt *Body);
	Defn *ActOnStartOfFunctionDef(Scope *S, Defn *D);
	Defn *ActOnFinishFunctionBody(Defn *Defn, Stmt *Body);
	Defn *ActOnFinishFunctionBody(Defn *Defn, Stmt *Body, bool IsInstantiation);
	Defn *ActOnStartOfClassDef(Scope *S, Defn *D);
	Defn *ActOnFinishClassBody(Defn *Defn, Stmt *Body);

	/// \brief Diagnose any unused parameters in the given sequence of
	/// ParmVarDefn pointers.
	void DiagnoseUnusedParameters(ParamVarDefn * const *Begin,
			ParamVarDefn * const *End);

	void DiagnoseInvalidJumps(Stmt *Body);
	Defn *ActOnFileScopeAsmDefn(SourceLocation Loc, Expr *expr);

	/// Scope actions.
	void ActOnPopScope(SourceLocation Loc, Scope *S);
	void ActOnTranslationUnitScope(Scope *S);

	bool isAcceptableTagRedefinition(const UserClassDefn *Previous,
			SourceLocation NewTagLoc, const IdentifierInfo &Name);

	enum TagUseKind {
		TUK_Reference, // Reference to a tag:  'struct foo *X;'
		TUK_Declaration, // Fwd decl of a tag:   'struct foo;'
		TUK_Definition, // Definition of a tag: 'struct foo { int X; } Y;'
		TUK_Friend	// Friend declaration:  'friend struct foo;'
	};

	Defn *ActOnTag(Scope *S, unsigned TagSpec, TagUseKind TUK,
			SourceLocation KWLoc, IdentifierInfo *Name, SourceLocation NameLoc,
			/*AttributeList *Attr,*/ bool &OwnedDefn, bool &IsDependent,
			TypeResult UnderlyingType);

	void ActOnDefs(Scope *S, Defn *TagD, SourceLocation DefnStart,
			IdentifierInfo *ClassName, llvm::SmallVectorImpl<Defn *> &Defns);
	Defn *ActOnField(Scope *S, Defn *TagD, SourceLocation DefnStart,
			Expr *BitfieldWidth);

	MemberDefn *HandleField(Scope *S, UserClassDefn *TagD,
			SourceLocation DefnStart, Expr *BitfieldWidth);

	MemberDefn *CheckMemberDefn(DefinitionName Name, Type T,
			UserClassDefn *Record, SourceLocation Loc, bool Mutable,
			Expr *BitfieldWidth, SourceLocation TSSL, NamedDefn *PrevDefn);

	enum ClassSpecialMember {
		CXXInvalid = -1,
		CXXConstructor = 0,
		CXXCopyConstructor = 1,
		CXXCopyAssignment = 2,
		CXXDestructor = 3
	};

	ClassSpecialMember getSpecialMember(const ClassMethodDefn *MD);

	bool CheckNontrivialField(MemberDefn *FD);
	void DiagnoseNontrivial(const Type* Record, ClassSpecialMember mem);

	void ActOnLastBitfield(SourceLocation DefnStart, Defn *IntfDefn,
			llvm::SmallVectorImpl<Defn *> &AllIvarDefns);
	Defn *ActOnIvar(Scope *S, SourceLocation DefnStart, Defn *IntfDefn,
			Expr *BitfieldWidth);

	// This is used for both record definitions and ObjC interface declarations.
	void ActOnFields(Scope* S, SourceLocation RecLoc, Defn *TagDefn,
			Defn **Fields, unsigned NumFields, SourceLocation LBrac,
			SourceLocation RBrac/*, AttributeList *AttrList*/);

	/// ActOnTagStartDefinition - Invoked when we have entered the
	/// scope of a tag's definition (e.g., for an enumeration, class,
	/// struct, or union).
	void ActOnTagStartDefinition(Scope *S, Defn *TagDefn);

	/// ActOnStartCXXMemberDeclarations - Invoked when we have parsed a
	/// C++ record definition's base-specifiers clause and are starting its
	/// member declarations.
	void ActOnStartCXXMemberDeclarations(Scope *S, Defn *TagDefn,
			SourceLocation LBraceLoc);

	/// ActOnTagFinishDefinition - Invoked once we have finished parsing
	/// the definition of a tag (enumeration, class, struct, or union).
	void ActOnTagFinishDefinition(Scope *S, Defn *TagDefn,
			SourceLocation RBraceLoc);

	/// ActOnTagDefinitionError - Invoked when there was an unrecoverable
	/// error parsing the definition of a tag.
	void ActOnTagDefinitionError(Scope *S, Defn *TagDefn);

	DefnContext *getContainingDC(DefnContext *DC);

	/// Set the current declaration context until it gets popped.
	void PushDefnContext(Scope *S, DefnContext *DC);
	void PopDefnContext();

	/// EnterDeclaratorContext - Used when we must lookup names in the context
	/// of a declarator's nested name specifier.
	void EnterDeclaratorContext(Scope *S, DefnContext *DC);
	void ExitDefnaratorContext(Scope *S);

	DefnContext *getFunctionLevelDefnContext();

	/// getCurFunctionDefn - If inside of a function body, this returns a pointer
	/// to the function defn for the function being parsed.  If we're currently
	/// in a 'block', this returns the containing context.
	FunctionDefn *getCurFunctionDefn();

	/// getCurFunctionOrMethodDefn - Return the Defn for the current ObjC method
	/// or C function we're in, otherwise return null.  If we're currently
	/// in a 'block', this returns the containing context.
	NamedDefn *getCurFunctionOrMethodDefn();

	/// Add this defn to the scope shadowed decl chains.
	void PushOnScopeChains(NamedDefn *D, Scope *S, bool AddToContext = true);

	/// isDefnInScope - If 'Ctx' is a function/method, isDefnInScope returns true
	/// if 'D' is in Scope 'S', otherwise 'S' is ignored and isDefnInScope returns
	/// true if 'D' belongs to the given declaration context.
	bool isDefnInScope(NamedDefn *&D, DefnContext *Ctx, Scope *S = 0);

	/// Finds the scope corresponding to the given decl context, if it
	/// happens to be an enclosing scope.  Otherwise return NULL.
	static Scope *getScopeForDefnContext(Scope *S, DefnContext *DC);

	/// Subroutines of ActOnDeclarator().
	bool MergeFunctionDefn(FunctionDefn *New, Defn *Old);
	bool MergeCompatibleFunctionDefns(FunctionDefn *New, FunctionDefn *Old);
	void MergeVarDefn(VarDefn *New, LookupResult &OldDefns);

	void AddMethodCandidate(DefnAccessPair FoundDefn, Type ObjectType,
			Expr **Args, unsigned NumArgs, bool SuppressUserConversion = false);

	Expr *
	BuildCallToMemberFunction(Scope *S, Expr *MemExpr,
			SourceLocation LParenLoc, Expr **Args, unsigned NumArgs,
			SourceLocation RParenLoc);
	Expr *
	BuildCallToObjectOfClassType(Scope *S, Expr *Object,
			SourceLocation LParenLoc, Expr **Args, unsigned NumArgs,
			SourceLocation RParenLoc);

	/// CheckCallReturnType - Checks that a call expression's return type is
	/// complete. Returns true on failure. The location passed in is the location
	/// that best represents the call.
	bool CheckCallReturnType(Type ReturnType, SourceLocation Loc,
			FunctionCall *CE, FunctionDefn *FD);

	/// Helpers for dealing with blocks and functions.
	bool CheckParmsForFunctionDefn(ParamVarDefn **P, ParamVarDefn **PEnd,
			bool CheckParameterNames);

	Scope *getNonMemberDefnScope(Scope *S);

	NamedDefn *LazilyCreateBuiltin(IdentifierInfo *II, unsigned ID, Scope *S,
			bool ForRedefinition, SourceLocation Loc);
	NamedDefn *ImplicitlyDefineFunction(SourceLocation Loc, IdentifierInfo &II,
			Scope *S);
	void AddKnownFunctionAttributes(FunctionDefn *FD);

	// More parsing and symbol table subroutines.

	// Defn attributes - this routine is the top level dispatcher.
	// void ProcessDefnAttributes(Scope *S, Defn *D, const Defnarator &PD);
	void ProcessDefnAttributeList(Scope *S, Defn *D/*, const AttributeList *AL*/);

	void WarnUndefinedMethod(SourceLocation ImpLoc, Defn *method,
			bool &IncompleteImpl, unsigned DiagID);

	void DiagnoseUnusedDefn(const NamedDefn *ND);
	void EmitDeprecationWarning(NamedDefn *D, SourceLocation Loc);
	bool DiagnoseUseOfDefn(NamedDefn *D, SourceLocation Loc);
	void DiagnoseSentinelCalls(NamedDefn *D, SourceLocation Loc, Expr **Args,
			unsigned NumArgs);

	typedef uintptr_t ParsingDefnStackState;
	ParsingDefnStackState PushParsingDefinition();
	void PopParsingDefinition(ParsingDefnStackState S, Defn *D);

	Defn *ActOnFinishStatementBody(Defn *dcl, StmtVector &Body);

	//===--------------------------------------------------------------------===//
	// Object-Oriented Programming Definitions: SemaDefnOOP.cpp
	//===--------------------------------------------------------------------===//
	Defn *ActOnStartLinkageSpecification(Scope *S, SourceLocation ExternLoc,
			SourceLocation LangLoc, llvm::StringRef Lang,
			SourceLocation LBraceLoc);
	Defn *ActOnFinishLinkageSpecification(Scope *S, Defn *LinkageSpec,
			SourceLocation RBraceLoc);

	/// MarkBaseAndMemberDestructorsReferenced - Given a record decl,
	/// mark all the non-trivial destructors of its members and bases as
	/// referenced.
	void MarkBaseAndMemberDestructorsReferenced(SourceLocation Loc,
			UserClassDefn *Record);

	/// \brief The list of classes whose vtables have been used within
	/// this translation unit, and the source locations at which the
	/// first use occurred.
	llvm::SmallVector<std::pair<UserClassDefn *, SourceLocation>, 16>
			VTableUses;

	/// \brief The set of classes whose vtables have been used within
	/// this translation unit, and a bit that will be true if the vtable is
	/// required to be emitted (otherwise, it should be emitted only if needed
	/// by code generation).
	llvm::DenseMap<UserClassDefn *, bool> VTablesUsed;

	/// \brief A list of all of the dynamic classes in this translation
	/// unit.
	llvm::SmallVector<UserClassDefn *, 16> DynamicClasses;

	enum AbstractDiagSelID {
		AbstractNone = -1,
		AbstractReturnType,
		AbstractParamType,
		AbstractVariableType,
		AbstractFieldType,
		AbstractArrayType
	};

	bool RequireNonAbstractType(SourceLocation Loc, Type T,
			const PartialDiagnostic &PD);

	bool RequireNonAbstractType(SourceLocation Loc, Type T, unsigned DiagID,
			AbstractDiagSelID SelID = AbstractNone);

	DefnResult
	ActOnExplicitInstantiation(Scope *S, SourceLocation ExternLoc,
			SourceLocation TemplateLoc, unsigned TagSpec, SourceLocation KWLoc/*,
			AttributeList *Attr*/);

	DefnResult
	ActOnExplicitInstantiation(Scope *S, SourceLocation ExternLoc,
			unsigned TagSpec, SourceLocation KWLoc, IdentifierInfo *Name,
			SourceLocation NameLoc/*, AttributeList *Attr*/);

	DefnResult ActOnExplicitInstantiation(Scope *S, SourceLocation ExternLoc);

	ExprResult RebuildExprInCurrentInstantiation(Expr *E);

	/// \brief The stack of calls expression undergoing template instantiation.
	///
	/// The top of this stack is used by a fixit instantiating unresolved
	/// function calls to fix the AST to match the textual change it prints.
	llvm::SmallVector<FunctionCall *, 8> CallsUndergoingInstantiation;

	void PrintInstantiationStack();

	/// \brief The current instantiation scope used to store local
	/// variables.
	// LocalInstantiationScope *CurrentInstantiationScope;

	/// \brief The number of typos corrected by CorrectTypo.
	unsigned TyposCorrected;

	/// \brief An entity for which implicit template instantiation is required.
	///
	/// The source location associated with the declaration is the first place in
	/// the source code where the declaration was "used". It is not necessarily
	/// the point of instantiation (which will be either before or after the
	/// namespace-scope declaration that triggered this implicit instantiation),
	/// However, it is the location that diagnostics should generally refer to,
	/// because users will need to know what code triggered the instantiation.
	typedef std::pair<ValueDefn *, SourceLocation> PendingImplicitInstantiation;

	/// \brief The queue of implicit template instantiations that are required
	/// but have not yet been performed.
	std::deque<PendingImplicitInstantiation> PendingInstantiations;

	/// \brief The queue of implicit template instantiations that are required
	/// and must be performed within the current local scope.
	///
	/// This queue is only used for member functions of local classes in
	/// templates, which must be instantiated in the same scope as their
	/// enclosing function, so that they can reference function-local
	/// types, static variables, enumerators, etc.
	std::deque<PendingImplicitInstantiation> PendingLocalImplicitInstantiations;

	void PerformPendingInstantiations(bool LocalOnly = false);

	// TODO yabin TypeSourceInfo *SubstType(SourceLocation Loc, DefinitionName Entity);

	Type SubstType(Type T, SourceLocation Loc, DefinitionName Entity);

	// TODO yabin TypeSourceInfo *SubstFunctionDefnType(SourceLocation Loc,
	//                                      DefinitionName Entity);

	ParamVarDefn *SubstParamVarDefn(ParamVarDefn *D);
	ExprResult SubstExpr(Expr *E);

	StmtResult SubstStmt(Stmt *S);

	Defn *SubstDefn(Defn *D, DefnContext *Owner);

	bool SubstBaseSpecifiers(UserClassDefn *Instantiation,
			UserClassDefn *Pattern);

	bool InstantiateClass(SourceLocation PointOfInstantiation,
			UserClassDefn *Instantiation, UserClassDefn *Pattern,
			bool Complain = true);

	void InstantiateAttrs(Defn *Pattern, Defn *Inst);

	void InstantiateClassMembers(SourceLocation PointOfInstantiation,
			UserClassDefn *Instantiation);

	void InstantiateFunctionDefinition(SourceLocation PointOfInstantiation,
			FunctionDefn *Function, bool Recursive = false,
			bool DefinitionRequired = false);
	void InstantiateStaticDataMemberDefinition(
			SourceLocation PointOfInstantiation, VarDefn *Var, bool Recursive =
					false, bool DefinitionRequired = false);

	// Objective-C declarations.
	Defn *ActOnStartClassInterface(SourceLocation AtInterfaceLoc,
			IdentifierInfo *ClassName, SourceLocation ClassLoc,
			IdentifierInfo *SuperName, SourceLocation SuperLoc,
			Defn * const *ProtoRefs, unsigned NumProtoRefs,
			const SourceLocation *ProtoLocs, SourceLocation EndProtoLoc/*,
			AttributeList *AttrList*/);

	Defn *ActOnCompatiblityAlias(SourceLocation AtCompatibilityAliasLoc,
			IdentifierInfo *AliasName, SourceLocation AliasLocation,
			IdentifierInfo *ClassName, SourceLocation ClassLocation);

	Defn *ActOnStartProtocolInterface(SourceLocation AtProtoInterfaceLoc,
			IdentifierInfo *ProtocolName, SourceLocation ProtocolLoc,
			Defn * const *ProtoRefNames, unsigned NumProtoRefs,
			const SourceLocation *ProtoLocs, SourceLocation EndProtoLoc/*,
			AttributeList *AttrList*/);

	Defn *ActOnStartCategoryInterface(SourceLocation AtInterfaceLoc,
			IdentifierInfo *ClassName, SourceLocation ClassLoc,
			IdentifierInfo *CategoryName, SourceLocation CategoryLoc,
			Defn * const *ProtoRefs, unsigned NumProtoRefs,
			const SourceLocation *ProtoLocs, SourceLocation EndProtoLoc);

	Defn *ActOnStartClassImplementation(SourceLocation AtClassImplLoc,
			IdentifierInfo *ClassName, SourceLocation ClassLoc,
			IdentifierInfo *SuperClassname, SourceLocation SuperClassLoc);

	Defn *ActOnStartCategoryImplementation(SourceLocation AtCatImplLoc,
			IdentifierInfo *ClassName, SourceLocation ClassLoc,
			IdentifierInfo *CatName, SourceLocation CatLoc);

	Defn *ActOnForwardClassDeclaration(SourceLocation Loc,
			IdentifierInfo **IdentList, SourceLocation *IdentLocs,
			unsigned NumElts);

	Defn *ActOnForwardProtocolDeclaration(SourceLocation AtProtoclLoc,
			const IdentifierLocPair *IdentList, unsigned NumElts/*,
			AttributeList *attrList*/);

	void FindProtocolDeclaration(bool WarnOnDefinitions,
			const IdentifierLocPair *ProtocolId, unsigned NumProtocols,
			llvm::SmallVectorImpl<Defn *> &Protocols);

	void CompareProperties(Defn *CDefn, Defn *MergeProtocols);

	//TODO yabin  void ActOnAtEnd(Scope *S, SourceRange AtEnd, Defn *classDefn,
	//                  Defn **allMethods = 0, unsigned allNum = 0,
	//                  Defn **allProperties = 0, unsigned pNum = 0,
	//                  DefnGroupPtrTy *allTUVars = 0, unsigned tuvNum = 0);

	Defn *ActOnProperty(Scope *S, SourceLocation AtLoc, Defn *ClassCategory,
			DefnContext *lexicalDC = 0);

	Defn *ActOnPropertyImplDefn(Scope *S, SourceLocation AtLoc,
			SourceLocation PropertyLoc, bool ImplKind, Defn *ClassImplDefn,
			IdentifierInfo *PropertyId, IdentifierInfo *PropertyIvar);

	VarDefn *BuildExceptionDeclaration(Scope *S, IdentifierInfo *Name,
				SourceLocation Loc);
	Defn *ActOnExceptionDeclarator(Scope *S);
	void DiagnoseReturnInConstructorExceptionHandler(TryCmd *TryBlock);

	//===--------------------------------------------------------------------===//
	// Commands Parsing Callbacks: SemaCmd.cpp.
	//===--------------------------------------------------------------------===//
public:
	StmtResult ActOnExprStmt(Expr* E, bool isShowResult); //FIXME yabin is this needed?
	StmtResult ActOnNullCmd(SourceLocation SemiLoc);
	StmtResult ActOnBlockCmd(SourceLocation L, SourceLocation R,
			MultiStmtArg elts, bool isStmtExpr);
	StmtResult ActOnDefnCmd(DefnGroupPtrTy Defn, SourceLocation StartLoc,
			SourceLocation EndLoc);
	void ActOnForEachDefnCmd(DefnGroupPtrTy Defn);
	StmtResult ActOnCaseCmd(SourceLocation CaseLoc, Expr *LHSVal, Stmt *RHSVal);
	void ActOnCaseCmdBody(Cmd *CaseCmd, Stmt *SubStmt);
	StmtResult ActOnOtherwiseCmd(SourceLocation OtherwiseLoc, Stmt *SubStmt,
			Scope *CurScope);
	StmtResult ActOnThenCmd(SourceLocation TLoc, MultiStmtArg ThenStmts);
	StmtResult ActOnElseCmd(SourceLocation ELoc, MultiStmtArg ElseStmts);
	StmtResult ActOnElseIfCmd(SourceLocation ElseIfLoc,	Expr *Cond,
			MultiStmtArg ElseIfStmts);
	StmtResult ActOnIfCmd(SourceLocation IfLoc, SourceLocation EndLoc,
			Expr *CondVal, Stmt *ThenVal,	SourceLocation ElseLoc, Stmt *ElseVal);
	StmtResult ActOnIfCmd(SourceLocation IfLoc, SourceLocation EndLoc,
			Expr *CondVal, Stmt *ThenVal,	MultiStmtArg ElseIfClauses,
			SourceLocation ElseLoc, Stmt *ElseVal);
	StmtResult ActOnCaseCmd(SourceLocation Cloc, Expr *Pat,
			MultiStmtArg Stmts);
	StmtResult ActOnOtherwiseCmd(SourceLocation Oloc, MultiStmtArg Stmts);
	StmtResult ActOnSwitchCmd(SourceLocation SwitchLoc,
			SourceLocation EndLoc, Expr *Var, Expr *Cond,
			MultiStmtArg CaseClauses, Stmt *OtherwiseCmd);
	StmtResult ActOnWhileCmd(SourceLocation WhileLoc, SourceLocation EndLoc,
			Expr *Cond, Stmt *Body);
	StmtResult ActOnDoCmd(SourceLocation DoLoc, Stmt *Body,
			SourceLocation WhileLoc, SourceLocation CondLParen, Expr *Cond,
			SourceLocation CondRParen);
	StmtResult ActOnForCmd(SourceLocation ForLoc, SourceLocation EndLoc,
			Expr *LoopVar, Expr *LoopCond, Stmt *Body);
	StmtResult ActOnContinueCmd(SourceLocation ContinueLoc, Scope *CurScope);
	StmtResult ActOnBreakCmd(SourceLocation BreakLoc, Scope *CurScope);
	StmtResult ActOnReturnCmd(SourceLocation ReturnLoc, Expr *RetValExp);
	StmtResult ActOnCatchBlock(SourceLocation CatchLoc, Defn *ExDefn,
			Stmt *HandlerBlock);
	StmtResult ActOnTryBlock(SourceLocation TryLoc, Stmt *TryBlock,
			MultiStmtArg Handlers);
	StmtResult ActOnDefnStmt(Defn * D);

	//===--------------------------------------------------------------------===//
	// Expression Parsing Callbacks: SemaExpr.cpp.
	//===--------------------------------------------------------------------===//
	/// DiagnoseUnusedExprResult- If the statement passed in is an expression
	/// whose result is unused, warn.
	void DiagnoseUnusedExprResult(const Stmt *S);
	void
	PushExpressionEvaluationContext(ExpressionEvaluationContext NewContext);
	void PopExpressionEvaluationContext();
	void MarkDeclarationReferenced(SourceLocation Loc, Defn *D);
	void MarkDeclarationsReferencedInType(SourceLocation Loc, Type T);
	void MarkDeclarationsReferencedInExpr(Expr *E);
	bool DiagRuntimeBehavior(SourceLocation Loc, const PartialDiagnostic &PD);

	// Primary Expressions.
	SourceRange getExprRange(Expr *E) const;
	ExprResult ActOnIdExpression(Scope *S, DefinitionNameInfo &NameInfo,
			bool HasTrailingLParen);
	bool DiagnoseEmptyLookup(Scope *S, LookupResult &R, CorrectTypoContext CTC =
			CTC_Unknown);
	ExprResult BuildDefnRefExpr(ValueDefn *D, Type Ty, SourceLocation Loc);
	ExprResult BuildDefnRefExpr(ValueDefn *D, Type Ty, const DefinitionNameInfo &NameInfo);
	ExprResult BuildAnonymousStructUnionMemberReference(SourceLocation Loc,
			MemberDefn *Field, Expr *BaseObjectExpr = 0, SourceLocation OpLoc =
					SourceLocation());
	ExprResult BuildPossibleImplicitMemberExpr(LookupResult &R);
	ExprResult
	BuildImplicitMemberExpr(LookupResult &R, bool IsDefiniteInstance);
	ExprResult BuildQualifiedDefinitionNameExpr(const Defn &NameInfo);
	ExprResult BuildDefinitionNameExpr(LookupResult &R, bool ADL);
	ExprResult BuildDefinitionNameExpr(const DefinitionNameInfo &NameInfo,
	                                   NamedDefn *D);
	ExprResult ActOnNumericConstant(const Token &);
	ExprResult ActOnCharacterConstant(const Token &);
	ExprResult ActOnParenExpr(SourceLocation L, SourceLocation R, Expr *Val);
	ExprResult ActOnParenOrParenListExpr(SourceLocation L, SourceLocation R,
			MultiExprArg Val, Type TypeOfCast = Type());

	/// ActOnStringLiteral - The specified tokens were lexed as pasted string
	/// fragments (e.g. "foo" "bar" L"baz").
	ExprResult ActOnStringLiteral(const Token *Toks, unsigned NumToks);

	ExprResult BuildMatrix(MultiExprArg Mats, SourceLocation LBracketLoc,
			SourceLocation RBracketLoc);
	ExprResult BuildCellArray(MultiExprArg Mats, SourceLocation LBracketLoc,
			SourceLocation RBracketLoc);
	ExprResult ActOnRowVectorExpr(MultiExprArg Rows);
	ExprResult ActOnMatrixOrCellExpr(MultiExprArg Mats, bool isCell,
			SourceLocation LLoc, SourceLocation RLoc);
	// Binary/Unary Operators.  'Tok' is the token for the operator.
	ExprResult CreateBuiltinUnaryOp(SourceLocation OpLoc, unsigned OpcIn,
			Expr *InputArg);
	ExprResult BuildUnaryOp(Scope *S, SourceLocation OpLoc,
			UnaryOperatorKind Opc, Expr *input);
	ExprResult ActOnUnaryOp(Scope *S, SourceLocation OpLoc, tok::TokenKind Op,
			bool isPost, Expr *Input);
	ExprResult ActOnPostfixUnaryOp(Scope *S, SourceLocation OpLoc,
			tok::TokenKind Kind, Expr *Input);
	ExprResult ActOnArrayIndexExpr(Scope *S, VarDefn *Var, Expr *Base,
			SourceLocation LLoc, MultiExprArg args, SourceLocation RLoc,
			bool isCell);
	ExprResult CreateBuiltinArrayIndexExpr(Expr *Base, SourceLocation LLoc,
			Expr *Idx, SourceLocation RLoc);
	ExprResult BuildMemberReferenceExpr(Expr *Base, Type BaseType,
			SourceLocation OpLoc, NamedDefn *FirstQualifierInScope,
			const Defn &NameInfo);
	ExprResult BuildMemberReferenceExpr(Expr *Base, Type BaseType,
			SourceLocation OpLoc, NamedDefn *FirstQualifierInScope,
			LookupResult &R);
	ExprResult LookupMemberExpr(LookupResult &R, Expr *&Base,
			SourceLocation OpLoc);
	ExprResult ActOnMemberAccessExpr(Scope *S, ExprResult Base,
			SourceLocation OpLoc, tok::TokenKind OpKind, ExprResult Member,
			bool HasTrailingLParen);
	bool ConvertArgumentsForCall(FunctionCall *Call, Expr *Fn,
			FunctionDefn *FDefn, const FunctionProtoType *Proto, Expr **Args,
			unsigned NumArgs, SourceLocation RParenLoc);

	ExprResult ActOnArrayIndexingOrFunctionCallExpr(Scope *S, Expr* Fn,
			SourceLocation LParenLoc, MultiExprArg args, SourceLocation RParenLoc);
	/// ActOnFunctionCallExpr - Handle a call to Fn with the specified array of
	/// arguments. This provides the location of the left/right parens and a list
	/// of comma locations.
	ExprResult ActOnFunctionCallExpr(Scope *S, FunctionDefn *FnD, Expr* Fn,
			SourceLocation LParenLoc, MultiExprArg args,
			SourceLocation RParenLoc);
	ExprResult BuildResolvedCallExpr(Expr *Fn, NamedDefn *NDefn,
			SourceLocation LParenLoc, Expr **Args, unsigned NumArgs,
			SourceLocation RParenLoc);
	bool TypeIsVectorType(Type Ty) {
		//   return GetTypeFromParser(Ty)->isVectorType();
		//FIXME yabin
		return false;
	}
	ExprResult MaybeConvertParenListExprToParenExpr(Scope *S, Expr *ME);
	ExprResult ActOnCastOfParenListExpr(Scope *S, SourceLocation LParenLoc,
			SourceLocation RParenLoc, Expr *E);
	ExprResult ActOnImaginaryLiteralExpr(SourceLocation LParenLoc, Type Ty,
			SourceLocation RParenLoc, Expr *Op);
	ExprResult BuildImaginaryLiteralExpr(SourceLocation LParenLoc,
			SourceLocation RParenLoc, Expr *InitExpr);
	ExprResult ActOnInitList(SourceLocation LParenLoc, MultiExprArg InitList,
			SourceLocation RParenLoc);
	ExprResult ActOnBinOp(Scope *S, SourceLocation TokLoc, tok::TokenKind Kind,
			Expr *LHS, Expr *RHS);
	ExprResult ActOnTernaryColonOp(Scope *S, SourceLocation FirstColonLoc,
			SourceLocation SecondColonLoc, Expr *Base, Expr *Inc, Expr *Limit);
	ExprResult BuildColonExpr(Scope *S, SourceLocation FirstColonLoc,
			SourceLocation SecondColonLoc, Expr *Base, Expr *Inc, Expr *Limit);
	ExprResult BuildBinOp(Scope *S, SourceLocation OpLoc,
			BinaryOperatorKind Opc, Expr *lhs, Expr *rhs);
	ExprResult CreateBuiltinBinOp(SourceLocation TokLoc, BinaryOperatorKind Opc,
			Expr *lhs, Expr *rhs);
	ExprResult ActOnStmtExpr(SourceLocation LPLoc, Stmt *SubStmt,
			SourceLocation RPLoc); // "({..})"
	Expr * ActOnClassPropertyRefExpr(IdentifierInfo &receiverName,
			IdentifierInfo &propertyName, SourceLocation receiverNameLoc,
			SourceLocation propertyNameLoc);
	Expr * ActOnClassMessage(Scope *S, Type Receiver, SourceLocation LBracLoc,
			SourceLocation SelectorLoc, SourceLocation RBracLoc,
			MultiExprArg Args);

	/// VerifyIntegerConstantExpression - verifies that an expression is an ICE,
	/// and reports the appropriate diagnostics. Returns false on success.
	/// Can optionally return the value of the expression.
	bool VerifyIntegerConstantExpression(const Expr *E, llvm::APSInt *Result=0);

	/// CheckBooleanCondition - Diagnose problems involving the use of
	/// the given expression as a boolean condition (e.g. in an if
	/// statement).  Also performs the standard function and array
	/// decays, possibly changing the input variable.
	///
	/// \param Loc - A location associated with the condition, e.g. the
	/// 'if' keyword.
	/// \return true iff there were any errors
	bool CheckBooleanCondition(Expr *&CondExpr, SourceLocation Loc);

	ExprResult ActOnBooleanCondition(Scope *S, SourceLocation Loc,
			Expr *SubExpr);

	/// DiagnoseAssignmentAsCondition - Given that an expression is
	/// being used as a boolean condition, warn if it's an assignment.
	void DiagnoseAssignmentAsCondition(Expr *E);

	/// CheckCXXBooleanCondition - Returns true if conversion to bool is invalid.
	bool CheckCXXBooleanCondition(Expr *&CondExpr);

	//===--------------------------------------------------------------------===//
	// Attributes tracking callbacks: SemaAttr.cpp
	//===--------------------------------------------------------------------===//
public:
	/// FreePackedContext - Deallocate and null out PackContext.
	void FreePackedContext();

	/// PushVisibilityAttr - Note that we've entered a context with a
	/// visibility attribute.
	// FIXME void PushVisibilityAttr(const VisibilityAttr *Attr);

	/// AddPushedVisibilityAttribute - If '#pragma GCC visibility' was used,
	/// add an appropriate visibility attribute.
	// FIXME void AddPushedVisibilityAttribute(Defn *RD);

	/// FreeVisContext - Deallocate and null out VisContext.
	void FreeVisContext();

	//===--------------------------------------------------------------------===//
	// Extra semantic analysis: SemaChecking.cpp
	//===--------------------------------------------------------------------===//
public:
	SourceLocation getLocationOfStringLiteralByte(const StringLiteral *SL,
			unsigned ByteNo) const;

private:
	bool CheckFunctionCall(FunctionDefn *FDefn, FunctionCall *TheCall);
	bool CheckBlockCall(NamedDefn *NDefn, FunctionCall *TheCall);

	ExprResult CheckBuiltinFunctionCall(unsigned BuiltinID,
			FunctionCall *TheCall);
	bool CheckX86BuiltinFunctionCall(unsigned BuiltinID, FunctionCall *TheCall);

	bool SemaBuiltinVAStart(FunctionCall *TheCall);
	bool SemaBuiltinUnorderedCompare(FunctionCall *TheCall);
	bool SemaBuiltinFPClassification(FunctionCall *TheCall, unsigned NumArgs);

public:
	// Used by C++ template instantiation.
	ExprResult SemaBuiltinShuffleVector(FunctionCall *TheCall);

private:
	bool SemaBuiltinPrefetch(FunctionCall *TheCall);
	bool SemaBuiltinObjectSize(FunctionCall *TheCall);
	bool SemaBuiltinLongjmp(FunctionCall *TheCall);
	ExprResult SemaBuiltinAtomicOverloaded(ExprResult TheCallResult);
	bool SemaBuiltinConstantArg(FunctionCall *TheCall, int ArgNum,
			llvm::APSInt &Result);
	bool SemaCheckStringLiteral(const Expr *E, const FunctionCall *TheCall,
			bool HasVAListArg, unsigned format_idx, unsigned firstDataArg,
			bool isPrintf);
	void CheckFormatString(const StringLiteral *FExpr,
			const Expr *OrigFormatExpr, const FunctionCall *TheCall,
			bool HasVAListArg, unsigned format_idx, unsigned firstDataArg,
			bool isPrintf);
	//  void CheckNonNullArguments(const NonNullAttr *NonNull,
	//                             const FunctionCall *TheCall);
	void CheckPrintfScanfArguments(const FunctionCall *TheCall,
			bool HasVAListArg, unsigned format_idx, unsigned firstDataArg,
			bool isPrintf);
	void CheckReturnStackAddr(Expr *RetValExp, Type lhsType,
			SourceLocation ReturnLoc);
	void CheckFloatComparison(SourceLocation loc, Expr* lex, Expr* rex);
	void CheckImplicitConversions(Expr *E, SourceLocation CC);

private:
	/// \brief The parser's current scope.
	///
	/// The parser maintains this state here.
	Scope *CurScope;

protected:
	friend class Parser;
	/// \brief Retrieve the parser's current scope.
	Scope *getCurScope() const {
		return CurScope;
	}

public:
	void PrintStats() const {
	}
};

/// \brief RAII object that enters a new expression evaluation context.
class EnterExpressionEvaluationContext {
	Sema &Actions;

public:
	EnterExpressionEvaluationContext(Sema &Actions,
			Sema::ExpressionEvaluationContext NewContext) :
		Actions(Actions) {
		Actions.PushExpressionEvaluationContext(NewContext);
	}

	~EnterExpressionEvaluationContext() {
		Actions.PopExpressionEvaluationContext();
	}
};

/// \brief An abstract interface that should be implemented by
/// external AST sources that also provide information for semantic
/// analysis.
class ExternalSemaSource: public ExternalASTSource {
public:
	ExternalSemaSource() {
		ExternalASTSource::SemaSource = true;
	}

	~ExternalSemaSource();

	/// \brief Initialize the semantic source with the Sema instance
	/// being used to perform semantic analysis on the abstract syntax
	/// tree.
	virtual void InitializeSema(Sema &S) {
	}

	/// \brief Inform the semantic consumer that Sema is no longer available.
	virtual void ForgetSema() {
	}

	// isa/cast/dyn_cast support
	static bool classof(const ExternalASTSource *Source) {
		return Source->SemaSource;
	}
	static bool classof(const ExternalSemaSource *) {
		return true;
	}
};
} // end namespace mlang

#endif /* SEMA_H_ */
