//===--- ExprConstant.cpp - Expression Constant Evaluator -----------------===//
//
// Copyright (C) 2010 Yabin Hu @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements the Expr constant evaluator.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/APValue.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/CharUnits.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/AST/StmtVisitor.h"
#include "mlang/AST/ASTDiagnostic.h"
#include "mlang/AST/Expr.h"
#include "mlang/Basic/Builtins.h"
#include "mlang/Basic/TargetInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstring>

using namespace mlang;
using llvm::APSInt;
using llvm::APFloat;

/// EvalInfo - This is a private struct used by the evaluator to capture
/// information about a subexpression as it is folded.  It retains information
/// about the AST context, but also maintains information about the folded
/// expression.
///
/// If an expression could be evaluated, it is still possible it is not a C
/// "integer constant expression" or constant expression.  If not, this struct
/// captures information about how and why not.
///
/// One bit of information passed *into* the request for constant folding
/// indicates whether the subexpression is "evaluated" or not according to C
/// rules.  For example, the RHS of (0 && foo()) is not evaluated.  We can
/// evaluate the expression regardless of what the RHS is, but C only allows
/// certain things in certain situations.
struct EvalInfo {
  ASTContext &Ctx;

  /// EvalResult - Contains information about the evaluation.
  Expr::EvalResult &EvalResult;

  EvalInfo(ASTContext &ctx, Expr::EvalResult& evalresult)
    : Ctx(ctx), EvalResult(evalresult) {}
};

namespace {
  struct ComplexValue {
  private:
    bool IsInt;

  public:
    APSInt IntReal, IntImag;
    APFloat FloatReal, FloatImag;

    ComplexValue() : FloatReal(APFloat::Bogus), FloatImag(APFloat::Bogus) {}

    void makeComplexFloat() { IsInt = false; }
    bool isComplexFloat() const { return !IsInt; }
    APFloat &getComplexFloatReal() { return FloatReal; }
    APFloat &getComplexFloatImag() { return FloatImag; }

    void makeComplexInt() { IsInt = true; }
    bool isComplexInt() const { return IsInt; }
    APSInt &getComplexIntReal() { return IntReal; }
    APSInt &getComplexIntImag() { return IntImag; }

    void moveInto(APValue &v) {
      if (isComplexFloat())
        v = APValue(FloatReal, FloatImag);
      else
        v = APValue(IntReal, IntImag);
    }
  };

  struct LValue {
    Expr *Base;
    CharUnits Offset;

    Expr *getLValueBase() { return Base; }
    CharUnits getLValueOffset() { return Offset; }

    void moveInto(APValue &v) {
      v = APValue(Base, Offset);
    }
  };
}

static bool EvaluateLValue(const Expr *E, LValue &Result, EvalInfo &Info);
static bool EvaluateInteger(const Expr *E, APSInt  &Result, EvalInfo &Info);
static bool EvaluateIntegerOrLValue(const Expr *E, APValue  &Result,
                                    EvalInfo &Info);
static bool EvaluateFloat(const Expr *E, APFloat &Result, EvalInfo &Info);
static bool EvaluateComplex(const Expr *E, ComplexValue &Res, EvalInfo &Info);

//===----------------------------------------------------------------------===//
// Misc utilities
//===----------------------------------------------------------------------===//
static bool IsGlobalLValue(const Expr* E) {
  if (!E) return true;

  if (const DefnRefExpr *DRE = dyn_cast<DefnRefExpr>(E)) {
    if (isa<FunctionDefn>(DRE->getDefn()))
      return true;
    if (const VarDefn *VD = dyn_cast<VarDefn>(DRE->getDefn()))
      return VD->hasGlobalStorage();
    return false;
  }

  return true;
}

static bool HandleConversionToBool(const Expr* E, bool& Result,
                                   EvalInfo &Info) {
  if (E->getType()->isIntegerType()) {
    APSInt IntResult;
    if (!EvaluateInteger(E, IntResult, Info))
      return false;
    Result = IntResult != 0;
    return true;
  } else if (E->getType()->isFloatingPointType()) {
	  if(E->getType().hasComplexAttr()){
		  ComplexValue ComplexResult;
		  if (!EvaluateComplex(E, ComplexResult, Info))
			  return false;
		  if (ComplexResult.isComplexFloat()) {
			  Result = !ComplexResult.getComplexFloatReal().isZero() ||
					  !ComplexResult.getComplexFloatImag().isZero();
		  } else {
			  Result = ComplexResult.getComplexIntReal().getBoolValue() ||
					  ComplexResult.getComplexIntImag().getBoolValue();
		      }
		  return true;
	  } else {
		  APFloat FloatResult(0.0);
		  if (!EvaluateFloat(E, FloatResult, Info))
			  return false;
		  Result = !FloatResult.isZero();
		  return true;
	  }
  }
  return false;
}

namespace {
class HasSideEffect
  : public StmtVisitor<HasSideEffect, bool> {
  EvalInfo &Info;
public:

  HasSideEffect(EvalInfo &info) : Info(info) {}

  // Unhandled nodes conservatively default to having side effects.
  bool VisitStmt(Stmt *S) {
    return true;
  }

  bool VisitParenExpr(ParenExpr *E) { return Visit(E->getSubExpr()); }
  bool VisitDefnRefExpr(DefnRefExpr *E) {
//    if (Info.Ctx.getCanonicalType(E->getType()).isVolatileQualified())
//      return true;
    return false;
  }
  // We don't want to evaluate BlockCmds multiple times, as they generate
  // a ton of codes.
  bool VisitMemberExpr(MemberExpr *E) { return Visit(E->getBase()); }
  bool VisitIntegerLiteral(IntegerLiteral *E) { return false; }
  bool VisitFloatingLiteral(FloatingLiteral *E) { return false; }
  bool VisitStringLiteral(StringLiteral *E) { return false; }
  bool VisitCharacterLiteral(CharacterLiteral *E) { return false; }
  bool VisitArrayIndex(ArrayIndex *E)
    { return false/*Visit(E->getLHS()) || Visit(E->getRHS())*/; }
  bool VisitBinAssign(BinaryOperator *E) { return true; }
  bool VisitCompoundAssignOperator(BinaryOperator *E) { return true; }
  bool VisitBinaryOperator(BinaryOperator *E)
  { return Visit(E->getLHS()) || Visit(E->getRHS()); }
  bool VisitUnaryPreInc(UnaryOperator *E) { return true; }
  bool VisitUnaryPostInc(UnaryOperator *E) { return true; }
  bool VisitUnaryPreDec(UnaryOperator *E) { return true; }
  bool VisitUnaryPostDec(UnaryOperator *E) { return true; }
  bool VisitUnaryOperator(UnaryOperator *E) { return Visit(E->getSubExpr()); }
};
} // end anonymous namespace

