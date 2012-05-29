//===--- CGExpr.cpp - Emit LLVM Code from Expressions ---------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGCall.h"
#include "CGOOPABI.h"
#include "CGRecordLayout.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "llvm/Intrinsics.h"
#include "llvm/Target/TargetData.h"
using namespace mlang;
using namespace CodeGen;

//===--------------------------------------------------------------------===//
//                        Miscellaneous Helper Methods
//===--------------------------------------------------------------------===//

/// CreateTempAlloca - This creates a alloca and inserts it into the entry
/// block.
llvm::AllocaInst *CodeGenFunction::CreateTempAlloca(llvm::Type *Ty,
                                                    const llvm::Twine &Name) {
  if (!Builder.isNamePreserving())
    return new llvm::AllocaInst(Ty, 0, "", AllocaInsertPt);
  return new llvm::AllocaInst(Ty, 0, Name, AllocaInsertPt);
}

void CodeGenFunction::InitTempAlloca(llvm::AllocaInst *Var,
                                     llvm::Value *Init) {
  llvm::StoreInst *Store = new llvm::StoreInst(Init, Var);
  llvm::BasicBlock *Block = AllocaInsertPt->getParent();
  Block->getInstList().insertAfter(&*AllocaInsertPt, Store);
}

llvm::AllocaInst *CodeGenFunction::CreateIRTemp(Type Ty,
                                                const llvm::Twine &Name) {
	llvm::AllocaInst *Alloc = CreateTempAlloca(ConvertType(Ty), Name);

  // FIXME: Should we prefer the preferred type alignment here?
  CharUnits Align = Ty.isNull() ? CharUnits::fromQuantity(4) :
		  getContext().getTypeAlignInChars(Ty);
  Alloc->setAlignment(Align.getQuantity());
  return Alloc;
}

llvm::AllocaInst *CodeGenFunction::CreateMemTemp(Type Ty,
                                                 const llvm::Twine &Name) {
  llvm::AllocaInst *Alloc = CreateTempAlloca(ConvertTypeForMem(Ty), Name);
  // FIXME: Should we prefer the preferred type alignment here?
  CharUnits Align = getContext().getTypeAlignInChars(Ty);
  Alloc->setAlignment(Align.getQuantity());
  return Alloc;
}

/// EvaluateExprAsBool - Perform the usual unary conversions on the specified
/// expression and compare the result against zero, returning an Int1Ty value.
llvm::Value *CodeGenFunction::EvaluateExprAsBool(const Expr *E) {
  Type BoolTy = getContext().LogicalTy;
  if (!E->getType().hasComplexAttr())
    return EmitScalarConversion(EmitScalarExpr(E), E->getType(), BoolTy);

  return EmitComplexToScalarConversion(EmitComplexExpr(E), E->getType(),BoolTy);
}

/// EmitIgnoredExpr - Emit code to compute the specified expression,
/// ignoring the result.
void CodeGenFunction::EmitIgnoredExpr(const Expr *E) {
  if (E->isRValue())
    return (void) EmitAnyExpr(E, AggValueSlot::ignored(), true);

  // Just emit it as an l-value and drop the result.
  EmitLValue(E);
}

/// EmitAnyExpr - Emit code to compute the specified expression which
/// can have any type.  The result is returned as an RValue struct.
/// If this is an aggregate expression, AggSlot indicates where the
/// result should be returned.
RValue CodeGenFunction::EmitAnyExpr(const Expr *E, AggValueSlot AggSlot,
                                    bool IgnoreResult) {
  if (!hasAggregateLLVMType(E->getType()))
    return RValue::get(EmitScalarExpr(E, IgnoreResult));
  else if (E->getType().hasComplexAttr())
    return RValue::getComplex(EmitComplexExpr(E, IgnoreResult, IgnoreResult));

  EmitAggExpr(E, AggSlot, IgnoreResult);
  return AggSlot.asRValue();
}

/// EmitAnyExprToTemp - Similary to EmitAnyExpr(), however, the result will
/// always be accessible even if no aggregate location is provided.
RValue CodeGenFunction::EmitAnyExprToTemp(const Expr *E) {
  AggValueSlot AggSlot = AggValueSlot::ignored();

  if (hasAggregateLLVMType(E->getType()) &&
      !E->getType().hasComplexAttr())
    AggSlot = CreateAggTemp(E->getType(), "agg.tmp");
  return EmitAnyExpr(E, AggSlot);
}

/// EmitAnyExprToMem - Evaluate an expression into a given memory
/// location.
void CodeGenFunction::EmitAnyExprToMem(const Expr *E,
                                       llvm::Value *Location,
                                       bool IsLocationVolatile,
                                       bool IsInit) {
  if (E->getType().hasComplexAttr())
    EmitComplexExprIntoAddr(E, Location, IsLocationVolatile);
  else if (hasAggregateLLVMType(E->getType()))
    EmitAggExpr(E, AggValueSlot::forAddr(Location, IsLocationVolatile, IsInit));
  else {
    RValue RV = RValue::get(EmitScalarExpr(E, /*Ignore*/ false));
    LValue LV = MakeAddrLValue(Location, E->getType());
    EmitStoreThroughLValue(RV, LV);
  }
}

