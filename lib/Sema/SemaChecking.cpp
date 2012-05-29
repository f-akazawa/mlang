//===--- SemaChecking.cpp - Extra Semantic Checking -----------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements extra semantic analysis beyond what is enforced
//  by the type system.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
#include "mlang/Sema/SemaDiagnostic.h"
//#include "mlang/Sema/SemaInternal.h"
#include "mlang/Sema/ScopeInfo.h"
//#include "mlang/Analysis/Analyses/FormatString.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/CharUnits.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/Stmt.h"
#include "mlang/AST/CmdAll.h"
#include "mlang/Lex/Preprocessor.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/raw_ostream.h"
//#include "mlang/Basic/TargetBuiltins.h"
#include "mlang/Basic/TargetInfo.h"
//#include "mlang/Basic/ConvertUTF.h"
#include <limits>
using namespace mlang;
using namespace sema;

SourceLocation Sema::getLocationOfStringLiteralByte(const StringLiteral *SL,
                                                    unsigned ByteNo) const {
  return SL->getLocationOfByte(ByteNo, PP.getSourceManager(),
                               PP.getLangOptions(), PP.getTargetInfo());
}
  
ExprResult
Sema::CheckBuiltinFunctionCall(unsigned BuiltinID, FunctionCall *TheCall) {
  ExprResult TheCallResult(Owned(TheCall));

#if 0
  // Find out if any arguments are required to be integer constant expressions.
  unsigned ICEArguments = 0;
  ASTContext::GetBuiltinTypeError Error;
  Context.GetBuiltinFunctionType(BuiltinID, Error, &ICEArguments);
  if (Error != ASTContext::GE_None)
    ICEArguments = 0;  // Don't diagnose previously diagnosed errors.
  
  // If any arguments are required to be ICE's, check and diagnose.
  for (unsigned ArgNo = 0; ICEArguments != 0; ++ArgNo) {
    // Skip arguments not required to be ICE's.
    if ((ICEArguments & (1 << ArgNo)) == 0) continue;
    
    llvm::APSInt Result;
    if (SemaBuiltinConstantArg(TheCall, ArgNo, Result))
      return true;
    ICEArguments &= ~(1 << ArgNo);
  }
  
  switch (BuiltinID) {
  case Builtin::BI__builtin___CFStringMakeConstantString:
    assert(TheCall->getNumArgs() == 1 &&
           "Wrong # arguments to builtin CFStringMakeConstantString");
    if (CheckObjCString(TheCall->getIdx(0)))
      return ExprError();
    break;
  case Builtin::BI__builtin_stdarg_start:
  case Builtin::BI__builtin_va_start:
    if (SemaBuiltinVAStart(TheCall))
      return ExprError();
    break;
  case Builtin::BI__builtin_isgreater:
  case Builtin::BI__builtin_isgreaterequal:
  case Builtin::BI__builtin_isless:
  case Builtin::BI__builtin_islessequal:
  case Builtin::BI__builtin_islessgreater:
  case Builtin::BI__builtin_isunordered:
    if (SemaBuiltinUnorderedCompare(TheCall))
      return ExprError();
    break;
  case Builtin::BI__builtin_fpclassify:
    if (SemaBuiltinFPClassification(TheCall, 6))
      return ExprError();
    break;
  case Builtin::BI__builtin_isfinite:
  case Builtin::BI__builtin_isinf:
  case Builtin::BI__builtin_isinf_sign:
  case Builtin::BI__builtin_isnan:
  case Builtin::BI__builtin_isnormal:
    if (SemaBuiltinFPClassification(TheCall, 1))
      return ExprError();
    break;
  case Builtin::BI__builtin_shufflevector:
    return SemaBuiltinShuffleVector(TheCall);
    // TheCall will be freed by the smart pointer here, but that's fine, since
    // SemaBuiltinShuffleVector guts it, but then doesn't release it.
  case Builtin::BI__builtin_prefetch:
    if (SemaBuiltinPrefetch(TheCall))
      return ExprError();
    break;
  case Builtin::BI__builtin_object_size:
    if (SemaBuiltinObjectSize(TheCall))
      return ExprError();
    break;
  case Builtin::BI__builtin_longjmp:
    if (SemaBuiltinLongjmp(TheCall))
      return ExprError();
    break;
  case Builtin::BI__builtin_constant_p:
    if (TheCall->getNumArgs() == 0)
      return Diag(TheCall->getLocEnd(), diag::err_typecheck_call_too_few_args)
        << 0 /*function call*/ << 1 << 0 << TheCall->getSourceRange();
    if (TheCall->getNumArgs() > 1)
      return Diag(TheCall->getIdx(1)->getLocStart(),
                  diag::err_typecheck_call_too_many_args)
        << 0 /*function call*/ << 1 << TheCall->getNumArgs()
        << TheCall->getIdx(1)->getSourceRange();
    break;
  case Builtin::BI__sync_fetch_and_add:
  case Builtin::BI__sync_fetch_and_sub:
  case Builtin::BI__sync_fetch_and_or:
  case Builtin::BI__sync_fetch_and_and:
  case Builtin::BI__sync_fetch_and_xor:
  case Builtin::BI__sync_add_and_fetch:
  case Builtin::BI__sync_sub_and_fetch:
  case Builtin::BI__sync_and_and_fetch:
  case Builtin::BI__sync_or_and_fetch:
  case Builtin::BI__sync_xor_and_fetch:
  case Builtin::BI__sync_val_compare_and_swap:
  case Builtin::BI__sync_bool_compare_and_swap:
  case Builtin::BI__sync_lock_test_and_set:
  case Builtin::BI__sync_lock_release:
    return SemaBuiltinAtomicOverloaded(move(TheCallResult));
  }
  
  // Since the target specific builtins for each arch overlap, only check those
  // of the arch we are compiling for.
  if (BuiltinID >= Builtin::FirstTSBuiltin) {
    switch (Context.Target.getTriple().getArch()) {
      case llvm::Triple::arm:
      case llvm::Triple::thumb:
        if (CheckARMBuiltinFunctionCall(BuiltinID, TheCall))
          return ExprError();
        break;
      default:
        break;
    }
  }
#endif
  return move(TheCallResult);
}

/// CheckFunctionCall - Check a direct function call for various correctness
/// and safety properties not strictly enforced by the C type system.
bool Sema::CheckFunctionCall(FunctionDefn *FDefn, FunctionCall *TheCall) {
  // Get the IdentifierInfo* for the called function.
  IdentifierInfo *FnInfo = FDefn->getIdentifier();

  // None of the checks below are needed for functions that don't have
  // simple names (e.g., C++ conversion functions).
  if (!FnInfo)
    return false;

#if 0
  // FIXME: This mechanism should be abstracted to be less fragile and
  // more efficient. For example, just map function ids to custom
  // handlers.

  // Printf and scanf checking.
  for (specific_attr_iterator<FormatAttr>
         i = FDefn->specific_attr_begin<FormatAttr>(),
         e = FDefn->specific_attr_end<FormatAttr>(); i != e ; ++i) {

    const FormatAttr *Format = *i;
    const bool b = Format->getType() == "scanf";
    if (b || CheckablePrintfAttr(Format, TheCall)) {
      bool HasVAListArg = Format->getFirstArg() == 0;
      CheckPrintfScanfArguments(TheCall, HasVAListArg,
                                Format->getFormatIdx() - 1,
                                HasVAListArg ? 0 : Format->getFirstArg() - 1,
                                !b);
    }
  }

  for (specific_attr_iterator<NonNullAttr>
         i = FDefn->specific_attr_begin<NonNullAttr>(),
         e = FDefn->specific_attr_end<NonNullAttr>(); i != e; ++i) {
    CheckNonNullArguments(*i, TheCall);
  }
#endif

  return false;
}

bool Sema::CheckBlockCall(NamedDefn *NDefn, FunctionCall *TheCall) {
#if 0
	// Printf checking.
  const FormatAttr *Format = NDefn->getAttr<FormatAttr>();
  if (!Format)
    return false;

  const VarDefn *V = dyn_cast<VarDefn>(NDefn);
  if (!V)
    return false;

  Type Ty = V->getType();
  if (!Ty->isBlockPointerType())
    return false;

  const bool b = Format->getType() == "scanf";
  if (!b && !CheckablePrintfAttr(Format, TheCall))
    return false;

  bool HasVAListArg = Format->getFirstArg() == 0;
  CheckPrintfScanfArguments(TheCall, HasVAListArg, Format->getFormatIdx() - 1,
                            HasVAListArg ? 0 : Format->getFirstArg() - 1, !b);
#endif

  return false;
}

