//===--- CGExprScalar.cpp - Emit LLVM Code for Scalar Exprs ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes with scalar LLVM types as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "mlang/Frontend/CodeGenOptions.h"
#include "CodeGenFunction.h"
#include "CGOOPABI.h"
#include "CodeGenModule.h"
#include "CGDebugInfo.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/AST/StmtVisitor.h"
#include "mlang/Basic/TargetInfo.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Intrinsics.h"
#include "llvm/Module.h"
#include "llvm/Support/CFG.h"
#include "llvm/Target/TargetData.h"
#include <cstdarg>

using namespace mlang;
using namespace CodeGen;
using llvm::Value;

//===----------------------------------------------------------------------===//
//                         Scalar Expression Emitter
//===----------------------------------------------------------------------===//

namespace {
struct BinOpInfo {
  Value *LHS;
  Value *RHS;
  Type Ty;  // Computation Type.
  BinaryOperator::Opcode Opcode; // Opcode of BinOp to perform
  const Expr *E;      // Entire expr, for error unsupported.  May not be binop.
};

class ScalarExprEmitter
  : public StmtVisitor<ScalarExprEmitter, Value*> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  bool IgnoreResultAssign;
  llvm::LLVMContext &VMContext;
public:

  ScalarExprEmitter(CodeGenFunction &cgf, bool ira=false)
    : CGF(cgf), Builder(CGF.Builder), IgnoreResultAssign(ira),
      VMContext(cgf.getLLVMContext()) {
  }

  //===--------------------------------------------------------------------===//
  //                               Utilities
  //===--------------------------------------------------------------------===//

  bool TestAndClearIgnoreResultAssign() {
    bool I = IgnoreResultAssign;
    IgnoreResultAssign = false;
    return I;
  }

  llvm::Type *ConvertType(Type T) { return CGF.ConvertType(T); }
  LValue EmitLValue(const Expr *E) { return CGF.EmitLValue(E); }
  LValue EmitCheckedLValue(const Expr *E) { return CGF.EmitCheckedLValue(E); }

  Value *EmitLoadOfLValue(LValue LV, Type T) {
    return CGF.EmitLoadOfLValue(LV, T).getScalarVal();
  }

  /// EmitLoadOfLValue - Given an expression with complex type that represents a
  /// value l-value, this method emits the address of the l-value, then loads
  /// and returns the result.
  Value *EmitLoadOfLValue(const Expr *E) {
    return EmitLoadOfLValue(EmitCheckedLValue(E), E->getType());
  }

  /// EmitConversionToBool - Convert the specified expression value to a
  /// boolean (i1) truth value.  This is equivalent to "Val != 0".
  Value *EmitConversionToBool(Value *Src, Type DstTy);

  /// EmitScalarConversion - Emit a conversion from the specified type to the
  /// specified destination type, both of which are LLVM scalar types.
  Value *EmitScalarConversion(Value *Src, Type SrcTy, Type DstTy);

  /// EmitComplexToScalarConversion - Emit a conversion from the specified
  /// complex type to the specified destination type, where the destination type
  /// is an LLVM scalar type.
  Value *EmitComplexToScalarConversion(CodeGenFunction::ComplexPairTy Src,
                                       Type SrcTy, Type DstTy);

  /// EmitNullValue - Emit a value that corresponds to null for the given type.
  Value *EmitNullValue(Type Ty);

  /// EmitFloatToBoolConversion - Perform an FP to boolean conversion.
  Value *EmitFloatToBoolConversion(Value *V) {
    // Compare against 0.0 for fp scalars.
    llvm::Value *Zero = llvm::Constant::getNullValue(V->getType());
    return Builder.CreateFCmpUNE(V, Zero, "tobool");
  }

  /// EmitPointerToBoolConversion - Perform a pointer to boolean conversion.
  Value *EmitPointerToBoolConversion(Value *V) {
    Value *Zero = llvm::ConstantPointerNull::get(
                                      cast<llvm::PointerType>(V->getType()));
    return Builder.CreateICmpNE(V, Zero, "tobool");
  }

  Value *EmitIntToBoolConversion(Value *V) {
    // Because of the type rules of C, we often end up computing a
    // logical value, then zero extending it to int, then wanting it
    // as a logical value again.  Optimize this common case.
    if (llvm::ZExtInst *ZI = dyn_cast<llvm::ZExtInst>(V)) {
      if (ZI->getOperand(0)->getType() == Builder.getInt1Ty()) {
        Value *Result = ZI->getOperand(0);
        // If there aren't any more uses, zap the instruction to save space.
        // Note that there can be more uses, for example if this
        // is the result of an assignment.
        if (ZI->use_empty())
          ZI->eraseFromParent();
        return Result;
      }
    }

    return Builder.CreateIsNotNull(V, "tobool");
  }

  //===--------------------------------------------------------------------===//
  //                            Visitor Methods
  //===--------------------------------------------------------------------===//

  Value *Visit(Expr *E) {
    return StmtVisitor<ScalarExprEmitter, Value*>::Visit(E);
  }
    
  Value *VisitStmt(Stmt *S) {
    S->dump(CGF.getContext().getSourceManager());
    assert(0 && "Stmt can't have complex result type!");
    return 0;
  }
  Value *VisitExpr(Expr *S);
  
  Value *VisitParenExpr(ParenExpr *PE) {
    return Visit(PE->getSubExpr()); 
  }
  // Leaves.
  Value *VisitIntegerLiteral(const IntegerLiteral *E) {
    return Builder.getInt(E->getValue());
  }
  Value *VisitFloatingLiteral(const FloatingLiteral *E) {
    return llvm::ConstantFP::get(VMContext, E->getValue());
  }
  Value *VisitCharacterLiteral(const CharacterLiteral *E) {
    return llvm::ConstantInt::get(ConvertType(E->getType()), E->getValue());
  }

  Value *VisitOpaqueValueExpr(OpaqueValueExpr *E) {
    if (E->isLValue())
      return EmitLoadOfLValue(CGF.getOpaqueLValueMapping(E), E->getType());

    // Otherwise, assume the mapping is the scalar directly.
    return CGF.getOpaqueRValueMapping(E).getScalarVal();
  }
    
  // l-values.
  Value *VisitDefnRefExpr(DefnRefExpr *E) {
    Expr::EvalResult Result;
    if (!E->Evaluate(Result, CGF.getContext()))
      return EmitLoadOfLValue(E);

    assert(!Result.HasSideEffects && "Constant declref with side-effect?!");

    llvm::Constant *C;
    if (Result.Val.isInt())
      C = Builder.getInt(Result.Val.getInt());
    else if (Result.Val.isFloat())
      C = llvm::ConstantFP::get(VMContext, Result.Val.getFloat());
    else
      return EmitLoadOfLValue(E);

    // Make sure we emit a debug reference to the global variable.
    if (VarDefn *VD = dyn_cast<VarDefn>(E->getDefn())) {
      if (!CGF.getContext().DefnMustBeEmitted(VD))
        CGF.EmitDefnRefExprDbgValue(E, C);
    }

    return C;
  }
  Value *VisitArrayIndex(ArrayIndex *E);
  Value *VisitMemberExpr(MemberExpr *E);
  Value *VisitExtVectorElementExpr(Expr *E) { return EmitLoadOfLValue(E); }
  Value *VisitInitListExpr(InitListExpr *E);
  Value *VisitFunctionCall(const FunctionCall *E) {
    if (E->getCallReturnType()->isReferenceType())
      return EmitLoadOfLValue(E);

    return CGF.EmitFunctionCall(E).getScalarVal();
  }

  // Unary Operators.
  Value *VisitUnaryPostDec(const UnaryOperator *E) {
    LValue LV = EmitLValue(E->getSubExpr());
    return EmitScalarPrePostIncDec(E, LV, false, false);
  }
  Value *VisitUnaryPostInc(const UnaryOperator *E) {
    LValue LV = EmitLValue(E->getSubExpr());
    return EmitScalarPrePostIncDec(E, LV, true, false);
  }
  Value *VisitUnaryPreDec(const UnaryOperator *E) {
    LValue LV = EmitLValue(E->getSubExpr());
    return EmitScalarPrePostIncDec(E, LV, false, true);
  }
  Value *VisitUnaryPreInc(const UnaryOperator *E) {
    LValue LV = EmitLValue(E->getSubExpr());
    return EmitScalarPrePostIncDec(E, LV, true, true);
  }

  llvm::Value *EmitAddConsiderOverflowBehavior(const UnaryOperator *E,
                                               llvm::Value *InVal,
                                               llvm::Value *NextVal,
                                               bool IsInc);

