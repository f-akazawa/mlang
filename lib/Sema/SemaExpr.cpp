//===--- SemaExpr.cpp - Semantic Analysis for Expressions -----------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for expressions.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
#include "mlang/Sema/Lookup.h"
#include "mlang/Sema/AnalysisBasedWarnings.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Diag/PartialDiagnostic.h"
#include "mlang/Lex/LiteralSupport.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Sema/Scope.h"
#include "mlang/Sema/ScopeInfo.h"
#include "mlang/Sema/SemaDiagnostic.h"

using namespace mlang;
using namespace sema;


static inline BinaryOperatorKind ConvertTokenKindToBinaryOpcode(
  tok::TokenKind Kind) {
  BinaryOperatorKind Opc;
  switch (Kind) {
  default: assert(0 && "Unknown binop!");
  case tok::DotCaret:             Opc = BO_Power; break;
  case tok::Caret:                Opc = BO_MatrixPower; break;
  case tok::Asterisk:             Opc = BO_MatrixMul; break;
  case tok::Slash:                Opc = BO_MatrixRDiv; break;
  case tok::BackSlash:            Opc = BO_MatrixLDiv; break;
  case tok::DotAsterisk:          Opc = BO_Mul; break;
  case tok::DotSlash:             Opc = BO_RDiv; break;
  case tok::DotBackSlash:         Opc = BO_LDiv; break;
  case tok::Addition:             Opc = BO_Add; break;
  case tok::Subtraction:          Opc = BO_Sub; break;
  case tok::ShiftLeft:            Opc = BO_Shl; break;
  case tok::ShiftRight:           Opc = BO_Shr; break;
  case tok::LessThanOREqualTo:    Opc = BO_LE; break;
  case tok::LessThan:             Opc = BO_LT; break;
  case tok::GreaterThanOREqualTo: Opc = BO_GE; break;
  case tok::GreaterThan:          Opc = BO_GT; break;
  case tok::NotEqualTo:           Opc = BO_NE; break;
  case tok::EqualTo:              Opc = BO_EQ; break;
  case tok::EleWiseAND:           Opc = BO_And; break;
  case tok::EleWiseOR:            Opc = BO_Or; break;
  case tok::ShortCircuitAND:      Opc = BO_LAnd; break;
  case tok::ShortCircuitOR:       Opc = BO_LOr; break;
  case tok::Assignment:           Opc = BO_Assign; break;
  case tok::MatPowerAssign:       Opc = BO_MatPowerAssign; break;
  case tok::MatMulAssign:         Opc = BO_MatMulAssign; break;
  case tok::MatRDivAssign:        Opc = BO_MatRDivAssign; break;
  case tok::MatLDivAssign:        Opc = BO_MatLDivAssign; break;
  case tok::PowerAssign:          Opc = BO_PowerAssign; break;
  case tok::MulAssign:            Opc = BO_MulAssign; break;
  case tok::RDivAssign:           Opc = BO_RDivAssign; break;
  case tok::LDivAssign:           Opc = BO_LDivAssign; break;
  case tok::AddAssign:            Opc = BO_AddAssign; break;
  case tok::SubAssign:            Opc = BO_SubAssign; break;
  case tok::ShlAssign:            Opc = BO_ShlAssign; break;
  case tok::ShrAssign:            Opc = BO_ShrAssign; break;
  case tok::AndAssign:            Opc = BO_AndAssign; break;
  case tok::OrAssign:             Opc = BO_OrAssign; break;
  case tok::Colon:                Opc = BO_Colon; break;
  case tok::Comma:                Opc = BO_Comma; break;
  }
  return Opc;
}

static inline UnaryOperatorKind ConvertTokenKindToUnaryOpcode(
  tok::TokenKind Kind, bool isPostfix) {
  UnaryOperatorKind Opc;
  switch (Kind) {
  default: assert(0 && "Unknown unary op!");
  case tok::DotQuote:          Opc = UO_Transpose; break;
  case tok::Quote:             Opc = UO_Quote; break;
  case tok::DoubleAdd:         Opc = isPostfix? UO_PostInc:UO_PreInc; break;
  case tok::DoubleSub:         Opc = isPostfix? UO_PostDec:UO_PreDec; break;
  case tok::Addition:          Opc = UO_Plus; break;
  case tok::Subtraction:       Opc = UO_Minus; break;
  case tok::At:                Opc = UO_Handle; break;
  case tok::Tilde:             Opc = UO_LNot; break;
  //case tok::exclaim:           Opc = UO_LNot; break;
  }
  return Opc;
}

static Type CheckCommaOperands(Sema &S, ExprResult &lex, ExprResult &rex,
		                    SourceLocation OpLoc) {
  return Type();
}