/// SemaBuiltinAtomicOverloaded - We have a call to a function like
/// __sync_fetch_and_add, which is an overloaded function based on the pointer
/// type of its first argument.  The main ActOnFunctionCall routines have already
/// promoted the types of arguments because all of these calls are prototyped as
/// void(...).
///
/// This function goes through and does final semantic checking for these
/// builtins,
ExprResult
Sema::SemaBuiltinAtomicOverloaded(ExprResult TheCallResult) {
  FunctionCall *TheCall = (FunctionCall *)TheCallResult.get();
  DefnRefExpr *DRE =cast<DefnRefExpr>(TheCall->getCallee()->IgnoreParenCasts());
  FunctionDefn *FDefn = cast<FunctionDefn>(DRE->getDefn());

  // Ensure that we have at least one argument to do type inference from.
  if (TheCall->getNumArgs() < 1) {
//    Diag(TheCall->getLocEnd(), diag::err_typecheck_call_too_few_args_at_least)
//      << 0 << 1 << TheCall->getNumArgs()
//      << TheCall->getCallee()->getSourceRange();
    return ExprError();
  }

#if 0
  // Inspect the first argument of the atomic builtin.  This should always be
  // a pointer type, whose element is an integral scalar or pointer type.
  // Because it is a pointer type, we don't have to worry about any implicit
  // casts here.
  // FIXME: We don't allow floating point scalars as input.
  Expr *FirstArg = TheCall->getIdx(0);
  if (!FirstArg->getType()->isPointerType()) {
//    Diag(DRE->getLocStart(), diag::err_atomic_builtin_must_be_pointer)
//      << FirstArg->getType() << FirstArg->getSourceRange();
    return ExprError();
  }

  Type ValType =
    FirstArg->getType()->getAs<PointerType>()->getPointeeType();
  if (!ValType->isIntegerType() && !ValType->isAnyPointerType() &&
      !ValType->isBlockPointerType()) {
    Diag(DRE->getLocStart(), diag::err_atomic_builtin_must_be_pointer_intptr)
      << FirstArg->getType() << FirstArg->getSourceRange();
    return ExprError();
  }

  // The majority of builtins return a value, but a few have special return
  // types, so allow them to override appropriately below.
  Type ResultType = ValType;

  // We need to figure out which concrete builtin this maps onto.  For example,
  // __sync_fetch_and_add with a 2 byte object turns into
  // __sync_fetch_and_add_2.
#define BUILTIN_ROW(x) \
  { Builtin::BI##x##_1, Builtin::BI##x##_2, Builtin::BI##x##_4, \
    Builtin::BI##x##_8, Builtin::BI##x##_16 }

  static const unsigned BuiltinIndices[][5] = {
    BUILTIN_ROW(__sync_fetch_and_add),
    BUILTIN_ROW(__sync_fetch_and_sub),
    BUILTIN_ROW(__sync_fetch_and_or),
    BUILTIN_ROW(__sync_fetch_and_and),
    BUILTIN_ROW(__sync_fetch_and_xor),

    BUILTIN_ROW(__sync_add_and_fetch),
    BUILTIN_ROW(__sync_sub_and_fetch),
    BUILTIN_ROW(__sync_and_and_fetch),
    BUILTIN_ROW(__sync_or_and_fetch),
    BUILTIN_ROW(__sync_xor_and_fetch),

    BUILTIN_ROW(__sync_val_compare_and_swap),
    BUILTIN_ROW(__sync_bool_compare_and_swap),
    BUILTIN_ROW(__sync_lock_test_and_set),
    BUILTIN_ROW(__sync_lock_release)
  };
#undef BUILTIN_ROW

  // Determine the index of the size.
  unsigned SizeIndex;
  switch (Context.getTypeSizeInChars(ValType).getQuantity()) {
  case 1: SizeIndex = 0; break;
  case 2: SizeIndex = 1; break;
  case 4: SizeIndex = 2; break;
  case 8: SizeIndex = 3; break;
  case 16: SizeIndex = 4; break;
  default:
    Diag(DRE->getLocStart(), diag::err_atomic_builtin_pointer_size)
      << FirstArg->getType() << FirstArg->getSourceRange();
    return ExprError();
  }

  // Each of these builtins has one pointer argument, followed by some number of
  // values (0, 1 or 2) followed by a potentially empty varags list of stuff
  // that we ignore.  Find out which row of BuiltinIndices to read from as well
  // as the number of fixed args.
  unsigned BuiltinID = FDefn->getBuiltinID();
  unsigned BuiltinIndex, NumFixed = 1;
  switch (BuiltinID) {
  default: assert(0 && "Unknown overloaded atomic builtin!");
  case Builtin::BI__sync_fetch_and_add: BuiltinIndex = 0; break;
  case Builtin::BI__sync_fetch_and_sub: BuiltinIndex = 1; break;
  case Builtin::BI__sync_fetch_and_or:  BuiltinIndex = 2; break;
  case Builtin::BI__sync_fetch_and_and: BuiltinIndex = 3; break;
  case Builtin::BI__sync_fetch_and_xor: BuiltinIndex = 4; break;

  case Builtin::BI__sync_add_and_fetch: BuiltinIndex = 5; break;
  case Builtin::BI__sync_sub_and_fetch: BuiltinIndex = 6; break;
  case Builtin::BI__sync_and_and_fetch: BuiltinIndex = 7; break;
  case Builtin::BI__sync_or_and_fetch:  BuiltinIndex = 8; break;
  case Builtin::BI__sync_xor_and_fetch: BuiltinIndex = 9; break;

  case Builtin::BI__sync_val_compare_and_swap:
    BuiltinIndex = 10;
    NumFixed = 2;
    break;
  case Builtin::BI__sync_bool_compare_and_swap:
    BuiltinIndex = 11;
    NumFixed = 2;
    ResultType = Context.BoolTy;
    break;
  case Builtin::BI__sync_lock_test_and_set: BuiltinIndex = 12; break;
  case Builtin::BI__sync_lock_release:
    BuiltinIndex = 13;
    NumFixed = 0;
    ResultType = Context.VoidTy;
    break;
  }

  // Now that we know how many fixed arguments we expect, first check that we
  // have at least that many.
  if (TheCall->getNumArgs() < 1+NumFixed) {
    Diag(TheCall->getLocEnd(), diag::err_typecheck_call_too_few_args_at_least)
      << 0 << 1+NumFixed << TheCall->getNumArgs()
      << TheCall->getCallee()->getSourceRange();
    return ExprError();
  }

  // Get the Defn for the concrete builtin from this, we can tell what the
  // concrete integer type we should convert to is.
  unsigned NewBuiltinID = BuiltinIndices[BuiltinIndex][SizeIndex];
  const char *NewBuiltinName = Context.BuiltinInfo.GetName(NewBuiltinID);
  IdentifierInfo *NewBuiltinII = PP.getIdentifierInfo(NewBuiltinName);
  FunctionDefn *NewBuiltinDefn =
    cast<FunctionDefn>(LazilyCreateBuiltin(NewBuiltinII, NewBuiltinID,
                                           BaseWorkspace, false, DRE->getLocStart()));

  // The first argument --- the pointer --- has a fixed type; we
  // deduce the types of the rest of the arguments accordingly.  Walk
  // the remaining arguments, converting them to the deduced value type.
  for (unsigned i = 0; i != NumFixed; ++i) {
    Expr *Arg = TheCall->getIdx(i+1);

    TheCall->setArg(i+1, Arg);
  }

  // Switch the DefnRefExpr to refer to the new Defn.
  DRE->setDefn(NewBuiltinDefn);
  DRE->setType(NewBuiltinDefn->getType());

  // Set the callee in the FunctionCall.
  // FIXME: This leaks the original parens and implicit casts.
  Expr *PromotedCall = DRE;
  UsualUnaryConversions(PromotedCall);
  TheCall->setCallee(PromotedCall);

  // Change the result type of the call to match the original value type. This
  // is arbitrary, but the codegen for these builtins ins design to handle it
  // gracefully.
  TheCall->setType(ResultType);
#endif

  return move(TheCallResult);
}

/// SemaBuiltinVAStart - Check the arguments to __builtin_va_start for validity.
/// Emit an error and return true on failure, return false on success.
bool Sema::SemaBuiltinVAStart(FunctionCall *TheCall) {
  Expr *Fn = TheCall->getCallee();
  if (TheCall->getNumArgs() > 2) {
//    Diag(TheCall->getIdx(2)->getLocStart(),
//         diag::err_typecheck_call_too_many_args)
//      << 0 /*function call*/ << 2 << TheCall->getNumArgs()
//      << Fn->getSourceRange()
//      << SourceRange(TheCall->getIdx(2)->getLocStart(),
//                     (*(TheCall->arg_end()-1))->getLocEnd());
    return true;
  }

  if (TheCall->getNumArgs() < 2) {
//    return Diag(TheCall->getLocEnd(),
//      diag::err_typecheck_call_too_few_args_at_least)
//      << 0 /*function call*/ << 2 << TheCall->getNumArgs();
    return false;
  }

#if 0
  // Determine whether the current function is variadic or not.
  BlockScopeInfo *CurBlock = getCurBlock();
  bool isVariadic;
  if (CurBlock)
    isVariadic = CurBlock->TheDefn->isVariadic();
  else if (FunctionDefn *FD = getCurFunctionDefn())
    isVariadic = FD->isVariadic();
  else
    isVariadic = getCurMethodDefn()->isVariadic();

  if (!isVariadic) {
    Diag(Fn->getLocStart(), diag::err_va_start_used_in_non_variadic_function);
    return true;
  }

  // Verify that the second argument to the builtin is the last argument of the
  // current function or method.
  bool SecondArgIsLastNamedArgument = false;
  const Expr *Arg = TheCall->getIdx(1)->IgnoreParenCasts();

  if (const DefnRefExpr *DR = dyn_cast<DefnRefExpr>(Arg)) {
    if (const ParamVarDefn *PV = dyn_cast<ParamVarDefn>(DR->getDefn())) {
      // FIXME: This isn't correct for methods (results in bogus warning).
      // Get the last formal in the current function.
      const ParamVarDefn *LastArg;
      if (CurBlock)
        LastArg = *(CurBlock->TheDefn->param_end()-1);
      else if (FunctionDefn *FD = getCurFunctionDefn())
        LastArg = *(FD->param_end()-1);
      else
        LastArg = *(getCurMethodDefn()->param_end()-1);
      SecondArgIsLastNamedArgument = PV == LastArg;
    }
  }

  if (!SecondArgIsLastNamedArgument)
    Diag(TheCall->getIdx(1)->getLocStart(),
         diag::warn_second_parameter_of_va_start_not_last_named_argument);
#endif
  return false;
}

/// SemaBuiltinUnorderedCompare - Handle functions like __builtin_isgreater and
/// friends.  This is declared to take (...), so we have to check everything.
bool Sema::SemaBuiltinUnorderedCompare(FunctionCall *TheCall) {
#if 0
	if (TheCall->getNumArgs() < 2)
    return Diag(TheCall->getLocEnd(), diag::err_typecheck_call_too_few_args)
      << 0 << 2 << TheCall->getNumArgs()/*function call*/;
  if (TheCall->getNumArgs() > 2)
    return Diag(TheCall->getIdx(2)->getLocStart(),
                diag::err_typecheck_call_too_many_args)
      << 0 /*function call*/ << 2 << TheCall->getNumArgs()
      << SourceRange(TheCall->getIdx(2)->getLocStart(),
                     (*(TheCall->arg_end()-1))->getLocEnd());

  Expr *OrigArg0 = TheCall->getIdx(0);
  Expr *OrigArg1 = TheCall->getIdx(1);

  // Do standard promotions between the two arguments, returning their common
  // type.
  Type Res = UsualArithmeticConversions(OrigArg0, OrigArg1, false);

  // Make sure any conversions are pushed back into the call; this is
  // type safe since unordered compare builtins are declared as "_Bool
  // foo(...)".
  TheCall->setArg(0, OrigArg0);
  TheCall->setArg(1, OrigArg1);

  if (OrigArg0->isTypeDependent() || OrigArg1->isTypeDependent())
    return false;

  // If the common type isn't a real floating type, then the arguments were
  // invalid for this operation.
  if (!Res->isRealFloatingType())
    return Diag(OrigArg0->getLocStart(),
                diag::err_typecheck_call_invalid_ordered_compare)
      << OrigArg0->getType() << OrigArg1->getType()
      << SourceRange(OrigArg0->getLocStart(), OrigArg1->getLocEnd());
#endif

  return false;
}

/// SemaBuiltinSemaBuiltinFPClassification - Handle functions like
/// __builtin_isnan and friends.  This is declared to take (...), so we have
/// to check everything. We expect the last argument to be a floating point
/// value.
bool Sema::SemaBuiltinFPClassification(FunctionCall *TheCall, unsigned NumArgs) {
#if 0
	if (TheCall->getNumArgs() < NumArgs)
    return Diag(TheCall->getLocEnd(), diag::err_typecheck_call_too_few_args)
      << 0 << NumArgs << TheCall->getNumArgs()/*function call*/;
  if (TheCall->getNumArgs() > NumArgs)
    return Diag(TheCall->getIdx(NumArgs)->getLocStart(),
                diag::err_typecheck_call_too_many_args)
      << 0 /*function call*/ << NumArgs << TheCall->getNumArgs()
      << SourceRange(TheCall->getIdx(NumArgs)->getLocStart(),
                     (*(TheCall->arg_end()-1))->getLocEnd());

  Expr *OrigArg = TheCall->getIdx(NumArgs-1);

  if (OrigArg->isTypeDependent())
    return false;

  // This operation requires a non-_Complex floating-point number.
  if (!OrigArg->getType()->isRealFloatingType())
    return Diag(OrigArg->getLocStart(),
                diag::err_typecheck_call_invalid_unary_fp)
      << OrigArg->getType() << OrigArg->getSourceRange();

  // If this is an implicit conversion from float -> double, remove it.
  if (ImplicitCastExpr *Cast = dyn_cast<ImplicitCastExpr>(OrigArg)) {
    Expr *CastArg = Cast->getSubExpr();
    if (CastArg->getType()->isSpecificBuiltinType(BuiltinType::Float)) {
      assert(Cast->getType()->isSpecificBuiltinType(BuiltinType::Double) &&
             "promotion from float to double is the only expected cast here");
      Cast->setSubExpr(0);
      TheCall->setArg(NumArgs-1, CastArg);
      OrigArg = CastArg;
    }
  }
#endif
  return false;
}

/// SemaBuiltinShuffleVector - Handle __builtin_shufflevector.
// This is declared to take (...), so we have to check everything.
ExprResult Sema::SemaBuiltinShuffleVector(FunctionCall *TheCall) {
#if 0
	if (TheCall->getNumArgs() < 2)
    return ExprError(Diag(TheCall->getLocEnd(),
                          diag::err_typecheck_call_too_few_args_at_least)
      << 0 /*function call*/ << 2 << TheCall->getNumArgs()
      << TheCall->getSourceRange());

  // Determine which of the following types of shufflevector we're checking:
  // 1) unary, vector mask: (lhs, mask)
  // 2) binary, vector mask: (lhs, rhs, mask)
  // 3) binary, scalar mask: (lhs, rhs, index, ..., index)
  Type resType = TheCall->getIdx(0)->getType();
  unsigned numElements = 0;
  
  if (!TheCall->getIdx(0)->isTypeDependent() &&
      !TheCall->getIdx(1)->isTypeDependent()) {
    Type LHSType = TheCall->getIdx(0)->getType();
    Type RHSType = TheCall->getIdx(1)->getType();
    
    if (!LHSType->isVectorType() || !RHSType->isVectorType()) {
      Diag(TheCall->getLocStart(), diag::err_shufflevector_non_vector)
        << SourceRange(TheCall->getIdx(0)->getLocStart(),
                       TheCall->getIdx(1)->getLocEnd());
      return ExprError();
    }
    
    numElements = LHSType->getAs<VectorType>()->getNumElements();
    unsigned numResElements = TheCall->getNumArgs() - 2;

    // Check to see if we have a call with 2 vector arguments, the unary shuffle
    // with mask.  If so, verify that RHS is an integer vector type with the
    // same number of elts as lhs.
    if (TheCall->getNumArgs() == 2) {
      if (!RHSType->hasIntegerRepresentation() || 
          RHSType->getAs<VectorType>()->getNumElements() != numElements)
        Diag(TheCall->getLocStart(), diag::err_shufflevector_incompatible_vector)
          << SourceRange(TheCall->getIdx(1)->getLocStart(),
                         TheCall->getIdx(1)->getLocEnd());
      numResElements = numElements;
    }
    else if (!Context.hasSameUnqualifiedType(LHSType, RHSType)) {
      Diag(TheCall->getLocStart(), diag::err_shufflevector_incompatible_vector)
        << SourceRange(TheCall->getIdx(0)->getLocStart(),
                       TheCall->getIdx(1)->getLocEnd());
      return ExprError();
    } else if (numElements != numResElements) {
      Type eltType = LHSType->getAs<VectorType>()->getElementType();
      resType = Context.getVectorType(eltType, numResElements,
                                      VectorType::GenericVector);
    }
  }

  for (unsigned i = 2; i < TheCall->getNumArgs(); i++) {
    if (TheCall->getIdx(i)->isTypeDependent() ||
        TheCall->getIdx(i)->isValueDependent())
      continue;

    llvm::APSInt Result(32);
    if (!TheCall->getIdx(i)->isIntegerConstantExpr(Result, Context))
      return ExprError(Diag(TheCall->getLocStart(),
                  diag::err_shufflevector_nonconstant_argument)
                << TheCall->getIdx(i)->getSourceRange());

    if (Result.getActiveBits() > 64 || Result.getZExtValue() >= numElements*2)
      return ExprError(Diag(TheCall->getLocStart(),
                  diag::err_shufflevector_argument_too_large)
               << TheCall->getIdx(i)->getSourceRange());
  }

  llvm::SmallVector<Expr*, 32> exprs;

  for (unsigned i = 0, e = TheCall->getNumArgs(); i != e; i++) {
    exprs.push_back(TheCall->getIdx(i));
    TheCall->setArg(i, 0);
  }

  return Owned(new (Context) ShuffleVectorExpr(Context, exprs.begin(),
                                            exprs.size(), resType,
                                            TheCall->getCallee()->getLocStart(),
                                            TheCall->getRParenLoc()));
#endif
  return Owned(TheCall);
}

/// SemaBuiltinPrefetch - Handle __builtin_prefetch.
// This is declared to take (const void*, ...) and can take two
// optional constant int args.
bool Sema::SemaBuiltinPrefetch(FunctionCall *TheCall) {
  unsigned NumArgs = TheCall->getNumArgs();

  if (NumArgs > 3)
    return Diag(TheCall->getLocEnd(),
             diag::err_typecheck_call_too_many_args_at_most)
             << 0 /*function call*/ << 3 << NumArgs
             << TheCall->getSourceRange();

  // Argument 0 is checked for us and the remaining arguments must be
  // constant integers.
  for (unsigned i = 1; i != NumArgs; ++i) {
    Expr *Arg = TheCall->getArg(i);
    
    llvm::APSInt Result;
    if (SemaBuiltinConstantArg(TheCall, i, Result))
      return true;

    // FIXME: gcc issues a warning and rewrites these to 0. These
    // seems especially odd for the third argument since the default
    // is 3.
    if (i == 1) {
      if (Result.getLimitedValue() > 1)
        return Diag(TheCall->getLocStart(), diag::err_argument_invalid_range)
             << "0" << "1" << Arg->getSourceRange();
    } else {
      if (Result.getLimitedValue() > 3)
        return Diag(TheCall->getLocStart(), diag::err_argument_invalid_range)
            << "0" << "3" << Arg->getSourceRange();
    }
  }

  return false;
}

/// SemaBuiltinConstantArg - Handle a check if argument ArgNum of FunctionCall
/// TheCall is a constant expression.
bool Sema::SemaBuiltinConstantArg(FunctionCall *TheCall, int ArgNum,
                                  llvm::APSInt &Result) {
  Expr *Arg = TheCall->getArg(ArgNum);
  DefnRefExpr *DRE =cast<DefnRefExpr>(TheCall->getCallee()->IgnoreParenCasts());
  FunctionDefn *FDefn = cast<FunctionDefn>(DRE->getDefn());
  
  if (!Arg->isIntegerConstantExpr(Result, Context))
    return Diag(TheCall->getLocStart(), diag::err_constant_integer_arg_type)
                << FDefn->getDefnName() <<  Arg->getSourceRange();
  
  return false;
}

/// SemaBuiltinObjectSize - Handle __builtin_object_size(void *ptr,
/// int type). This simply type checks that type is one of the defined
/// constants (0-3).
// For compatability check 0-3, llvm only handles 0 and 2.
bool Sema::SemaBuiltinObjectSize(FunctionCall *TheCall) {
  llvm::APSInt Result;
  
  // Check constant-ness first.
  if (SemaBuiltinConstantArg(TheCall, 1, Result))
    return true;

  Expr *Arg = TheCall->getArg(1);
  if (Result.getSExtValue() < 0 || Result.getSExtValue() > 3) {
    return Diag(TheCall->getLocStart(), diag::err_argument_invalid_range)
             << "0" << "3" << SourceRange(Arg->getLocStart(), Arg->getLocEnd());
  }

  return false;
}

/// SemaBuiltinLongjmp - Handle __builtin_longjmp(void *env[5], int val).
/// This checks that val is a constant 1.
bool Sema::SemaBuiltinLongjmp(FunctionCall *TheCall) {
  Expr *Arg = TheCall->getArg(1);
  llvm::APSInt Result;

  // TODO: This is less than ideal. Overload this to take a value.
  if (SemaBuiltinConstantArg(TheCall, 1, Result))
    return true;
  
  if (Result != 1)
    return Diag(TheCall->getLocStart(), diag::err_builtin_longjmp_invalid_val)
             << SourceRange(Arg->getLocStart(), Arg->getLocEnd());

  return false;
}

// Handle i > 1 ? "x" : "y", recursivelly
bool Sema::SemaCheckStringLiteral(const Expr *E, const FunctionCall *TheCall,
                                  bool HasVAListArg,
                                  unsigned format_idx, unsigned firstDataArg,
                                  bool isPrintf) {
 tryAgain:

  switch (E->getStmtClass()) {
  case Stmt::IntegerLiteralClass:
    // Technically -Wformat-nonliteral does not warn about this case.
    // The behavior of printf and friends in this case is implementation
    // dependent.  Ideally if the format string cannot be null then
    // it should have a 'nonnull' attribute in the function prototype.
    return true;

  case Stmt::ParenExprClass: {
    E = cast<ParenExpr>(E)->getSubExpr();
    goto tryAgain;
  }

  case Stmt::DefnRefExprClass: {
    const DefnRefExpr *DR = cast<DefnRefExpr>(E);

    // As an exception, do not flag errors for variables binding to
    // const string literals.
    if (const VarDefn *VD = dyn_cast<VarDefn>(DR->getDefn())) {
      bool isConstant = false;
      Type T = DR->getType();

      if (const ArrayType *AT = dyn_cast<ArrayType>(T.getRawTypePtr())) {
//        isConstant = AT->getElementType().isConstant(Context);
      }
      if (isConstant) {
        if (const Expr *Init = VD->getInit())
          return SemaCheckStringLiteral(Init, TheCall,
                                        HasVAListArg, format_idx, firstDataArg,
                                        isPrintf);
      }

      // For vprintf* functions (i.e., HasVAListArg==true), we add a
      // special check to see if the format string is a function parameter
      // of the function calling the printf function.  If the function
      // has an attribute indicating it is a printf-like function, then we
      // should suppress warnings concerning non-literals being used in a call
      // to a vprintf function.  For example:
      //
      // void
      // logmessage(char const *fmt __attribute__ (format (printf, 1, 2)), ...){
      //      va_list ap;
      //      va_start(ap, fmt);
      //      vprintf(fmt, ap);  // Do NOT emit a warning about "fmt".
      //      ...
      //
      //
      //  FIXME: We don't have full attribute support yet, so just check to see
      //    if the argument is a DefnRefExpr that references a parameter.  We'll
      //    add proper support for checking the attribute later.
      if (HasVAListArg)
        if (isa<ParamVarDefn>(VD))
          return true;
    }

    return false;
  }

  case Stmt::FunctionCallClass: {
    return false;
  }
  case Stmt::StringLiteralClass: {
    const StringLiteral *StrE = cast<StringLiteral>(E);

    if (StrE) {
      CheckFormatString(StrE, E, TheCall, HasVAListArg, format_idx,
                        firstDataArg, isPrintf);
      return true;
    }

    return false;
  }

  default:
    return false;
  }
}

//void
//Sema::CheckNonNullArguments(const NonNullAttr *NonNull,
//                            const FunctionCall *TheCall) {
#if 0
  for (NonNullAttr::args_iterator i = NonNull->args_begin(),
                                  e = NonNull->args_end();
       i != e; ++i) {
    const Expr *ArgExpr = TheCall->getIdx(*i);
    if (ArgExpr->isNullPointerConstant(Context,
                                       Expr::NPC_ValueDependentIsNotNull))
      Diag(TheCall->getCallee()->getLocStart(), diag::warn_null_arg)
        << ArgExpr->getSourceRange();
  }
#endif
//}

/// CheckPrintfScanfArguments - Check calls to printf and scanf (and similar
/// functions) for correct use of format strings.
void
Sema::CheckPrintfScanfArguments(const FunctionCall *TheCall, bool HasVAListArg,
                                unsigned format_idx, unsigned firstDataArg,
                                bool isPrintf) {

  const Expr *Fn = TheCall->getCallee();

  // The way the format attribute works in GCC, the implicit this argument
  // of member functions is counted. However, it doesn't appear in our own
  // lists, so decrement format_idx in that case.
  if (isa<ClassMethodCall>(TheCall)) {
    const ClassMethodDefn *method_defn =
      dyn_cast<ClassMethodDefn>(TheCall->getCalleeDefn());
    if (method_defn && method_defn->isInstance()) {
      // Catch a format attribute mistakenly referring to the object argument.
      if (format_idx == 0)
        return;
      --format_idx;
      if(firstDataArg != 0)
        --firstDataArg;
    }
  }

  // CHECK: printf/scanf-like function is called with no format string.
  if (format_idx >= TheCall->getNumArgs()) {
    Diag(TheCall->getRParenLoc(), diag::warn_missing_format_string)
      << Fn->getSourceRange();
    return;
  }

  const Expr *OrigFormatExpr = TheCall->getArg(format_idx)->IgnoreParenCasts();

  // CHECK: format string is not a string literal.
  //
  // Dynamically generated format strings are difficult to
  // automatically vet at compile time.  Requiring that format strings
  // are string literals: (1) permits the checking of format strings by
  // the compiler and thereby (2) can practically remove the source of
  // many format string exploits.

  // Format string can be either ObjC string (e.g. @"%d") or
  // C string (e.g. "%d")
  // ObjC string uses the same format specifiers as C string, so we can use
  // the same format string checking logic for both ObjC and C strings.
  if (SemaCheckStringLiteral(OrigFormatExpr, TheCall, HasVAListArg, format_idx,
                             firstDataArg, isPrintf))
    return;  // Literal format string found, check done!

  // If there are no arguments specified, warn with -Wformat-security, otherwise
  // warn only with -Wformat-nonliteral.
  if (TheCall->getNumArgs() == format_idx+1)
    Diag(TheCall->getArg(format_idx)->getLocStart(),
         diag::warn_format_nonliteral_noargs)
      << OrigFormatExpr->getSourceRange();
  else
    Diag(TheCall->getArg(format_idx)->getLocStart(),
         diag::warn_format_nonliteral)
           << OrigFormatExpr->getSourceRange();
}

void Sema::CheckFormatString(const StringLiteral *FExpr,
                             const Expr *OrigFormatExpr,
                             const FunctionCall *TheCall, bool HasVAListArg,
                             unsigned format_idx, unsigned firstDataArg,
                             bool isPrintf) {
#if 0
  // CHECK: is the format string a wide literal?
  if (FExpr->isWide()) {
    Diag(FExpr->getLocStart(),
         diag::warn_format_string_is_wide_literal)
    << OrigFormatExpr->getSourceRange();
    return;
  }
  
  // Str - The format string.  NOTE: this is NOT null-terminated!
  llvm::StringRef StrRef = FExpr->getString();
  const char *Str = StrRef.data();
  unsigned StrLen = StrRef.size();
  
  // CHECK: empty format string?
  if (StrLen == 0) {
    Diag(FExpr->getLocStart(), diag::warn_empty_format_string)
    << OrigFormatExpr->getSourceRange();
    return;
  }
  
  if (isPrintf) {
    CheckPrintfHandler H(*this, FExpr, OrigFormatExpr, firstDataArg,
                         TheCall->getNumArgs() - firstDataArg,
                         isa<ObjCStringLiteral>(OrigFormatExpr), Str,
                         HasVAListArg, TheCall, format_idx);
  
    if (!analyze_format_string::ParsePrintfString(H, Str, Str + StrLen))
      H.DoneProcessing();
  }
  else {
    CheckScanfHandler H(*this, FExpr, OrigFormatExpr, firstDataArg,
                        TheCall->getNumArgs() - firstDataArg,
                        isa<ObjCStringLiteral>(OrigFormatExpr), Str,
                        HasVAListArg, TheCall, format_idx);
    
    if (!analyze_format_string::ParseScanfString(H, Str, Str + StrLen))
      H.DoneProcessing();
  }
#endif
}

//===--- CHECK: Return Address of Stack Variable --------------------------===//

static Expr *EvalVal(Expr *E, llvm::SmallVectorImpl<DefnRefExpr *> &refVars);
static Expr *EvalAddr(Expr* E, llvm::SmallVectorImpl<DefnRefExpr *> &refVars);

/// CheckReturnStackAddr - Check if a return statement returns the address
///   of a stack variable.
void
Sema::CheckReturnStackAddr(Expr *RetValExp, Type lhsType,
                           SourceLocation ReturnLoc) {
#if 0
  Expr *stackE = 0;
  llvm::SmallVector<DefnRefExpr *, 8> refVars;

  // Perform checking for returned stack addresses, local blocks,
  // label addresses or references to temporaries.
  if (lhsType->isPointerType() || lhsType->isBlockPointerType()) {
    stackE = EvalAddr(RetValExp, refVars);
  } else if (lhsType->isReferenceType()) {
    stackE = EvalVal(RetValExp, refVars);
  }

  if (stackE == 0)
    return; // Nothing suspicious was found.

  SourceLocation diagLoc;
  SourceRange diagRange;
  if (refVars.empty()) {
    diagLoc = stackE->getLocStart();
    diagRange = stackE->getSourceRange();
  } else {
    // We followed through a reference variable. 'stackE' contains the
    // problematic expression but we will warn at the return statement pointing
    // at the reference variable. We will later display the "trail" of
    // reference variables using notes.
    diagLoc = refVars[0]->getLocStart();
    diagRange = refVars[0]->getSourceRange();
  }

  if (DefnRefExpr *DR = dyn_cast<DefnRefExpr>(stackE)) { //address of local var.
    Diag(diagLoc, lhsType->isReferenceType() ? diag::warn_ret_stack_ref
                                             : diag::warn_ret_stack_addr)
     << DR->getDefn()->getDefnName() << diagRange;
  } else if (isa<BlockCmd>(stackE)) { // local block.
    Diag(diagLoc, diag::err_ret_local_block) << diagRange;
  } else { // local temporary.
    Diag(diagLoc, lhsType->isReferenceType() ? diag::warn_ret_local_temp_ref
                                             : diag::warn_ret_local_temp_addr)
     << diagRange;
  }

  // Display the "trail" of reference variables that we followed until we
  // found the problematic expression using notes.
  for (unsigned i = 0, e = refVars.size(); i != e; ++i) {
    VarDefn *VD = cast<VarDefn>(refVars[i]->getDefn());
    // If this var binds to another reference var, show the range of the next
    // var, otherwise the var binds to the problematic expression, in which case
    // show the range of the expression.
    SourceRange range = (i < e-1) ? refVars[i+1]->getSourceRange()
                                  : stackE->getSourceRange();
    Diag(VD->getLocation(), diag::note_ref_var_local_bind)
      << VD->getDefnName() << range;
  }
#endif
}

/// EvalAddr - EvalAddr and EvalVal are mutually recursive functions that
///  check if the expression in a return statement evaluates to an address
///  to a location on the stack, a local block, an address of a label, or a
///  reference to local temporary. The recursion is used to traverse the
///  AST of the return expression, with recursion backtracking when we
///  encounter a subexpression that (1) clearly does not lead to one of the
///  above problematic expressions (2) is something we cannot determine leads to
///  a problematic expression based on such local checking.
///
///  Both EvalAddr and EvalVal follow through reference variables to evaluate
///  the expression that they point to. Such variables are added to the
///  'refVars' vector so that we know what the reference variable "trail" was.
///
///  EvalAddr processes expressions that are pointers that are used as
///  references (and not L-values).  EvalVal handles all other values.
///  At the base case of the recursion is a check for the above problematic
///  expressions.
///
///  This implementation handles:
///
///   * pointer-to-pointer casts
///   * implicit conversions from array references to pointers
///   * taking the address of fields
///   * arbitrary interplay between "&" and "*" operators
///   * pointer arithmetic from an address of a stack variable
///   * taking the address of an array element where the array is on the stack
static Expr *EvalAddr(Expr *E, llvm::SmallVectorImpl<DefnRefExpr *> &refVars) {
  // Our "symbolic interpreter" is just a dispatch off the currently
  // viewed AST node.  We then recursively traverse the AST by calling
  // EvalAddr and EvalVal appropriately.
  switch (E->getStmtClass()) {
  case Stmt::ParenExprClass:
    // Ignore parentheses.
    return EvalAddr(cast<ParenExpr>(E)->getSubExpr(), refVars);

  case Stmt::DefnRefExprClass: {
    DefnRefExpr *DR = cast<DefnRefExpr>(E);

    if (VarDefn *V = dyn_cast<VarDefn>(DR->getDefn()))
      // If this is a reference variable, follow through to the expression that
      // it points to.
      if(V->getType()->isReferenceType() && V->hasInit()) {
        // Add the reference variable to the "trail".
        refVars.push_back(DR);
        return EvalAddr(V->getInit(), refVars);
      }

    return NULL;
  }

  case Stmt::UnaryOperatorClass: {
    // The only unary operator that make sense to handle here
    // is AddrOf.  All others don't make sense as pointers.
    UnaryOperator *U = cast<UnaryOperator>(E);
    return NULL;
  }

  case Stmt::BinaryOperatorClass: {
    // Handle pointer arithmetic.  All other binary operators are not valid
    // in this context.
    BinaryOperator *B = cast<BinaryOperator>(E);
    BinaryOperatorKind op = B->getOpcode();

    if (op != BO_Add && op != BO_Sub)
      return NULL;

    Expr *Base = B->getLHS();

    Base = B->getRHS();

//    assert (Base->getType()->isPointerType());
    return EvalAddr(Base, refVars);
  }

//  case Stmt::BlockCmdClass:
//    if (cast<BlockCmd>(E)->hasBlockDefnRefExprs())
//      return E; // local block.
//    return NULL;

  // Everything else: we simply don't reason about them.
  default:
    return NULL;
  }
}


///  EvalVal - This function is complements EvalAddr in the mutual recursion.
///   See the comments for EvalAddr for more details.
static Expr *EvalVal(Expr *E, llvm::SmallVectorImpl<DefnRefExpr *> &refVars) {
do {
  // We should only be called for evaluating non-pointer expressions, or
  // expressions with a pointer type that are not used as references but instead
  // are l-values (e.g., DefnRefExpr with a pointer type).

  // Our "symbolic interpreter" is just a dispatch off the currently
  // viewed AST node.  We then recursively traverse the AST by calling
  // EvalAddr and EvalVal appropriately.
  switch (E->getStmtClass()) {
  case Stmt::DefnRefExprClass: {
    // When we hit a DefnRefExpr we are looking at code that refers to a
    // variable's name. If it's not a reference variable we check if it has
    // local storage within the function, and if so, return the expression.
    DefnRefExpr *DR = cast<DefnRefExpr>(E);

    if (VarDefn *V = dyn_cast<VarDefn>(DR->getDefn())) {
    	if (!V->getType()->isReferenceType())
    		return DR;

    	// Reference variable, follow through to the expression that
    	// it points to.
    	if (V->hasInit()) {
    		// Add the reference variable to the "trail".
    		refVars.push_back(DR);
    		return EvalVal(V->getInit(), refVars);
    	}
    }

    return NULL;
  }

  case Stmt::ParenExprClass: {
    // Ignore parentheses.
    E = cast<ParenExpr>(E)->getSubExpr();
    continue;
  }

  case Stmt::UnaryOperatorClass: {
    // The only unary operator that make sense to handle here
    // is Deref.  All others don't resolve to a "name."  This includes
    // handling all sorts of rvalues passed to a unary operator.
    UnaryOperator *U = cast<UnaryOperator>(E);
    return NULL;
  }

  case Stmt::ArrayIndexClass: {
    // Array subscripts are potential references to data on the stack.  We
    // retrieve the DefnRefExpr* for the array variable if it indeed
    // has local storage.
    return EvalAddr(cast<ArrayIndex>(E)->getBase(), refVars);
  }

  // Accesses to members are potential references to data on the stack.
  case Stmt::MemberExprClass: {
    MemberExpr *M = cast<MemberExpr>(E);

    // Check whether the member type is itself a reference, in which case
    // we're not going to refer to the member, but to what the member refers to.
    if (M->getMemberDefn()->getType()->isReferenceType())
      return NULL;

    return EvalVal(M->getBase(), refVars);
  }

  default:
    // Everything else: we simply don't reason about them.
    return NULL;
  }
} while (true);
}

//===--- CHECK: Floating-Point comparisons (-Wfloat-equal) ---------------===//

/// Check for comparisons of floating point operands using != and ==.
/// Issue a warning if these are no self-comparisons, as they are not likely
/// to do what the programmer intended.
void Sema::CheckFloatComparison(SourceLocation loc, Expr* lex, Expr *rex) {
  bool EmitWarning = true;

  Expr* LeftExprSansParen = lex->IgnoreParenImpCasts();
  Expr* RightExprSansParen = rex->IgnoreParenImpCasts();

  // Special case: check for x == x (which is OK).
  // Do not emit warnings for such cases.
  if (DefnRefExpr* DRL = dyn_cast<DefnRefExpr>(LeftExprSansParen))
    if (DefnRefExpr* DRR = dyn_cast<DefnRefExpr>(RightExprSansParen))
      if (DRL->getDefn() == DRR->getDefn())
        EmitWarning = false;


  // Special case: check for comparisons against literals that can be exactly
  //  represented by APFloat.  In such cases, do not emit a warning.  This
  //  is a heuristic: often comparison against such literals are used to
  //  detect if a value in a variable has not changed.  This clearly can
  //  lead to false negatives.
  if (EmitWarning) {
    if (FloatingLiteral* FLL = dyn_cast<FloatingLiteral>(LeftExprSansParen)) {
      if (FLL->isExact())
        EmitWarning = false;
    } else
      if (FloatingLiteral* FLR = dyn_cast<FloatingLiteral>(RightExprSansParen)){
        if (FLR->isExact())
          EmitWarning = false;
    }
  }

  // Check for comparisons with builtin types.
  if (EmitWarning)
    if (FunctionCall* CL = dyn_cast<FunctionCall>(LeftExprSansParen))
      if (CL->isBuiltinCall(Context))
        EmitWarning = false;

  if (EmitWarning)
    if (FunctionCall* CR = dyn_cast<FunctionCall>(RightExprSansParen))
      if (CR->isBuiltinCall(Context))
        EmitWarning = false;

  // Emit the diagnostic.
  if (EmitWarning)
    Diag(loc, diag::warn_floatingpoint_eq)
      << lex->getSourceRange() << rex->getSourceRange();
}

//===--- CHECK: Integer mixed-sign comparisons (-Wsign-compare) --------===//
//===--- CHECK: Lossy implicit conversions (-Wconversion) --------------===//

namespace {

/// Structure recording the 'active' range of an integer-valued
/// expression.
struct IntRange {
  /// The number of bits active in the int.
  unsigned Width;

  /// True if the int is known not to have negative values.
  bool NonNegative;

  IntRange(unsigned Width, bool NonNegative)
    : Width(Width), NonNegative(NonNegative)
  {}

  /// Returns the range of the bool type.
  static IntRange forBoolType() {
    return IntRange(1, true);
  }

  /// Returns the range of an opaque value of the given integral type.
  static IntRange forValueOfType(ASTContext &C, Type T) {
    return forValueOfCanonicalType(C, T.getRawTypePtr());
  }

  /// Returns the range of an opaque value of a canonical integral type.
  static IntRange forValueOfCanonicalType(ASTContext &C, const RawType *T) {
    if (const VectorType *VT = dyn_cast<VectorType>(T))
      T = VT->getElementType().getRawTypePtr();

    const SimpleNumericType *BT = cast<SimpleNumericType>(T);
    assert(BT->isInteger());

    return IntRange(C.getIntWidth(Type(T, 0)), BT->isUnsignedInteger());
  }

  /// Returns the "target" range of a canonical integral type, i.e.
  /// the range of values expressible in the type.
  ///
  /// This matches forValueOfCanonicalType except that enums have the
  /// full range of their type, not the range of their enumerators.
  static IntRange forTargetOfCanonicalType(ASTContext &C, const RawType *T) {
    if (const VectorType *VT = dyn_cast<VectorType>(T))
      T = VT->getElementType().getRawTypePtr();

    const SimpleNumericType *BT = cast<SimpleNumericType>(T);
    assert(BT->isInteger());

    return IntRange(C.getIntWidth(Type(T, 0)), BT->isUnsignedInteger());
  }

  /// Returns the supremum of two ranges: i.e. their conservative merge.
  static IntRange join(IntRange L, IntRange R) {
    return IntRange(std::max(L.Width, R.Width),
                    L.NonNegative && R.NonNegative);
  }

  /// Returns the infinum of two ranges: i.e. their aggressive merge.
  static IntRange meet(IntRange L, IntRange R) {
    return IntRange(std::min(L.Width, R.Width),
                    L.NonNegative || R.NonNegative);
  }
};

IntRange GetValueRange(ASTContext &C, llvm::APSInt &value, unsigned MaxWidth) {
  if (value.isSigned() && value.isNegative())
    return IntRange(value.getMinSignedBits(), false);

  if (value.getBitWidth() > MaxWidth)
    value = value.trunc(MaxWidth);

  // isNonNegative() just checks the sign bit without considering
  // signedness.
  return IntRange(value.getActiveBits(), true);
}

IntRange GetValueRange(ASTContext &C, APValue &result, Type Ty,
                       unsigned MaxWidth) {
  if (result.isInt())
    return GetValueRange(C, result.getInt(), MaxWidth);

  if (result.isVector()) {
    IntRange R = GetValueRange(C, result.getVectorElt(0), Ty, MaxWidth);
    for (unsigned i = 1, e = result.getVectorLength(); i != e; ++i) {
      IntRange El = GetValueRange(C, result.getVectorElt(i), Ty, MaxWidth);
      R = IntRange::join(R, El);
    }
    return R;
  }

  if (result.isComplexInt()) {
    IntRange R = GetValueRange(C, result.getComplexIntReal(), MaxWidth);
    IntRange I = GetValueRange(C, result.getComplexIntImag(), MaxWidth);
    return IntRange::join(R, I);
  }

  // This can happen with lossless casts to intptr_t of "based" lvalues.
  // Assume it might use arbitrary bits.
  // FIXME: The only reason we need to pass the type in here is to get
  // the sign right on this one case.  It would be nice if APValue
  // preserved this.
  assert(result.isLValue());
  return IntRange(MaxWidth, Ty->isUnsignedIntegerType());
}

/// Pseudo-evaluate the given integer expression, estimating the
/// range of values it might take.
///
/// \param MaxWidth - the width to which the value will be truncated
IntRange GetExprRange(ASTContext &C, Expr *E, unsigned MaxWidth) {
  E = E->IgnoreParens();

  // Try a full evaluation first.
  Expr::EvalResult result;
  if (E->Evaluate(result, C))
    return GetValueRange(C, result.Val, E->getType(), MaxWidth);

  if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
    switch (BO->getOpcode()) {

    // Boolean-valued operations are single-bit and positive.
    case BO_LAnd:
    case BO_LOr:
    case BO_LT:
    case BO_GT:
    case BO_LE:
    case BO_GE:
    case BO_EQ:
    case BO_NE:
      return IntRange::forBoolType();

    // The type of these compound assignments is the type of the LHS,
    // so the RHS is not necessarily an integer.
    case BO_MulAssign:
    case BO_RDivAssign:
    case BO_LDivAssign:
    case BO_AddAssign:
    case BO_SubAssign:
      return IntRange::forValueOfType(C, E->getType());

    // Bitwise-and uses the *infinum* of the two source ranges.
    case BO_And:
    case BO_AndAssign:
      return IntRange::meet(GetExprRange(C, BO->getLHS(), MaxWidth),
                            GetExprRange(C, BO->getRHS(), MaxWidth));

    // Left shift gets black-listed based on a judgement call.
    case BO_Shl:
      // ...except that we want to treat '1 << (blah)' as logically
      // positive.  It's an important idiom.
      if (IntegerLiteral *I
            = dyn_cast<IntegerLiteral>(BO->getLHS()->IgnoreParenCasts())) {
        if (I->getValue() == 1) {
          IntRange R = IntRange::forValueOfType(C, E->getType());
          return IntRange(R.Width, /*NonNegative*/ true);
        }
      }
      // fallthrough

    case BO_ShlAssign:
      return IntRange::forValueOfType(C, E->getType());

    // Right shift by a constant can narrow its left argument.
    case BO_Shr:
    case BO_ShrAssign: {
      IntRange L = GetExprRange(C, BO->getLHS(), MaxWidth);

      // If the shift amount is a positive constant, drop the width by
      // that much.
      llvm::APSInt shift;
      if (BO->getRHS()->isIntegerConstantExpr(shift, C) &&
          shift.isNonNegative()) {
        unsigned zext = shift.getZExtValue();
        if (zext >= L.Width)
          L.Width = (L.NonNegative ? 0 : 1);
        else
          L.Width -= zext;
      }

      return L;
    }

    // Comma acts as its right operand.
    case BO_Comma:
      return GetExprRange(C, BO->getRHS(), MaxWidth);

    // Black-list pointer subtractions.
    case BO_Sub:
      // fallthrough

    default:
      break;
    }

    // Treat every other operator as if it were closed on the
    // narrowest type that encompasses both operands.
    IntRange L = GetExprRange(C, BO->getLHS(), MaxWidth);
    IntRange R = GetExprRange(C, BO->getRHS(), MaxWidth);
    return IntRange::join(L, R);
  }

  if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
    switch (UO->getOpcode()) {
    // Boolean-valued operations are white-listed.
    case UO_LNot:
      return IntRange::forBoolType();

    default:
      return GetExprRange(C, UO->getSubExpr(), MaxWidth);
    }
  }
  
  return IntRange::forValueOfType(C, E->getType());
}