//===----------------------------------------------------------------------===//
// LValue Evaluation
//===----------------------------------------------------------------------===//
namespace {
class LValueExprEvaluator
  : public StmtVisitor<LValueExprEvaluator, bool> {
  EvalInfo &Info;
  LValue &Result;

  bool Success(Expr *E) {
    Result.Base = E;
    Result.Offset = CharUnits::Zero();
    return true;
  }
public:

  LValueExprEvaluator(EvalInfo &info, LValue &Result) :
    Info(info), Result(Result) {}

  bool VisitStmt(Stmt *S) {
    return false;
  }
  
  bool VisitParenExpr(ParenExpr *E) { return Visit(E->getSubExpr()); }
  bool VisitDefnRefExpr(DefnRefExpr *E);
  bool VisitMemberExpr(MemberExpr *E);
  bool VisitStringLiteral(StringLiteral *E) { return Success(E); }
  bool VisitArrayIndex(ArrayIndex *E);
  // FIXME: Missing: __real__, __imag__
};
} // end anonymous namespace

static bool EvaluateLValue(const Expr* E, LValue& Result, EvalInfo &Info) {
  return LValueExprEvaluator(Info, Result).Visit(const_cast<Expr*>(E));
}

bool LValueExprEvaluator::VisitDefnRefExpr(DefnRefExpr *E) {
  if (isa<FunctionDefn>(E->getDefn())) {
    return Success(E);
  } else if (VarDefn* VD = dyn_cast<VarDefn>(E->getDefn())) {
//    if (!VD->getType()->isReferenceType())
//      return Success(E);
    // Reference parameters can refer to anything even if they have an
    // "initializer" in the form of a default argument.
    if (isa<ParamVarDefn>(VD))
      return false;
    // FIXME: Check whether VD might be overridden!
    if (const Expr *Init = VD->getInit())
      return Visit(const_cast<Expr *>(Init));
  }

  return false;
}

bool LValueExprEvaluator::VisitMemberExpr(MemberExpr *E) {
  Type Ty;
  if (!Visit(E->getBase()))
	  return false;

  Ty = E->getBase()->getType();
#if 0
  UserClassDefn *RD = Ty->getAs<ClassdefType>()->getDefn();
  const ASTRecordLayout &RL = Info.Ctx.getASTRecordLayout(RD);

  MemberDefn *FD = dyn_cast<MemberDefn>(E->getMemberDefn());
  if (!FD) // FIXME: deal with other kinds of member expressions
    return false;

  if (FD->getType()->isReferenceType())
    return false;

  // FIXME: This is linear time.
  unsigned i = 0;
  for (TypeDefn::field_iterator Field = RD->field_begin(),
                               FieldEnd = RD->field_end();
       Field != FieldEnd; (void)++Field, ++i) {
    if (*Field == FD)
      break;
  }

  Result.Offset += CharUnits::fromQuantity(RL.getFieldOffset(i) / 8);
#endif

  return true;
}

bool LValueExprEvaluator::VisitArrayIndex(ArrayIndex *E) {
  APSInt Index;
#if 0
  if (!EvaluateInteger(E->getIdx(), Index, Info))
    return false;
#endif

  CharUnits ElementSize = Info.Ctx.getTypeSizeInChars(E->getType());
  Result.Offset += Index.getSExtValue() * ElementSize;
  return true;
}

//===----------------------------------------------------------------------===//
// Integer Evaluation
//===----------------------------------------------------------------------===//
namespace {
class IntExprEvaluator
  : public StmtVisitor<IntExprEvaluator, bool> {
  EvalInfo &Info;
  APValue &Result;
public:
  IntExprEvaluator(EvalInfo &info, APValue &result)
    : Info(info), Result(result) {}

  bool Success(const llvm::APSInt &SI, const Expr *E) {
    assert(E->getType()->isIntegerType() &&
           "Invalid evaluation result.");
    assert(SI.isSigned() == E->getType()->isSignedIntegerType() &&
           "Invalid evaluation result.");
    assert(SI.getBitWidth() == Info.Ctx.getIntWidth(E->getType()) &&
           "Invalid evaluation result.");
    Result = APValue(SI);
    return true;
  }

  bool Success(const llvm::APInt &I, const Expr *E) {
    assert(E->getType()->isIntegerType() &&
           "Invalid evaluation result.");
    assert(I.getBitWidth() == Info.Ctx.getIntWidth(E->getType()) &&
           "Invalid evaluation result.");
    Result = APValue(APSInt(I));
    Result.getInt().setIsUnsigned(E->getType()->isUnsignedIntegerType());
    return true;
  }

  bool Success(uint64_t Value, const Expr *E) {
    assert(E->getType()->isIntegerType() &&
           "Invalid evaluation result.");
    Result = APValue(Info.Ctx.MakeIntValue(Value, E->getType()));
    return true;
  }

  bool Error(SourceLocation L, diag::kind D, const Expr *E) {
    // Take the first error.
    if (Info.EvalResult.Diag == 0) {
      Info.EvalResult.DiagLoc = L;
      Info.EvalResult.Diag = D;
      Info.EvalResult.DiagExpr = E;
    }
    return false;
  }

  //===--------------------------------------------------------------------===//
  //                            Visitor Methods
  //===--------------------------------------------------------------------===//

  bool VisitStmt(Stmt *) {
    assert(0 && "This should be called on integers, stmts are not integers");
    return false;
  }

  bool VisitExpr(Expr *E) {
    return Error(E->getLocStart(), diag::note_invalid_subexpr_in_ice, E);
  }

  bool VisitParenExpr(ParenExpr *E) { return Visit(E->getSubExpr()); }

  bool VisitIntegerLiteral(const IntegerLiteral *E) {
    return Success(E->getValue(), E);
  }
  bool VisitCharacterLiteral(const CharacterLiteral *E) {
    return Success(E->getValue(), E);
  }

  bool CheckReferencedDefn(const Expr *E, const Defn *D);
  bool VisitDefnRefExpr(const DefnRefExpr *E) {
    return CheckReferencedDefn(E, E->getDefn());
  }
  bool VisitMemberExpr(const MemberExpr *E) {
    if (CheckReferencedDefn(E, E->getMemberDefn())) {
      // Conservatively assume a MemberExpr will have side-effects
      Info.EvalResult.HasSideEffects = true;
      return true;
    }
    return false;
  }

  bool VisitFunctionCall(FunctionCall *E);
  bool VisitBinaryOperator(const BinaryOperator *E);
  bool VisitUnaryOperator(const UnaryOperator *E);
  bool VisitUnaryReal(const UnaryOperator *E);
  bool VisitUnaryImag(const UnaryOperator *E);
    
private:
  CharUnits GetAlignOfExpr(const Expr *E);
  CharUnits GetAlignOfType(Type T);
  static Type GetObjectType(const Expr *E);
  bool TryEvaluateBuiltinObjectSize(FunctionCall *E);
  // FIXME: Missing: array subscript of vector, member of vector
};
} // end anonymous namespace

static bool EvaluateIntegerOrLValue(const Expr* E, APValue &Result, EvalInfo &Info) {
  assert(E->getType()->isIntegerType());
  return IntExprEvaluator(Info, Result).Visit(const_cast<Expr*>(E));
}