void Sema::DiagnoseUnusedExprResult(const Stmt *S) {

}

void Sema::PushExpressionEvaluationContext(
		ExpressionEvaluationContext NewContext) {

}

void Sema::PopExpressionEvaluationContext() {

}

void Sema::MarkDeclarationReferenced(SourceLocation Loc, Defn *D) {

}

void Sema::MarkDeclarationsReferencedInType(SourceLocation Loc, Type T) {

}

void Sema::MarkDeclarationsReferencedInExpr(Expr *E) {

}

bool Sema::DiagRuntimeBehavior(SourceLocation Loc, const PartialDiagnostic &PD){
  return false;
}

SourceRange Sema::getExprRange(Expr *E) const {
  return SourceRange();
}

/// ActOnIdExpression - Recognize what kind this Identifier is.
ExprResult Sema::ActOnIdExpression(Scope *S, DefinitionNameInfo &NameInfo,
		bool HasTrailingLParenOrLBrace) {
	DefinitionName Name = NameInfo.getName();
	IdentifierInfo *II = Name.getAsIdentifierInfo();
	SourceLocation NameLoc = NameInfo.getLoc();

	LookupResult R(*this, NameInfo, LookupOrdinaryName);
	LookupName(R, S, true);

	if(R.empty()) {
		// NULL is the top DefnContext
		VarDefn *D = VarDefn::Create(getASTContext(), CurContext, NameLoc, II, Type(),
				                         mlang::SC_Global, mlang::SC_Global);
		if(D) {
			CurContext->addDefn(D);
			R.addDefn(D, mlang::AS_public);
			if(II) {
				S->AddDefn(D);
				IdResolver.AddDefn(D);
			}
		}
	}

	return BuildDefinitionNameExpr(R, false);
}

bool Sema::DiagnoseEmptyLookup(Scope *S, LookupResult &R,
		CorrectTypoContext CTC) {
  return false;
}

ExprResult Sema::BuildDefnRefExpr(ValueDefn *D, Type Ty, SourceLocation Loc) {
	DefnRefExpr *E = DefnRefExpr::Create(Context, NULL, SourceRange(), D,
			                                 Loc, Ty, mlang::VK_LValue);
	return Owned(E);
}

ExprResult Sema::BuildDefnRefExpr(ValueDefn *D, Type Ty,
		                              const DefinitionNameInfo &NameInfo) {
	return BuildDefnRefExpr(D, Ty, NameInfo.getLoc());
}

ExprResult Sema::BuildAnonymousStructUnionMemberReference(SourceLocation Loc,
		MemberDefn *Field, Expr *BaseObjectExpr, SourceLocation OpLoc) {
	return ExprError();
}

ExprResult Sema::BuildPossibleImplicitMemberExpr(LookupResult &R) {
	return ExprError();
}

ExprResult
Sema::BuildImplicitMemberExpr(LookupResult &R, bool IsDefiniteInstance) {
	return ExprError();
}

ExprResult Sema::BuildQualifiedDefinitionNameExpr(const Defn &NameInfo) {
	return ExprError();
}

ExprResult Sema::BuildDefinitionNameExpr(LookupResult &R, bool ADL) {
	if(R.isSingleResult()) {
		return BuildDefinitionNameExpr(R.getLookupNameInfo(),
		                               R.getFoundDefn());
	}
	return ExprError();
}

/// \brief Complete semantic analysis for a reference to the given declaration.
ExprResult Sema::BuildDefinitionNameExpr(const DefinitionNameInfo &NameInfo,
                               NamedDefn *D) {
  assert(D && "Cannot refer to a NULL declaration");

  SourceLocation Loc = NameInfo.getLoc();

  // Make sure that we're referring to a value.
  ValueDefn *VD = dyn_cast<ValueDefn>(D);
  if (!VD) {
    Diag(Loc, diag::err_ref_non_value)
      << D;
    Diag(D->getLocation(), diag::note_declared_at);
    return ExprError();
  }

  // Only create DefnRefExpr's for valid Decl's.
  if (VD->isInvalidDefn())
    return ExprError();

  Type Ty ;
  FunctionDefn *FD = dyn_cast<FunctionDefn>(D);
  if(FD) {
	  Ty = Context.getFunctionHandleType(VD->getType());
  } else {
	  Ty = VD->getType();
  }
  return BuildDefnRefExpr(VD, Ty, NameInfo);
}