IntRange GetExprRange(ASTContext &C, Expr *E) {
  return GetExprRange(C, E, C.getIntWidth(E->getType()));
}

/// Checks whether the given value, which currently has the given
/// source semantics, has the same value when coerced through the
/// target semantics.
bool IsSameFloatAfterCast(const llvm::APFloat &value,
                          const llvm::fltSemantics &Src,
                          const llvm::fltSemantics &Tgt) {
  llvm::APFloat truncated = value;

  bool ignored;
  truncated.convert(Src, llvm::APFloat::rmNearestTiesToEven, &ignored);
  truncated.convert(Tgt, llvm::APFloat::rmNearestTiesToEven, &ignored);

  return truncated.bitwiseIsEqual(value);
}

/// Checks whether the given value, which currently has the given
/// source semantics, has the same value when coerced through the
/// target semantics.
///
/// The value might be a vector of floats (or a complex number).
bool IsSameFloatAfterCast(const APValue &value,
                          const llvm::fltSemantics &Src,
                          const llvm::fltSemantics &Tgt) {
  if (value.isFloat())
    return IsSameFloatAfterCast(value.getFloat(), Src, Tgt);

  if (value.isVector()) {
    for (unsigned i = 0, e = value.getVectorLength(); i != e; ++i)
      if (!IsSameFloatAfterCast(value.getVectorElt(i), Src, Tgt))
        return false;
    return true;
  }

  assert(value.isComplexFloat());
  return (IsSameFloatAfterCast(value.getComplexFloatReal(), Src, Tgt) &&
          IsSameFloatAfterCast(value.getComplexFloatImag(), Src, Tgt));
}