/// getAccessedFieldNo - Given an encoded value and a result number, return the
/// input field number being accessed.
unsigned CodeGenFunction::getAccessedFieldNo(unsigned Idx,
                                             const llvm::Constant *Elts) {
  if (isa<llvm::ConstantAggregateZero>(Elts))
    return 0;

  return cast<llvm::ConstantInt>(Elts->getOperand(Idx))->getZExtValue();
}

void CodeGenFunction::EmitCheck(llvm::Value *Address, unsigned Size) {
  if (!CatchUndefined)
    return;

  Address = Builder.CreateBitCast(Address, Int8Ty);

  llvm::Type *IntPtrT = IntPtrTy;
  llvm::Value *F = CGM.getIntrinsic(llvm::Intrinsic::objectsize, IntPtrT);
  llvm::IntegerType *Int1Ty = llvm::Type::getInt1Ty(getLLVMContext());

  // In time, people may want to control this and use a 1 here.
  llvm::Value *Arg = llvm::ConstantInt::get(Int1Ty, 0);
  llvm::Value *C = Builder.CreateCall2(F, Address, Arg);
  llvm::BasicBlock *Cont = createBasicBlock();
  llvm::BasicBlock *Check = createBasicBlock();
  llvm::Value *NegativeOne = llvm::ConstantInt::get(IntPtrTy, -1ULL);
  Builder.CreateCondBr(Builder.CreateICmpEQ(C, NegativeOne), Cont, Check);
    
  EmitBlock(Check);
  Builder.CreateCondBr(Builder.CreateICmpUGE(C,
                                        llvm::ConstantInt::get(IntPtrTy, Size)),
                       Cont, getTrapBB());
  EmitBlock(Cont);
}


CodeGenFunction::ComplexPairTy CodeGenFunction::
EmitComplexPrePostIncDec(const UnaryOperator *E, LValue LV,
                         bool isInc, bool isPre) {
  ComplexPairTy InVal = LoadComplexFromAddr(LV.getAddress(),
                                            LV.isVolatileQualified());
  
  llvm::Value *NextVal;
  if (isa<llvm::IntegerType>(InVal.first->getType())) {
    uint64_t AmountVal = isInc ? 1 : -1;
    NextVal = llvm::ConstantInt::get(InVal.first->getType(), AmountVal, true);
    
    // Add the inc/dec to the real part.
    NextVal = Builder.CreateAdd(InVal.first, NextVal, isInc ? "inc" : "dec");
  } else {
    Type ElemTy = E->getType();
    llvm::APFloat FVal(getContext().getFloatTypeSemantics(ElemTy), 1);
    if (!isInc)
      FVal.changeSign();
    NextVal = llvm::ConstantFP::get(getLLVMContext(), FVal);
    
    // Add the inc/dec to the real part.
    NextVal = Builder.CreateFAdd(InVal.first, NextVal, isInc ? "inc" : "dec");
  }
  
  ComplexPairTy IncVal(NextVal, InVal.second);
  
  // Store the updated result through the lvalue.
  StoreComplexToAddr(IncVal, LV.getAddress(), LV.isVolatileQualified());
  
  // If this is a postinc, return the value read from memory, otherwise use the
  // updated value.
  return isPre ? IncVal : InVal;
}


//===----------------------------------------------------------------------===//
//                         LValue Expression Emission
//===----------------------------------------------------------------------===//

RValue CodeGenFunction::GetUndefRValue(Type Ty) {
  // If this is a use of an undefined aggregate type, the aggregate must have an
  // identifiable address.  Just because the contents of the value are undefined
  // doesn't mean that the address can't be taken and compared.
  if (hasAggregateLLVMType(Ty)) {
    llvm::Value *DestPtr = CreateMemTemp(Ty, "undef.agg.tmp");
    return RValue::getAggregate(DestPtr);
  }
  
  return RValue::get(llvm::UndefValue::get(ConvertType(Ty)));
}

RValue CodeGenFunction::EmitUnsupportedRValue(const Expr *E,
                                              const char *Name) {
  ErrorUnsupported(E, Name);
  return GetUndefRValue(E->getType());
}

LValue CodeGenFunction::EmitUnsupportedLValue(const Expr *E,
                                              const char *Name) {
  ErrorUnsupported(E, Name);
  llvm::Type *Ty = llvm::PointerType::getUnqual(ConvertType(E->getType()));
  return MakeAddrLValue(llvm::UndefValue::get(Ty), E->getType());
}

LValue CodeGenFunction::EmitCheckedLValue(const Expr *E) {
  LValue LV = EmitLValue(E);
  if (!isa<DefnRefExpr>(E) && LV.isSimple())
    EmitCheck(LV.getAddress(), getContext().getTypeSize(E->getType()) / 8);
  return LV;
}