ExprResult Sema::ActOnNumericConstant(const Token &Tok) {
	// Fast path for a single digit (which is quite common).  A single digit
	// cannot have a trigraph, escaped newline, radix prefix, or type suffix.
	if (Tok.getLength() == 1) {
		const char Val = PP.getSpellingOfSingleCharacterNumericConstant(Tok);
		unsigned IntSize = Context.Target.getIntWidth();
		return Owned(IntegerLiteral::Create(Context, llvm::APInt(IntSize, Val
				- '0'), Context.Int32Ty, Tok.getLocation()));
	}

	llvm::SmallString<512> IntegerBuffer;
	// Add padding so that NumericLiteralParser can overread by one character.
	IntegerBuffer.resize(Tok.getLength() + 1);
	const char *ThisTokBegin = &IntegerBuffer[0];

	// Get the spelling of the token, which eliminates trigraphs, etc.
	bool Invalid = false;
	unsigned ActualLength = PP.getSpelling(Tok, ThisTokBegin, &Invalid);
	if (Invalid)
		return ExprError();

	NumericLiteralParser Literal(ThisTokBegin, ThisTokBegin + ActualLength,
			Tok.getLocation(), PP);
	if (Literal.hadError)
		return ExprError();

	Expr *Res;

	if (Literal.isFloatingLiteral()) {
		Type Ty;
		if (Literal.isFloat)
			Ty = Context.SingleTy;
		else
			Ty = Context.DoubleTy;

		const llvm::fltSemantics &Format = Context.getFloatTypeSemantics(Ty);

		using llvm::APFloat;
		APFloat Val(Format);

		APFloat::opStatus result = Literal.GetFloatValue(Val);

		// Overflow is always an error, but underflow is only an error if
		// we underflowed to zero (APFloat reports denormals as underflow).
		if ((result & APFloat::opOverflow) || ((result & APFloat::opUnderflow)
				&& Val.isZero())) {
			unsigned diagnostic;
			llvm::SmallString<20> buffer;
			if (result & APFloat::opOverflow) {
				diagnostic = diag::warn_float_overflow;
				APFloat::getLargest(Format).toString(buffer);
			} else {
				diagnostic = diag::warn_float_underflow;
				APFloat::getSmallest(Format).toString(buffer);
			}

			Diag(Tok.getLocation(), diagnostic) << Ty << llvm::StringRef(
					buffer.data(), buffer.size());
		}

		bool isExact = (result == APFloat::opOK);
		Res = FloatingLiteral::Create(Context, Val, isExact, Ty,
				Tok.getLocation());

//		if (getLangOptions().SinglePrecisionConstants && Ty == Context.DoubleTy)
//			ImpCastExprToType(Res, Context.FloatTy, CK_FloatingCast);
//
	} else if (!Literal.isIntegerLiteral()) {
		return ExprError();
	} else {
		Type Ty;

		// long long is a C99 feature.
//		if (!getLangOptions().C99 && !getLangOptions().CPlusPlus0x
//				&& Literal.isLongLong)
//			Diag(Tok.getLocation(), diag::ext_longlong);

		// Get the value in the widest-possible width.
		llvm::APInt ResultVal(Context.Target.getIntMaxTWidth(), 0);

		if (Literal.GetIntegerValue(ResultVal)) {
			// If this value didn't fit into uintmax_t, warn and force to ull.
			Diag(Tok.getLocation(), diag::warn_integer_too_large);
			Ty = Context.UInt64Ty;
			assert(Context.getTypeSize(Ty) == ResultVal.getBitWidth() &&
					"long long is not intmax_t?");
		} else {
			// If this value fits into a ULL, try to figure out what else it fits into
			// according to the rules of C99 6.4.4.1p5.

			// Octal, Hexadecimal, and integers with a U suffix are allowed to
			// be an unsigned int.
			bool AllowUnsigned = Literal.isUnsigned || Literal.getRadix() != 10;

			// Check from smallest to largest, picking the smallest type we can.
			unsigned Width = 0;
			if (!Literal.isLong && !Literal.isLongLong) {
				// Are int/unsigned possibilities?
				unsigned IntSize = Context.Target.getIntWidth();

				// Does it fit in a unsigned int?
				if (ResultVal.isIntN(IntSize)) {
					// Does it fit in a signed int?
					if (!Literal.isUnsigned && ResultVal[IntSize - 1] == 0)
						Ty = Context.Int32Ty;
					else if (AllowUnsigned)
						Ty = Context.UInt32Ty;
					Width = IntSize;
				}
			}

			// Are long/unsigned long possibilities?
			if (Ty.isNull() && !Literal.isLongLong) {
				unsigned LongSize = Context.Target.getLongWidth();

				// Does it fit in a unsigned long?
				if (ResultVal.isIntN(LongSize)) {
					// Does it fit in a signed long?
					if (!Literal.isUnsigned && ResultVal[LongSize - 1] == 0)
						Ty = Context.Int64Ty;
					else if (AllowUnsigned)
						Ty = Context.UInt64Ty;
					Width = LongSize;
				}
			}

			// Finally, check long long if needed.
//			if (Ty.isNull()) {
//				unsigned LongLongSize = Context.Target.getLongLongWidth();
//
//				// Does it fit in a unsigned long long?
//				if (ResultVal.isIntN(LongLongSize)) {
//					// Does it fit in a signed long long?
//					if (!Literal.isUnsigned && ResultVal[LongLongSize - 1] == 0)
//						Ty = Context.LongLongTy;
//					else if (AllowUnsigned)
//						Ty = Context.UnsignedLongLongTy;
//					Width = LongLongSize;
//				}
//			}

			// If we still couldn't decide a type, we probably have something that
			// does not fit in a signed long long, but has no U suffix.
//			if (Ty.isNull()) {
//				Diag(Tok.getLocation(), diag::warn_integer_too_large_for_signed);
//				Ty = Context.UnsignedLongLongTy;
//				Width = Context.Target.getLongLongWidth();
//			}

			if (ResultVal.getBitWidth() != Width)
				ResultVal = ResultVal.trunc(Width);
		}
		Res = IntegerLiteral::Create(Context, ResultVal, Ty, Tok.getLocation());
	}

	// If this is an imaginary literal, create the ImaginaryLiteral wrapper.
	if (Literal.isImaginary)
		Res = new (Context) ImaginaryLiteral(Res, Res->getType());

	return Owned(Res);
}