  llvm::Value *EmitScalarPrePostIncDec(const UnaryOperator *E, LValue LV,
                                       bool isInc, bool isPre);

    
  Value *VisitUnaryAddrOf(const UnaryOperator *E) {
    return EmitLValue(E->getSubExpr()).getAddress();
  }
  Value *VisitUnaryDeref(const UnaryOperator *E) {
    return EmitLoadOfLValue(E);
  }
  Value *VisitUnaryPlus(const UnaryOperator *E) {
    // This differs from gcc, though, most likely due to a bug in gcc.
    TestAndClearIgnoreResultAssign();
    return Visit(E->getSubExpr());
  }
  Value *VisitUnaryMinus    (const UnaryOperator *E);
  Value *VisitUnaryNot      (const UnaryOperator *E);
  Value *VisitUnaryLNot     (const UnaryOperator *E);
  Value *VisitUnaryReal     (const UnaryOperator *E);
  Value *VisitUnaryImag     (const UnaryOperator *E);
  Value *VisitUnaryExtension(const UnaryOperator *E) {
    return Visit(E->getSubExpr());
  }
    
  // Binary Operators.
  Value *EmitMul(const BinOpInfo &Ops) {
    if (Ops.Ty->isSignedIntegerType()) {
      switch (CGF.getContext().getLangOptions().getSignedOverflowBehavior()) {
      case LangOptions::SOB_Undefined:
        return Builder.CreateNSWMul(Ops.LHS, Ops.RHS, "mul");
      case LangOptions::SOB_Defined:
        return Builder.CreateMul(Ops.LHS, Ops.RHS, "mul");
      case LangOptions::SOB_Trapping:
        return EmitOverflowCheckedBinOp(Ops);
      }
    }
    
    if (Ops.LHS->getType()->isFPOrFPVectorTy())
      return Builder.CreateFMul(Ops.LHS, Ops.RHS, "mul");
    return Builder.CreateMul(Ops.LHS, Ops.RHS, "mul");
  }
  bool isTrapvOverflowBehavior() {
    return CGF.getContext().getLangOptions().getSignedOverflowBehavior() 
               == LangOptions::SOB_Trapping; 
  }
  /// Create a binary op that checks for overflow.
  /// Currently only supports +, - and *.
  Value *EmitOverflowCheckedBinOp(const BinOpInfo &Ops);
  // Emit the overflow BB when -ftrapv option is activated. 
  void EmitOverflowBB(llvm::BasicBlock *overflowBB) {
    Builder.SetInsertPoint(overflowBB);
    llvm::Function *Trap = CGF.CGM.getIntrinsic(llvm::Intrinsic::trap);
    Builder.CreateCall(Trap);
    Builder.CreateUnreachable();
  }
  // Check for undefined division and modulus behaviors.
  void EmitUndefinedBehaviorIntegerDivAndRemCheck(const BinOpInfo &Ops, 
                                                  llvm::Value *Zero,bool isDiv);
  Value *EmitRDiv(const BinOpInfo &Ops);
  Value *EmitLDiv(const BinOpInfo &Ops);
  Value *EmitAdd(const BinOpInfo &Ops);
  Value *EmitSub(const BinOpInfo &Ops);
  Value *EmitShl(const BinOpInfo &Ops);
  Value *EmitShr(const BinOpInfo &Ops);
  Value *EmitAnd(const BinOpInfo &Ops) {
    return Builder.CreateAnd(Ops.LHS, Ops.RHS, "and");
  }
  Value *EmitXor(const BinOpInfo &Ops) {
    return Builder.CreateXor(Ops.LHS, Ops.RHS, "xor");
  }
  Value *EmitOr (const BinOpInfo &Ops) {
    return Builder.CreateOr(Ops.LHS, Ops.RHS, "or");
  }

  BinOpInfo EmitBinOps(const BinaryOperator *E);
  LValue EmitCompoundAssignLValue(const CompoundAssignOperator *E,
                            Value *(ScalarExprEmitter::*F)(const BinOpInfo &),
                                  Value *&Result);

  Value *EmitCompoundAssign(const CompoundAssignOperator *E,
                            Value *(ScalarExprEmitter::*F)(const BinOpInfo &));

  // Binary operators and binary compound assignment operators.
#define HANDLEBINOP(OP) \
  Value *VisitBin ## OP(const BinaryOperator *E) {                         \
    return Emit ## OP(EmitBinOps(E));                                      \
  }                                                                        \
  Value *VisitBin ## OP ## Assign(const CompoundAssignOperator *E) {       \
    return EmitCompoundAssign(E, &ScalarExprEmitter::Emit ## OP);          \
  }
  HANDLEBINOP(Mul)
  HANDLEBINOP(RDiv)
  HANDLEBINOP(LDiv)
  HANDLEBINOP(Add)
  HANDLEBINOP(Sub)
  HANDLEBINOP(Shl)
  HANDLEBINOP(Shr)
  HANDLEBINOP(And)
  HANDLEBINOP(Xor)
  HANDLEBINOP(Or)
#undef HANDLEBINOP

  // Comparisons.
  Value *EmitCompare(const BinaryOperator *E, unsigned UICmpOpc,
                     unsigned SICmpOpc, unsigned FCmpOpc);
#define VISITCOMP(CODE, UI, SI, FP) \
    Value *VisitBin##CODE(const BinaryOperator *E) { \
      return EmitCompare(E, llvm::ICmpInst::UI, llvm::ICmpInst::SI, \
                         llvm::FCmpInst::FP); }
  VISITCOMP(LT, ICMP_ULT, ICMP_SLT, FCMP_OLT)
  VISITCOMP(GT, ICMP_UGT, ICMP_SGT, FCMP_OGT)
  VISITCOMP(LE, ICMP_ULE, ICMP_SLE, FCMP_OLE)
  VISITCOMP(GE, ICMP_UGE, ICMP_SGE, FCMP_OGE)
  VISITCOMP(EQ, ICMP_EQ , ICMP_EQ , FCMP_OEQ)
  VISITCOMP(NE, ICMP_NE , ICMP_NE , FCMP_UNE)
#undef VISITCOMP

  Value *VisitBinAssign     (const BinaryOperator *E);

  Value *VisitBinLAnd       (const BinaryOperator *E);
  Value *VisitBinLOr        (const BinaryOperator *E);
  Value *VisitBinComma      (const BinaryOperator *E);

  Value *VisitBinPtrMemD(const Expr *E) { return EmitLoadOfLValue(E); }
  Value *VisitBinPtrMemI(const Expr *E) { return EmitLoadOfLValue(E); }
};
}  // end anonymous namespace.

//===----------------------------------------------------------------------===//
//                                Utilities
//===----------------------------------------------------------------------===//

/// EmitConversionToBool - Convert the specified expression value to a
/// boolean (i1) truth value.  This is equivalent to "Val != 0".
Value *ScalarExprEmitter::EmitConversionToBool(Value *Src, Type SrcType) {
  //assert(SrcType && "EmitScalarConversion strips typedefs");

  if (SrcType->isFloatingPointType())
    return EmitFloatToBoolConversion(Src);

  assert((SrcType->isIntegerType() || isa<llvm::PointerType>(Src->getType())) &&
         "Unknown scalar type to convert");

  if (isa<llvm::IntegerType>(Src->getType()))
    return EmitIntToBoolConversion(Src);

  assert(isa<llvm::PointerType>(Src->getType()));
  return EmitPointerToBoolConversion(Src);
}