/// EmitLValue - Emit code to compute a designator that specifies the location
/// of the expression.
///
/// This can return one of two things: a simple address or a bitfield reference.
/// In either case, the LLVM Value* in the LValue structure is guaranteed to be
/// an LLVM pointer type.
///
/// If this returns a bitfield reference, nothing about the pointee type of the
/// LLVM value is known: For example, it may not be a pointer to an integer.
///
/// If this returns a normal address, and if the lvalue's C type is fixed size,
/// this method guarantees that the returned pointer type will point to an LLVM
/// type of the same size of the lvalue's type.  If the lvalue has a variable
/// length type, this is not possible.
///
LValue CodeGenFunction::EmitLValue(const Expr *E) {
	switch (E->getStmtClass()) {
	  default: return EmitUnsupportedLValue(E, "l-value expression");

	  case Expr::BinaryOperatorClass:
	    return EmitBinaryOperatorLValue(cast<BinaryOperator>(E));
	  case Expr::CompoundAssignOperatorClass:
	    if (!E->getType().hasComplexAttr())
	      return EmitCompoundAssignmentLValue(cast<CompoundAssignOperator>(E));
	    return EmitComplexCompoundAssignmentLValue(cast<CompoundAssignOperator>(E));
	  case Expr::FunctionCallClass:
	    return EmitFunctionCallLValue(cast<FunctionCall>(E));
	  case Expr::DefnRefExprClass:
	    return EmitDefnRefLValue(cast<DefnRefExpr>(E));
	  case Expr::ParenExprClass:return EmitLValue(cast<ParenExpr>(E)->getSubExpr());
	  case Expr::StringLiteralClass:
	    return EmitStringLiteralLValue(cast<StringLiteral>(E));
	  case Expr::UnaryOperatorClass:
	    return EmitUnaryOpLValue(cast<UnaryOperator>(E));
	  case Expr::ArrayIndexClass:
	    return EmitArrayIndexExpr(cast<ArrayIndex>(E));
	  case Expr::MemberExprClass:
	    return EmitMemberExpr(cast<MemberExpr>(E));
//    case Expr::OpaqueValueExprClass:
//	    return EmitOpaqueValueLValue(cast<OpaqueValueExpr>(E));
  }
}

llvm::Value *CodeGenFunction::EmitLoadOfScalar(llvm::Value *Addr, bool Volatile,
                                              unsigned Alignment, Type Ty,
                                              llvm::MDNode *TBAAInfo) {
  llvm::LoadInst *Load = Builder.CreateLoad(Addr, "tmp");
  if (Volatile)
    Load->setVolatile(true);
  if (Alignment)
    Load->setAlignment(Alignment);
  if (TBAAInfo)
    CGM.DecorateInstruction(Load, TBAAInfo);

  return EmitFromMemory(Load, Ty);
}

static bool isBooleanUnderlyingType(Type Ty) {
  return false;
}

llvm::Value *CodeGenFunction::EmitToMemory(llvm::Value *Value, Type Ty) {
  // Bool has a different representation in memory than in registers.
  if (Ty->isLogicalType() || isBooleanUnderlyingType(Ty)) {
    // This should really always be an i1, but sometimes it's already
    // an i8, and it's awkward to track those cases down.
    if (Value->getType()->isIntegerTy(1))
      return Builder.CreateZExt(Value, Builder.getInt8Ty(), "frombool");
    assert(Value->getType()->isIntegerTy(8) && "value rep of bool not i1/i8");
  }

  return Value;
}

llvm::Value *CodeGenFunction::EmitFromMemory(llvm::Value *Value, Type Ty) {
  // Bool has a different representation in memory than in registers.
  if (Ty->isLogicalType() || isBooleanUnderlyingType(Ty)) {
    assert(Value->getType()->isIntegerTy(8) && "memory rep of bool not i8");
    return Builder.CreateTrunc(Value, Builder.getInt1Ty(), "tobool");
  }

  return Value;
}

void CodeGenFunction::EmitStoreOfScalar(llvm::Value *Value, llvm::Value *Addr,
                                        bool Volatile, unsigned Alignment,
                                        Type Ty,
                                        llvm::MDNode *TBAAInfo) {
  Value = EmitToMemory(Value, Ty);
  llvm::StoreInst *Store = Builder.CreateStore(Value, Addr, Volatile);
  if (Alignment)
    Store->setAlignment(Alignment);
  if (TBAAInfo)
    CGM.DecorateInstruction(Store, TBAAInfo);
}

void CodeGenFunction::EmitStoreOfScalar(llvm::Value *value, LValue lvalue) {
  EmitStoreOfScalar(value, lvalue.getAddress(), /*lvalue.isVolatile()*/false,
                    lvalue.getAlignment(), lvalue.getType(),
                    lvalue.getTBAAInfo());
}

/// EmitLoadOfLValue - Given an expression that represents a value lvalue, this
/// method emits the address of the lvalue, then loads the result as an rvalue,
/// returning the rvalue.
RValue CodeGenFunction::EmitLoadOfLValue(LValue LV, Type ExprType) {
  if (LV.isSimple()) {
    llvm::Value *Ptr = LV.getAddress();

    // Functions are l-values that don't require loading.
    if (ExprType->isFunctionType())
      return RValue::get(Ptr);

    // Everything needs a load.
    return RValue::get(EmitLoadOfScalar(Ptr,
    		                                /*LV.isVolatileQualified()*/false,
                                        LV.getAlignment(), ExprType,
                                        LV.getTBAAInfo()));

  }

  if (LV.isVectorElt()) {
    llvm::Value *Vec = Builder.CreateLoad(LV.getVectorAddr(),
    		                                  /*LV.isVolatileQualified()*/false,
    		                                  "tmp");
    return RValue::get(Builder.CreateExtractElement(Vec, LV.getVectorIdx(),
                                                    "vecext"));
  }

  // If this is a reference to a subset of the elements of a vector, either
  // shuffle the input or extract/insert them as appropriate.
  if (LV.isExtVectorElt())
    return EmitLoadOfExtVectorElementLValue(LV, ExprType);

  assert(LV.isPropertyRef() && "Unknown LValue type!");
  //FIXME huyabin
  //return EmitLoadOfPropertyRefLValue(LV);
  return RValue();
}