ExprResult Sema::ActOnCharacterConstant(const Token &) {
	return ExprError();
}

ExprResult Sema::ActOnParenExpr(SourceLocation L, SourceLocation R, Expr *Val){
	return ExprError();
}

ExprResult Sema::ActOnParenOrParenListExpr(SourceLocation L, SourceLocation R,
		MultiExprArg Val, Type TypeOfCast) {
	return ExprError();
}

ExprResult Sema::ActOnStringLiteral(const Token *StringToks,
		unsigned NumStringToks) {
	assert(NumStringToks && "Must have at least one string!");

	StringLiteralParser Literal(StringToks, NumStringToks, PP);
	if (Literal.hadError)
		return ExprError();

	llvm::SmallVector<SourceLocation, 4> StringTokLocs;
	for (unsigned i = 0; i != NumStringToks; ++i)
		StringTokLocs.push_back(StringToks[i].getLocation());

	Type StrTy = Context.CharTy;

  // Pass &StringTokLocs[0], StringTokLocs.size() to factory!
	return Owned(StringLiteral::Create(Context, Literal.GetString(),
	                                   Literal.GetStringLength(),
	                                   StrTy,
	                                   &StringTokLocs[0],
	                                   StringTokLocs.size()));
}

ExprResult Sema::ActOnRowVectorExpr(MultiExprArg Rows) {
	Expr** exprs = Rows.release();
	unsigned NumRows = Rows.size();
	ASTContext &ctx = getASTContext();
	Type EleTy = exprs[0]->getType();
	Type VTy = Context.getVectorType(EleTy, NumRows,
			VectorType::GenericVector, true);

	RowVectorExpr *E = new (ctx) RowVectorExpr(ctx,	exprs, NumRows, VTy);
	if(E == NULL)
		return ExprError();
	return Owned(E);
}

ExprResult Sema::BuildMatrix(MultiExprArg Mats, SourceLocation LBracketLoc,
		SourceLocation RBracketLoc) {
  Expr** exprs = Mats.release();
  unsigned NumRows = Mats.size();
  unsigned NumCols = 1; //FIXME
  RowVectorExpr *VE = cast<RowVectorExpr>(exprs[0]);
  if(VE)
	  NumCols = VE->getNumSubExprs();

  ASTContext &ctx = getASTContext();
  Type T = exprs[0]->getType();
  const VectorType *VT = T->getAs<VectorType>();
  Type EleTy = VT->getElementType();
  Type MTy = Context.getMatrixType(EleTy, NumRows, NumCols,
  			MatrixType::Full);

  ConcatExpr *E = new (ctx) ConcatExpr(ctx,	exprs, NumRows, MTy,
		  LBracketLoc, RBracketLoc);
  if(E==NULL)
  		return ExprError();
  return Owned(E);
}

ExprResult Sema::BuildCellArray(MultiExprArg Mats, SourceLocation LBraceLoc,
		SourceLocation RBraceLoc) {
	return ExprError();
}