static bool EvaluateInteger(const Expr* E, APSInt &Result, EvalInfo &Info) {
  assert(E->getType()->isIntegerType());

  APValue Val;
  if (!EvaluateIntegerOrLValue(E, Val, Info) || !Val.isInt())
    return false;
  Result = Val.getInt();
  return true;
}

bool IntExprEvaluator::CheckReferencedDefn(const Expr* E, const Defn* D) {
#if 0
	// In C++, const, non-volatile integers initialized with ICEs are ICEs.
  // In C, they can also be folded, although they are not ICEs.
  if (Info.Ctx.getCanonicalType(E->getType()).getCVRQualifiers() 
                                                        == Qualifiers::Const) {

    if (isa<ParamVarDefn>(D))
      return Error(E->getLocStart(), diag::note_invalid_subexpr_in_ice, E);

    if (const VarDefn *VD = dyn_cast<VarDefn>(D)) {
      if (const Expr *Init = VD->getAnyInitializer()) {
        if (APValue *V = VD->getEvaluatedValue()) {
          if (V->isInt())
            return Success(V->getInt(), E);
          return Error(E->getLocStart(), diag::note_invalid_subexpr_in_ice, E);
        }

        if (VD->isEvaluatingValue())
          return Error(E->getLocStart(), diag::note_invalid_subexpr_in_ice, E);

        VD->setEvaluatingValue();

        Expr::EvalResult EResult;
        if (Init->Evaluate(EResult, Info.Ctx) && !EResult.HasSideEffects &&
            EResult.Val.isInt()) {
          // Cache the evaluated value in the variable declaration.
          Result = EResult.Val;
          VD->setEvaluatedValue(Result);
          return true;
        }

        VD->setEvaluatedValue(APValue());
      }
    }
  }
#endif

  // Otherwise, random variable references are not constants.
  return Error(E->getLocStart(), diag::note_invalid_subexpr_in_ice, E);
}

/// Retrieves the "underlying object type" of the given expression,
/// as used by __builtin_object_size.
Type IntExprEvaluator::GetObjectType(const Expr *E) {
  if (const DefnRefExpr *DRE = dyn_cast<DefnRefExpr>(E)) {
    if (const VarDefn *VD = dyn_cast<VarDefn>(DRE->getDefn()))
      return VD->getType();
  }

  return Type();
}

bool IntExprEvaluator::TryEvaluateBuiltinObjectSize(FunctionCall *E) {
  // TODO: Perhaps we should let LLVM lower this?
  LValue Base;

  // If we can prove the base is null, lower to zero now.
  const Expr *LVBase = Base.getLValueBase();
  if (!LVBase) return Success(0, E);

  Type T = GetObjectType(LVBase);
  if (T.isNull())
    return false;

  CharUnits Size = Info.Ctx.getTypeSizeInChars(T);
  CharUnits Offset = Base.getLValueOffset();

  if (!Offset.isNegative() && Offset <= Size)
    Size -= Offset;
  else
    Size = CharUnits::Zero();
  return Success(Size.getQuantity(), E);
}

bool IntExprEvaluator::VisitFunctionCall(FunctionCall *E) {
  switch (E->isBuiltinCall(Info.Ctx)) {
  default:
    return Error(E->getLocStart(), diag::note_invalid_subexpr_in_ice, E);

  case Builtin::BIstrlen:
    // As an extension, we support strlen() and __builtin_strlen() as constant
    // expressions when the argument is a string literal.
    if (StringLiteral *S
               = dyn_cast<StringLiteral>(E->getArg(0)->IgnoreParenImpCasts())) {
      // The string literal may have embedded null characters. Find the first
      // one and truncate there.
      llvm::StringRef Str = S->getString();
      llvm::StringRef::size_type Pos = Str.find(0);
      if (Pos != llvm::StringRef::npos)
        Str = Str.substr(0, Pos);
      
      return Success(Str.size(), E);
    }
      
    return Error(E->getLocStart(), diag::note_invalid_subexpr_in_ice, E);
  }
}