// If this is a reference to a subset of the elements of a vector, create an
// appropriate shufflevector.
RValue CodeGenFunction::EmitLoadOfExtVectorElementLValue(LValue LV,
                                                         Type ExprType) {
  llvm::Value *Vec = Builder.CreateLoad(LV.getExtVectorAddr(),
                                        LV.isVolatileQualified(), "tmp");

  const llvm::Constant *Elts = LV.getExtVectorElts();

  // If the result of the expression is a non-vector type, we must be extracting
  // a single element.  Just codegen as an extractelement.
  const VectorType *ExprVT = ExprType->getAs<VectorType>();
  if (!ExprVT) {
    unsigned InIdx = getAccessedFieldNo(0, Elts);
    llvm::Value *Elt = llvm::ConstantInt::get(Int32Ty, InIdx);
    return RValue::get(Builder.CreateExtractElement(Vec, Elt, "tmp"));
  }

  // Always use shuffle vector to try to retain the original program structure
  unsigned NumResultElts = ExprVT->getNumElements();

  llvm::SmallVector<llvm::Constant*, 4> Mask;
  for (unsigned i = 0; i != NumResultElts; ++i) {
    unsigned InIdx = getAccessedFieldNo(i, Elts);
    Mask.push_back(llvm::ConstantInt::get(Int32Ty, InIdx));
  }

  llvm::Value *MaskV = llvm::ConstantVector::get(Mask);
  Vec = Builder.CreateShuffleVector(Vec,
                                    llvm::UndefValue::get(Vec->getType()),
                                    MaskV, "tmp");
  return RValue::get(Vec);
}



/// EmitStoreThroughLValue - Store the specified rvalue into the specified
/// lvalue, where both are guaranteed to the have the same type, and that type
/// is 'Ty'.
void CodeGenFunction::EmitStoreThroughLValue(RValue Src, LValue Dst) {
  if (!Dst.isSimple()) {
    if (Dst.isVectorElt()) {
      // Read/modify/write the vector, inserting the new element.
      llvm::Value *Vec = Builder.CreateLoad(Dst.getVectorAddr(),
                                            Dst.isVolatileQualified(), "tmp");
      Vec = Builder.CreateInsertElement(Vec, Src.getScalarVal(),
                                        Dst.getVectorIdx(), "vecins");
      Builder.CreateStore(Vec, Dst.getVectorAddr(),Dst.isVolatileQualified());
      return;
    }

    // If this is an update of extended vector elements, insert them as
    // appropriate.
    if (Dst.isExtVectorElt())
      return EmitStoreThroughExtVectorComponentLValue(Src, Dst);

    assert(Dst.isPropertyRef() && "Unknown LValue type");
//    return EmitStoreThroughPropertyRefLValue(Src, Dst);
  }

  assert(Src.isScalar() && "Can't emit an agg store with this method");
  EmitStoreOfScalar(Src.getScalarVal(), Dst);
}

void CodeGenFunction::EmitStoreThroughExtVectorComponentLValue(RValue Src,
                                                               LValue Dst) {
  // This access turns into a read/modify/write of the vector.  Load the input
  // value now.
  llvm::Value *Vec = Builder.CreateLoad(Dst.getExtVectorAddr(),
                                        Dst.isVolatileQualified(), "tmp");
  const llvm::Constant *Elts = Dst.getExtVectorElts();

  llvm::Value *SrcVal = Src.getScalarVal();

  if (const VectorType *VTy = Dst.getType()->getAs<VectorType>()) {
    unsigned NumSrcElts = VTy->getNumElements();
    unsigned NumDstElts =
       cast<llvm::VectorType>(Vec->getType())->getNumElements();
    if (NumDstElts == NumSrcElts) {
      // Use shuffle vector is the src and destination are the same number of
      // elements and restore the vector mask since it is on the side it will be
      // stored.
      llvm::SmallVector<llvm::Constant*, 4> Mask(NumDstElts);
      for (unsigned i = 0; i != NumSrcElts; ++i) {
        unsigned InIdx = getAccessedFieldNo(i, Elts);
        Mask[InIdx] = llvm::ConstantInt::get(Int32Ty, i);
      }

      llvm::Value *MaskV = llvm::ConstantVector::get(Mask);
      Vec = Builder.CreateShuffleVector(SrcVal,
                                        llvm::UndefValue::get(Vec->getType()),
                                        MaskV, "tmp");
    } else if (NumDstElts > NumSrcElts) {
      // Extended the source vector to the same length and then shuffle it
      // into the destination.
      // FIXME: since we're shuffling with undef, can we just use the indices
      //        into that?  This could be simpler.
      llvm::SmallVector<llvm::Constant*, 4> ExtMask;
      unsigned i;
      for (i = 0; i != NumSrcElts; ++i)
        ExtMask.push_back(llvm::ConstantInt::get(Int32Ty, i));
      for (; i != NumDstElts; ++i)
        ExtMask.push_back(llvm::UndefValue::get(Int32Ty));
      llvm::Value *ExtMaskV = llvm::ConstantVector::get(ExtMask);
      llvm::Value *ExtSrcVal =
        Builder.CreateShuffleVector(SrcVal,
                                    llvm::UndefValue::get(SrcVal->getType()),
                                    ExtMaskV, "tmp");
      // build identity
      llvm::SmallVector<llvm::Constant*, 4> Mask;
      for (unsigned i = 0; i != NumDstElts; ++i)
        Mask.push_back(llvm::ConstantInt::get(Int32Ty, i));

      // modify when what gets shuffled in
      for (unsigned i = 0; i != NumSrcElts; ++i) {
        unsigned Idx = getAccessedFieldNo(i, Elts);
        Mask[Idx] = llvm::ConstantInt::get(Int32Ty, i+NumDstElts);
      }
      llvm::Value *MaskV = llvm::ConstantVector::get(Mask);
      Vec = Builder.CreateShuffleVector(Vec, ExtSrcVal, MaskV, "tmp");
    } else {
      // We should never shorten the vector
      assert(0 && "unexpected shorten vector length");
    }
  } else {
    // If the Src is a scalar (not a vector) it must be updating one element.
    unsigned InIdx = getAccessedFieldNo(0, Elts);
    llvm::Value *Elt = llvm::ConstantInt::get(Int32Ty, InIdx);
    Vec = Builder.CreateInsertElement(Vec, SrcVal, Elt, "tmp");
  }

  Builder.CreateStore(Vec, Dst.getExtVectorAddr(), Dst.isVolatileQualified());
}