ExprResult Sema::ActOnMatrixOrCellExpr(MultiExprArg Mats, bool isCell,
		SourceLocation LLoc, SourceLocation RLoc) {
	if(isCell)
		return BuildCellArray(Mats, LLoc, RLoc);

	return BuildMatrix(Mats, LLoc, RLoc);
}

// Binary/Unary Operators.  'Tok' is the token for the operator.
ExprResult Sema::CreateBuiltinUnaryOp(SourceLocation OpLoc, unsigned OpcIn,
		Expr *InputArg) {
	return ExprError();
}

ExprResult Sema::BuildUnaryOp(Scope *S, SourceLocation OpLoc,
		UnaryOperatorKind Opc, Expr *Input) {
	ExprValueKind VK = VK_RValue;
	Type resultType;
	switch (Opc) {
	case UO_PreInc:
	case UO_PreDec:
	case UO_PostInc:
	case UO_PostDec:
//		resultType = CheckIncrementDecrementOperand(*this, Input, VK, OpLoc,
//				Opc == UO_PreInc || Opc == UO_PostInc, Opc == UO_PreInc || Opc
//						== UO_PreDec);
		break;
	case UO_Plus:
	case UO_Minus:
		// UsualUnaryConversions(Input);
		resultType = Input->getType();
		//if (resultType->isVectorType())
		break;

//		return ExprError(Diag(OpLoc, diag::err_typecheck_unary_expr)
//				<< resultType << Input->getSourceRange());
	case UO_LNot: // logical negation
		// Unlike +/-/~, integer promotions aren't done here (C99 6.5.3.3p5).
//		DefaultFunctionArrayLvalueConversion(Input);
//		resultType = Input->getType();
//		if (resultType->isScalarType()) { // C99 6.5.3.3p1
//			// ok, fallthrough
//		} else {
//			return ExprError(Diag(OpLoc, diag::err_typecheck_unary_expr)
//					<< resultType << Input->getSourceRange());
//		}

		resultType = Context.LogicalTy;
		break;
	case UO_Quote:
	case UO_Transpose:
		break;
	case UO_Handle:
		if(DefnRefExpr * DRE = cast<DefnRefExpr>(Input)) {
			FunctionDefn *D = cast<FunctionDefn>(DRE->getDefn());
			if(D) {
				resultType = Input->getType();
			}
		} else {
			// TODO Diag for DefnRefExpr after '@' is not a function name
			return ExprError();
		}
		break;
	default:
		assert(0 && "unary operator not exist");
	}
//	if (resultType.isNull())
//		return ExprError();

	return Owned(new (Context) UnaryOperator(Input, Opc, resultType, VK, OpLoc));
}

ExprResult Sema::ActOnUnaryOp(Scope *S, SourceLocation OpLoc,
		tok::TokenKind Op, bool isPostfix, Expr *Input) {
	return BuildUnaryOp(S, OpLoc, ConvertTokenKindToUnaryOpcode(Op, isPostfix), Input);
}

ExprResult Sema::ActOnPostfixUnaryOp(Scope *S, SourceLocation OpLoc,
		tok::TokenKind Kind, Expr *Input) {
	return ExprError();
}

ExprResult Sema::BuildMemberReferenceExpr(Expr *Base, Type BaseType,
		SourceLocation OpLoc, NamedDefn *FirstQualifierInScope,
		const Defn &NameInfo) {
	return ExprError();
}

ExprResult Sema::BuildMemberReferenceExpr(Expr *Base, Type BaseType,
		SourceLocation OpLoc, NamedDefn *FirstQualifierInScope,
		LookupResult &R) {
	return ExprError();
}

ExprResult Sema::LookupMemberExpr(LookupResult &R, Expr *&Base,
		SourceLocation OpLoc) {
	return ExprError();
}

ExprResult Sema::ActOnMemberAccessExpr(Scope *S, ExprResult Base,
		SourceLocation OpLoc, tok::TokenKind OpKind, ExprResult Member,
		bool HasTrailingLParen) {
	return ExprError();
}

bool Sema::ConvertArgumentsForCall(FunctionCall *Call, Expr *Fn,
		FunctionDefn *FDefn, const FunctionProtoType *Proto, Expr **Args,
		unsigned NumArgs, SourceLocation RParenLoc) {
	return false;
}