void AnalyzeImplicitConversions(Sema &S, Expr *E, SourceLocation CC);

static bool IsZero(Sema &S, Expr *E) {
  // Suppress cases where the '0' value is expanded from a macro.
  if (E->getLocStart().isMacroID())
    return false;

  llvm::APSInt Value;
  return E->isIntegerConstantExpr(Value, S.Context) && Value == 0;
}

static bool HasEnumType(Expr *E) {
#if 0
	// Strip off implicit integral promotions.
  while (ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E)) {
    if (ICE->getCastKind() != CK_IntegralCast &&
        ICE->getCastKind() != CK_NoOp)
      break;
    E = ICE->getSubExpr();
  }

  return E->getType()->isEnumeralType();
#endif
  return false;
}

void CheckTrivialUnsignedComparison(Sema &S, BinaryOperator *E) {
  BinaryOperatorKind op = E->getOpcode();

  if (op == BO_LT && IsZero(S, E->getRHS())) {
    S.Diag(E->getOperatorLoc(), diag::warn_lunsigned_always_true_comparison)
      << "< 0" << "false" << HasEnumType(E->getLHS())
      << E->getLHS()->getSourceRange() << E->getRHS()->getSourceRange();
  } else if (op == BO_GE && IsZero(S, E->getRHS())) {
    S.Diag(E->getOperatorLoc(), diag::warn_lunsigned_always_true_comparison)
      << ">= 0" << "true" << HasEnumType(E->getLHS())
      << E->getLHS()->getSourceRange() << E->getRHS()->getSourceRange();
  } else if (op == BO_GT && IsZero(S, E->getLHS())) {
    S.Diag(E->getOperatorLoc(), diag::warn_runsigned_always_true_comparison)
      << "0 >" << "false" << HasEnumType(E->getRHS())
      << E->getLHS()->getSourceRange() << E->getRHS()->getSourceRange();
  } else if (op == BO_LE && IsZero(S, E->getLHS())) {
    S.Diag(E->getOperatorLoc(), diag::warn_runsigned_always_true_comparison)
      << "0 <=" << "true" << HasEnumType(E->getRHS())
      << E->getLHS()->getSourceRange() << E->getRHS()->getSourceRange();
  }
}