bool IntExprEvaluator::VisitBinaryOperator(const BinaryOperator *E) {
  if (E->getOpcode() == BO_Comma) {
    if (!Visit(E->getRHS()))
      return false;

    // If we can't evaluate the LHS, it might have side effects;
    // conservatively mark it.
    if (!E->getLHS()->isEvaluatable(Info.Ctx))
      Info.EvalResult.HasSideEffects = true;

    return true;
  }

  if (E->isLogicalOp()) {
    // These need to be handled specially because the operands aren't
    // necessarily integral
    bool lhsResult, rhsResult;

    if (HandleConversionToBool(E->getLHS(), lhsResult, Info)) {
      // We were able to evaluate the LHS, see if we can get away with not
      // evaluating the RHS: 0 && X -> 0, 1 || X -> 1
      if (lhsResult == (E->getOpcode() == BO_LOr))
        return Success(lhsResult, E);

      if (HandleConversionToBool(E->getRHS(), rhsResult, Info)) {
        if (E->getOpcode() == BO_LOr)
          return Success(lhsResult || rhsResult, E);
        else
          return Success(lhsResult && rhsResult, E);
      }
    } else {
      if (HandleConversionToBool(E->getRHS(), rhsResult, Info)) {
        // We can't evaluate the LHS; however, sometimes the result
        // is determined by the RHS: X && 0 -> 0, X || 1 -> 1.
        if (rhsResult == (E->getOpcode() == BO_LOr) ||
            !rhsResult == (E->getOpcode() == BO_LAnd)) {
          // Since we weren't able to evaluate the left hand side, it
          // must have had side effects.
          Info.EvalResult.HasSideEffects = true;

          return Success(rhsResult, E);
        }
      }
    }

    return false;
  }

  Type LHSTy = E->getLHS()->getType();
  Type RHSTy = E->getRHS()->getType();

  if (LHSTy.hasComplexAttr()) {
    assert(RHSTy.hasComplexAttr() && "Invalid comparison");
    ComplexValue LHS, RHS;

    if (!EvaluateComplex(E->getLHS(), LHS, Info))
      return false;

    if (!EvaluateComplex(E->getRHS(), RHS, Info))
      return false;

    if (LHS.isComplexFloat()) {
      APFloat::cmpResult CR_r =
        LHS.getComplexFloatReal().compare(RHS.getComplexFloatReal());
      APFloat::cmpResult CR_i =
        LHS.getComplexFloatImag().compare(RHS.getComplexFloatImag());

      if (E->getOpcode() == BO_EQ)
        return Success((CR_r == APFloat::cmpEqual &&
                        CR_i == APFloat::cmpEqual), E);
      else {
        assert(E->getOpcode() == BO_NE &&
               "Invalid complex comparison.");
        return Success(((CR_r == APFloat::cmpGreaterThan ||
                         CR_r == APFloat::cmpLessThan ||
                         CR_r == APFloat::cmpUnordered) ||
                        (CR_i == APFloat::cmpGreaterThan ||
                         CR_i == APFloat::cmpLessThan ||
                         CR_i == APFloat::cmpUnordered)), E);
      }
    } else {
      if (E->getOpcode() == BO_EQ)
        return Success((LHS.getComplexIntReal() == RHS.getComplexIntReal() &&
                        LHS.getComplexIntImag() == RHS.getComplexIntImag()), E);
      else {
        assert(E->getOpcode() == BO_NE &&
               "Invalid compex comparison.");
        return Success((LHS.getComplexIntReal() != RHS.getComplexIntReal() ||
                        LHS.getComplexIntImag() != RHS.getComplexIntImag()), E);
      }
    }
  }

  if (LHSTy->isFloatingPointType() &&
      RHSTy->isFloatingPointType()) {
    APFloat RHS(0.0), LHS(0.0);

    if (!EvaluateFloat(E->getRHS(), RHS, Info))
      return false;

    if (!EvaluateFloat(E->getLHS(), LHS, Info))
      return false;

    APFloat::cmpResult CR = LHS.compare(RHS);

    switch (E->getOpcode()) {
    default:
      assert(0 && "Invalid binary operator!");
    case BO_LT:
      return Success(CR == APFloat::cmpLessThan, E);
    case BO_GT:
      return Success(CR == APFloat::cmpGreaterThan, E);
    case BO_LE:
      return Success(CR == APFloat::cmpLessThan || CR == APFloat::cmpEqual, E);
    case BO_GE:
      return Success(CR == APFloat::cmpGreaterThan || CR == APFloat::cmpEqual,
                     E);
    case BO_EQ:
      return Success(CR == APFloat::cmpEqual, E);
    case BO_NE:
      return Success(CR == APFloat::cmpGreaterThan
                     || CR == APFloat::cmpLessThan
                     || CR == APFloat::cmpUnordered, E);
    }
  }

  if (!LHSTy->isIntegerType() ||
      !RHSTy->isIntegerType()) {
    // We can't continue from here for non-integral types, and they
    // could potentially confuse the following operations.
    return false;
  }

  // The LHS of a constant expr is always evaluated and needed.
  if (!Visit(E->getLHS()))
    return false; // error in subexpression.

  APValue RHSVal;
  if (!EvaluateIntegerOrLValue(E->getRHS(), RHSVal, Info))
    return false;

  // Handle cases like (unsigned long)&a + 4.
  if (E->isAdditiveOp() && Result.isLValue() && RHSVal.isInt()) {
    CharUnits Offset = Result.getLValueOffset();
    CharUnits AdditionalOffset = CharUnits::fromQuantity(
                                     RHSVal.getInt().getZExtValue());
    if (E->getOpcode() == BO_Add)
      Offset += AdditionalOffset;
    else
      Offset -= AdditionalOffset;
    Result = APValue(Result.getLValueBase(), Offset);
    return true;
  }

  // Handle cases like 4 + (unsigned long)&a
  if (E->getOpcode() == BO_Add &&
        RHSVal.isLValue() && Result.isInt()) {
    CharUnits Offset = RHSVal.getLValueOffset();
    Offset += CharUnits::fromQuantity(Result.getInt().getZExtValue());
    Result = APValue(RHSVal.getLValueBase(), Offset);
    return true;
  }

  // All the following cases expect both operands to be an integer
  if (!Result.isInt() || !RHSVal.isInt())
    return false;

  APSInt& RHS = RHSVal.getInt();

  switch (E->getOpcode()) {
  default:
    return Error(E->getOperatorLoc(), diag::note_invalid_subexpr_in_ice, E);
  case BO_Mul: return Success(Result.getInt() * RHS, E);
  case BO_Add: return Success(Result.getInt() + RHS, E);
  case BO_Sub: return Success(Result.getInt() - RHS, E);
  case BO_And: return Success(Result.getInt() & RHS, E);
  case BO_Or:  return Success(Result.getInt() | RHS, E);
  case BO_LDiv:
    if (RHS == 0)
      return Error(E->getOperatorLoc(), diag::note_expr_divide_by_zero, E);
    return Success(Result.getInt() / RHS, E);
  case BO_RDiv:
    if (RHS == 0)
      return Error(E->getOperatorLoc(), diag::note_expr_divide_by_zero, E);
    return Success(Result.getInt() % RHS, E);
  case BO_Shl: {
    // During constant-folding, a negative shift is an opposite shift.
    if (RHS.isSigned() && RHS.isNegative()) {
      RHS = -RHS;
      goto shift_right;
    }

  shift_left:
    unsigned SA
      = (unsigned) RHS.getLimitedValue(Result.getInt().getBitWidth()-1);
    return Success(Result.getInt() << SA, E);
  }
  case BO_Shr: {
    // During constant-folding, a negative shift is an opposite shift.
    if (RHS.isSigned() && RHS.isNegative()) {
      RHS = -RHS;
      goto shift_left;
    }

  shift_right:
    unsigned SA =
      (unsigned) RHS.getLimitedValue(Result.getInt().getBitWidth()-1);
    return Success(Result.getInt() >> SA, E);
  }

  case BO_LT: return Success(Result.getInt() < RHS, E);
  case BO_GT: return Success(Result.getInt() > RHS, E);
  case BO_LE: return Success(Result.getInt() <= RHS, E);
  case BO_GE: return Success(Result.getInt() >= RHS, E);
  case BO_EQ: return Success(Result.getInt() == RHS, E);
  case BO_NE: return Success(Result.getInt() != RHS, E);
  }
}

CharUnits IntExprEvaluator::GetAlignOfType(Type T) {
  // C++ [expr.sizeof]p2: "When applied to a reference or a reference type,
  //   the result is the size of the referenced type."
  // C++ [expr.alignof]p3: "When alignof is applied to a reference type, the
  //   result shall be the alignment of the referenced type."
//  if (const ReferenceType *Ref = T->getAs<ReferenceType>())
//    T = Ref->getPointeeType();

  // Get information about the alignment.
  unsigned CharSize = Info.Ctx.Target.getCharWidth();

  // __alignof is defined to return the preferred alignment.
  return CharUnits::fromQuantity(
      Info.Ctx.getPreferredTypeAlign(T.getRawTypePtr()) / CharSize);
}

CharUnits IntExprEvaluator::GetAlignOfExpr(const Expr *E) {
  E = E->IgnoreParens();

  // alignof decl is always accepted, even if it doesn't make sense: we default
  // to 1 in those cases.
  if (const DefnRefExpr *DRE = dyn_cast<DefnRefExpr>(E))
    return Info.Ctx.getDefnAlign(DRE->getDefn(),
                                 /*RefAsPointee*/true);

  if (const MemberExpr *ME = dyn_cast<MemberExpr>(E))
    return Info.Ctx.getDefnAlign(ME->getMemberDefn(),
                                 /*RefAsPointee*/true);

  return GetAlignOfType(E->getType());
}