/// EmitScalarConversion - Emit a conversion from the specified type to the
/// specified destination type, both of which are LLVM scalar types.
Value *ScalarExprEmitter::EmitScalarConversion(Value *Src, Type SrcType,
                                               Type DstType) {
  if (SrcType == DstType) return Src;

  //if (DstType->isVoidType()) return 0;

  // Handle conversions to bool first, they are special: comparisons against 0.
  if (DstType->isLogicalType())
    return EmitConversionToBool(Src, SrcType);

  llvm::Type *DstTy = ConvertType(DstType);

  // Ignore conversions like int -> uint.
  if (Src->getType() == DstTy)
    return Src;

  // Handle pointer conversions next: pointers can only be converted to/from
  // other pointers and integers. Check for pointer types in terms of LLVM, as
  // some native types (like Obj-C id) may map to a pointer type.
  if (isa<llvm::PointerType>(DstTy)) {
    // The source value may be an integer, or a pointer.
    if (isa<llvm::PointerType>(Src->getType()))
      return Builder.CreateBitCast(Src, DstTy, "conv");

    assert(SrcType->isIntegerType() && "Not ptr->ptr or int->ptr conversion?");
    // First, convert to the correct width so that we control the kind of
    // extension.
    llvm::Type *MiddleTy = CGF.IntPtrTy;
    bool InputSigned = SrcType->isSignedIntegerType();
    llvm::Value* IntResult =
        Builder.CreateIntCast(Src, MiddleTy, InputSigned, "conv");
    // Then, cast to pointer.
    return Builder.CreateIntToPtr(IntResult, DstTy, "conv");
  }

  if (isa<llvm::PointerType>(Src->getType())) {
    // Must be an ptr to int cast.
    assert(isa<llvm::IntegerType>(DstTy) && "not ptr->int?");
    return Builder.CreatePtrToInt(Src, DstTy, "conv");
  }

  // A scalar can be splatted to an extended vector of the same element type
  if (DstType->isVectorType() && !SrcType->isVectorType()) {
    // Cast the scalar to element type
    Type EltTy = DstType->getAs<VectorType>()->getElementType();
    llvm::Value *Elt = EmitScalarConversion(Src, SrcType, EltTy);

    // Insert the element in element zero of an undef vector
    llvm::Value *UnV = llvm::UndefValue::get(DstTy);
    llvm::Value *Idx = Builder.getInt32(0);
    UnV = Builder.CreateInsertElement(UnV, Elt, Idx, "tmp");

    // Splat the element across to all elements
    llvm::SmallVector<llvm::Constant*, 16> Args;
    unsigned NumElements = cast<llvm::VectorType>(DstTy)->getNumElements();
    for (unsigned i = 0; i != NumElements; ++i)
      Args.push_back(Builder.getInt32(0));

    llvm::Constant *Mask = llvm::ConstantVector::get(Args);
    llvm::Value *Yay = Builder.CreateShuffleVector(UnV, UnV, Mask, "splat");
    return Yay;
  }

  // Allow bitcast from vector to integer/fp of the same size.
  if (isa<llvm::VectorType>(Src->getType()) ||
      isa<llvm::VectorType>(DstTy))
    return Builder.CreateBitCast(Src, DstTy, "conv");

  // Finally, we have the arithmetic types: real int/float.
  if (isa<llvm::IntegerType>(Src->getType())) {
    bool InputSigned = SrcType->isSignedIntegerType();
    if (isa<llvm::IntegerType>(DstTy))
      return Builder.CreateIntCast(Src, DstTy, InputSigned, "conv");
    else if (InputSigned)
      return Builder.CreateSIToFP(Src, DstTy, "conv");
    else
      return Builder.CreateUIToFP(Src, DstTy, "conv");
  }

  assert(Src->getType()->isFloatingPointTy() && "Unknown real conversion");
  if (isa<llvm::IntegerType>(DstTy)) {
    if (DstType->isSignedIntegerType())
      return Builder.CreateFPToSI(Src, DstTy, "conv");
    else
      return Builder.CreateFPToUI(Src, DstTy, "conv");
  }

  assert(DstTy->isFloatingPointTy() && "Unknown real conversion");
  if (DstTy->getTypeID() < Src->getType()->getTypeID())
    return Builder.CreateFPTrunc(Src, DstTy, "conv");
  else
    return Builder.CreateFPExt(Src, DstTy, "conv");
}

/// EmitComplexToScalarConversion - Emit a conversion from the specified complex
/// type to the specified destination type, where the destination type is an
/// LLVM scalar type.
Value *ScalarExprEmitter::
EmitComplexToScalarConversion(CodeGenFunction::ComplexPairTy Src,
                              Type SrcTy, Type DstTy) {
  // Get the source element type.
  //SrcTy = SrcTy->getAs<ComplexType>()->getElementType();

  // Handle conversions to bool first, they are special: comparisons against 0.
  if (DstTy->isLogicalType()) {
    //  Complex != 0  -> (Real != 0) | (Imag != 0)
    Src.first  = EmitScalarConversion(Src.first, SrcTy, DstTy);
    Src.second = EmitScalarConversion(Src.second, SrcTy, DstTy);
    return Builder.CreateOr(Src.first, Src.second, "tobool");
  }

  // C99 6.3.1.7p2: "When a value of complex type is converted to a real type,
  // the imaginary part of the complex value is discarded and the value of the
  // real part is converted according to the conversion rules for the
  // corresponding real type.
  return EmitScalarConversion(Src.first, SrcTy, DstTy);
}

Value *ScalarExprEmitter::EmitNullValue(Type Ty) {
  return llvm::Constant::getNullValue(ConvertType(Ty));
}

//===----------------------------------------------------------------------===//
//                            Visitor Methods
//===----------------------------------------------------------------------===//

Value *ScalarExprEmitter::VisitExpr(Expr *E) {
  CGF.ErrorUnsupported(E, "scalar expression");
//  if (E->getType()->isVoidType())
//    return 0;
  return llvm::UndefValue::get(CGF.ConvertType(E->getType()));
}

Value *ScalarExprEmitter::VisitMemberExpr(MemberExpr *E) {
  Expr::EvalResult Result;
  if (E->Evaluate(Result, CGF.getContext()) && Result.Val.isInt()) {
    EmitLValue(E->getBase());
    return Builder.getInt(Result.Val.getInt());
  }

  // Emit debug info for aggregate now, if it was delayed to reduce 
  // debug info size.
  CGDebugInfo *DI = CGF.getDebugInfo();
  if (DI && CGF.CGM.getCodeGenOpts().LimitDebugInfo) {
    Type PQTy = E->getBase()->IgnoreParenImpCasts()->getType();
  }
  return EmitLoadOfLValue(E);
}

Value *ScalarExprEmitter::VisitArrayIndex(ArrayIndex *E) {
  TestAndClearIgnoreResultAssign();

  // Emit subscript expressions in rvalue context's.  For most cases, this just
  // loads the lvalue formed by the subscript expr.  However, we have to be
  // careful, because the base of a vector subscript is occasionally an rvalue,
  // so we can't get it as an lvalue.
  if (!E->getBase()->getType()->isVectorType())
    return EmitLoadOfLValue(E);

  // Handle the vector case.  The base must be a vector, the index must be an
  // integer value.
  Value *Base = Visit(E->getBase());
  Value *Idx  = Visit(E->getIdx(0));
  bool IdxSigned = E->getIdx(0)->getType()->isSignedIntegerType();
  Idx = Builder.CreateIntCast(Idx, CGF.Int32Ty, IdxSigned, "vecidxcast");
  return Builder.CreateExtractElement(Base, Idx, "vecext");
}

static llvm::Constant *getMaskElt(llvm::ShuffleVectorInst *SVI, unsigned Idx,
                                  unsigned Off, llvm::Type *I32Ty) {
  int MV = SVI->getMaskValue(Idx);
  if (MV == -1) 
    return llvm::UndefValue::get(I32Ty);
  return llvm::ConstantInt::get(I32Ty, Off+MV);
}