/// Analyze the operands of the given comparison.  Implements the
/// fallback case from AnalyzeComparison.
void AnalyzeImpConvsInComparison(Sema &S, BinaryOperator *E) {
  AnalyzeImplicitConversions(S, E->getLHS(), E->getOperatorLoc());
  AnalyzeImplicitConversions(S, E->getRHS(), E->getOperatorLoc());
}

/// \brief Implements -Wsign-compare.
///
/// \param lex the left-hand expression
/// \param rex the right-hand expression
/// \param OpLoc the location of the joining operator
/// \param BinOpc binary opcode or 0
void AnalyzeComparison(Sema &S, BinaryOperator *E) {
  // The type the comparison is being performed in.
  Type T = E->getLHS()->getType();
//  assert(S.Context.hasSameUnqualifiedType(T, E->getRHS()->getType())
//         && "comparison with mismatched types");

  // We don't do anything special if this isn't an unsigned integral
  // comparison:  we're only interested in integral comparisons, and
  // signed comparisons only happen in cases we don't care to warn about.
//  if (!T->hasUnsignedIntegerRepresentation())
//    return AnalyzeImpConvsInComparison(S, E);

  Expr *lex = E->getLHS()->IgnoreParenImpCasts();
  Expr *rex = E->getRHS()->IgnoreParenImpCasts();

  // Check to see if one of the (unmodified) operands is of different
  // signedness.
  Expr *signedOperand, *unsignedOperand;
//  if (lex->getType()->hasSignedIntegerRepresentation()) {
//    assert(!rex->getType()->hasSignedIntegerRepresentation() &&
//           "unsigned comparison between two signed integer expressions?");
//    signedOperand = lex;
//    unsignedOperand = rex;
//  } else if (rex->getType()->hasSignedIntegerRepresentation()) {
//    signedOperand = rex;
//    unsignedOperand = lex;
//  } else {
//    CheckTrivialUnsignedComparison(S, E);
//    return AnalyzeImpConvsInComparison(S, E);
//  }

  // Otherwise, calculate the effective range of the signed operand.
  IntRange signedRange = GetExprRange(S.Context, signedOperand);

  // Go ahead and analyze implicit conversions in the operands.  Note
  // that we skip the implicit conversions on both sides.
  AnalyzeImplicitConversions(S, lex, E->getOperatorLoc());
  AnalyzeImplicitConversions(S, rex, E->getOperatorLoc());

  // If the signed range is non-negative, -Wsign-compare won't fire,
  // but we should still check for comparisons which are always true
  // or false.
  if (signedRange.NonNegative)
    return CheckTrivialUnsignedComparison(S, E);

  // For (in)equality comparisons, if the unsigned operand is a
  // constant which cannot collide with a overflowed signed operand,
  // then reinterpreting the signed operand as unsigned will not
  // change the result of the comparison.
  if (E->isEqualityOp()) {
    unsigned comparisonWidth = S.Context.getIntWidth(T);
    IntRange unsignedRange = GetExprRange(S.Context, unsignedOperand);

    // We should never be unable to prove that the unsigned operand is
    // non-negative.
    assert(unsignedRange.NonNegative && "unsigned range includes negative?");

    if (unsignedRange.Width < comparisonWidth)
      return;
  }

  S.Diag(E->getOperatorLoc(), diag::warn_mixed_sign_comparison)
    << lex->getType() << rex->getType()
    << lex->getSourceRange() << rex->getSourceRange();
}