bool IntExprEvaluator::VisitUnaryOperator(const UnaryOperator *E) {
//  if (E->getOpcode() == UO_LNot) {
//    // LNot's operand isn't necessarily an integer, so we handle it specially.
//    bool bres;
//    if (!HandleConversionToBool(E->getSubExpr(), bres, Info))
//      return false;
//    return Success(!bres, E);
//  }

  // Only handle integral operations...
  if (!E->getSubExpr()->getType()->isIntegerType())
    return false;

  // Get the operand value into 'Result'.
  if (!Visit(E->getSubExpr()))
    return false;

  switch (E->getOpcode()) {
  default:
    // Address, indirect, pre/post inc/dec, etc are not valid constant exprs.
    // See C99 6.6p3.
    return Error(E->getOperatorLoc(), diag::note_invalid_subexpr_in_ice, E);
  case UO_Plus:
    // The result is always just the subexpr.
    return true;
  case UO_Minus:
    if (!Result.isInt()) return false;
    return Success(-Result.getInt(), E);
  case UO_LNot:
    if (!Result.isInt()) return false;
    return Success(~Result.getInt(), E);
  }
}

bool IntExprEvaluator::VisitUnaryReal(const UnaryOperator *E) {
  if (E->getSubExpr()->getType().hasComplexAttr()) {
    ComplexValue LV;
    if (!EvaluateComplex(E->getSubExpr(), LV, Info) || !LV.isComplexInt())
      return Error(E->getExprLoc(), diag::note_invalid_subexpr_in_ice, E);
    return Success(LV.getComplexIntReal(), E);
  }

  return Visit(E->getSubExpr());
}

bool IntExprEvaluator::VisitUnaryImag(const UnaryOperator *E) {
  if (E->getSubExpr()->getType().hasComplexAttr()) {
    ComplexValue LV;
    if (!EvaluateComplex(E->getSubExpr(), LV, Info) || !LV.isComplexInt())
      return Error(E->getExprLoc(), diag::note_invalid_subexpr_in_ice, E);
    return Success(LV.getComplexIntImag(), E);
  }

  if (!E->getSubExpr()->isEvaluatable(Info.Ctx))
    Info.EvalResult.HasSideEffects = true;
  return Success(0, E);
}

//===----------------------------------------------------------------------===//
// Float Evaluation
//===----------------------------------------------------------------------===//

namespace {
class FloatExprEvaluator
  : public StmtVisitor<FloatExprEvaluator, bool> {
  EvalInfo &Info;
  APFloat &Result;
public:
  FloatExprEvaluator(EvalInfo &info, APFloat &result)
    : Info(info), Result(result) {}

  bool VisitStmt(Stmt *S) {
    return false;
  }

  bool VisitParenExpr(ParenExpr *E) { return Visit(E->getSubExpr()); }
  bool VisitFunctionCall(const FunctionCall *E);

  bool VisitUnaryOperator(const UnaryOperator *E);
  bool VisitBinaryOperator(const BinaryOperator *E);
  bool VisitFloatingLiteral(const FloatingLiteral *E);
  bool VisitUnaryExtension(const UnaryOperator *E)
    { return Visit(E->getSubExpr()); }
  bool VisitUnaryReal(const UnaryOperator *E);
  bool VisitUnaryImag(const UnaryOperator *E);
  bool VisitDefnRefExpr(const DefnRefExpr *E);

  // FIXME: Missing: array subscript of vector, member of vector,
  //                 ImplicitValueInitExpr
};
} // end anonymous namespace

static bool EvaluateFloat(const Expr* E, APFloat& Result, EvalInfo &Info) {
  assert(E->getType()->isFloatingPointType());
  return FloatExprEvaluator(Info, Result).Visit(const_cast<Expr*>(E));
}

static bool TryEvaluateBuiltinNaN(ASTContext &Context,
                                  Type ResultTy,
                                  const Expr *Arg,
                                  bool SNaN,
                                  llvm::APFloat &Result) {
  const StringLiteral *S = dyn_cast<StringLiteral>(Arg->IgnoreParenCasts());
  if (!S) return false;

  const llvm::fltSemantics &Sem = Context.getFloatTypeSemantics(ResultTy);

  llvm::APInt fill;

  // Treat empty strings as if they were zero.
  if (S->getString().empty())
    fill = llvm::APInt(32, 0);
  else if (S->getString().getAsInteger(0, fill))
    return false;

  if (SNaN)
    Result = llvm::APFloat::getSNaN(Sem, false, &fill);
  else
    Result = llvm::APFloat::getQNaN(Sem, false, &fill);
  return true;
}

bool FloatExprEvaluator::VisitFunctionCall(const FunctionCall *E) {
  switch (E->isBuiltinCall(Info.Ctx)) {
  default: return false;
  }
}

bool FloatExprEvaluator::VisitDefnRefExpr(const DefnRefExpr *E) {
  const Defn *D = E->getDefn();
  if (!isa<VarDefn>(D) || isa<ParamVarDefn>(D)) return false;
  const VarDefn *VD = cast<VarDefn>(D);

#if 0
  // Require the qualifiers to be const and not volatile.
  Type T = Info.Ctx.getCanonicalType(E->getType());
  if (!T.isConstQualified() || T.isVolatileQualified())
    return false;

  const Expr *Init = VD->getAnyInitializer();
  if (!Init) return false;

  if (APValue *V = VD->getEvaluatedValue()) {
    if (V->isFloat()) {
      Result = V->getFloat();
      return true;
    }
    return false;
  }

  if (VD->isEvaluatingValue())
    return false;

  VD->setEvaluatingValue();

  Expr::EvalResult InitResult;
  if (Init->Evaluate(InitResult, Info.Ctx) && !InitResult.HasSideEffects &&
      InitResult.Val.isFloat()) {
    // Cache the evaluated value in the variable declaration.
    Result = InitResult.Val.getFloat();
    VD->setEvaluatedValue(InitResult.Val);
    return true;
  }

  VD->setEvaluatedValue(APValue());
#endif

  return false;
}

bool FloatExprEvaluator::VisitUnaryReal(const UnaryOperator *E) {
  if (E->getSubExpr()->getType().hasComplexAttr()) {
    ComplexValue CV;
    if (!EvaluateComplex(E->getSubExpr(), CV, Info))
      return false;
    Result = CV.FloatReal;
    return true;
  }

  return Visit(E->getSubExpr());
}

bool FloatExprEvaluator::VisitUnaryImag(const UnaryOperator *E) {
  if (E->getSubExpr()->getType().hasComplexAttr()) {
    ComplexValue CV;
    if (!EvaluateComplex(E->getSubExpr(), CV, Info))
      return false;
    Result = CV.FloatImag;
    return true;
  }

  if (!E->getSubExpr()->isEvaluatable(Info.Ctx))
    Info.EvalResult.HasSideEffects = true;
  const llvm::fltSemantics &Sem = Info.Ctx.getFloatTypeSemantics(E->getType());
  Result = llvm::APFloat::getZero(Sem);
  return true;
}

bool FloatExprEvaluator::VisitUnaryOperator(const UnaryOperator *E) {
  if (!EvaluateFloat(E->getSubExpr(), Result, Info))
    return false;

  switch (E->getOpcode()) {
  default: return false;
  case UO_Plus:
    return true;
  case UO_Minus:
    Result.changeSign();
    return true;
  }
}