Value *ScalarExprEmitter::VisitInitListExpr(InitListExpr *E) {
  bool Ignore = TestAndClearIgnoreResultAssign();
  (void)Ignore;
  assert (Ignore == false && "init list ignored");
  unsigned NumInitElements = E->getNumInits();
  
  if (E->hadArrayRangeDesignator())
    CGF.ErrorUnsupported(E, "GNU array range designator extension");
  
  llvm::VectorType *VType =
    dyn_cast<llvm::VectorType>(ConvertType(E->getType()));
  
  // We have a scalar in braces. Just use the first element.
  if (!VType)
    return Visit(E->getInit(0));
  
  unsigned ResElts = VType->getNumElements();
  
  // Loop over initializers collecting the Value for each, and remembering 
  // whether the source was swizzle (ExtVectorElementExpr).  This will allow
  // us to fold the shuffle for the swizzle into the shuffle for the vector
  // initializer, since LLVM optimizers generally do not want to touch
  // shuffles.
  unsigned CurIdx = 0;
  bool VIsUndefShuffle = false;
  llvm::Value *V = llvm::UndefValue::get(VType);
  for (unsigned i = 0; i != NumInitElements; ++i) {
    Expr *IE = E->getInit(i);
    Value *Init = Visit(IE);
    llvm::SmallVector<llvm::Constant*, 16> Args;
    
    llvm::VectorType *VVT = dyn_cast<llvm::VectorType>(Init->getType());
    
    // Handle scalar elements.  If the scalar initializer is actually one
    // element of a different vector of the same width, use shuffle instead of 
    // extract+insert.
    if (!VVT) {
      V = Builder.CreateInsertElement(V, Init, Builder.getInt32(CurIdx),
                                      "vecinit");
      VIsUndefShuffle = false;
      ++CurIdx;
      continue;
    }
    
    unsigned InitElts = VVT->getNumElements();

    // If the initializer is an ExtVecEltExpr (a swizzle), and the swizzle's 
    // input is the same width as the vector being constructed, generate an
    // optimized shuffle of the swizzle input into the result.
    unsigned Offset = (CurIdx == 0) ? 0 : ResElts;

    // Extend init to result vector length, and then shuffle its contribution
    // to the vector initializer into V.
    if (Args.empty()) {
      for (unsigned j = 0; j != InitElts; ++j)
        Args.push_back(Builder.getInt32(j));
      for (unsigned j = InitElts; j != ResElts; ++j)
        Args.push_back(llvm::UndefValue::get(CGF.Int32Ty));
      llvm::Constant *Mask = llvm::ConstantVector::get(Args);
      Init = Builder.CreateShuffleVector(Init, llvm::UndefValue::get(VVT),
                                         Mask, "vext");

      Args.clear();
      for (unsigned j = 0; j != CurIdx; ++j)
        Args.push_back(Builder.getInt32(j));
      for (unsigned j = 0; j != InitElts; ++j)
        Args.push_back(Builder.getInt32(j+Offset));
      for (unsigned j = CurIdx + InitElts; j != ResElts; ++j)
        Args.push_back(llvm::UndefValue::get(CGF.Int32Ty));
    }

    // If V is undef, make sure it ends up on the RHS of the shuffle to aid
    // merging subsequent shuffles into this one.
    if (CurIdx == 0)
      std::swap(V, Init);
    llvm::Constant *Mask = llvm::ConstantVector::get(Args);
    V = Builder.CreateShuffleVector(V, Init, Mask, "vecinit");
    VIsUndefShuffle = isa<llvm::UndefValue>(Init);
    CurIdx += InitElts;
  }
  
  // FIXME: evaluate codegen vs. shuffling against constant null vector.
  // Emit remaining default initializers.
  llvm::Type *EltTy = VType->getElementType();
  
  // Emit remaining default initializers
  for (/* Do not initialize i*/; CurIdx < ResElts; ++CurIdx) {
    Value *Idx = Builder.getInt32(CurIdx);
    llvm::Value *Init = llvm::Constant::getNullValue(EltTy);
    V = Builder.CreateInsertElement(V, Init, Idx, "vecinit");
  }
  return V;
}

//===----------------------------------------------------------------------===//
//                             Unary Operators
//===----------------------------------------------------------------------===//

llvm::Value *ScalarExprEmitter::
EmitAddConsiderOverflowBehavior(const UnaryOperator *E,
                                llvm::Value *InVal,
                                llvm::Value *NextVal, bool IsInc) {
  switch (CGF.getContext().getLangOptions().getSignedOverflowBehavior()) {
  case LangOptions::SOB_Undefined:
    return Builder.CreateNSWAdd(InVal, NextVal, IsInc ? "inc" : "dec");
    break;
  case LangOptions::SOB_Defined:
    return Builder.CreateAdd(InVal, NextVal, IsInc ? "inc" : "dec");
    break;
  case LangOptions::SOB_Trapping:
    BinOpInfo BinOp;
    BinOp.LHS = InVal;
    BinOp.RHS = NextVal;
    BinOp.Ty = E->getType();
    BinOp.Opcode = BO_Add;
    BinOp.E = E;
    return EmitOverflowCheckedBinOp(BinOp);
    break;
  }
  assert(false && "Unknown SignedOverflowBehaviorTy");
  return 0;
}

llvm::Value *
ScalarExprEmitter::EmitScalarPrePostIncDec(const UnaryOperator *E, LValue LV,
                                           bool isInc, bool isPre) {
  
  Type type = E->getSubExpr()->getType();
  llvm::Value *value = EmitLoadOfLValue(LV, type);
  llvm::Value *input = value;

  int amount = (isInc ? 1 : -1);

  // Special case of integer increment that we have to check first: bool++.
  // Due to promotion rules, we get:
  //   bool++ -> bool = bool + 1
  //          -> bool = (int)bool + 1
  //          -> bool = ((int)bool + 1 != 0)
  // An interesting aspect of this is that increment is always true.
  // Decrement does not have this property.
  if (isInc && type->isLogicalType()) {
    value = Builder.getTrue();

  // Most common case by far: integer increment.
  } else if (type->isIntegerType()) {

    llvm::Value *amt = llvm::ConstantInt::get(value->getType(), amount);

    // Note that signed integer inc/dec with width less than int can't
    // overflow because of promotion rules; we're just eliding a few steps here.
    if (type->isSignedIntegerType() &&
        value->getType()->getPrimitiveSizeInBits() >=
            CGF.IntTy->getBitWidth())
      value = EmitAddConsiderOverflowBehavior(E, value, amt, isInc);
    else
      value = Builder.CreateAdd(value, amt, isInc ? "inc" : "dec");
  
  // Next most common: pointer increment.
  } else if (type->isVectorType()) {
//    if (type->hasIntegerRepresentation()) {
//      llvm::Value *amt = llvm::ConstantInt::get(value->getType(), amount);
//
//      value = Builder.CreateAdd(value, amt, isInc ? "inc" : "dec");
//    } else {
      value = Builder.CreateFAdd(
                  value,
                  llvm::ConstantFP::get(value->getType(), amount),
                  isInc ? "inc" : "dec");
 //   }

  // Floating point.
  } else if (type->isFloatingPointType()) {
    // Add the inc/dec to the real part.
    llvm::Value *amt;
    if (value->getType()->isFloatTy())
      amt = llvm::ConstantFP::get(VMContext,
                                  llvm::APFloat(static_cast<float>(amount)));
    else if (value->getType()->isDoubleTy())
      amt = llvm::ConstantFP::get(VMContext,
                                  llvm::APFloat(static_cast<double>(amount)));
    else {
      llvm::APFloat F(static_cast<float>(amount));
      bool ignored;
      F.convert(CGF.Target.getLongDoubleFormat(), llvm::APFloat::rmTowardZero,
                &ignored);
      amt = llvm::ConstantFP::get(VMContext, F);
    }
    value = Builder.CreateFAdd(value, amt, isInc ? "inc" : "dec");
  }
  
  // Store the updated result through the lvalue.
  CGF.EmitStoreThroughLValue(RValue::get(value), LV);
  
  // If this is a postinc, return the value read from memory, otherwise use the
  // updated value.
  return isPre ? value : input;
}



Value *ScalarExprEmitter::VisitUnaryMinus(const UnaryOperator *E) {
  TestAndClearIgnoreResultAssign();
  // Emit unary minus with EmitSub so we handle overflow cases etc.
  BinOpInfo BinOp;
  BinOp.RHS = Visit(E->getSubExpr());
  
  if (BinOp.RHS->getType()->isFPOrFPVectorTy())
    BinOp.LHS = llvm::ConstantFP::getZeroValueForNegation(BinOp.RHS->getType());
  else 
    BinOp.LHS = llvm::Constant::getNullValue(BinOp.RHS->getType());
  BinOp.Ty = E->getType();
  BinOp.Opcode = BO_Sub;
  BinOp.E = E;
  return EmitSub(BinOp);
}

Value *ScalarExprEmitter::VisitUnaryNot(const UnaryOperator *E) {
  TestAndClearIgnoreResultAssign();
  Value *Op = Visit(E->getSubExpr());
  return Builder.CreateNot(Op, "neg");
}

Value *ScalarExprEmitter::VisitUnaryLNot(const UnaryOperator *E) {
  // Compare operand to zero.
  Value *BoolVal = CGF.EvaluateExprAsBool(E->getSubExpr());

  // Invert value.
  // TODO: Could dynamically modify easy computations here.  For example, if
  // the operand is an icmp ne, turn into icmp eq.
  BoolVal = Builder.CreateNot(BoolVal, "lnot");

  // ZExt result to the expr type.
  return Builder.CreateZExt(BoolVal, ConvertType(E->getType()), "lnot.ext");
}

Value *ScalarExprEmitter::VisitUnaryReal(const UnaryOperator *E) {
  Expr *Op = E->getSubExpr();
  if (Op->getType().hasComplexAttr()) {
    // If it's an l-value, load through the appropriate subobject l-value.
    // Note that we have to ask E because Op might be an l-value that
    // this won't work for, e.g. an Obj-C property.
    if (E->isLValue())
      return CGF.EmitLoadOfLValue(CGF.EmitLValue(E), Op->getType()).getScalarVal();

    // Otherwise, calculate and project.
    return CGF.EmitComplexExpr(Op, false, true).first;
  }

  return Visit(Op);
}