ExprResult Sema::ActOnArrayIndexingOrFunctionCallExpr(Scope *S, Expr* Fn,
		SourceLocation LParenLoc, MultiExprArg args, SourceLocation RParenLoc) {
	if(isa<DefnRefExpr>(Fn)) {
		DefnRefExpr *Def = cast<DefnRefExpr>(Fn);
		DefinitionNameInfo NameInfo = Def->getNameInfo();

		LookupResult R(*this, NameInfo, LookupOrdinaryName);
		LookupName(R, S, false);
		if(R.empty()) {
			return ExprError();
		}

		assert(R.isSingleResult()&& "should have at least a definition");
		if(VarDefn *Var = R.getAsSingle<VarDefn>()) {
			return ActOnArrayIndexExpr(S, Var, Fn, LParenLoc, args, RParenLoc, false);
		} else {
			FunctionDefn *FnDefn = R.getAsSingle<FunctionDefn>();
			return ActOnFunctionCallExpr(S, FnDefn, Fn, LParenLoc, args, RParenLoc);
		}
	} else
		return ExprError();
}

ExprResult Sema::ActOnArrayIndexExpr(Scope *S, VarDefn *Var, Expr *Base,
		SourceLocation LLoc, MultiExprArg args, SourceLocation RLoc,
		bool isCell) {
	Expr **Args = args.release();
	unsigned NumArgs = args.size();

	// FIXME : we should not use Base type here
	ArrayIndex *Ai = new(getASTContext())ArrayIndex(getASTContext(),
			Base,Args,NumArgs,Base->getType(),mlang::VK_LValue,RLoc, isCell);

	if(Ai==NULL)
		return ExprError();

	return Owned(Ai);
}

ExprResult Sema::CreateBuiltinArrayIndexExpr(Expr *Base, SourceLocation LLoc,
		Expr *Idx, SourceLocation RLoc) {
	return ExprError();
}

ExprResult Sema::ActOnFunctionCallExpr(Scope *S, FunctionDefn *FnD, Expr* Fn,
		SourceLocation LParenLoc, MultiExprArg args, SourceLocation RParenLoc) {
	unsigned NumArgs = args.size();
	Expr **Args = args.release();

	FunctionCall *FNCall = new(getASTContext())FunctionCall(getASTContext(),
			Fn,Args,NumArgs,FnD->getType(),mlang::VK_RValue,RParenLoc);
	if(FNCall==NULL)
		return ExprError();

	return Owned(FNCall);
}

ExprResult Sema::BuildResolvedCallExpr(Expr *Fn, NamedDefn *NDefn,
		SourceLocation LParenLoc, Expr **Args, unsigned NumArgs,
		SourceLocation RParenLoc) {
	return ExprError();
}

ExprResult Sema::MaybeConvertParenListExprToParenExpr(Scope *S, Expr *ME) {
	return ExprError();
}

ExprResult Sema::ActOnCastOfParenListExpr(Scope *S, SourceLocation LParenLoc,
		SourceLocation RParenLoc, Expr *E) {
	return ExprError();
}

ExprResult Sema::ActOnImaginaryLiteralExpr(SourceLocation LParenLoc, Type Ty,
		SourceLocation RParenLoc, Expr *Op) {
	return ExprError();
}

ExprResult Sema::BuildImaginaryLiteralExpr(SourceLocation LParenLoc,
		SourceLocation RParenLoc, Expr *InitExpr) {
	return ExprError();
}

ExprResult Sema::ActOnInitList(SourceLocation LParenLoc, MultiExprArg InitList,
		SourceLocation RParenLoc) {
	return ExprError();
}

ExprResult Sema::ActOnBinOp(Scope *S, SourceLocation TokLoc,
		tok::TokenKind Kind, Expr *LHS, Expr *RHS) {
	BinaryOperatorKind Opc = ConvertTokenKindToBinaryOpcode(Kind);
	ASTContext &ctx = getASTContext();
	switch(Kind) {
	case tok::Asterisk:
	case tok::DotAsterisk:
	case tok::Slash:
	case tok::DotSlash:
	case tok::BackSlash:
	case tok::DotBackSlash:
	case tok::Caret:
	case tok::DotCaret:
	case tok::Addition:
	case tok::Subtraction:
	case tok::ShiftLeft:
	case tok::ShiftRight:
	case tok::LessThan:
	case tok::LessThanOREqualTo:
	case tok::GreaterThan:
	case tok::GreaterThanOREqualTo:
	case tok::EqualTo:
	case tok::NotEqualTo:
	case tok::EleWiseAND:
	case tok::EleWiseOR:
	case tok::ShortCircuitAND:
	case tok::ShortCircuitOR:
	case tok::Assignment:
	case tok::Comma: {
		return BuildBinOp(S, TokLoc, Opc, LHS, RHS);
	}
	case tok::MatPowerAssign:
	case tok::MatMulAssign:
	case tok::MatRDivAssign:
	case tok::MatLDivAssign:
	case tok::PowerAssign:
	case tok::MulAssign:
	case tok::RDivAssign:
	case tok::LDivAssign:
	case tok::AddAssign:
	case tok::SubAssign:
	case tok::ShlAssign:
	case tok::ShrAssign:
	case tok::AndAssign:
	case tok::OrAssign: {
		CompoundAssignOperator *BO =
				new (ctx) CompoundAssignOperator(LHS, RHS, Opc,
						LHS->getType(), LHS->getType(), LHS->getType(), TokLoc);
		return Owned(BO);
	}
	case tok::Colon:
		return BuildColonExpr(S, TokLoc, TokLoc, LHS, NULL, RHS);
	default:
		return ExprError();
	}
}