bool FloatExprEvaluator::VisitBinaryOperator(const BinaryOperator *E) {
  if (E->getOpcode() == BO_Comma) {
    if (!EvaluateFloat(E->getRHS(), Result, Info))
      return false;

    // If we can't evaluate the LHS, it might have side effects;
    // conservatively mark it.
    if (!E->getLHS()->isEvaluatable(Info.Ctx))
      Info.EvalResult.HasSideEffects = true;

    return true;
  }

  // FIXME: Diagnostics?  I really don't understand how the warnings
  // and errors are supposed to work.
  APFloat RHS(0.0);
  if (!EvaluateFloat(E->getLHS(), Result, Info))
    return false;
  if (!EvaluateFloat(E->getRHS(), RHS, Info))
    return false;

  switch (E->getOpcode()) {
  default: return false;
  case BO_Mul:
    Result.multiply(RHS, APFloat::rmNearestTiesToEven);
    return true;
  case BO_Add:
    Result.add(RHS, APFloat::rmNearestTiesToEven);
    return true;
  case BO_Sub:
    Result.subtract(RHS, APFloat::rmNearestTiesToEven);
    return true;
  case BO_LDiv:
    Result.divide(RHS, APFloat::rmNearestTiesToEven);
    return true;
  }
}

bool FloatExprEvaluator::VisitFloatingLiteral(const FloatingLiteral *E) {
  Result = E->getValue();
  return true;
}

//===----------------------------------------------------------------------===//
// Complex Evaluation (for float and integer)
//===----------------------------------------------------------------------===//
namespace {
class ComplexExprEvaluator
  : public StmtVisitor<ComplexExprEvaluator, bool> {
  EvalInfo &Info;
  ComplexValue &Result;

public:
  ComplexExprEvaluator(EvalInfo &info, ComplexValue &Result)
    : Info(info), Result(Result) {}

  //===--------------------------------------------------------------------===//
  //                            Visitor Methods
  //===--------------------------------------------------------------------===//

  bool VisitStmt(Stmt *S) {
    return false;
  }

  bool VisitParenExpr(ParenExpr *E) { return Visit(E->getSubExpr()); }
  bool VisitImaginaryLiteral(ImaginaryLiteral *E);
  bool VisitBinaryOperator(const BinaryOperator *E);
  bool VisitUnaryOperator(const UnaryOperator *E);
};
} // end anonymous namespace

static bool EvaluateComplex(const Expr *E, ComplexValue &Result,
                            EvalInfo &Info) {
  assert(E->getType().hasComplexAttr());
  return ComplexExprEvaluator(Info, Result).Visit(const_cast<Expr*>(E));
}

bool ComplexExprEvaluator::VisitImaginaryLiteral(ImaginaryLiteral *E) {
  Expr* SubExpr = E->getSubExpr();

  if (SubExpr->getType()->isFloatingPointType()) {
    Result.makeComplexFloat();
    APFloat &Imag = Result.FloatImag;
    if (!EvaluateFloat(SubExpr, Imag, Info))
      return false;

    Result.FloatReal = APFloat(Imag.getSemantics());
    return true;
  } else {
    assert(SubExpr->getType()->isIntegerType() &&
           "Unexpected imaginary literal.");

    Result.makeComplexInt();
    APSInt &Imag = Result.IntImag;
    if (!EvaluateInteger(SubExpr, Imag, Info))
      return false;

    Result.IntReal = APSInt(Imag.getBitWidth(), !Imag.isSigned());
    return true;
  }
}

bool ComplexExprEvaluator::VisitBinaryOperator(const BinaryOperator *E) {
  if (E->getOpcode() == BO_Comma) {
    if (!Visit(E->getRHS()))
      return false;

    // If we can't evaluate the LHS, it might have side effects;
    // conservatively mark it.
    if (!E->getLHS()->isEvaluatable(Info.Ctx))
      Info.EvalResult.HasSideEffects = true;

    return true;
  }
  if (!Visit(E->getLHS()))
    return false;

  ComplexValue RHS;
  if (!EvaluateComplex(E->getRHS(), RHS, Info))
    return false;

  assert(Result.isComplexFloat() == RHS.isComplexFloat() &&
         "Invalid operands to binary operator.");
  switch (E->getOpcode()) {
  default: return false;
  case BO_Add:
    if (Result.isComplexFloat()) {
      Result.getComplexFloatReal().add(RHS.getComplexFloatReal(),
                                       APFloat::rmNearestTiesToEven);
      Result.getComplexFloatImag().add(RHS.getComplexFloatImag(),
                                       APFloat::rmNearestTiesToEven);
    } else {
      Result.getComplexIntReal() += RHS.getComplexIntReal();
      Result.getComplexIntImag() += RHS.getComplexIntImag();
    }
    break;
  case BO_Sub:
    if (Result.isComplexFloat()) {
      Result.getComplexFloatReal().subtract(RHS.getComplexFloatReal(),
                                            APFloat::rmNearestTiesToEven);
      Result.getComplexFloatImag().subtract(RHS.getComplexFloatImag(),
                                            APFloat::rmNearestTiesToEven);
    } else {
      Result.getComplexIntReal() -= RHS.getComplexIntReal();
      Result.getComplexIntImag() -= RHS.getComplexIntImag();
    }
    break;
  case BO_Mul:
    if (Result.isComplexFloat()) {
      ComplexValue LHS = Result;
      APFloat &LHS_r = LHS.getComplexFloatReal();
      APFloat &LHS_i = LHS.getComplexFloatImag();
      APFloat &RHS_r = RHS.getComplexFloatReal();
      APFloat &RHS_i = RHS.getComplexFloatImag();

      APFloat Tmp = LHS_r;
      Tmp.multiply(RHS_r, APFloat::rmNearestTiesToEven);
      Result.getComplexFloatReal() = Tmp;
      Tmp = LHS_i;
      Tmp.multiply(RHS_i, APFloat::rmNearestTiesToEven);
      Result.getComplexFloatReal().subtract(Tmp, APFloat::rmNearestTiesToEven);

      Tmp = LHS_r;
      Tmp.multiply(RHS_i, APFloat::rmNearestTiesToEven);
      Result.getComplexFloatImag() = Tmp;
      Tmp = LHS_i;
      Tmp.multiply(RHS_r, APFloat::rmNearestTiesToEven);
      Result.getComplexFloatImag().add(Tmp, APFloat::rmNearestTiesToEven);
    } else {
      ComplexValue LHS = Result;
      Result.getComplexIntReal() =
        (LHS.getComplexIntReal() * RHS.getComplexIntReal() -
         LHS.getComplexIntImag() * RHS.getComplexIntImag());
      Result.getComplexIntImag() =
        (LHS.getComplexIntReal() * RHS.getComplexIntImag() +
         LHS.getComplexIntImag() * RHS.getComplexIntReal());
    }
    break;
  case BO_LDiv:
    if (Result.isComplexFloat()) {
      ComplexValue LHS = Result;
      APFloat &LHS_r = LHS.getComplexFloatReal();
      APFloat &LHS_i = LHS.getComplexFloatImag();
      APFloat &RHS_r = RHS.getComplexFloatReal();
      APFloat &RHS_i = RHS.getComplexFloatImag();
      APFloat &Res_r = Result.getComplexFloatReal();
      APFloat &Res_i = Result.getComplexFloatImag();

      APFloat Den = RHS_r;
      Den.multiply(RHS_r, APFloat::rmNearestTiesToEven);
      APFloat Tmp = RHS_i;
      Tmp.multiply(RHS_i, APFloat::rmNearestTiesToEven);
      Den.add(Tmp, APFloat::rmNearestTiesToEven);

      Res_r = LHS_r;
      Res_r.multiply(RHS_r, APFloat::rmNearestTiesToEven);
      Tmp = LHS_i;
      Tmp.multiply(RHS_i, APFloat::rmNearestTiesToEven);
      Res_r.add(Tmp, APFloat::rmNearestTiesToEven);
      Res_r.divide(Den, APFloat::rmNearestTiesToEven);

      Res_i = LHS_i;
      Res_i.multiply(RHS_r, APFloat::rmNearestTiesToEven);
      Tmp = LHS_r;
      Tmp.multiply(RHS_i, APFloat::rmNearestTiesToEven);
      Res_i.subtract(Tmp, APFloat::rmNearestTiesToEven);
      Res_i.divide(Den, APFloat::rmNearestTiesToEven);
    } else {
      if (RHS.getComplexIntReal() == 0 && RHS.getComplexIntImag() == 0) {
        // FIXME: what about diagnostics?
        return false;
      }
      ComplexValue LHS = Result;
      APSInt Den = RHS.getComplexIntReal() * RHS.getComplexIntReal() +
        RHS.getComplexIntImag() * RHS.getComplexIntImag();
      Result.getComplexIntReal() =
        (LHS.getComplexIntReal() * RHS.getComplexIntReal() +
         LHS.getComplexIntImag() * RHS.getComplexIntImag()) / Den;
      Result.getComplexIntImag() =
        (LHS.getComplexIntImag() * RHS.getComplexIntReal() -
         LHS.getComplexIntReal() * RHS.getComplexIntImag()) / Den;
    }
    break;
  }

  return true;
}