Value *ScalarExprEmitter::VisitUnaryImag(const UnaryOperator *E) {
  Expr *Op = E->getSubExpr();
  if (Op->getType().hasComplexAttr()) {
    // If it's an l-value, load through the appropriate subobject l-value.
    // Note that we have to ask E because Op might be an l-value that
    // this won't work for, e.g. an Obj-C property.
    if (Op->isLValue())
      return CGF.EmitLoadOfLValue(CGF.EmitLValue(E), Op->getType()).getScalarVal();

    // Otherwise, calculate and project.
    return CGF.EmitComplexExpr(Op, true, false).second;
  }

  // __imag on a scalar returns zero.  Emit the subexpr to ensure side
  // effects are evaluated, but not the actual value.
  CGF.EmitScalarExpr(Op, true);
  return llvm::Constant::getNullValue(ConvertType(E->getType()));
}

//===----------------------------------------------------------------------===//
//                           Binary Operators
//===----------------------------------------------------------------------===//

BinOpInfo ScalarExprEmitter::EmitBinOps(const BinaryOperator *E) {
  TestAndClearIgnoreResultAssign();
  BinOpInfo Result;
  Result.LHS = Visit(E->getLHS());
  Result.RHS = Visit(E->getRHS());
  Result.Ty  = E->getType();
  Result.Opcode = E->getOpcode();
  Result.E = E;
  return Result;
}

LValue ScalarExprEmitter::EmitCompoundAssignLValue(
                                              const CompoundAssignOperator *E,
                        Value *(ScalarExprEmitter::*Func)(const BinOpInfo &),
                                                   Value *&Result) {
  Type LHSTy = E->getLHS()->getType();
  BinOpInfo OpInfo;
  
  if (E->getComputationResultType().hasComplexAttr()) {
    // This needs to go through the complex expression emitter, but it's a tad
    // complicated to do that... I'm leaving it out for now.  (Note that we do
    // actually need the imaginary part of the RHS for multiplication and
    // division.)
    CGF.ErrorUnsupported(E, "complex compound assignment");
    Result = llvm::UndefValue::get(CGF.ConvertType(E->getType()));
    return LValue();
  }
  
  // Emit the RHS first.  __block variables need to have the rhs evaluated
  // first, plus this should improve codegen a little.
  OpInfo.RHS = Visit(E->getRHS());
  OpInfo.Ty = E->getComputationResultType();
  OpInfo.Opcode = E->getOpcode();
  OpInfo.E = E;
  // Load/convert the LHS.
  LValue LHSLV = EmitCheckedLValue(E->getLHS());
  OpInfo.LHS = EmitLoadOfLValue(LHSLV, OpInfo.Ty);
  OpInfo.LHS = EmitScalarConversion(OpInfo.LHS, LHSTy,
                                    E->getComputationLHSType());
  
  // Expand the binary operator.
  Result = (this->*Func)(OpInfo);
  
  // Convert the result back to the LHS type.
  Result = EmitScalarConversion(Result, E->getComputationResultType(), LHSTy);
  
  // Store the result value into the LHS lvalue. Bit-fields are handled
  // specially because the result is altered by the store, i.e., [C99 6.5.16p1]
  // 'An assignment expression has the value of the left operand after the
  // assignment...'.
  CGF.EmitStoreThroughLValue(RValue::get(Result), LHSLV);

  return LHSLV;
}

Value *ScalarExprEmitter::EmitCompoundAssign(const CompoundAssignOperator *E,
                      Value *(ScalarExprEmitter::*Func)(const BinOpInfo &)) {
  bool Ignore = TestAndClearIgnoreResultAssign();
  Value *RHS;
  LValue LHS = EmitCompoundAssignLValue(E, Func, RHS);

  // If the result is clearly ignored, return now.
  if (Ignore)
    return 0;

  // The result of an assignment in C is the assigned r-value.
  if (!CGF.getContext().getLangOptions().OOP)
    return RHS;

  // Objective-C property assignment never reloads the value following a store.
  if (LHS.isPropertyRef())
    return RHS;

  // If the lvalue is non-volatile, return the computed value of the assignment.
  if (!LHS.isVolatileQualified())
    return RHS;

  // Otherwise, reload the value.
  return EmitLoadOfLValue(LHS, E->getType());
}

void ScalarExprEmitter::EmitUndefinedBehaviorIntegerDivAndRemCheck(
     					    const BinOpInfo &Ops, 
				     	    llvm::Value *Zero, bool isDiv) {
  llvm::BasicBlock *overflowBB = CGF.createBasicBlock("overflow", CGF.CurFn);
  llvm::BasicBlock *contBB =
    CGF.createBasicBlock(isDiv ? "div.cont" : "rem.cont", CGF.CurFn);

  llvm::IntegerType *Ty = cast<llvm::IntegerType>(Zero->getType());

  if (Ops.Ty->hasSignedIntegerRepresentation()) {
    llvm::Value *IntMin =
      Builder.getInt(llvm::APInt::getSignedMinValue(Ty->getBitWidth()));
    llvm::Value *NegOne = llvm::ConstantInt::get(Ty, -1ULL);

    llvm::Value *Cond1 = Builder.CreateICmpEQ(Ops.RHS, Zero);
    llvm::Value *LHSCmp = Builder.CreateICmpEQ(Ops.LHS, IntMin);
    llvm::Value *RHSCmp = Builder.CreateICmpEQ(Ops.RHS, NegOne);
    llvm::Value *Cond2 = Builder.CreateAnd(LHSCmp, RHSCmp, "and");
    Builder.CreateCondBr(Builder.CreateOr(Cond1, Cond2, "or"), 
                         overflowBB, contBB);
  } else {
    CGF.Builder.CreateCondBr(Builder.CreateICmpEQ(Ops.RHS, Zero), 
                             overflowBB, contBB);
  }
  EmitOverflowBB(overflowBB);
  Builder.SetInsertPoint(contBB);
}

Value *ScalarExprEmitter::EmitRDiv(const BinOpInfo &Ops) {
  if (isTrapvOverflowBehavior()) { 
    llvm::Value *Zero = llvm::Constant::getNullValue(ConvertType(Ops.Ty));

    if (Ops.Ty->isIntegerType())
      EmitUndefinedBehaviorIntegerDivAndRemCheck(Ops, Zero, true);
    else if (Ops.Ty->isFloatingPointType()) {
      llvm::BasicBlock *overflowBB = CGF.createBasicBlock("overflow",
                                                          CGF.CurFn);
      llvm::BasicBlock *DivCont = CGF.createBasicBlock("div.cont", CGF.CurFn);
      CGF.Builder.CreateCondBr(Builder.CreateFCmpOEQ(Ops.RHS, Zero), 
                               overflowBB, DivCont);
      EmitOverflowBB(overflowBB);
      Builder.SetInsertPoint(DivCont);
    }
  }
  if (Ops.LHS->getType()->isFPOrFPVectorTy())
    return Builder.CreateFDiv(Ops.LHS, Ops.RHS, "div");
  else if (Ops.Ty->hasUnsignedIntegerRepresentation())
    return Builder.CreateUDiv(Ops.LHS, Ops.RHS, "div");
  else
    return Builder.CreateSDiv(Ops.LHS, Ops.RHS, "div");
}

Value *ScalarExprEmitter::EmitLDiv(const BinOpInfo &Ops) {
  // Rem in C can't be a floating point type: C99 6.5.5p2.
  if (isTrapvOverflowBehavior()) {
    llvm::Value *Zero = llvm::Constant::getNullValue(ConvertType(Ops.Ty));

    if (Ops.Ty->isIntegerType()) 
      EmitUndefinedBehaviorIntegerDivAndRemCheck(Ops, Zero, false);
  }

  if (Ops.Ty->hasUnsignedIntegerRepresentation())
    return Builder.CreateURem(Ops.LHS, Ops.RHS, "rem");
  else
    return Builder.CreateSRem(Ops.LHS, Ops.RHS, "rem");
}