ExprResult Sema::ActOnTernaryColonOp(Scope *S, SourceLocation FirstColonLoc,
			SourceLocation SecondColonLoc, Expr *Base, Expr *Inc, Expr *Limit) {
	return BuildColonExpr(S, FirstColonLoc, SecondColonLoc, Base, Inc, Limit);
}

ExprResult Sema::BuildColonExpr(Scope *S, SourceLocation FirstColonLoc,
		SourceLocation SecondColonLoc, Expr *Base, Expr *Inc, Expr *Limit) {
	ASTContext &ctx = getASTContext();
	ColonExpr *BO =	new (ctx) ColonExpr(ctx, Base, Inc, Limit,
			Base->getType(), FirstColonLoc, SecondColonLoc);

	return Owned(BO);
}

/// CreateBuiltinBinOp - Creates a new built-in binary operation with
/// operator @p Opc at location @c TokLoc. This routine only supports
/// built-in operations; ActOnBinOp handles overloaded operators.
ExprResult Sema::CreateBuiltinBinOp(SourceLocation OpLoc,
                                    BinaryOperatorKind Opc,
                                    Expr *lhsExpr, Expr *rhsExpr) {
  ExprResult lhs = Owned(lhsExpr), rhs = Owned(rhsExpr);
  Type ResultTy;     // Result type of the binary operator.
  // The following two variables are used for compound assignment operators
  Type CompLHSTy;    // Type of LHS after promotions for computation
  Type CompResultTy; // Type of computation result
  ExprValueKind VK = VK_RValue;

  switch (Opc) {
  case BO_Assign:
    ResultTy = CheckAssignmentOperands(lhs.get(), rhs, OpLoc, Type());
    if (getLangOptions().OOP) {
      VK = lhs.get()->getValueKind();
    }
//    if (!ResultTy.isNull())
//      DiagnoseSelfAssignment(*this, lhs.get(), rhs.get(), OpLoc);
    break;
  case BO_Power:
  case BO_MatrixPower:
    ResultTy = CheckPowerOperands(lhs, rhs, OpLoc,
                                  Opc == BO_Power);
    break;
  case BO_Mul:
  case BO_RDiv:
  case BO_LDiv:
  case BO_MatrixMul:
  case BO_MatrixRDiv:
  case BO_MatrixLDiv:
    ResultTy = CheckMultiplyDivideOperands(lhs, rhs, OpLoc, false,
                                           Opc == BO_RDiv || Opc == BO_LDiv);
    break;
  case BO_Add:
    ResultTy = CheckAdditionOperands(lhs, rhs, OpLoc);
    break;
  case BO_Sub:
    ResultTy = CheckSubtractionOperands(lhs, rhs, OpLoc);
    break;
  case BO_Shl:
  case BO_Shr:
    ResultTy = CheckShiftOperands(lhs, rhs, OpLoc, Opc);
    break;
  case BO_LE:
  case BO_LT:
  case BO_GE:
  case BO_GT:
    ResultTy = CheckCompareOperands(lhs, rhs, OpLoc, Opc, true);
    break;
  case BO_EQ:
  case BO_NE:
    ResultTy = CheckCompareOperands(lhs, rhs, OpLoc, Opc, false);
    break;
  case BO_And:
  case BO_Or:
    ResultTy = CheckBitwiseOperands(lhs, rhs, OpLoc);
    break;
  case BO_LAnd:
  case BO_LOr:
    ResultTy = CheckLogicalOperands(lhs, rhs, OpLoc, Opc);
    break;
  case BO_MatPowerAssign:
  case BO_PowerAssign:
	  ResultTy = CheckPowerOperands(lhs, rhs, OpLoc,
	                                Opc == BO_PowerAssign);
	  break;
  case BO_MatMulAssign:
  case BO_MatRDivAssign:
  case BO_MatLDivAssign:
  case BO_MulAssign:
  case BO_RDivAssign:
  case BO_LDivAssign:
    CompResultTy = CheckMultiplyDivideOperands(lhs, rhs, OpLoc, true,
    		Opc == BO_MulAssign || Opc == BO_RDivAssign || Opc == BO_LDivAssign );
    CompLHSTy = CompResultTy;
    if (!CompResultTy.isNull() && !lhs.isInvalid() && !rhs.isInvalid())
      ResultTy = CheckAssignmentOperands(lhs.get(), rhs, OpLoc, CompResultTy);
    break;
  case BO_AddAssign:
    CompResultTy = CheckAdditionOperands(lhs, rhs, OpLoc, &CompLHSTy);
    if (!CompResultTy.isNull() && !lhs.isInvalid() && !rhs.isInvalid())
      ResultTy = CheckAssignmentOperands(lhs.get(), rhs, OpLoc, CompResultTy);
    break;
  case BO_SubAssign:
    CompResultTy = CheckSubtractionOperands(lhs, rhs, OpLoc, &CompLHSTy);
    if (!CompResultTy.isNull() && !lhs.isInvalid() && !rhs.isInvalid())
      ResultTy = CheckAssignmentOperands(lhs.get(), rhs, OpLoc, CompResultTy);
    break;
  case BO_ShlAssign:
  case BO_ShrAssign:
    CompResultTy = CheckShiftOperands(lhs, rhs, OpLoc, Opc, true);
    CompLHSTy = CompResultTy;
    if (!CompResultTy.isNull() && !lhs.isInvalid() && !rhs.isInvalid())
      ResultTy = CheckAssignmentOperands(lhs.get(), rhs, OpLoc, CompResultTy);
    break;
  case BO_AndAssign:
  case BO_OrAssign:
    CompResultTy = CheckBitwiseOperands(lhs, rhs, OpLoc, true);
    CompLHSTy = CompResultTy;
    if (!CompResultTy.isNull() && !lhs.isInvalid() && !rhs.isInvalid())
      ResultTy = CheckAssignmentOperands(lhs.get(), rhs, OpLoc, CompResultTy);
    break;
  case BO_Colon:
	  ResultTy = CheckColonOperands(lhs, rhs, OpLoc);
	  break;
  case BO_Comma:
    ResultTy = CheckCommaOperands(*this, lhs, rhs, OpLoc);
    if (getLangOptions().OOP && !rhs.isInvalid()) {
      VK = rhs.get()->getValueKind();
    }
    break;
  }
  if (ResultTy.isNull() || lhs.isInvalid() || rhs.isInvalid())
    return ExprError();
  if (CompResultTy.isNull())
    return Owned(new (Context) BinaryOperator(lhs.take(), rhs.take(), Opc,
                                              ResultTy, VK, OpLoc));
  if (getLangOptions().OOP) {
    VK = VK_LValue;
  }
  return Owned(new (Context) CompoundAssignOperator(lhs.take(), rhs.take(), Opc,
                                                    ResultTy, /*VK, */CompLHSTy,
                                                    CompResultTy, OpLoc));
}