static LValue EmitGlobalVarDefnLValue(CodeGenFunction &CGF,
                                      const Expr *E, const VarDefn *VD) {
//  assert((VD->hasExternalStorage() || VD->isFileVarDefn()) &&
//         "Var decl must have external storage or be a file var decl!");

  llvm::Value *V = CGF.CGM.GetAddrOfGlobalVar(VD);
  if (VD->getType()->isReferenceType())
    V = CGF.Builder.CreateLoad(V, "tmp");
  unsigned Alignment = CGF.getContext().getDefnAlign(VD).getQuantity();
  LValue LV = CGF.MakeAddrLValue(V, E->getType(), Alignment);
  return LV;
}

static LValue EmitFunctionDefnLValue(CodeGenFunction &CGF,
                                      const Expr *E, const FunctionDefn *FD) {
  llvm::Value *V = CGF.CGM.GetAddrOfFunction(FD);

  unsigned Alignment = CGF.getContext().getDefnAlign(FD).getQuantity();
  return CGF.MakeAddrLValue(V, E->getType(), Alignment);
}

LValue CodeGenFunction::EmitDefnRefLValue(const DefnRefExpr *E) {
  const NamedDefn *ND = E->getDefn();
  unsigned Alignment = getContext().getDefnAlign(ND).getQuantity();

  if (const VarDefn *VD = dyn_cast<VarDefn>(ND)) {

    // Check if this is a global variable.
//    if (VD->hasExternalStorage() || VD->isFileVarDefn())
//      return EmitGlobalVarDefnLValue(*this, E, VD);
//
//    bool NonGCable = VD->hasLocalStorage() &&
//                     !VD->getType()->isReferenceType() &&
//                     !VD->hasAttr<BlocksAttr>();

    llvm::Value *V = LocalDefnMap[VD];
    // if this DefnRefExpr not entered in LocalDefnMap, we create and add it
    if(!V) {
    	unsigned align = getContext().getDefnAlign(VD).getQuantity();

    	llvm::AllocaInst *alloca =
    	      CreateMemTemp(VD->getType(), VD->getNameAsString());
    	alloca->setAlignment(align);
    	LocalDefnMap[VD] = alloca;
    	V = alloca;
    }

//    if (VD->hasAttr<BlocksAttr>()) {
//      V = Builder.CreateStructGEP(V, 1, "forwarding");
//      V = Builder.CreateLoad(V);
//      V = Builder.CreateStructGEP(V, getByRefValueLLVMField(VD),
//                                  VD->getNameAsString());
//    }
    if (VD->getType()->isReferenceType())
      V = Builder.CreateLoad(V, "tmp");

    LValue LV = MakeAddrLValue(V, E->getType(), Alignment);
//    if (NonGCable) {
//      LV.setNonGC(true);
//    }
    return LV;
  }
  
  // If we're emitting an instance method as an independent lvalue,
  // we're actually emitting a member pointer.
  if (const ClassMethodDefn *MD = dyn_cast<ClassMethodDefn>(ND))
    if (MD->isInstance()) {
      llvm::Value *V = CGM.getOOPABI().EmitMemberPointer(MD);
      return MakeAddrLValue(V, MD->getType(), Alignment);
    }
  if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(ND))
    return EmitFunctionDefnLValue(*this, E, FD);
  
  // If we're emitting a field as an independent lvalue, we're
  // actually emitting a member pointer.
  if (const MemberDefn *FD = dyn_cast<MemberDefn>(ND)) {
    llvm::Value *V = CGM.getOOPABI().EmitMemberPointer(FD);
    return MakeAddrLValue(V, FD->getType(), Alignment);
  }
  
  assert(false && "Unhandled DefnRefExpr");
  
  // an invalid LValue, but the assert will
  // ensure that this point is never reached.
  return LValue();
}