bool ComplexExprEvaluator::VisitUnaryOperator(const UnaryOperator *E) {
  // Get the operand value into 'Result'.
  if (!Visit(E->getSubExpr()))
    return false;

  switch (E->getOpcode()) {
  default:
    // FIXME: what about diagnostics?
    return false;
  case UO_Plus:
    // The result is always just the subexpr.
    return true;
  case UO_Minus:
    if (Result.isComplexFloat()) {
      Result.getComplexFloatReal().changeSign();
      Result.getComplexFloatImag().changeSign();
    }
    else {
      Result.getComplexIntReal() = -Result.getComplexIntReal();
      Result.getComplexIntImag() = -Result.getComplexIntImag();
    }
    return true;
  case UO_LNot:
    if (Result.isComplexFloat())
      Result.getComplexFloatImag().changeSign();
    else
      Result.getComplexIntImag() = -Result.getComplexIntImag();
    return true;
  }
}

//===----------------------------------------------------------------------===//
// Top level Expr::Evaluate method.
//===----------------------------------------------------------------------===//
/// Evaluate - Return true if this is a constant which we can fold using
/// any crazy technique (that has nothing to do with language standards) that
/// we want to.  If this function returns true, it returns the folded constant
/// in Result.
bool Expr::Evaluate(EvalResult &Result, ASTContext &Ctx) const {
  const Expr *E = this;
  EvalInfo Info(Ctx, Result);
  if (E->getType()->isIntegerType()) {
    if (!IntExprEvaluator(Info, Info.EvalResult.Val).Visit(const_cast<Expr*>(E)))
      return false;
    if (Result.Val.isLValue() && !IsGlobalLValue(Result.Val.getLValueBase()))
      return false;
  } else if (E->getType()->isFloatingPointType()) {
    llvm::APFloat F(0.0);
    if (!EvaluateFloat(E, F, Info))
      return false;

    Info.EvalResult.Val = APValue(F);
  } else if (E->getType().hasComplexAttr()) {
    ComplexValue C;
    if (!EvaluateComplex(E, C, Info))
      return false;
    C.moveInto(Info.EvalResult.Val);
  } else
    return false;

  return true;
}

bool Expr::EvaluateAsBooleanCondition(bool &Result, ASTContext &Ctx) const {
  EvalResult Scratch;
  EvalInfo Info(Ctx, Scratch);

  return HandleConversionToBool(this, Result, Info);
}

bool Expr::EvaluateAsLValue(EvalResult &Result, ASTContext &Ctx) const {
  EvalInfo Info(Ctx, Result);

  LValue LV;
  if (EvaluateLValue(this, LV, Info) &&
      !Result.HasSideEffects &&
      IsGlobalLValue(LV.Base)) {
    LV.moveInto(Result.Val);
    return true;
  }
  return false;
}

bool Expr::EvaluateAsAnyLValue(EvalResult &Result, ASTContext &Ctx) const {
  EvalInfo Info(Ctx, Result);

  LValue LV;
  if (EvaluateLValue(this, LV, Info)) {
    LV.moveInto(Result.Val);
    return true;
  }
  return false;
}

/// isEvaluatable - Call Evaluate to see if this expression can be constant
/// folded, but discard the result.
bool Expr::isEvaluatable(ASTContext &Ctx) const {
  EvalResult Result;
  return Evaluate(Result, Ctx) && !Result.HasSideEffects;
}

bool Expr::HasSideEffects(ASTContext &Ctx) const {
  Expr::EvalResult Result;
  EvalInfo Info(Ctx, Result);
  return HasSideEffect(Info).Visit(const_cast<Expr*>(this));
}

APSInt Expr::EvaluateAsInt(ASTContext &Ctx) const {
  EvalResult EvalResult;
  bool Result = Evaluate(EvalResult, Ctx);
  (void)Result;
  assert(Result && "Could not evaluate expression");
  assert(EvalResult.Val.isInt() && "Expression did not evaluate to integer");

  return EvalResult.Val.getInt();
}

 bool Expr::EvalResult::isGlobalLValue() const {
   assert(Val.isLValue());
   return IsGlobalLValue(Val.getLValueBase());
 }

/// isIntegerConstantExpr - this recursive routine will test if an expression is
/// an integer constant expression.

/// FIXME: Pass up a reason why! Invalid operation in i-c-e, division by zero,
/// comma, etc
///
/// FIXME: Handle offsetof.  Two things to do:  Handle GCC's __builtin_offsetof
/// to support gcc 4.0+  and handle the idiom GCC recognizes with a null pointer
/// cast+dereference.