Value *ScalarExprEmitter::EmitOverflowCheckedBinOp(const BinOpInfo &Ops) {
  unsigned IID;
  unsigned OpID = 0;

  switch (Ops.Opcode) {
  case BO_Add:
  case BO_AddAssign:
    OpID = 1;
    IID = llvm::Intrinsic::sadd_with_overflow;
    break;
  case BO_Sub:
  case BO_SubAssign:
    OpID = 2;
    IID = llvm::Intrinsic::ssub_with_overflow;
    break;
  case BO_Mul:
  case BO_MulAssign:
    OpID = 3;
    IID = llvm::Intrinsic::smul_with_overflow;
    break;
  default:
    assert(false && "Unsupported operation for overflow detection");
    IID = 0;
  }
  OpID <<= 1;
  OpID |= 1;

  llvm::Type *opTy = CGF.CGM.getTypes().ConvertType(Ops.Ty);

  llvm::Function *intrinsic = CGF.CGM.getIntrinsic(IID, opTy);

  Value *resultAndOverflow = Builder.CreateCall2(intrinsic, Ops.LHS, Ops.RHS);
  Value *result = Builder.CreateExtractValue(resultAndOverflow, 0);
  Value *overflow = Builder.CreateExtractValue(resultAndOverflow, 1);

  // Branch in case of overflow.
  llvm::BasicBlock *initialBB = Builder.GetInsertBlock();
  llvm::BasicBlock *overflowBB = CGF.createBasicBlock("overflow", CGF.CurFn);
  llvm::BasicBlock *continueBB = CGF.createBasicBlock("nooverflow", CGF.CurFn);

  Builder.CreateCondBr(overflow, overflowBB, continueBB);

  // Handle overflow with llvm.trap.
  const std::string *handlerName = 
    &CGF.getContext().getLangOptions().OverflowHandler;
  if (handlerName->empty()) {
    EmitOverflowBB(overflowBB);
    Builder.SetInsertPoint(continueBB);
    return result;
  }

  // If an overflow handler is set, then we want to call it and then use its
  // result, if it returns.
  Builder.SetInsertPoint(overflowBB);

  // Get the overflow handler.
  llvm::Type *Int8Ty = llvm::Type::getInt8Ty(VMContext);
  llvm::Type *argTypes[] = { CGF.Int64Ty, CGF.Int64Ty, Int8Ty, Int8Ty };
  llvm::FunctionType *handlerTy =
      llvm::FunctionType::get(CGF.Int64Ty, argTypes, true);
  llvm::Value *handler = CGF.CGM.CreateRuntimeFunction(handlerTy, *handlerName);

  // Sign extend the args to 64-bit, so that we can use the same handler for
  // all types of overflow.
  llvm::Value *lhs = Builder.CreateSExt(Ops.LHS, CGF.Int64Ty);
  llvm::Value *rhs = Builder.CreateSExt(Ops.RHS, CGF.Int64Ty);

  // Call the handler with the two arguments, the operation, and the size of
  // the result.
  llvm::Value *handlerResult = Builder.CreateCall4(handler, lhs, rhs,
      Builder.getInt8(OpID),
      Builder.getInt8(cast<llvm::IntegerType>(opTy)->getBitWidth()));

  // Truncate the result back to the desired size.
  handlerResult = Builder.CreateTrunc(handlerResult, opTy);
  Builder.CreateBr(continueBB);

  Builder.SetInsertPoint(continueBB);
  llvm::PHINode *phi = Builder.CreatePHI(opTy, 2);
  phi->addIncoming(result, initialBB);
  phi->addIncoming(handlerResult, overflowBB);

  return phi;
}

Value *ScalarExprEmitter::EmitAdd(const BinOpInfo &op) {
//  if (op.LHS->getType()->isPointerTy() ||
//      op.RHS->getType()->isPointerTy())
//    return emitPointerArithmetic(CGF, op, /*subtraction*/ false);

  if (op.Ty->isSignedIntegerType()) {
    switch (CGF.getContext().getLangOptions().getSignedOverflowBehavior()) {
    case LangOptions::SOB_Undefined:
      return Builder.CreateNSWAdd(op.LHS, op.RHS, "add");
    case LangOptions::SOB_Defined:
      return Builder.CreateAdd(op.LHS, op.RHS, "add");
    case LangOptions::SOB_Trapping:
      return EmitOverflowCheckedBinOp(op);
    }
  }
    
  if (op.LHS->getType()->isFPOrFPVectorTy())
    return Builder.CreateFAdd(op.LHS, op.RHS, "add");

  return Builder.CreateAdd(op.LHS, op.RHS, "add");
}

Value *ScalarExprEmitter::EmitSub(const BinOpInfo &op) {
  // The LHS is always a pointer if either side is.
  if (!op.LHS->getType()->isPointerTy()) {
    if (op.Ty->isSignedIntegerType()) {
      switch (CGF.getContext().getLangOptions().getSignedOverflowBehavior()) {
      case LangOptions::SOB_Undefined:
        return Builder.CreateNSWSub(op.LHS, op.RHS, "sub");
      case LangOptions::SOB_Defined:
        return Builder.CreateSub(op.LHS, op.RHS, "sub");
      case LangOptions::SOB_Trapping:
        return EmitOverflowCheckedBinOp(op);
      }
    }
    
    if (op.LHS->getType()->isFPOrFPVectorTy())
      return Builder.CreateFSub(op.LHS, op.RHS, "sub");

    return Builder.CreateSub(op.LHS, op.RHS, "sub");
  }

  // If the RHS is not a pointer, then we have normal pointer
  // arithmetic.
//  if (!op.RHS->getType()->isPointerTy())
//    return emitPointerArithmetic(CGF, op, /*subtraction*/ true);

  // Otherwise, this is a pointer subtraction.

  // Do the raw subtraction part.
  llvm::Value *LHS
    = Builder.CreatePtrToInt(op.LHS, CGF.PtrDiffTy, "sub.ptr.lhs.cast");
  llvm::Value *RHS
    = Builder.CreatePtrToInt(op.RHS, CGF.PtrDiffTy, "sub.ptr.rhs.cast");
  Value *diffInChars = Builder.CreateSub(LHS, RHS, "sub.ptr.sub");

  // Okay, figure out the element size.
  const BinaryOperator *expr = cast<BinaryOperator>(op.E);
  Type elementType = expr->getLHS()->getType()/*->getPointeeType()*/;

  llvm::Value *divisor = 0;

  CharUnits elementSize;

  // Handle GCC extension for pointer arithmetic on void* and
  // function pointer types.
  if (elementType->isFunctionType())
	  elementSize = CharUnits::One();
  else
	  elementSize = CGF.getContext().getTypeSizeInChars(elementType);

  // Don't even emit the divide for element size of 1.
  if (elementSize.isOne())
	  return diffInChars;

  divisor = CGF.CGM.getSize(elementSize);
  
  // Otherwise, do a full sdiv. This uses the "exact" form of sdiv, since
  // pointer difference in C is only defined in the case where both operands
  // are pointing to elements of an array.
  return Builder.CreateExactSDiv(diffInChars, divisor, "sub.ptr.div");
}

Value *ScalarExprEmitter::EmitShl(const BinOpInfo &Ops) {
  // LLVM requires the LHS and RHS to be the same type: promote or truncate the
  // RHS to the same size as the LHS.
  Value *RHS = Ops.RHS;
  if (Ops.LHS->getType() != RHS->getType())
    RHS = Builder.CreateIntCast(RHS, Ops.LHS->getType(), false, "sh_prom");

  if (CGF.CatchUndefined 
      && isa<llvm::IntegerType>(Ops.LHS->getType())) {
    unsigned Width = cast<llvm::IntegerType>(Ops.LHS->getType())->getBitWidth();
    llvm::BasicBlock *Cont = CGF.createBasicBlock("cont");
    CGF.Builder.CreateCondBr(Builder.CreateICmpULT(RHS,
                                 llvm::ConstantInt::get(RHS->getType(), Width)),
                             Cont, CGF.getTrapBB());
    CGF.EmitBlock(Cont);
  }

  return Builder.CreateShl(Ops.LHS, RHS, "shl");
}

Value *ScalarExprEmitter::EmitShr(const BinOpInfo &Ops) {
  // LLVM requires the LHS and RHS to be the same type: promote or truncate the
  // RHS to the same size as the LHS.
  Value *RHS = Ops.RHS;
  if (Ops.LHS->getType() != RHS->getType())
    RHS = Builder.CreateIntCast(RHS, Ops.LHS->getType(), false, "sh_prom");

  if (CGF.CatchUndefined 
      && isa<llvm::IntegerType>(Ops.LHS->getType())) {
    unsigned Width = cast<llvm::IntegerType>(Ops.LHS->getType())->getBitWidth();
    llvm::BasicBlock *Cont = CGF.createBasicBlock("cont");
    CGF.Builder.CreateCondBr(Builder.CreateICmpULT(RHS,
                                 llvm::ConstantInt::get(RHS->getType(), Width)),
                             Cont, CGF.getTrapBB());
    CGF.EmitBlock(Cont);
  }

  if (Ops.Ty->hasUnsignedIntegerRepresentation())
    return Builder.CreateLShr(Ops.LHS, RHS, "shr");
  return Builder.CreateAShr(Ops.LHS, RHS, "shr");
}