/// Analyze the given simple or compound assignment for warning-worthy
/// operations.
void AnalyzeAssignment(Sema &S, BinaryOperator *E) {
  // Just recurse on the LHS.
  AnalyzeImplicitConversions(S, E->getLHS(), E->getOperatorLoc());
}

/// Diagnose an implicit cast;  purely a helper for CheckImplicitConversion.
void DiagnoseImpCast(Sema &S, Expr *E, Type T, SourceLocation CContext,
                     unsigned diag) {
  S.Diag(E->getExprLoc(), diag)
    << E->getType() << T << E->getSourceRange() << SourceRange(CContext);
}

std::string PrettyPrintInRange(const llvm::APSInt &Value, IntRange Range) {
  if (!Range.Width) return "0";

  llvm::APSInt ValueInRange = Value;
  ValueInRange.setIsSigned(!Range.NonNegative);
  ValueInRange = ValueInRange.trunc(Range.Width);
  return ValueInRange.toString(10);
}

void CheckImplicitConversion(Sema &S, Expr *E, Type T,
                             SourceLocation CC, bool *ICContext = 0) {
#if 0
	if (E->isTypeDependent() || E->isValueDependent()) return;

  const Type *Source = S.Context.getCanonicalType(E->getType()).getRawTypePtr();
  const Type *Target = S.Context.getCanonicalType(T).getRawTypePtr();
  if (Source == Target) return;
  if (Target->isDependentType()) return;

  // If the conversion context location is invalid or instantiated
  // from a system macro, don't complain.
  if (CC.isInvalid() ||
      (CC.isMacroID() && S.Context.getSourceManager().isInSystemHeader(
                           S.Context.getSourceManager().getSpellingLoc(CC))))
    return;

  // Never diagnose implicit casts to bool.
  if (Target->isSpecificBuiltinType(BuiltinType::Bool))
    return;

  // Strip vector types.
  if (isa<VectorType>(Source)) {
    if (!isa<VectorType>(Target))
      return DiagnoseImpCast(S, E, T, CC, diag::warn_impcast_vector_scalar);

    Source = cast<VectorType>(Source)->getElementType().getRawTypePtr();
    Target = cast<VectorType>(Target)->getElementType().getRawTypePtr();
  }

  // Strip complex types.
  if (isa<ComplexType>(Source)) {
    if (!isa<ComplexType>(Target))
      return DiagnoseImpCast(S, E, T, CC, diag::warn_impcast_complex_scalar);

    Source = cast<ComplexType>(Source)->getElementType().getRawTypePtr();
    Target = cast<ComplexType>(Target)->getElementType().getRawTypePtr();
  }

  const BuiltinType *SourceBT = dyn_cast<BuiltinType>(Source);
  const BuiltinType *TargetBT = dyn_cast<BuiltinType>(Target);

  // If the source is floating point...
  if (SourceBT && SourceBT->isFloatingPoint()) {
    // ...and the target is floating point...
    if (TargetBT && TargetBT->isFloatingPoint()) {
      // ...then warn if we're dropping FP rank.

      // Builtin FP kinds are ordered by increasing FP rank.
      if (SourceBT->getKind() > TargetBT->getKind()) {
        // Don't warn about float constants that are precisely
        // representable in the target type.
        Expr::EvalResult result;
        if (E->Evaluate(result, S.Context)) {
          // Value might be a float, a float vector, or a float complex.
          if (IsSameFloatAfterCast(result.Val,
                   S.Context.getFloatTypeSemantics(Type(TargetBT, 0)),
                   S.Context.getFloatTypeSemantics(Type(SourceBT, 0))))
            return;
        }

        DiagnoseImpCast(S, E, T, CC, diag::warn_impcast_float_precision);
      }
      return;
    }

    // If the target is integral, always warn.
    if ((TargetBT && TargetBT->isInteger()))
      // TODO: don't warn for integer values?
      DiagnoseImpCast(S, E, T, CC, diag::warn_impcast_float_integer);

    return;
  }

  IntRange SourceRange = GetExprRange(S.Context, E);
  IntRange TargetRange = IntRange::forTargetOfCanonicalType(S.Context, Target);

  if (SourceRange.Width > TargetRange.Width) {
    // If the source is a constant, use a default-on diagnostic.
    // TODO: this should happen for bitfield stores, too.
    llvm::APSInt Value(32);
    if (E->isIntegerConstantExpr(Value, S.Context)) {
      std::string PrettySourceValue = Value.toString(10);
      std::string PrettyTargetValue = PrettyPrintInRange(Value, TargetRange);

      S.Diag(E->getExprLoc(), diag::warn_impcast_integer_precision_constant)
        << PrettySourceValue << PrettyTargetValue
        << E->getType() << T << E->getSourceRange() << mlang::SourceRange(CC);
      return;
    }

    // People want to build with -Wshorten-64-to-32 and not -Wconversion
    // and by god we'll let them.
    if (SourceRange.Width == 64 && TargetRange.Width == 32)
      return DiagnoseImpCast(S, E, T, CC, diag::warn_impcast_integer_64_32);
    return DiagnoseImpCast(S, E, T, CC, diag::warn_impcast_integer_precision);
  }

  if ((TargetRange.NonNegative && !SourceRange.NonNegative) ||
      (!TargetRange.NonNegative && SourceRange.NonNegative &&
       SourceRange.Width == TargetRange.Width)) {
    unsigned DiagID = diag::warn_impcast_integer_sign;

    // Traditionally, gcc has warned about this under -Wsign-compare.
    // We also want to warn about it in -Wconversion.
    // So if -Wconversion is off, use a completely identical diagnostic
    // in the sign-compare group.
    // The conditional-checking code will 
    if (ICContext) {
      DiagID = diag::warn_impcast_integer_sign_conditional;
      *ICContext = true;
    }

    return DiagnoseImpCast(S, E, T, CC, DiagID);
  }
#endif
  return;
}