// CheckICE - This function does the fundamental ICE checking: the returned
// ICEDiag contains a Val of 0, 1, or 2, and a possibly null SourceLocation.
// Note that to reduce code duplication, this helper does no evaluation
// itself; the caller checks whether the expression is evaluatable, and
// in the rare cases where CheckICE actually cares about the evaluated
// value, it calls into Evalute.
//
// Meanings of Val:
// 0: This expression is an ICE if it can be evaluated by Evaluate.
// 1: This expression is not an ICE, but if it isn't evaluated, it's
//    a legal subexpression for an ICE. This return value is used to handle
//    the comma operator in C99 mode.
// 2: This expression is not an ICE, and is not a legal subexpression for one.

namespace {
struct ICEDiag {
  unsigned Val;
  SourceLocation Loc;

  public:
  ICEDiag(unsigned v, SourceLocation l) : Val(v), Loc(l) {}
  ICEDiag() : Val(0) {}
};
}

static ICEDiag NoDiag() { return ICEDiag(); }

static ICEDiag CheckEvalInICE(const Expr* E, ASTContext &Ctx) {
  Expr::EvalResult EVResult;
  if (!E->Evaluate(EVResult, Ctx) || EVResult.HasSideEffects ||
      !EVResult.Val.isInt()) {
    return ICEDiag(2, E->getLocStart());
  }
  return NoDiag();
}

static ICEDiag CheckICE(const Expr* E, ASTContext &Ctx) {
  if (!E->getType()->isIntegerType()) {
    return ICEDiag(2, E->getLocStart());
  }

  switch (E->getStmtClass()) {
#define CMD(Node, Base) case Expr::Node##Class:
#define EXPR(Node, Base)
#define ABSTRACT_STMT(STMT)
#include "mlang/AST/StmtNodes.inc"

  case Expr::FloatingLiteralClass:
  case Expr::ImaginaryLiteralClass:
  case Expr::StringLiteralClass:
  case Expr::ArrayIndexClass:
  case Expr::MemberExprClass:
  case Expr::CompoundAssignOperatorClass:
  case Expr::ParenListExprClass:
  case Expr::NoStmtClass:
    return ICEDiag(2, E->getLocStart());

  case Expr::ParenExprClass:
    return CheckICE(cast<ParenExpr>(E)->getSubExpr(), Ctx);
  case Expr::IntegerLiteralClass:
  case Expr::CharacterLiteralClass:
    return NoDiag();
  case Expr::FunctionCallClass: {
    const FunctionCall *CE = cast<FunctionCall>(E);
    if (CE->isBuiltinCall(Ctx))
      return CheckEvalInICE(E, Ctx);
    return ICEDiag(2, E->getLocStart());
  }
  case Expr::DefnRefExprClass:
    return ICEDiag(2, E->getLocStart());
  case Expr::UnaryOperatorClass: {
    const UnaryOperator *Exp = cast<UnaryOperator>(E);
    switch (Exp->getOpcode()) {
    case UO_PostInc:
    case UO_PostDec:
    case UO_PreInc:
    case UO_PreDec:
      return ICEDiag(2, E->getLocStart());
//    case UO_LNot:
    case UO_Plus:
    case UO_Minus:
    case UO_LNot:
      return CheckICE(Exp->getSubExpr(), Ctx);
    }
  }
  case Expr::BinaryOperatorClass: {
    const BinaryOperator *Exp = cast<BinaryOperator>(E);
    switch (Exp->getOpcode()) {
    case BO_Assign:
    case BO_MulAssign:
    case BO_LDivAssign:
    case BO_RDivAssign:
    case BO_AddAssign:
    case BO_SubAssign:
    case BO_ShlAssign:
    case BO_ShrAssign:
    case BO_AndAssign:
    case BO_OrAssign:
      return ICEDiag(2, E->getLocStart());

    case BO_Mul:
    case BO_LDiv:
    case BO_RDiv:
    case BO_Add:
    case BO_Sub:
    case BO_Shl:
    case BO_Shr:
    case BO_LT:
    case BO_GT:
    case BO_LE:
    case BO_GE:
    case BO_EQ:
    case BO_NE:
    case BO_And:
    case BO_Or:
    case BO_Comma: {
      ICEDiag LHSResult = CheckICE(Exp->getLHS(), Ctx);
      ICEDiag RHSResult = CheckICE(Exp->getRHS(), Ctx);
      if (Exp->getOpcode() == BO_LDiv ||
          Exp->getOpcode() == BO_RDiv) {
        // Evaluate gives an error for undefined Div/Rem, so make sure
        // we don't evaluate one.
        if (LHSResult.Val != 2 && RHSResult.Val != 2) {
          llvm::APSInt REval = Exp->getRHS()->EvaluateAsInt(Ctx);
          if (REval == 0)
            return ICEDiag(1, E->getLocStart());
          if (REval.isSigned() && REval.isAllOnesValue()) {
            llvm::APSInt LEval = Exp->getLHS()->EvaluateAsInt(Ctx);
            if (LEval.isMinSignedValue())
              return ICEDiag(1, E->getLocStart());
          }
        }
      }
      if (Exp->getOpcode() == BO_Comma) {
        if (Ctx.getLangOptions().MATLABKeywords) {
          // C99 6.6p3 introduces a strange edge case: comma can be in an ICE
          // if it isn't evaluated.
          if (LHSResult.Val == 0 && RHSResult.Val == 0)
            return ICEDiag(1, E->getLocStart());
        } else {
          // In both C89 and C++, commas in ICEs are illegal.
          return ICEDiag(2, E->getLocStart());
        }
      }
      if (LHSResult.Val >= RHSResult.Val)
        return LHSResult;
      return RHSResult;
    }
    case BO_LAnd:
    case BO_LOr: {
      ICEDiag LHSResult = CheckICE(Exp->getLHS(), Ctx);
      ICEDiag RHSResult = CheckICE(Exp->getRHS(), Ctx);
      if (LHSResult.Val == 0 && RHSResult.Val == 1) {
        // Rare case where the RHS has a comma "side-effect"; we need
        // to actually check the condition to see whether the side
        // with the comma is evaluated.
        if ((Exp->getOpcode() == BO_LAnd) !=
            (Exp->getLHS()->EvaluateAsInt(Ctx) == 0))
          return RHSResult;
        return NoDiag();
      }

      if (LHSResult.Val >= RHSResult.Val)
        return LHSResult;
      return RHSResult;
    }
    }
  }

  // Silence a GCC warning
  return ICEDiag(2, E->getLocStart());
}
}

bool Expr::isIntegerConstantExpr(llvm::APSInt &Result, ASTContext &Ctx,
                                 SourceLocation *Loc, bool isEvaluated) const {
  ICEDiag d = CheckICE(this, Ctx);
  if (d.Val != 0) {
    if (Loc) *Loc = d.Loc;
    return false;
  }
  EvalResult EvalResult;
  if (!Evaluate(EvalResult, Ctx))
    llvm_unreachable("ICE cannot be evaluated!");
  assert(!EvalResult.HasSideEffects && "ICE with side effects!");
  assert(EvalResult.Val.isInt() && "ICE that isn't integer!");
  Result = EvalResult.Val.getInt();
  return true;
}

bool mlang::Expr::EvalResult::isPersistentLValue() const
{
}