enum IntrinsicType { VCMPEQ, VCMPGT };
// return corresponding comparison intrinsic for given vector type
static llvm::Intrinsic::ID GetIntrinsic(IntrinsicType IT,
                                        SimpleNumericType::NumericKind ElemKind) {
  switch (ElemKind) {
  default: assert(0 && "unexpected element type");
  case SimpleNumericType::Char:
    return (IT == VCMPEQ) ? llvm::Intrinsic::ppc_altivec_vcmpequb_p :
                            llvm::Intrinsic::ppc_altivec_vcmpgtub_p;
    break;
  case SimpleNumericType::UInt8:
  case SimpleNumericType::UInt16:
    return (IT == VCMPEQ) ? llvm::Intrinsic::ppc_altivec_vcmpequh_p :
                            llvm::Intrinsic::ppc_altivec_vcmpgtuh_p;
    break;
  case SimpleNumericType::Int8:
  case SimpleNumericType::Int16:
    return (IT == VCMPEQ) ? llvm::Intrinsic::ppc_altivec_vcmpequh_p :
                            llvm::Intrinsic::ppc_altivec_vcmpgtsh_p;
    break;
  case SimpleNumericType::UInt32:
  case SimpleNumericType::UInt64:
    return (IT == VCMPEQ) ? llvm::Intrinsic::ppc_altivec_vcmpequw_p :
                            llvm::Intrinsic::ppc_altivec_vcmpgtuw_p;
    break;
  case SimpleNumericType::Int32:
  case SimpleNumericType::Int64:
    return (IT == VCMPEQ) ? llvm::Intrinsic::ppc_altivec_vcmpequw_p :
                            llvm::Intrinsic::ppc_altivec_vcmpgtsw_p;
    break;
  case SimpleNumericType::Single:
    return (IT == VCMPEQ) ? llvm::Intrinsic::ppc_altivec_vcmpeqfp_p :
                            llvm::Intrinsic::ppc_altivec_vcmpgtfp_p;
    break;
  }
  return llvm::Intrinsic::not_intrinsic;
}

Value *ScalarExprEmitter::EmitCompare(const BinaryOperator *E,unsigned UICmpOpc,
                                      unsigned SICmpOpc, unsigned FCmpOpc) {
  TestAndClearIgnoreResultAssign();
  Value *Result;
  Type LHSTy = E->getLHS()->getType();
  if (!LHSTy.hasComplexAttr()) {
    Value *LHS = Visit(E->getLHS());
    Value *RHS = Visit(E->getRHS());

    // If AltiVec, the comparison results in a numeric type, so we use
    // intrinsics comparing vectors and giving 0 or 1 as a result
    if (LHSTy->isVectorType() && !E->getType()->isVectorType()) {
      // constants for mapping CR6 register bits to predicate result
      enum { CR6_EQ=0, CR6_EQ_REV, CR6_LT, CR6_LT_REV } CR6;

      llvm::Intrinsic::ID ID = llvm::Intrinsic::not_intrinsic;

      // in several cases vector arguments order will be reversed
      Value *FirstVecArg = LHS,
            *SecondVecArg = RHS;

      Type ElTy = LHSTy->getAs<VectorType>()->getElementType();
      const SimpleNumericType *BTy = ElTy->getAs<SimpleNumericType>();
      SimpleNumericType::NumericKind ElementKind = BTy->getKind();

      switch(E->getOpcode()) {
      default: assert(0 && "is not a comparison operation");
      case BO_EQ:
        CR6 = CR6_LT;
        ID = GetIntrinsic(VCMPEQ, ElementKind);
        break;
      case BO_NE:
        CR6 = CR6_EQ;
        ID = GetIntrinsic(VCMPEQ, ElementKind);
        break;
      case BO_LT:
        CR6 = CR6_LT;
        ID = GetIntrinsic(VCMPGT, ElementKind);
        std::swap(FirstVecArg, SecondVecArg);
        break;
      case BO_GT:
        CR6 = CR6_LT;
        ID = GetIntrinsic(VCMPGT, ElementKind);
        break;
      case BO_LE:
        if (ElementKind == SimpleNumericType::Single) {
          CR6 = CR6_LT;
          ID = llvm::Intrinsic::ppc_altivec_vcmpgefp_p;
          std::swap(FirstVecArg, SecondVecArg);
        }
        else {
          CR6 = CR6_EQ;
          ID = GetIntrinsic(VCMPGT, ElementKind);
        }
        break;
      case BO_GE:
        if (ElementKind == SimpleNumericType::Single) {
          CR6 = CR6_LT;
          ID = llvm::Intrinsic::ppc_altivec_vcmpgefp_p;
        }
        else {
          CR6 = CR6_EQ;
          ID = GetIntrinsic(VCMPGT, ElementKind);
          std::swap(FirstVecArg, SecondVecArg);
        }
        break;
      }

      Value *CR6Param = Builder.getInt32(CR6);
      llvm::Function *F = CGF.CGM.getIntrinsic(ID);
      Result = Builder.CreateCall3(F, CR6Param, FirstVecArg, SecondVecArg, "");
      return EmitScalarConversion(Result, CGF.getContext().LogicalTy, E->getType());
    }

    if (LHS->getType()->isFPOrFPVectorTy()) {
      Result = Builder.CreateFCmp((llvm::CmpInst::Predicate)FCmpOpc,
                                  LHS, RHS, "cmp");
    } else if (LHSTy->hasSignedIntegerRepresentation()) {
      Result = Builder.CreateICmp((llvm::ICmpInst::Predicate)SICmpOpc,
                                  LHS, RHS, "cmp");
    } else {
      // Unsigned integers and pointers.
      Result = Builder.CreateICmp((llvm::ICmpInst::Predicate)UICmpOpc,
                                  LHS, RHS, "cmp");
    }

    // If this is a vector comparison, sign extend the result to the appropriate
    // vector integer type and return it (don't convert to bool).
    if (LHSTy->isVectorType())
      return Builder.CreateSExt(Result, ConvertType(E->getType()), "sext");

  } else {
    // Complex Comparison: can only be an equality comparison.
    CodeGenFunction::ComplexPairTy LHS = CGF.EmitComplexExpr(E->getLHS());
    CodeGenFunction::ComplexPairTy RHS = CGF.EmitComplexExpr(E->getRHS());

    //Type CETy = LHSTy->getAs<ComplexType>()->getElementType();

    Value *ResultR, *ResultI;
    if (LHSTy->isFloatingPointType()) {
      ResultR = Builder.CreateFCmp((llvm::FCmpInst::Predicate)FCmpOpc,
                                   LHS.first, RHS.first, "cmp.r");
      ResultI = Builder.CreateFCmp((llvm::FCmpInst::Predicate)FCmpOpc,
                                   LHS.second, RHS.second, "cmp.i");
    } else {
      // Complex comparisons can only be equality comparisons.  As such, signed
      // and unsigned opcodes are the same.
      ResultR = Builder.CreateICmp((llvm::ICmpInst::Predicate)UICmpOpc,
                                   LHS.first, RHS.first, "cmp.r");
      ResultI = Builder.CreateICmp((llvm::ICmpInst::Predicate)UICmpOpc,
                                   LHS.second, RHS.second, "cmp.i");
    }

    if (E->getOpcode() == BO_EQ) {
      Result = Builder.CreateAnd(ResultR, ResultI, "and.ri");
    } else {
      assert(E->getOpcode() == BO_NE &&
             "Complex comparison other than == or != ?");
      Result = Builder.CreateOr(ResultR, ResultI, "or.ri");
    }
  }

  return EmitScalarConversion(Result, CGF.getContext().LogicalTy, E->getType());
}

Value *ScalarExprEmitter::VisitBinAssign(const BinaryOperator *E) {
  bool Ignore = TestAndClearIgnoreResultAssign();

  Value *RHS;
  LValue LHS;

  // If the result is clearly ignored, return now.
  if (Ignore)
    return 0;

  // The result of an assignment in C is the assigned r-value.
  if (!CGF.getContext().getLangOptions().OOP)
    return RHS;

  // Objective-C property assignment never reloads the value following a store.
  if (LHS.isPropertyRef())
    return RHS;

  // If the lvalue is non-volatile, return the computed value of the assignment.
  if (!LHS.isVolatileQualified())
    return RHS;

  // Otherwise, reload the value.
  return EmitLoadOfLValue(LHS, E->getType());
}