/// AnalyzeImplicitConversions - Find and report any interesting
/// implicit conversions in the given expression.  There are a couple
/// of competing diagnostics here, -Wconversion and -Wsign-compare.
void AnalyzeImplicitConversions(Sema &S, Expr *OrigE, SourceLocation CC) {
  Type T = OrigE->getType();
  Expr *E = OrigE->IgnoreParenImpCasts();

  // Go ahead and check any implicit conversions we might have skipped.
  // The non-canonical typecheck is just an optimization;
  // CheckImplicitConversion will filter out dead implicit conversions.
  if (E->getType() != T)
    CheckImplicitConversion(S, E, T, CC);

  // Now continue drilling into this expression.

  if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
    // Do a somewhat different check with comparison operators.
    if (BO->isComparisonOp())
      return AnalyzeComparison(S, BO);

    // And with assignments and compound assignments.
    if (BO->isAssignmentOp())
      return AnalyzeAssignment(S, BO);
  }

  // Now just recurse over the expression's children.
  CC = E->getExprLoc();
  for (Stmt::child_iterator I = E->child_begin(), IE = E->child_end();
         I != IE; ++I)
    AnalyzeImplicitConversions(S, cast<Expr>(*I), CC);
}

} // end anonymous namespace

/// Diagnoses "dangerous" implicit conversions within the given
/// expression (which is a full expression).  Implements -Wconversion
/// and -Wsign-compare.
///
/// \param CC the "context" location of the implicit conversion, i.e.
///   the most location of the syntactic entity requiring the implicit
///   conversion
void Sema::CheckImplicitConversions(Expr *E, SourceLocation CC) {
  // Don't diagnose in unevaluated contexts.
  if (ExprEvalContexts.back().Context == Sema::Unevaluated)
    return;

  // This is not the right CC for (e.g.) a variable initialization.
  AnalyzeImplicitConversions(*this, E, CC);
}