LValue CodeGenFunction::EmitBlockDefnRefLValue(const DefnRefExpr *E) {
  unsigned Alignment =
    getContext().getDefnAlign(E->getDefn()).getQuantity();
  return MakeAddrLValue(GetAddrOfBlockDefn(E), E->getType(), Alignment);
}

LValue CodeGenFunction::EmitUnaryOpLValue(const UnaryOperator *E) {
  Type ExprTy = E->getSubExpr()->getType();
  switch (E->getOpcode()) {
  default: assert(0 && "Unknown unary operator lvalue!");
  case UO_PreInc:
  case UO_PreDec: {
    LValue LV = EmitLValue(E->getSubExpr());
    bool isInc = E->getOpcode() == UO_PreInc;
    
    if (E->getType().hasComplexAttr())
      EmitComplexPrePostIncDec(E, LV, isInc, true/*isPre*/);
    else
      EmitScalarPrePostIncDec(E, LV, isInc, true/*isPre*/);
    return LV;
  }
  }
}

LValue CodeGenFunction::EmitStringLiteralLValue(const StringLiteral *E) {
  return MakeAddrLValue(CGM.GetAddrOfConstantStringFromLiteral(E),
                        E->getType());
}

llvm::BasicBlock *CodeGenFunction::getTrapBB() {
  const CodeGenOptions &GCO = CGM.getCodeGenOpts();

  // If we are not optimzing, don't collapse all calls to trap in the function
  // to the same call, that way, in the debugger they can see which operation
  // did in fact fail.  If we are optimizing, we collapse all calls to trap down
  // to just one per function to save on codesize.
  if (GCO.OptimizationLevel && TrapBB)
    return TrapBB;

  llvm::BasicBlock *Cont = 0;
  if (HaveInsertPoint()) {
    Cont = createBasicBlock("cont");
    EmitBranch(Cont);
  }
  TrapBB = createBasicBlock("trap");
  EmitBlock(TrapBB);

  llvm::Value *F = CGM.getIntrinsic(llvm::Intrinsic::trap);
  llvm::CallInst *TrapCall = Builder.CreateCall(F);
  TrapCall->setDoesNotReturn();
  TrapCall->setDoesNotThrow();
  Builder.CreateUnreachable();

  if (Cont)
    EmitBlock(Cont);
  return TrapBB;
}

/// isSimpleArrayDecayOperand - If the specified expr is a simple decay from an
/// array to pointer, return the array subexpression.
static const Expr *isSimpleArrayDecayOperand(const Expr *E) {
  // If this isn't just an array->pointer decay, bail out.
//  const CastExpr *CE = dyn_cast<CastExpr>(E);
//  if (CE == 0 || CE->getCastKind() != CK_ArrayToPointerDecay)
//    return 0;
//
//  // If this is a decay from variable width array, bail out.
//  const Expr *SubExpr = CE->getSubExpr();
//  if (SubExpr->getType()->isVariableArrayType())
//    return 0;
//
//  return SubExpr;
	return E;
}

LValue CodeGenFunction::EmitArrayIndexExpr(const ArrayIndex *E) {
  // The index must always be an integer, which is not an aggregate.  Emit it.
  llvm::Value *Idx = EmitScalarExpr(E->getIdx(0));
  Type IdxTy  = E->getIdx(0)->getType();
  bool IdxSigned = IdxTy->isSignedIntegerType();

  // If the base is a vector type, then we are forming a vector element lvalue
  // with this subscript.
  if (E->getBase()->getType()->isVectorType()) {
    // Emit the vector as an lvalue to get its address.
    LValue LHS = EmitLValue(E->getBase());
    assert(LHS.isSimple() && "Can only subscript lvalue vectors here!");
    Idx = Builder.CreateIntCast(Idx, Int32Ty, IdxSigned, "vidx");
    return LValue::MakeVectorElt(LHS.getAddress(), Idx,
                                 E->getBase()->getType());
  }

  // Extend or truncate the index type to 32 or 64-bits.
  if (Idx->getType() != IntPtrTy)
    Idx = Builder.CreateIntCast(Idx, IntPtrTy,
                                IdxSigned, "idxprom");

  // FIXME: As llvm implements the object size checking, this can come out.
  if (CatchUndefined) {
		if (const DefnRefExpr *DRE = dyn_cast<DefnRefExpr>(E->getBase())) {
//			if (const ConstantArrayType *CAT
//              = getContext().getAsConstantArrayType(DRE->getType())) {
//				llvm::APInt Size = CAT->getSize();
//				llvm::BasicBlock *Cont = createBasicBlock("cont");
//				Builder.CreateCondBr(Builder.CreateICmpULE(Idx,
//						llvm::ConstantInt::get(Idx->getType(), Size)), Cont,
//						getTrapBB());
//				EmitBlock(Cont);
//			}
		}
	}

  // We know that the pointer points to a type of the correct size, unless the
  // size is a VLA or Objective-C interface.
  llvm::Value *Address = 0;
  /*if (const VariableArrayType *VAT =
        getContext().getAsVariableArrayType(E->getType())) {
    llvm::Value *VLASize = GetVLASize(VAT);

    Idx = Builder.CreateMul(Idx, VLASize);

    llvm::Type *i8PTy = llvm::Type::getInt8PtrTy(VMContext);

    // The base must be a pointer, which is not an aggregate.  Emit it.
    llvm::Value *Base = EmitScalarExpr(E->getBase());

    Address = Builder.CreateInBoundsGEP(Builder.CreateBitCast(Base, i8PTy),
                                        Idx, "arrayidx");
    Address = Builder.CreateBitCast(Address, Base->getType());
  } else */if (const Expr *Array = isSimpleArrayDecayOperand(E->getBase())) {
    // If this is A[i] where A is an array, the frontend will have decayed the
    // base to be a ArrayToPointerDecay implicit cast.  While correct, it is
    // inefficient at -O0 to emit a "gep A, 0, 0" when codegen'ing it, then a
    // "gep x, i" here.  Emit one "gep A, 0, i".
    assert(Array->getType()->isArrayType() &&
           "Array to pointer decay must have array source type!");
    llvm::Value *ArrayPtr = EmitLValue(Array).getAddress();
    llvm::Value *Zero = llvm::ConstantInt::get(Int32Ty, 0);
    llvm::Value *Args[] = { Zero, Idx };

  	Address = Builder.CreateInBoundsGEP(ArrayPtr, Args, "arrayidx");
  } else {
    // The base must be a pointer, which is not an aggregate.  Emit it.
    llvm::Value *Base = EmitScalarExpr(E->getBase());
    Address = Builder.CreateInBoundsGEP(Base, Idx, "arrayidx");
  }

  Type T = E->getBase()->getType()/*->getPointeeType()*/;
  assert(!T.isNull() &&
         "CodeGenFunction::EmitArraySubscriptExpr(): Illegal base type");

  LValue LV = MakeAddrLValue(Address, T);
  LV.getQuals().setArraySize(E->getBase()->getType().getArraySize());

  return LV;
}