ExprResult Sema::BuildBinOp(Scope *S, SourceLocation OpLoc,
		BinaryOperatorKind Opc, Expr *lhs, Expr *rhs) {
//	ASTContext &ctx = getASTContext();
//	BinaryOperator *BO = new (ctx) BinaryOperator(lhs, rhs, Opc,
//			lhs->getType(), mlang::VK_RValue, OpLoc);
//	return Owned(BO);

	// -----------------
	// left for codes which deal with overloadable binary operator
	//------------------

	// Build a built-in binary operation.
	return CreateBuiltinBinOp(OpLoc, Opc, lhs, rhs);
}

ExprResult Sema::ActOnStmtExpr(SourceLocation LPLoc, Stmt *SubStmt,
		SourceLocation RPLoc) {
	return ExprError();
}

Expr *Sema::ActOnClassPropertyRefExpr(IdentifierInfo &receiverName,
		IdentifierInfo &propertyName, SourceLocation receiverNameLoc,
		SourceLocation propertyNameLoc) {
  return NULL;
}

Expr *Sema::ActOnClassMessage(Scope *S, Type Receiver,
		SourceLocation LBracLoc, SourceLocation SelectorLoc,
		SourceLocation RBracLoc, MultiExprArg Args) {
  return NULL;
}

bool Sema::CheckBooleanCondition(Expr *&CondExpr, SourceLocation Loc) {
  return false;
}

ExprResult Sema::ActOnBooleanCondition(Scope *S, SourceLocation Loc,
		Expr *SubExpr) {
	return ExprError();
}

void Sema::DiagnoseAssignmentAsCondition(Expr *E) {

}

bool Sema::CheckCXXBooleanCondition(Expr *&CondExpr) {
  return false;
}