/// CheckParmsForFunctionDefn - Check that the parameters of the given
/// function are appropriate for the definition of a function. This
/// takes care of any checks that cannot be performed on the
/// declaration itself, e.g., that the types of each of the function
/// parameters are complete.
bool Sema::CheckParmsForFunctionDefn(ParamVarDefn **P, ParamVarDefn **PEnd,
                                    bool CheckParameterNames) {
  bool HasInvalidParm = false;
  for (; P != PEnd; ++P) {
    ParamVarDefn *Param = *P;
    
    // C99 6.7.5.3p4: the parameters in a parameter type list in a
    // function declarator that is part of a function definition of
    // that function shall not have incomplete type.
    //
    // This is also C++ [dcl.fct]p6.
    if (!Param->isInvalidDefn() &&
        RequireCompleteType(Param->getLocation(), Param->getType(),
                               diag::err_typecheck_decl_incomplete_type)) {
      Param->setInvalidDefn();
      HasInvalidParm = true;
    }

    // C99 6.9.1p5: If the declarator includes a parameter type list, the
    // declaration of each parameter shall include an identifier.
    if (CheckParameterNames &&
        Param->getIdentifier() == 0 &&
        !Param->isImplicit())
      Diag(Param->getLocation(), diag::err_parameter_name_omitted);

  }

  return HasInvalidParm;
}

/// CheckCastAlign - Implements -Wcast-align, which warns when a
/// pointer cast increases the alignment requirements.
void Sema::CheckCastAlign(Expr *Op, Type T, SourceRange TRange) {
#if 0
  // This is actually a lot of work to potentially be doing on every
  // cast; don't do it if we're ignoring -Wcast_align (as is the default).
  if (getDiagnostics().getDiagnosticLevel(diag::warn_cast_align,
                                          TRange.getBegin())
        == Diagnostic::Ignored)
    return;

  // Ignore dependent types.
  if (T->isDependentType() || Op->getType()->isDependentType())
    return;

  // Require that the destination be a pointer type.
  const PointerType *DestPtr = T->getAs<PointerType>();
  if (!DestPtr) return;

  // If the destination has alignment 1, we're done.
  Type DestPointee = DestPtr->getPointeeType();
  if (DestPointee->isIncompleteType()) return;
  CharUnits DestAlign = Context.getTypeAlignInChars(DestPointee);
  if (DestAlign.isOne()) return;

  // Require that the source be a pointer type.
  const PointerType *SrcPtr = Op->getType()->getAs<PointerType>();
  if (!SrcPtr) return;
  Type SrcPointee = SrcPtr->getPointeeType();

  // Whitelist casts from cv void*.  We already implicitly
  // whitelisted casts to cv void*, since they have alignment 1.
  // Also whitelist casts involving incomplete types, which implicitly
  // includes 'void'.
  if (SrcPointee->isIncompleteType()) return;

  CharUnits SrcAlign = Context.getTypeAlignInChars(SrcPointee);
  if (SrcAlign >= DestAlign) return;

  Diag(TRange.getBegin(), diag::warn_cast_align)
    << Op->getType() << T
    << static_cast<unsigned>(SrcAlign.getQuantity())
    << static_cast<unsigned>(DestAlign.getQuantity())
    << TRange << Op->getSourceRange();
#endif
}

//==========================================
// Check Operands
//==========================================
Type Sema::CheckPowerOperands(ExprResult &lex, ExprResult &rex,
		SourceLocation OpLoc, bool isIndirect) {
  return getASTContext().DoubleTy;
}

Type Sema::CheckMultiplyDivideOperands(ExprResult &lex, ExprResult &rex,
		SourceLocation OpLoc, bool isCompAssign, bool isDivide) {
	return getASTContext().DoubleTy;
}

Type Sema::CheckAdditionOperands(ExprResult &lex, ExprResult &rex,
		SourceLocation OpLoc, Type* CompLHSTy) {
	//default we return double type
	//return getASTContext().DoubleTy;
	return lex.get()->getType();
}

Type Sema::CheckSubtractionOperands(ExprResult &lex, ExprResult &rex,
		SourceLocation OpLoc, Type* CompLHSTy) {
	//return getASTContext().DoubleTy;
	return lex.get()->getType();
}

Type Sema::CheckShiftOperands(ExprResult &lex, ExprResult &rex,
		SourceLocation OpLoc, unsigned Opc,	bool isCompAssign) {
	return getASTContext().DoubleTy;
}

Type Sema::CheckCompareOperands(ExprResult &lex, ExprResult &rex,
		SourceLocation OpLoc, unsigned Opc,	bool isRelational) {
	return getASTContext().DoubleTy;
}

Type Sema::CheckBitwiseOperands(ExprResult &lex, ExprResult &rex,
		SourceLocation OpLoc, bool isCompAssign) {
	return getASTContext().DoubleTy;
}

Type Sema::CheckLogicalOperands(ExprResult &lex, ExprResult &rex,
		SourceLocation OpLoc, unsigned Opc) {
	return getASTContext().DoubleTy;
}

Type Sema::CheckAssignmentOperands(Expr *lex, ExprResult &rex,
		SourceLocation OpLoc, Type convertedType) {
	assert((isa<DefnRefExpr>(lex) || isa<ArrayIndex>(lex))
			&& "must be a VarDefn reference");

	DefnRefExpr *DRF;
	ArrayIndex *AI = dyn_cast_or_null<ArrayIndex>(lex);

	if(AI) {
		assert(isa<DefnRefExpr>(AI->getBase()) && "should be a DefnRefExpr");
		DRF = dyn_cast<DefnRefExpr>(AI->getBase());
	} else {
		DRF = dyn_cast<DefnRefExpr>(lex);
	}

	if (DRF) {
		if (ValueDefn *val = DRF->getDefn()) {
			if (val->getKind() == Defn::Var)
				val->setType(rex.get()->getType());
			else {
				return Type();
			}
		} else {
			return Type();
		}
		if (DRF->getType().isNull())
			DRF->setType(rex.get()->getType());

		if(AI) {
			AI->setType(DRF->getType());
		}
	}

	return rex.get()->getType();
}

Type Sema::CheckColonOperands(ExprResult &lex, ExprResult &rex,
		SourceLocation OpLoc) {
	return getASTContext().DoubleTy;
}