Value *ScalarExprEmitter::VisitBinLAnd(const BinaryOperator *E) {
  llvm::Type *ResTy = ConvertType(E->getType());
  
  // If we have 0 && RHS, see if we can elide RHS, if so, just return 0.
  // If we have 1 && X, just emit X without inserting the control flow.
  bool LHSCondVal;
  if (CGF.ConstantFoldsToSimpleInteger(E->getLHS(), LHSCondVal)) {
    if (LHSCondVal) { // If we have 1 && X, just emit X.
      Value *RHSCond = CGF.EvaluateExprAsBool(E->getRHS());
      // ZExt result to int or bool.
      return Builder.CreateZExtOrBitCast(RHSCond, ResTy, "land.ext");
    }

    // 0 && RHS: If it is safe, just elide the RHS, and return 0/false.
    return llvm::Constant::getNullValue(ResTy);
  }

  llvm::BasicBlock *ContBlock = CGF.createBasicBlock("land.end");
  llvm::BasicBlock *RHSBlock  = CGF.createBasicBlock("land.rhs");

  CodeGenFunction::ConditionalEvaluation eval(CGF);

  // Branch on the LHS first.  If it is false, go to the failure (cont) block.
  CGF.EmitBranchOnBoolExpr(E->getLHS(), RHSBlock, ContBlock);

  // Any edges into the ContBlock are now from an (indeterminate number of)
  // edges from this first condition.  All of these values will be false.  Start
  // setting up the PHI node in the Cont Block for this.
  llvm::PHINode *PN = llvm::PHINode::Create(llvm::Type::getInt1Ty(VMContext), 2,
                                            "", ContBlock);
  for (llvm::pred_iterator PI = pred_begin(ContBlock), PE = pred_end(ContBlock);
       PI != PE; ++PI)
    PN->addIncoming(llvm::ConstantInt::getFalse(VMContext), *PI);

  eval.begin(CGF);
  CGF.EmitBlock(RHSBlock);
  Value *RHSCond = CGF.EvaluateExprAsBool(E->getRHS());
  eval.end(CGF);

  // Reaquire the RHS block, as there may be subblocks inserted.
  RHSBlock = Builder.GetInsertBlock();

  // Emit an unconditional branch from this block to ContBlock.  Insert an entry
  // into the phi node for the edge with the value of RHSCond.
  if (CGF.getDebugInfo())
    // There is no need to emit line number for unconditional branch.
    Builder.SetCurrentDebugLocation(llvm::DebugLoc());
  CGF.EmitBlock(ContBlock);
  PN->addIncoming(RHSCond, RHSBlock);

  // ZExt result to int.
  return Builder.CreateZExtOrBitCast(PN, ResTy, "land.ext");
}

Value *ScalarExprEmitter::VisitBinLOr(const BinaryOperator *E) {
  llvm::Type *ResTy = ConvertType(E->getType());
  
  // If we have 1 || RHS, see if we can elide RHS, if so, just return 1.
  // If we have 0 || X, just emit X without inserting the control flow.
  bool LHSCondVal;
  if (CGF.ConstantFoldsToSimpleInteger(E->getLHS(), LHSCondVal)) {
    if (!LHSCondVal) { // If we have 0 || X, just emit X.
      Value *RHSCond = CGF.EvaluateExprAsBool(E->getRHS());
      // ZExt result to int or bool.
      return Builder.CreateZExtOrBitCast(RHSCond, ResTy, "lor.ext");
    }

    // 1 || RHS: If it is safe, just elide the RHS, and return 1/true.
    return llvm::ConstantInt::get(ResTy, 1);
  }

  llvm::BasicBlock *ContBlock = CGF.createBasicBlock("lor.end");
  llvm::BasicBlock *RHSBlock = CGF.createBasicBlock("lor.rhs");

  CodeGenFunction::ConditionalEvaluation eval(CGF);

  // Branch on the LHS first.  If it is true, go to the success (cont) block.
  CGF.EmitBranchOnBoolExpr(E->getLHS(), ContBlock, RHSBlock);

  // Any edges into the ContBlock are now from an (indeterminate number of)
  // edges from this first condition.  All of these values will be true.  Start
  // setting up the PHI node in the Cont Block for this.
  llvm::PHINode *PN = llvm::PHINode::Create(llvm::Type::getInt1Ty(VMContext), 2,
                                            "", ContBlock);
  for (llvm::pred_iterator PI = pred_begin(ContBlock), PE = pred_end(ContBlock);
       PI != PE; ++PI)
    PN->addIncoming(llvm::ConstantInt::getTrue(VMContext), *PI);

  eval.begin(CGF);

  // Emit the RHS condition as a bool value.
  CGF.EmitBlock(RHSBlock);
  Value *RHSCond = CGF.EvaluateExprAsBool(E->getRHS());

  eval.end(CGF);

  // Reaquire the RHS block, as there may be subblocks inserted.
  RHSBlock = Builder.GetInsertBlock();

  // Emit an unconditional branch from this block to ContBlock.  Insert an entry
  // into the phi node for the edge with the value of RHSCond.
  CGF.EmitBlock(ContBlock);
  PN->addIncoming(RHSCond, RHSBlock);

  // ZExt result to int.
  return Builder.CreateZExtOrBitCast(PN, ResTy, "lor.ext");
}

Value *ScalarExprEmitter::VisitBinComma(const BinaryOperator *E) {
  CGF.EmitIgnoredExpr(E->getLHS());
  CGF.EnsureInsertPoint();
  return Visit(E->getRHS());
}

//===----------------------------------------------------------------------===//
//                             Other Operators
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
//                         Entry Point into this File
//===----------------------------------------------------------------------===//

/// EmitScalarExpr - Emit the computation of the specified expression of scalar
/// type, ignoring the result.
Value *CodeGenFunction::EmitScalarExpr(const Expr *E, bool IgnoreResultAssign) {
  assert(E && !hasAggregateLLVMType(E->getType()) &&
         "Invalid scalar expression to emit");

  Value *V = ScalarExprEmitter(*this, IgnoreResultAssign)
    .Visit(const_cast<Expr*>(E));
  return V;
}

/// EmitScalarConversion - Emit a conversion from the specified type to the
/// specified destination type, both of which are LLVM scalar types.
Value *CodeGenFunction::EmitScalarConversion(Value *Src, Type SrcTy,
                                             Type DstTy) {
  assert(!hasAggregateLLVMType(SrcTy) && !hasAggregateLLVMType(DstTy) &&
         "Invalid scalar expression to emit");
  return ScalarExprEmitter(*this).EmitScalarConversion(Src, SrcTy, DstTy);
}

/// EmitComplexToScalarConversion - Emit a conversion from the specified complex
/// type to the specified destination type, where the destination type is an
/// LLVM scalar type.
Value *CodeGenFunction::EmitComplexToScalarConversion(ComplexPairTy Src,
                                                      Type SrcTy,
                                                      Type DstTy) {
  assert(SrcTy.hasComplexAttr() && !hasAggregateLLVMType(DstTy) &&
         "Invalid complex -> scalar conversion");
  return ScalarExprEmitter(*this).EmitComplexToScalarConversion(Src, SrcTy,
                                                                DstTy);
}


llvm::Value *CodeGenFunction::
EmitScalarPrePostIncDec(const UnaryOperator *E, LValue LV,
                        bool isInc, bool isPre) {
  return ScalarExprEmitter(*this).EmitScalarPrePostIncDec(E, LV, isInc, isPre);
}

LValue CodeGenFunction::EmitCompoundAssignmentLValue(
                                            const CompoundAssignOperator *E) {
  ScalarExprEmitter Scalar(*this);
  Value *Result = 0;
  switch (E->getOpcode()) {
#define COMPOUND_OP(Op)                                                       \
    case BO_##Op##Assign:                                                     \
      return Scalar.EmitCompoundAssignLValue(E, &ScalarExprEmitter::Emit##Op, \
                                             Result)
  COMPOUND_OP(Mul);
  COMPOUND_OP(RDiv);
  COMPOUND_OP(LDiv);
  COMPOUND_OP(Add);
  COMPOUND_OP(Sub);
  COMPOUND_OP(Shl);
  COMPOUND_OP(Shr);
  COMPOUND_OP(And);
  COMPOUND_OP(Or);
#undef COMPOUND_OP
      
  case BO_Mul:
  case BO_RDiv:
  case BO_LDiv:
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
  case BO_LAnd:
  case BO_LOr:
  case BO_Assign:
  case BO_Comma:
    assert(false && "Not valid compound assignment operators");
    break;
  }
   
  llvm_unreachable("Unhandled compound assignment operator");
}