static
llvm::Constant *GenerateConstantVector(llvm::LLVMContext &VMContext,
                                       llvm::SmallVector<unsigned, 4> &Elts) {
  llvm::SmallVector<llvm::Constant*, 4> CElts;

  llvm::Type *Int32Ty = llvm::Type::getInt32Ty(VMContext);
  for (unsigned i = 0, e = Elts.size(); i != e; ++i)
    CElts.push_back(llvm::ConstantInt::get(Int32Ty, Elts[i]));

  return llvm::ConstantVector::get(CElts);
}

LValue CodeGenFunction::EmitMemberExpr(const MemberExpr *E) {
  bool isNonGC = false;
  Expr *BaseExpr = E->getBase();
  llvm::Value *BaseValue = NULL;
  TypeInfo BaseQuals;

  // If this is s.x, emit s as an lvalue.  If it is s->x, emit s as a scalar.
  LValue BaseLV = EmitLValue(BaseExpr);
  if (BaseLV.isNonGC())
	  isNonGC = true;
  // FIXME: this isn't right for bitfields.
  BaseValue = BaseLV.getAddress();
  Type BaseTy = BaseExpr->getType();
  BaseQuals = BaseTy.getTypeInfo();

  NamedDefn *ND = E->getMemberDefn();
  if (MemberDefn *Field = dyn_cast<MemberDefn>(ND)) {
    LValue LV = EmitLValueForField(BaseValue, Field, 
                                   BaseQuals.getFastTypeInfo());
    LV.setNonGC(isNonGC);
    return LV;
  }
  
  if (VarDefn *VD = dyn_cast<VarDefn>(ND))
    return EmitGlobalVarDefnLValue(*this, E, VD);

  if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(ND))
    return EmitFunctionDefnLValue(*this, E, FD);

  assert(false && "Unhandled member declaration!");
  return LValue();
}

LValue CodeGenFunction::EmitLValueForField(llvm::Value *BaseValue,
                                           const MemberDefn *Field,
                                           unsigned CVRQualifiers) {
  const CGRecordLayout &RL =
    CGM.getTypes().getCGRecordLayout(Field->getParent());
  unsigned idx = RL.getLLVMFieldNo(Field);
  llvm::Value *V = Builder.CreateStructGEP(BaseValue, idx, "tmp");

  if (Field->getType()->isReferenceType())
    V = Builder.CreateLoad(V, "tmp");

  unsigned Alignment = getContext().getDefnAlign(Field).getQuantity();
  LValue LV = MakeAddrLValue(V, Field->getType(), Alignment);
  LV.getQuals().addAttrs(CVRQualifiers);

  return LV;
}

LValue 
CodeGenFunction::EmitLValueForFieldInitialization(llvm::Value *BaseValue, 
                                                  const MemberDefn *Field,
                                                  unsigned CVRQualifiers) {
  Type FieldType = Field->getType();
  
  if (!FieldType->isReferenceType())
    return EmitLValueForField(BaseValue, Field, CVRQualifiers);

  const CGRecordLayout &RL =
    CGM.getTypes().getCGRecordLayout(Field->getParent());
  unsigned idx = RL.getLLVMFieldNo(Field);
  llvm::Value *V = Builder.CreateStructGEP(BaseValue, idx, "tmp");

  unsigned Alignment = getContext().getDefnAlign(Field).getQuantity();
  return MakeAddrLValue(V, FieldType, Alignment);
}

LValue CodeGenFunction::EmitImaginaryLiteralLValue(const ImaginaryLiteral *E){
  llvm::Value *DefnPtr = CreateMemTemp(E->getType(), ".compoundliteral");
  const Expr *InitExpr = E->getSubExpr();
  LValue Result = MakeAddrLValue(DefnPtr, E->getType());

  EmitAnyExprToMem(InitExpr, DefnPtr, /*Volatile*/ false, /*Init*/ true);

  return Result;
}

//===--------------------------------------------------------------------===//
//                             Expression Emission
//===--------------------------------------------------------------------===//
//FIXME should be in CGBuiltin.cpp
RValue CodeGenFunction::EmitBuiltinExpr(const FunctionDefn *FD,
                                        unsigned BuiltinID, const FunctionCall *E) {
	return RValue();
}

RValue CodeGenFunction::EmitFunctionCall(const FunctionCall *E,
                                     ReturnValueSlot ReturnValue) {
  // Builtins never have block type.
//  if (E->getCallee()->getType()->isBlockPointerType())
//    return EmitBlockCallExpr(E, ReturnValue);
//
//  if (const CXXMemberCallExpr *CE = dyn_cast<CXXMemberCallExpr>(E))
//    return EmitClassMemberCallExpr(CE, ReturnValue);

  const Defn *TargetDefn = 0;
  if (const DefnRefExpr *DRE = dyn_cast<DefnRefExpr>(E->getCallee())) {
      TargetDefn = DRE->getDefn();
      if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(TargetDefn))
        if (unsigned builtinID = FD->getBuiltinID())
          return EmitBuiltinExpr(FD, builtinID, E);
    }

//  if (const CXXOperatorCallExpr *CE = dyn_cast<CXXOperatorCallExpr>(E))
//    if (const ClassMethodDefn *MD = dyn_cast_or_null<ClassMethodDefn>(TargetDefn))
//      return EmitCXXOperatorMemberCallExpr(CE, MD, ReturnValue);
//
//  if (isa<CXXPseudoDestructorExpr>(E->getCallee()->IgnoreParens())) {
//    // C++ [expr.pseudo]p1:
//    //   The result shall only be used as the operand for the function call
//    //   operator (), and the result of such a call has type void. The only
//    //   effect is the evaluation of the postfix-expression before the dot or
//    //   arrow.
//    EmitScalarExpr(E->getCallee());
//    return RValue::get(0);
//  }

  llvm::Value *Callee = EmitScalarExpr(E->getCallee());
  return EmitCall(E->getCallee()->getType(), Callee, ReturnValue,
                  E->arg_begin(), E->arg_end(), TargetDefn);
}

LValue CodeGenFunction::EmitBinaryOperatorLValue(const BinaryOperator *E) {
  // Comma expressions just emit their LHS then their RHS as an l-value.
  if (E->getOpcode() == BO_Comma) {
    EmitIgnoredExpr(E->getLHS());
    EnsureInsertPoint();
    return EmitLValue(E->getRHS());
  }

//  if (E->getOpcode() == BO_PtrMemD ||
//      E->getOpcode() == BO_PtrMemI)
//    return EmitPointerToDataMemberBinaryExpr(E);

  assert(E->getOpcode() == BO_Assign && "unexpected binary l-value");
  
  if (!hasAggregateLLVMType(E->getType())) {
    // __block variables need the RHS evaluated first.
    RValue RV = EmitAnyExpr(E->getRHS());
    LValue LV = EmitLValue(E->getLHS());
    EmitStoreThroughLValue(RV, LV);
    return LV;
  }

  if (E->getType().hasComplexAttr())
    return EmitComplexAssignmentLValue(E);

  return EmitAggExprToLValue(E);
}

LValue CodeGenFunction::EmitFunctionCallLValue(const FunctionCall *E) {
  RValue RV = EmitFunctionCall(E);

  if (!RV.isScalar())
    return MakeAddrLValue(RV.getAggregateAddr(), E->getType());
    
  assert(E->getCallReturnType()->isReferenceType() &&
         "Can't have a scalar return unless the return type is a "
         "reference type!");

  return MakeAddrLValue(RV.getScalarVal(), E->getType());
}

RValue CodeGenFunction::EmitCall(Type CalleeType, llvm::Value *Callee,
                                 ReturnValueSlot ReturnValue,
                                 FunctionCall::const_arg_iterator ArgBeg,
                                 FunctionCall::const_arg_iterator ArgEnd,
                                 const Defn *TargetDefn) {
  // Get the actual function type. The callee type will always be a pointer to
  // function type or a block pointer type.
  assert(CalleeType->isFunctionHandleType() &&
         "Call must have function pointer type!");

  const FunctionType *FnType
    = cast<FunctionType>(cast<FunctionHandleType>(CalleeType)->getPointeeType());
  Type ResTy = FnType->getResultType();

  CallArgList Args;
  EmitCallArgs(Args, dyn_cast<FunctionProtoType>(FnType), ArgBeg, ArgEnd);

  return EmitCall(CGM.getTypes().getFunctionInfo(Args, FnType),
                  Callee, ReturnValue, Args, TargetDefn);
}
