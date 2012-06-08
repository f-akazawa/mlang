//===----- CGCall.h - Encapsulate calling convention details ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These classes wrap the information about a call or function
// definition used to handle ABI compliancy.
//
//===----------------------------------------------------------------------===//

#include "CGCall.h"
#include "CGOOPABI.h"
#include "ABIInfo.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "llvm/Attributes.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Target/TargetData.h"
#include "llvm/InlineAsm.h"
#include "llvm/Transforms/Utils/Local.h"
using namespace mlang;
using namespace CodeGen;

/***/

static unsigned ClangCallConvToLLVMCallConv(CallingConv CC) {
  switch (CC) {
  default: return llvm::CallingConv::C;
  case CC_X86StdCall: return llvm::CallingConv::X86_StdCall;
  case CC_X86FastCall: return llvm::CallingConv::X86_FastCall;
  case CC_X86ThisCall: return llvm::CallingConv::X86_ThisCall;
  // TODO: add support for CC_X86Pascal to llvm
  }
}

/// Returns the canonical formal type of the given C++ method.
static const FunctionProtoType *GetFormalType(const ClassMethodDefn *MD) {
  return MD->getType()->getAs<FunctionProtoType>();
}

/// Returns the "extra-canonicalized" return type, which discards
/// qualifiers on the return type.  Codegen doesn't care about them,
/// and it makes ABI code a little easier to be able to assume that
/// all parameter and return types are top-level unqualified.
static Type GetReturnType(Type RetTy) {
  return RetTy;
}

const CGFunctionInfo &
CodeGenTypes::getFunctionInfo(const FunctionNoProtoType *FTNP,
                              bool IsRecursive) {
  return getFunctionInfo(FTNP->getResultType(),
                         llvm::SmallVector<Type, 16>(),
                         FTNP->getExtInfo(), IsRecursive);
}

/// \param Args - contains any initial parameters besides those
///   in the formal type
static const CGFunctionInfo &getFunctionInfo(CodeGenTypes &CGT,
                                  llvm::SmallVectorImpl<Type> &ArgTys,
                                             const FunctionProtoType *FTP,
                                             bool IsRecursive = false) {
  // FIXME: Kill copy.
  for (unsigned i = 0, e = FTP->getNumArgs(); i != e; ++i)
    ArgTys.push_back(FTP->getArgType(i));
  Type ResTy = FTP->getResultType();
  return CGT.getFunctionInfo(ResTy, ArgTys, FTP->getExtInfo(), IsRecursive);
}

const CGFunctionInfo &
CodeGenTypes::getFunctionInfo(const FunctionProtoType *FTP,
                              bool IsRecursive) {
  llvm::SmallVector<Type, 16> ArgTys;
  return ::getFunctionInfo(*this, ArgTys, FTP, IsRecursive);
}

static CallingConv getCallingConventionForDecl(const Defn *D) {
  // Set the appropriate calling convention for the Function.
  return CC_C;
}

const CGFunctionInfo &CodeGenTypes::getFunctionInfo(const UserClassDefn *RD,
                                                 const FunctionProtoType *FTP) {
  llvm::SmallVector<Type, 16> ArgTys;

  return ::getFunctionInfo(*this, ArgTys, FTP);
}

const CGFunctionInfo &CodeGenTypes::getFunctionInfo(const ClassMethodDefn *MD) {
  llvm::SmallVector<Type, 16> ArgTys;

  assert(!isa<ClassConstructorDefn>(MD) && "wrong method for contructors!");
  assert(!isa<ClassDestructorDefn>(MD) && "wrong method for destructors!");

  return ::getFunctionInfo(*this, ArgTys, GetFormalType(MD));
}

const CGFunctionInfo &CodeGenTypes::getFunctionInfo(const ClassConstructorDefn *D,
                                                    ClassCtorType Ty) {
  llvm::SmallVector<Type, 16> ArgTys;
  Type ResTy = Context.Int32Ty;

  TheOOPABI.BuildConstructorSignature(D, Ty, ResTy, ArgTys);

  const FunctionProtoType *FTP = GetFormalType(D);

  // Add the formal parameters.
  for (unsigned i = 0, e = FTP->getNumArgs(); i != e; ++i)
    ArgTys.push_back(FTP->getArgType(i));

  return getFunctionInfo(ResTy, ArgTys, FTP->getExtInfo());
}

const CGFunctionInfo &CodeGenTypes::getFunctionInfo(const ClassDestructorDefn *D,
                                                    ClassDtorType Ty) {
  llvm::SmallVector<Type, 2> ArgTys;
  Type ResTy = Context.Int32Ty;

  TheOOPABI.BuildDestructorSignature(D, Ty, ResTy, ArgTys);

  const FunctionProtoType *FTP = GetFormalType(D);
  assert(FTP->getNumArgs() == 0 && "dtor with formal parameters");

  return getFunctionInfo(ResTy, ArgTys, FTP->getExtInfo());
}

const CGFunctionInfo &CodeGenTypes::getFunctionInfo(const FunctionDefn *FD) {
  if (const ClassMethodDefn *MD = dyn_cast<ClassMethodDefn>(FD))
    if (MD->isInstance())
      return getFunctionInfo(MD);

  Type FTy = FD->getType();
  assert(isa<FunctionType>(FTy));
  if (isa<FunctionNoProtoType>(FTy))
    return getFunctionInfo(FTy->getAs<FunctionNoProtoType>());
  assert(isa<FunctionProtoType>(FTy));
  return getFunctionInfo(FTy->getAs<FunctionProtoType>());
}

const CGFunctionInfo &CodeGenTypes::getFunctionInfo(GlobalDefn GD) {
  // FIXME: Do we need to handle ObjCMethodDecl?
  const FunctionDefn *FD = cast<FunctionDefn>(GD.getDefn());

  if (const ClassConstructorDefn *CD = dyn_cast<ClassConstructorDefn>(FD))
    return getFunctionInfo(CD, GD.getCtorType());

  if (const ClassDestructorDefn *DD = dyn_cast<ClassDestructorDefn>(FD))
    return getFunctionInfo(DD, GD.getDtorType());

  return getFunctionInfo(FD);
}

const CGFunctionInfo &CodeGenTypes::getFunctionInfo(Type ResTy,
                                                    const CallArgList &Args,
                                            const FunctionType::ExtInfo &Info) {
  // FIXME: Kill copy.
  llvm::SmallVector<Type, 16> ArgTys;
  for (CallArgList::const_iterator i = Args.begin(), e = Args.end();
       i != e; ++i)
    ArgTys.push_back(i->Ty);
  return getFunctionInfo(GetReturnType(ResTy), ArgTys, Info);
}

const CGFunctionInfo &CodeGenTypes::getFunctionInfo(Type ResTy,
                                                    const FunctionArgList &Args,
                                            const FunctionType::ExtInfo &Info) {
  // FIXME: Kill copy.
  llvm::SmallVector<Type, 16> ArgTys;
  for (FunctionArgList::const_iterator i = Args.begin(), e = Args.end();
       i != e; ++i)
    ArgTys.push_back((*i)->getType());
  return getFunctionInfo(GetReturnType(ResTy), ArgTys, Info);
}

//const CGFunctionInfo &CodeGenTypes::getNullaryFunctionInfo() {
//  llvm::SmallVector<Type, 1> args;
//  return getFunctionInfo(getContext().VoidTy, args, FunctionType::ExtInfo());
//}

const CGFunctionInfo &CodeGenTypes::getFunctionInfo(Type ResTy,
                           const llvm::SmallVectorImpl<Type> &ArgTys,
                                            const FunctionType::ExtInfo &Info,
                                                    bool IsRecursive) {
#ifndef NDEBUG
//  for (llvm::SmallVectorImpl<Type>::const_iterator
//         I = ArgTys.begin(), E = ArgTys.end(); I != E; ++I)
//    assert(I->isCanonicalAsParam());
#endif

  unsigned CC = ClangCallConvToLLVMCallConv(Info.getCC());

  // Lookup or create unique function info.
  llvm::FoldingSetNodeID ID;
  CGFunctionInfo::Profile(ID, Info, ResTy,
                          ArgTys.begin(), ArgTys.end());

  void *InsertPos = 0;
  CGFunctionInfo *FI = FunctionInfos.FindNodeOrInsertPos(ID, InsertPos);
  if (FI)
    return *FI;

  // Construct the function info.
  FI = new CGFunctionInfo(CC, Info.getNoReturn(), Info.getProducesResult(),
                          Info.getHasRegParm(), Info.getRegParm(), ResTy,
                          ArgTys.data(), ArgTys.size());
  FunctionInfos.InsertNode(FI, InsertPos);

  // Compute ABI information.
  //getABIInfo().computeInfo(*FI);

  // Loop over all of the computed argument and return value info.  If any of
  // them are direct or extend without a specified coerce type, specify the
  // default now.
//  ABIArgInfo &RetInfo = FI->getReturnInfo();
//  if (RetInfo.canHaveCoerceToType() && RetInfo.getCoerceToType() == 0)
//    RetInfo.setCoerceToType(ConvertTypeRecursive(FI->getReturnType()));

  for (CGFunctionInfo::arg_iterator I = FI->arg_begin(), E = FI->arg_end();
       I != E; ++I)
    if (I->info.canHaveCoerceToType() && I->info.getCoerceToType() == 0)
      I->info.setCoerceToType(ConvertTypeRecursive(I->type));

  return *FI;
}

CGFunctionInfo::CGFunctionInfo(unsigned _CallingConvention,
                               bool _NoReturn, bool returnsRetained,
                               bool _HasRegParm, unsigned _RegParm,
                               Type ResTy,
                               const Type *ArgTys,
                               unsigned NumArgTys)
  : CallingConvention(_CallingConvention),
    EffectiveCallingConvention(_CallingConvention),
    NoReturn(_NoReturn), ReturnsRetained(returnsRetained),
    HasRegParm(_HasRegParm), RegParm(_RegParm)
{
  NumArgs = NumArgTys;

  // FIXME: Coallocate with the CGFunctionInfo object.
  Args = new ArgInfo[1 + NumArgTys];
  Args[0].type = ResTy;
  for (unsigned i = 0; i != NumArgTys; ++i)
    Args[1 + i].type = ArgTys[i];
}

/***/

void CodeGenTypes::GetExpandedTypes(Type type,
                     llvm::SmallVectorImpl<llvm::Type*> &expandedTypes,
                                    bool isRecursive) {
  const StructType *RT = type->getAsStructureType();
  assert(RT && "Can only expand structure types.");
  const StructDefn *RD = RT->getDefn();
//  assert(!RD->hasFlexibleArrayMember() &&
//         "Cannot expand structure with flexible array.");

  for (StructDefn::member_iterator i = RD->member_begin(), e = RD->member_end();
         i != e; ++i) {
    const MemberDefn *FD = *i;

    Type fieldType = FD->getType();
    if (fieldType->isStructType())
      GetExpandedTypes(fieldType, expandedTypes, isRecursive);
    else
      expandedTypes.push_back(ConvertType(fieldType, isRecursive));
  }
}

llvm::Function::arg_iterator
CodeGenFunction::ExpandTypeFromArgs(Type Ty, LValue LV,
                                    llvm::Function::arg_iterator AI) {
  const StructType *RT = Ty->getAsStructureType();
  assert(RT && "Can only expand structure types.");

  StructDefn *RD = RT->getDefn();
  assert(LV.isSimple() &&
         "Unexpected non-simple lvalue during struct expansion.");
  llvm::Value *Addr = LV.getAddress();
  for (StructDefn::member_iterator i = RD->member_begin(), e = RD->member_end();
         i != e; ++i) {
    MemberDefn *FD = *i;
    Type FT = FD->getType();

    // FIXME: What are the right qualifiers here?
    LValue LV = EmitLValueForField(Addr, FD, 0);
    if (CodeGenFunction::hasAggregateLLVMType(FT)) {
      AI = ExpandTypeFromArgs(FT, LV, AI);
    } else {
      EmitStoreThroughLValue(RValue::get(AI), LV);
      ++AI;
    }
  }

  return AI;
}

void
CodeGenFunction::ExpandTypeToArgs(Type Ty, RValue RV,
                                  llvm::SmallVector<llvm::Value*, 16> &Args) {
  const StructType *RT = Ty->getAsStructureType();
  assert(RT && "Can only expand structure types.");

  StructDefn *RD = RT->getDefn();
  assert(RV.isAggregate() && "Unexpected rvalue during struct expansion");
  llvm::Value *Addr = RV.getAggregateAddr();
  for (StructDefn::member_iterator i = RD->member_begin(), e = RD->member_end();
         i != e; ++i) {
	  MemberDefn *FD = *i;
    Type FT = FD->getType();

    // FIXME: What are the right qualifiers here?
    LValue LV = EmitLValueForField(Addr, FD, 0);
    if (CodeGenFunction::hasAggregateLLVMType(FT)) {
      ExpandTypeToArgs(FT, RValue::getAggregate(LV.getAddress()), Args);
    } else {
      RValue RV = EmitLoadOfLValue(LV,FT);
      assert(RV.isScalar() &&
             "Unexpected non-scalar rvalue during struct expansion.");
      Args.push_back(RV.getScalarVal());
    }
  }
}

/// EnterStructPointerForCoercedAccess - Given a struct pointer that we are
/// accessing some number of bytes out of it, try to gep into the struct to get
/// at its inner goodness.  Dive as deep as possible without entering an element
/// with an in-memory size smaller than DstSize.
static llvm::Value *
EnterStructPointerForCoercedAccess(llvm::Value *SrcPtr,
                                   llvm::StructType *SrcSTy,
                                   uint64_t DstSize, CodeGenFunction &CGF) {
  // We can't dive into a zero-element struct.
  if (SrcSTy->getNumElements() == 0) return SrcPtr;

  llvm::Type *FirstElt = SrcSTy->getElementType(0);

  // If the first elt is at least as large as what we're looking for, or if the
  // first element is the same size as the whole struct, we can enter it.
  uint64_t FirstEltSize =
    CGF.CGM.getTargetData().getTypeAllocSize(FirstElt);
  if (FirstEltSize < DstSize &&
      FirstEltSize < CGF.CGM.getTargetData().getTypeAllocSize(SrcSTy))
    return SrcPtr;

  // GEP into the first element.
  SrcPtr = CGF.Builder.CreateConstGEP2_32(SrcPtr, 0, 0, "coerce.dive");

  // If the first element is a struct, recurse.
  llvm::Type *SrcTy =
    cast<llvm::PointerType>(SrcPtr->getType())->getElementType();
  if (llvm::StructType *SrcSTy = dyn_cast<llvm::StructType>(SrcTy))
    return EnterStructPointerForCoercedAccess(SrcPtr, SrcSTy, DstSize, CGF);

  return SrcPtr;
}

/// CoerceIntOrPtrToIntOrPtr - Convert a value Val to the specific Ty where both
/// are either integers or pointers.  This does a truncation of the value if it
/// is too large or a zero extension if it is too small.
static llvm::Value *CoerceIntOrPtrToIntOrPtr(llvm::Value *Val,
                                             llvm::Type *Ty,
                                             CodeGenFunction &CGF) {
  if (Val->getType() == Ty)
    return Val;

  if (isa<llvm::PointerType>(Val->getType())) {
    // If this is Pointer->Pointer avoid conversion to and from int.
    if (isa<llvm::PointerType>(Ty))
      return CGF.Builder.CreateBitCast(Val, Ty, "coerce.val");

    // Convert the pointer to an integer so we can play with its width.
    Val = CGF.Builder.CreatePtrToInt(Val, CGF.IntPtrTy, "coerce.val.pi");
  }

  llvm::Type *DestIntTy = Ty;
  if (isa<llvm::PointerType>(DestIntTy))
    DestIntTy = CGF.IntPtrTy;

  if (Val->getType() != DestIntTy)
    Val = CGF.Builder.CreateIntCast(Val, DestIntTy, false, "coerce.val.ii");

  if (isa<llvm::PointerType>(Ty))
    Val = CGF.Builder.CreateIntToPtr(Val, Ty, "coerce.val.ip");
  return Val;
}



/// CreateCoercedLoad - Create a load from \arg SrcPtr interpreted as
/// a pointer to an object of type \arg Ty.
///
/// This safely handles the case when the src type is smaller than the
/// destination type; in this situation the values of bits which not
/// present in the src are undefined.
static llvm::Value *CreateCoercedLoad(llvm::Value *SrcPtr,
                                      llvm::Type *Ty,
                                      CodeGenFunction &CGF) {
  llvm::Type *SrcTy =
    cast<llvm::PointerType>(SrcPtr->getType())->getElementType();

  // If SrcTy and Ty are the same, just do a load.
  if (SrcTy == Ty)
    return CGF.Builder.CreateLoad(SrcPtr);

  uint64_t DstSize = CGF.CGM.getTargetData().getTypeAllocSize(Ty);

  if (llvm::StructType *SrcSTy = dyn_cast<llvm::StructType>(SrcTy)) {
    SrcPtr = EnterStructPointerForCoercedAccess(SrcPtr, SrcSTy, DstSize, CGF);
    SrcTy = cast<llvm::PointerType>(SrcPtr->getType())->getElementType();
  }

  uint64_t SrcSize = CGF.CGM.getTargetData().getTypeAllocSize(SrcTy);

  // If the source and destination are integer or pointer types, just do an
  // extension or truncation to the desired type.
  if ((isa<llvm::IntegerType>(Ty) || isa<llvm::PointerType>(Ty)) &&
      (isa<llvm::IntegerType>(SrcTy) || isa<llvm::PointerType>(SrcTy))) {
    llvm::LoadInst *Load = CGF.Builder.CreateLoad(SrcPtr);
    return CoerceIntOrPtrToIntOrPtr(Load, Ty, CGF);
  }

  // If load is legal, just bitcast the src pointer.
  if (SrcSize >= DstSize) {
    // Generally SrcSize is never greater than DstSize, since this means we are
    // losing bits. However, this can happen in cases where the structure has
    // additional padding, for example due to a user specified alignment.
    //
    // FIXME: Assert that we aren't truncating non-padding bits when have access
    // to that information.
    llvm::Value *Casted =
      CGF.Builder.CreateBitCast(SrcPtr, llvm::PointerType::getUnqual(Ty));
    llvm::LoadInst *Load = CGF.Builder.CreateLoad(Casted);
    // FIXME: Use better alignment / avoid requiring aligned load.
    Load->setAlignment(1);
    return Load;
  }

  // Otherwise do coercion through memory. This is stupid, but
  // simple.
  llvm::Value *Tmp = CGF.CreateTempAlloca(Ty);
  llvm::Value *Casted =
    CGF.Builder.CreateBitCast(Tmp, llvm::PointerType::getUnqual(SrcTy));
  llvm::StoreInst *Store =
    CGF.Builder.CreateStore(CGF.Builder.CreateLoad(SrcPtr), Casted);
  // FIXME: Use better alignment / avoid requiring aligned store.
  Store->setAlignment(1);
  return CGF.Builder.CreateLoad(Tmp);
}

// Function to store a first-class aggregate into memory.  We prefer to
// store the elements rather than the aggregate to be more friendly to
// fast-isel.
// FIXME: Do we need to recurse here?
static void BuildAggStore(CodeGenFunction &CGF, llvm::Value *Val,
                          llvm::Value *DestPtr, bool DestIsVolatile,
                          bool LowAlignment) {
  // Prefer scalar stores to first-class aggregate stores.
  if (llvm::StructType *STy =
        dyn_cast<llvm::StructType>(Val->getType())) {
    for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i) {
      llvm::Value *EltPtr = CGF.Builder.CreateConstGEP2_32(DestPtr, 0, i);
      llvm::Value *Elt = CGF.Builder.CreateExtractValue(Val, i);
      llvm::StoreInst *SI = CGF.Builder.CreateStore(Elt, EltPtr,
                                                    DestIsVolatile);
      if (LowAlignment)
        SI->setAlignment(1);
    }
  } else {
    CGF.Builder.CreateStore(Val, DestPtr, DestIsVolatile);
  }
}

/// CreateCoercedStore - Create a store to \arg DstPtr from \arg Src,
/// where the source and destination may have different types.
///
/// This safely handles the case when the src type is larger than the
/// destination type; the upper bits of the src will be lost.
static void CreateCoercedStore(llvm::Value *Src,
                               llvm::Value *DstPtr,
                               bool DstIsVolatile,
                               CodeGenFunction &CGF) {
  llvm::Type *SrcTy = Src->getType();
  llvm::Type *DstTy =
    cast<llvm::PointerType>(DstPtr->getType())->getElementType();
  if (SrcTy == DstTy) {
    CGF.Builder.CreateStore(Src, DstPtr, DstIsVolatile);
    return;
  }

  uint64_t SrcSize = CGF.CGM.getTargetData().getTypeAllocSize(SrcTy);

  if (llvm::StructType *DstSTy = dyn_cast<llvm::StructType>(DstTy)) {
    DstPtr = EnterStructPointerForCoercedAccess(DstPtr, DstSTy, SrcSize, CGF);
    DstTy = cast<llvm::PointerType>(DstPtr->getType())->getElementType();
  }

  // If the source and destination are integer or pointer types, just do an
  // extension or truncation to the desired type.
  if ((isa<llvm::IntegerType>(SrcTy) || isa<llvm::PointerType>(SrcTy)) &&
      (isa<llvm::IntegerType>(DstTy) || isa<llvm::PointerType>(DstTy))) {
    Src = CoerceIntOrPtrToIntOrPtr(Src, DstTy, CGF);
    CGF.Builder.CreateStore(Src, DstPtr, DstIsVolatile);
    return;
  }

  uint64_t DstSize = CGF.CGM.getTargetData().getTypeAllocSize(DstTy);

  // If store is legal, just bitcast the src pointer.
  if (SrcSize <= DstSize) {
    llvm::Value *Casted =
      CGF.Builder.CreateBitCast(DstPtr, llvm::PointerType::getUnqual(SrcTy));
    // FIXME: Use better alignment / avoid requiring aligned store.
    BuildAggStore(CGF, Src, Casted, DstIsVolatile, true);
  } else {
    // Otherwise do coercion through memory. This is stupid, but
    // simple.

    // Generally SrcSize is never greater than DstSize, since this means we are
    // losing bits. However, this can happen in cases where the structure has
    // additional padding, for example due to a user specified alignment.
    //
    // FIXME: Assert that we aren't truncating non-padding bits when have access
    // to that information.
    llvm::Value *Tmp = CGF.CreateTempAlloca(SrcTy);
    CGF.Builder.CreateStore(Src, Tmp);
    llvm::Value *Casted =
      CGF.Builder.CreateBitCast(Tmp, llvm::PointerType::getUnqual(DstTy));
    llvm::LoadInst *Load = CGF.Builder.CreateLoad(Casted);
    // FIXME: Use better alignment / avoid requiring aligned load.
    Load->setAlignment(1);
    CGF.Builder.CreateStore(Load, DstPtr, DstIsVolatile);
  }
}

/***/

bool CodeGenModule::ReturnTypeUsesSRet(const CGFunctionInfo &FI) {
  return FI.getReturnInfo().isIndirect();
}

bool CodeGenModule::ReturnTypeUsesFPRet(Type ResultType) {
  if (const SimpleNumericType *BT = ResultType->getAs<SimpleNumericType>()) {
    switch (BT->getKind()) {
    default:
      return false;
    case SimpleNumericType::Single:
      return getContext().Target.useObjCFPRetForRealType(TargetInfo::Float);
    case SimpleNumericType::Double:
      return getContext().Target.useObjCFPRetForRealType(TargetInfo::Double);
    }
  }

  return false;
}

llvm::FunctionType *CodeGenTypes::GetFunctionType(GlobalDefn GD) {
  const CGFunctionInfo &FI = getFunctionInfo(GD);

  // For definition purposes, don't consider a K&R function variadic.
  bool Variadic = false;
  if (const FunctionProtoType *FPT =
        cast<FunctionDefn>(GD.getDefn())->getType()->getAs<FunctionProtoType>())
    Variadic = FPT->isVariadic();

  return GetFunctionType(FI, Variadic, false);
}

llvm::FunctionType *
CodeGenTypes::GetFunctionType(const CGFunctionInfo &FI, bool isVariadic,
                              bool isRecursive) {
  llvm::SmallVector<llvm::Type*, 8> argTypes;
  llvm::Type *resultType = 0;

//  const ABIArgInfo &retAI = FI.getReturnInfo();
//  switch (retAI.getKind()) {
//  case ABIArgInfo::Expand:
//    llvm_unreachable("Invalid ABI kind for return argument");
//
//  case ABIArgInfo::Extend:
//  case ABIArgInfo::Direct:
//    resultType = retAI.getCoerceToType();
//    break;
//
//  case ABIArgInfo::Indirect: {
//    assert(!retAI.getIndirectAlign() && "Align unused on indirect return.");
//    resultType = llvm::Type::getVoidTy(getLLVMContext());
//
//    Type ret = FI.getReturnType();
//    llvm::Type *ty = ConvertType(ret, isRecursive);
//    unsigned addressSpace = Context.getTargetAddressSpace(ret);
//    argTypes.push_back(llvm::PointerType::get(ty, addressSpace));
//    break;
//  }
//
//  case ABIArgInfo::Ignore:
//    resultType = llvm::Type::getVoidTy(getLLVMContext());
//    break;
//  }

//  for (CGFunctionInfo::const_arg_iterator it = FI.arg_begin(),
//         ie = FI.arg_end(); it != ie; ++it) {
//    const ABIArgInfo &argAI = it->info;
//
//    switch (argAI.getKind()) {
//    case ABIArgInfo::Ignore:
//      break;
//
//    case ABIArgInfo::Indirect: {
//      // indirect arguments are always on the stack, which is addr space #0.
//      llvm::Type *LTy = ConvertTypeForMem(it->type, isRecursive);
//      argTypes.push_back(LTy->getPointerTo());
//      break;
//    }
//
//    case ABIArgInfo::Extend:
//    case ABIArgInfo::Direct: {
//      // If the coerce-to type is a first class aggregate, flatten it.  Either
//      // way is semantically identical, but fast-isel and the optimizer
//      // generally likes scalar values better than FCAs.
//      llvm::Type *argType = argAI.getCoerceToType();
//      if (llvm::StructType *st = dyn_cast<llvm::StructType>(argType)) {
//        for (unsigned i = 0, e = st->getNumElements(); i != e; ++i)
//          argTypes.push_back(st->getElementType(i));
//      } else {
//        argTypes.push_back(argType);
//      }
//      break;
//    }
//
//    case ABIArgInfo::Expand:
//      GetExpandedTypes(it->type, argTypes, isRecursive);
//      break;
//    }
//  }

  resultType = llvm::Type::getVoidTy(getLLVMContext());
  return llvm::FunctionType::get(resultType, argTypes, isVariadic);
}

llvm::Type *CodeGenTypes::GetFunctionTypeForVTable(GlobalDefn GD) {
  const ClassMethodDefn *MD = cast<ClassMethodDefn>(GD.getDefn());
  const FunctionProtoType *FPT = MD->getType()->getAs<FunctionProtoType>();

  if (!isFuncTypeConvertible(FPT))
    return llvm::StructType::get(getLLVMContext());

  const CGFunctionInfo *Info;
  if (isa<ClassDestructorDefn>(MD))
	  Info = &getFunctionInfo(cast<ClassDestructorDefn>(MD), GD.getDtorType());
  else
	  Info = &getFunctionInfo(MD);

  return GetFunctionType(*Info, FPT->isVariadic());
}

void CodeGenModule::ConstructAttributeList(const CGFunctionInfo &FI,
                                           const Defn *TargetDecl,
                                           AttributeListType &PAL,
                                           unsigned &CallingConv) {
  llvm::Attributes FuncAttrs;
  llvm::Attributes RetAttrs;

  CallingConv = FI.getEffectiveCallingConvention();

  if (FI.isNoReturn())
    FuncAttrs |= llvm::Attribute::NoReturn;

  // FIXME: handle sseregparm someday...
//  if (TargetDecl) {
//    if (TargetDecl->hasAttr<NoThrowAttr>())
//      FuncAttrs |= llvm::Attribute::NoUnwind;
//    else if (const FunctionDecl *Fn = dyn_cast<FunctionDecl>(TargetDecl)) {
//      const FunctionProtoType *FPT = Fn->getType()->getAs<FunctionProtoType>();
//      if (FPT && FPT->isNothrow(getContext()))
//        FuncAttrs |= llvm::Attribute::NoUnwind;
//    }
//
//    if (TargetDecl->hasAttr<NoReturnAttr>())
//      FuncAttrs |= llvm::Attribute::NoReturn;
//    if (TargetDecl->hasAttr<ConstAttr>())
//      FuncAttrs |= llvm::Attribute::ReadNone;
//    else if (TargetDecl->hasAttr<PureAttr>())
//      FuncAttrs |= llvm::Attribute::ReadOnly;
//    if (TargetDecl->hasAttr<MallocAttr>())
//      RetAttrs |= llvm::Attribute::NoAlias;
//  }

  if (CodeGenOpts.OptimizeSize)
    FuncAttrs |= llvm::Attribute::OptimizeForSize;
  if (CodeGenOpts.DisableRedZone)
    FuncAttrs |= llvm::Attribute::NoRedZone;
  if (CodeGenOpts.NoImplicitFloat)
    FuncAttrs |= llvm::Attribute::NoImplicitFloat;

  Type RetTy = FI.getReturnType();
  unsigned Index = 1;
  const ABIArgInfo &RetAI = FI.getReturnInfo();
  switch (RetAI.getKind()) {
  case ABIArgInfo::Extend:
//   if (RetTy->hasSignedIntegerRepresentation())
//     RetAttrs |= llvm::Attribute::SExt;
//   else if (RetTy->hasUnsignedIntegerRepresentation())
//     RetAttrs |= llvm::Attribute::ZExt;
    break;
  case ABIArgInfo::Direct:
  case ABIArgInfo::Ignore:
    break;

  case ABIArgInfo::Indirect:
    PAL.push_back(llvm::AttributeWithIndex::get(Index,
                                                llvm::Attribute::StructRet));
    ++Index;
    // sret disables readnone and readonly
    FuncAttrs &= ~(llvm::Attribute::ReadOnly |
                   llvm::Attribute::ReadNone);
    break;

  case ABIArgInfo::Expand:
    assert(0 && "Invalid ABI kind for return argument");
  }

  if (RetAttrs)
    PAL.push_back(llvm::AttributeWithIndex::get(0, RetAttrs));

  // FIXME: RegParm should be reduced in case of global register variable.
  signed RegParm;
  if (FI.getHasRegParm())
    RegParm = FI.getRegParm();
  else
    RegParm = CodeGenOpts.NumRegisterParameters;

  unsigned PointerWidth = getContext().Target.getPointerWidth(0);
  for (CGFunctionInfo::const_arg_iterator it = FI.arg_begin(),
         ie = FI.arg_end(); it != ie; ++it) {
    Type ParamType = it->type;
    const ABIArgInfo &AI = it->info;
    llvm::Attributes Attributes;

    // 'restrict' -> 'noalias' is done in EmitFunctionProlog when we
    // have the corresponding parameter variable.  It doesn't make
    // sense to do it here because parameters are so messed up.
    switch (AI.getKind()) {
    case ABIArgInfo::Extend:
      // FALL THROUGH
    case ABIArgInfo::Direct:
      if (RegParm > 0 &&
          (ParamType->isIntegerType()) ) {
        RegParm -=
        (Context.getTypeSize(ParamType) + PointerWidth - 1) / PointerWidth;
        if (RegParm >= 0)
          Attributes |= llvm::Attribute::InReg;
      }
      // FIXME: handle sseregparm someday...

      if (llvm::StructType *STy =
            dyn_cast<llvm::StructType>(AI.getCoerceToType()))
        Index += STy->getNumElements()-1;  // 1 will be added below.
      break;

    case ABIArgInfo::Indirect:
      if (AI.getIndirectByVal())
        Attributes |= llvm::Attribute::ByVal;

      Attributes |=
        llvm::Attribute::constructAlignmentFromInt(AI.getIndirectAlign());
      // byval disables readnone and readonly.
      FuncAttrs &= ~(llvm::Attribute::ReadOnly |
                     llvm::Attribute::ReadNone);
      break;

    case ABIArgInfo::Ignore:
      // Skip increment, no matching LLVM parameter.
      continue;

    case ABIArgInfo::Expand: {
      llvm::SmallVector<llvm::Type*, 8> types;
      // FIXME: This is rather inefficient. Do we ever actually need to do
      // anything here? The result should be just reconstructed on the other
      // side, so extension should be a non-issue.
      getTypes().GetExpandedTypes(ParamType, types, false);
      Index += types.size();
      continue;
    }
    }

    if (Attributes)
      PAL.push_back(llvm::AttributeWithIndex::get(Index, Attributes));
    ++Index;
  }
  if (FuncAttrs)
    PAL.push_back(llvm::AttributeWithIndex::get(~0, FuncAttrs));
}

/// An argument came in as a promoted argument; demote it back to its
/// declared type.
static llvm::Value *emitArgumentDemotion(CodeGenFunction &CGF,
                                         const VarDefn *var,
                                         llvm::Value *value) {
  llvm::Type *varType = CGF.ConvertType(var->getType());

  // This can happen with promotions that actually don't change the
  // underlying type, like the enum promotions.
  if (value->getType() == varType) return value;

  assert((varType->isIntegerTy() || varType->isFloatingPointTy())
         && "unexpected promotion type");

  if (isa<llvm::IntegerType>(varType))
    return CGF.Builder.CreateTrunc(value, varType, "arg.unpromote");

  return CGF.Builder.CreateFPCast(value, varType, "arg.unpromote");
}

void CodeGenFunction::EmitFunctionProlog(const CGFunctionInfo &FI,
                                         llvm::Function *Fn,
                                         const FunctionArgList &Args) {
  // If this is an implicit-return-zero function, go ahead and
  // initialize the return value.  TODO: it might be nice to have
  // a more general mechanism for this that didn't require synthesized
  // return statements.
  if (const FunctionDefn *FD = dyn_cast_or_null<FunctionDefn>(CurFuncDefn)) {
//    if (FD->hasImplicitReturnZero()) {
//      Type RetTy = FD->getResultType();
//      llvm::Type* LLVMTy = CGM.getTypes().ConvertType(RetTy);
//      llvm::Constant* Zero = llvm::Constant::getNullValue(LLVMTy);
//      Builder.CreateStore(Zero, ReturnValue);
//    }
  }

  // FIXME: We no longer need the types from FunctionArgList; lift up and
  // simplify.

  // Emit allocs for param decls.  Give the LLVM Argument nodes names.
  llvm::Function::arg_iterator AI = Fn->arg_begin();

  // Name the struct return argument.
  if (CGM.ReturnTypeUsesSRet(FI)) {
    AI->setName("agg.result");
    ++AI;
  }

  assert(FI.arg_size() == Args.size() &&
         "Mismatch between function signature & arguments.");
  unsigned ArgNo = 1;
  CGFunctionInfo::const_arg_iterator info_it = FI.arg_begin();
  for (FunctionArgList::const_iterator i = Args.begin(), e = Args.end(); 
       i != e; ++i, ++info_it, ++ArgNo) {
    const VarDefn *Arg = *i;
    Type Ty = info_it->type;
    const ABIArgInfo &ArgI = info_it->info;

    bool isPromoted =
      isa<ParamVarDefn>(Arg) /*&& cast<ParamVarDefn>(Arg)->isKNRPromoted()*/;

    switch (ArgI.getKind()) {
    case ABIArgInfo::Indirect: {
      llvm::Value *V = AI;

      if (hasAggregateLLVMType(Ty)) {
        // Aggregates and complex variables are accessed by reference.  All we
        // need to do is realign the value, if requested
        if (ArgI.getIndirectRealign()) {
          llvm::Value *AlignedTemp = CreateMemTemp(Ty, "coerce");

          // Copy from the incoming argument pointer to the temporary with the
          // appropriate alignment.
          //
          // FIXME: We should have a common utility for generating an aggregate
          // copy.
          llvm::Type *I8PtrTy = Builder.getInt8PtrTy();
          CharUnits Size = getContext().getTypeSizeInChars(Ty);
          llvm::Value *Dst = Builder.CreateBitCast(AlignedTemp, I8PtrTy);
          llvm::Value *Src = Builder.CreateBitCast(V, I8PtrTy);
          Builder.CreateMemCpy(Dst,
                               Src,
                               llvm::ConstantInt::get(IntPtrTy, 
                                                      Size.getQuantity()),
                               ArgI.getIndirectAlign(),
                               false);
          V = AlignedTemp;
        }
      } else {
        // Load scalar value from indirect argument.
        CharUnits Alignment = getContext().getTypeAlignInChars(Ty);
        V = EmitLoadOfScalar(V, false, Alignment.getQuantity(), Ty);

        if (isPromoted)
          V = emitArgumentDemotion(*this, Arg, V);
      }
      EmitParmDefn(*Arg, V, ArgNo);
      break;
    }

    case ABIArgInfo::Extend:
    case ABIArgInfo::Direct: {
      // If we have the trivial case, handle it with no muss and fuss.
      if (!isa<llvm::StructType>(ArgI.getCoerceToType()) &&
          ArgI.getCoerceToType() == ConvertType(Ty) &&
          ArgI.getDirectOffset() == 0) {
        assert(AI != Fn->arg_end() && "Argument mismatch!");
        llvm::Value *V = AI;

//        if (Arg->getType().isRestrictQualified())
//          AI->addAttr(llvm::Attribute::NoAlias);

        if (isPromoted)
          V = emitArgumentDemotion(*this, Arg, V);

        EmitParmDefn(*Arg, V, ArgNo);
        break;
      }

      llvm::AllocaInst *Alloca = CreateMemTemp(Ty, "coerce");

      // The alignment we need to use is the max of the requested alignment for
      // the argument plus the alignment required by our access code below.
      unsigned AlignmentToUse =
        CGM.getTargetData().getABITypeAlignment(ArgI.getCoerceToType());
      AlignmentToUse = std::max(AlignmentToUse,
                        (unsigned)getContext().getDefnAlign(Arg).getQuantity());

      Alloca->setAlignment(AlignmentToUse);
      llvm::Value *V = Alloca;
      llvm::Value *Ptr = V;    // Pointer to store into.

      // If the value is offset in memory, apply the offset now.
      if (unsigned Offs = ArgI.getDirectOffset()) {
        Ptr = Builder.CreateBitCast(Ptr, Builder.getInt8PtrTy());
        Ptr = Builder.CreateConstGEP1_32(Ptr, Offs);
        Ptr = Builder.CreateBitCast(Ptr,
                          llvm::PointerType::getUnqual(ArgI.getCoerceToType()));
      }

      // If the coerce-to type is a first class aggregate, we flatten it and
      // pass the elements. Either way is semantically identical, but fast-isel
      // and the optimizer generally likes scalar values better than FCAs.
      if (llvm::StructType *STy =
            dyn_cast<llvm::StructType>(ArgI.getCoerceToType())) {
        Ptr = Builder.CreateBitCast(Ptr, llvm::PointerType::getUnqual(STy));

        for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i) {
          assert(AI != Fn->arg_end() && "Argument mismatch!");
          AI->setName(Arg->getName() + ".coerce" + llvm::Twine(i));
          llvm::Value *EltPtr = Builder.CreateConstGEP2_32(Ptr, 0, i);
          Builder.CreateStore(AI++, EltPtr);
        }
      } else {
        // Simple case, just do a coerced store of the argument into the alloca.
        assert(AI != Fn->arg_end() && "Argument mismatch!");
        AI->setName(Arg->getName() + ".coerce");
        CreateCoercedStore(AI++, Ptr, /*DestIsVolatile=*/false, *this);
      }


      // Match to what EmitParmDecl is expecting for this type.
      if (!CodeGenFunction::hasAggregateLLVMType(Ty)) {
        V = EmitLoadOfScalar(V, false, AlignmentToUse, Ty);
        if (isPromoted)
          V = emitArgumentDemotion(*this, Arg, V);
      }
      EmitParmDefn(*Arg, V, ArgNo);
      continue;  // Skip ++AI increment, already done.
    }

    case ABIArgInfo::Expand: {
      // If this structure was expanded into multiple arguments then
      // we need to create a temporary and reconstruct it from the
      // arguments.
      llvm::Value *Temp = CreateMemTemp(Ty, Arg->getName() + ".addr");
      llvm::Function::arg_iterator End =
        ExpandTypeFromArgs(Ty, MakeAddrLValue(Temp, Ty), AI);
      EmitParmDefn(*Arg, Temp, ArgNo);

      // Name the arguments used in expansion and increment AI.
      unsigned Index = 0;
      for (; AI != End; ++AI, ++Index)
        AI->setName(Arg->getName() + "." + llvm::Twine(Index));
      continue;
    }

    case ABIArgInfo::Ignore:
      // Initialize the local variable appropriately.
      if (hasAggregateLLVMType(Ty))
        EmitParmDefn(*Arg, CreateMemTemp(Ty), ArgNo);
      else
        EmitParmDefn(*Arg, llvm::UndefValue::get(ConvertType(Arg->getType())),
                     ArgNo);

      // Skip increment, no matching LLVM parameter.
      continue;
    }

    ++AI;
  }
  assert(AI == Fn->arg_end() && "Argument mismatch!");
}

/// Try to emit a fused autorelease of a return result.
static llvm::Value *tryEmitFusedAutoreleaseOfResult(CodeGenFunction &CGF,
                                                    llvm::Value *result) {
  // We must be immediately followed the cast.
  llvm::BasicBlock *BB = CGF.Builder.GetInsertBlock();
  if (BB->empty()) return 0;
  if (&BB->back() != result) return 0;

  llvm::Type *resultType = result->getType();

  // result is in a BasicBlock and is therefore an Instruction.
  llvm::Instruction *generator = cast<llvm::Instruction>(result);

  llvm::SmallVector<llvm::Instruction*,4> insnsToKill;

  // Look for:
  //  %generator = bitcast %type1* %generator2 to %type2*
  while (llvm::BitCastInst *bitcast = dyn_cast<llvm::BitCastInst>(generator)) {
    // We would have emitted this as a constant if the operand weren't
    // an Instruction.
    generator = cast<llvm::Instruction>(bitcast->getOperand(0));

    // Require the generator to be immediately followed by the cast.
    if (generator->getNextNode() != bitcast)
      return 0;

    insnsToKill.push_back(bitcast);
  }

  // Look for:
  //   %generator = call i8* @objc_retain(i8* %originalResult)
  // or
  //   %generator = call i8* @objc_retainAutoreleasedReturnValue(i8* %originalResult)
  llvm::CallInst *call = dyn_cast<llvm::CallInst>(generator);
  if (!call) return 0;

  bool doRetainAutorelease;

//  if (call->getCalledValue() == CGF.CGM.getARCEntrypoints().objc_retain) {
//    doRetainAutorelease = true;
//  } else if (call->getCalledValue() == CGF.CGM.getARCEntrypoints()
//                                          .objc_retainAutoreleasedReturnValue) {
//    doRetainAutorelease = false;
//
//    // Look for an inline asm immediately preceding the call and kill it, too.
//    llvm::Instruction *prev = call->getPrevNode();
//    if (llvm::CallInst *asmCall = dyn_cast_or_null<llvm::CallInst>(prev))
//      if (asmCall->getCalledValue()
//            == CGF.CGM.getARCEntrypoints().retainAutoreleasedReturnValueMarker)
//        insnsToKill.push_back(prev);
//  } else {
//    return 0;
//  }
//
//  result = call->getArgOperand(0);
//  insnsToKill.push_back(call);
//
//  // Keep killing bitcasts, for sanity.  Note that we no longer care
//  // about precise ordering as long as there's exactly one use.
//  while (llvm::BitCastInst *bitcast = dyn_cast<llvm::BitCastInst>(result)) {
//    if (!bitcast->hasOneUse()) break;
//    insnsToKill.push_back(bitcast);
//    result = bitcast->getOperand(0);
//  }
//
//  // Delete all the unnecessary instructions, from latest to earliest.
//  for (llvm::SmallVectorImpl<llvm::Instruction*>::iterator
//         i = insnsToKill.begin(), e = insnsToKill.end(); i != e; ++i)
//    (*i)->eraseFromParent();
//
//  // Do the fused retain/autorelease if we were asked to.
////  if (doRetainAutorelease)
////    result = CGF.EmitARCRetainAutoreleaseReturnValue(result);
//
//  // Cast back to the result type.
//  return CGF.Builder.CreateBitCast(result, resultType);
  return NULL;
}

/// Emit an ARC autorelease of the result of a function.
static llvm::Value *emitAutoreleaseOfResult(CodeGenFunction &CGF,
                                            llvm::Value *result) {
  // At -O0, try to emit a fused retain/autorelease.
  if (CGF.shouldUseFusedARCCalls())
    if (llvm::Value *fused = tryEmitFusedAutoreleaseOfResult(CGF, result))
      return fused;

  // return CGF.EmitARCAutoreleaseReturnValue(result);
  return NULL;
}

void CodeGenFunction::EmitFunctionEpilog(const CGFunctionInfo &FI) {
  // Functions with no result always return void.
  if (ReturnValue == 0) {
    Builder.CreateRetVoid();
    return;
  }

  llvm::DebugLoc RetDbgLoc;
  llvm::Value *RV = 0;
  Type RetTy = FI.getReturnType();
  const ABIArgInfo &RetAI = FI.getReturnInfo();

  switch (RetAI.getKind()) {
  case ABIArgInfo::Indirect: {
    unsigned Alignment = getContext().getTypeAlignInChars(RetTy).getQuantity();
    if (RetTy.hasComplexAttr()) {
      ComplexPairTy RT = LoadComplexFromAddr(ReturnValue, false);
      StoreComplexToAddr(RT, CurFn->arg_begin(), false);
    } else if (CodeGenFunction::hasAggregateLLVMType(RetTy)) {
      // Do nothing; aggregrates get evaluated directly into the destination.
    } else {
      EmitStoreOfScalar(Builder.CreateLoad(ReturnValue), CurFn->arg_begin(),
                        false, Alignment, RetTy);
    }
    break;
  }

  case ABIArgInfo::Extend:
  case ABIArgInfo::Direct:
    if (RetAI.getCoerceToType() == ConvertType(RetTy) &&
        RetAI.getDirectOffset() == 0) {
      // The internal return value temp always will have pointer-to-return-type
      // type, just do a load.

      // If the instruction right before the insertion point is a store to the
      // return value, we can elide the load, zap the store, and usually zap the
      // alloca.
      llvm::BasicBlock *InsertBB = Builder.GetInsertBlock();
      llvm::StoreInst *SI = 0;
      if (InsertBB->empty() ||
          !(SI = dyn_cast<llvm::StoreInst>(&InsertBB->back())) ||
          SI->getPointerOperand() != ReturnValue || SI->isVolatile()) {
        RV = Builder.CreateLoad(ReturnValue);
      } else {
        // Get the stored value and nuke the now-dead store.
        RetDbgLoc = SI->getDebugLoc();
        RV = SI->getValueOperand();
        SI->eraseFromParent();

        // If that was the only use of the return value, nuke it as well now.
        if (ReturnValue->use_empty() && isa<llvm::AllocaInst>(ReturnValue)) {
          cast<llvm::AllocaInst>(ReturnValue)->eraseFromParent();
          ReturnValue = 0;
        }
      }
    } else {
      llvm::Value *V = ReturnValue;
      // If the value is offset in memory, apply the offset now.
      if (unsigned Offs = RetAI.getDirectOffset()) {
        V = Builder.CreateBitCast(V, Builder.getInt8PtrTy());
        V = Builder.CreateConstGEP1_32(V, Offs);
        V = Builder.CreateBitCast(V,
                         llvm::PointerType::getUnqual(RetAI.getCoerceToType()));
      }

      RV = CreateCoercedLoad(V, RetAI.getCoerceToType(), *this);
    }

    // In ARC, end functions that return a retainable type with a call
    // to objc_autoreleaseReturnValue.
//    if (AutoreleaseResult) {
//      assert(getLangOptions().ObjCAutoRefCount &&
//             !FI.isReturnsRetained() &&
//             RetTy->isObjCRetainableType());
//      RV = emitAutoreleaseOfResult(*this, RV);
//    }

    break;

  case ABIArgInfo::Ignore:
    break;

  case ABIArgInfo::Expand:
    assert(0 && "Invalid ABI kind for return argument");
  }

  llvm::Instruction *Ret = RV ? Builder.CreateRet(RV) : Builder.CreateRetVoid();
  if (!RetDbgLoc.isUnknown())
    Ret->setDebugLoc(RetDbgLoc);
}

void CodeGenFunction::EmitDelegateCallArg(CallArgList &args,
                                          const VarDefn *param) {
  // StartFunction converted the ABI-lowered parameter(s) into a
  // local alloca.  We need to turn that into an r-value suitable
  // for EmitCall.
  llvm::Value *local = GetAddrOfLocalVar(param);

  Type type = param->getType();

  // For the most part, we just need to load the alloca, except:
  // 1) aggregate r-values are actually pointers to temporaries, and
  // 2) references to aggregates are pointers directly to the aggregate.
  // I don't know why references to non-aggregates are different here.
  if (const ReferenceType *ref = type->getAs<ReferenceType>()) {
    if (hasAggregateLLVMType(ref->getPointeeType()))
      return args.add(RValue::getAggregate(local), type);

    // Locals which are references to scalars are represented
    // with allocas holding the pointer.
    return args.add(RValue::get(Builder.CreateLoad(local)), type);
  }

  if (type.hasComplexAttr()) {
    ComplexPairTy complex = LoadComplexFromAddr(local, /*volatile*/ false);
    return args.add(RValue::getComplex(complex), type);
  }

  if (hasAggregateLLVMType(type))
    return args.add(RValue::getAggregate(local), type);

  unsigned alignment = getContext().getDefnAlign(param).getQuantity();
  llvm::Value *value = EmitLoadOfScalar(local, false, alignment, type);
  return args.add(RValue::get(value), type);
}

static bool isProvablyNull(llvm::Value *addr) {
  return isa<llvm::ConstantPointerNull>(addr);
}

static bool isProvablyNonNull(llvm::Value *addr) {
  return isa<llvm::AllocaInst>(addr);
}

/// Emit the actual writing-back of a writeback.
static void emitWriteback(CodeGenFunction &CGF,
                          const CallArgList::Writeback &writeback) {
  llvm::Value *srcAddr = writeback.Address;
  assert(!isProvablyNull(srcAddr) &&
         "shouldn't have writeback for provably null argument");

  llvm::BasicBlock *contBB = 0;

  // If the argument wasn't provably non-null, we need to null check
  // before doing the store.
  bool provablyNonNull = isProvablyNonNull(srcAddr);
  if (!provablyNonNull) {
    llvm::BasicBlock *writebackBB = CGF.createBasicBlock("icr.writeback");
    contBB = CGF.createBasicBlock("icr.done");

    llvm::Value *isNull = CGF.Builder.CreateIsNull(srcAddr, "icr.isnull");
    CGF.Builder.CreateCondBr(isNull, contBB, writebackBB);
    CGF.EmitBlock(writebackBB);
  }

  // Load the value to writeback.
  llvm::Value *value = CGF.Builder.CreateLoad(writeback.Temporary);

  // Cast it back, in case we're writing an id to a Foo* or something.
  value = CGF.Builder.CreateBitCast(value,
               cast<llvm::PointerType>(srcAddr->getType())->getElementType(),
                            "icr.writeback-cast");
  
  // Perform the writeback.
  Type srcAddrType = writeback.AddressType;
  CGF.EmitStoreThroughLValue(RValue::get(value),
                             CGF.MakeAddrLValue(srcAddr, srcAddrType));

  // Jump to the continuation block.
  if (!provablyNonNull)
    CGF.EmitBlock(contBB);
}

static void emitWritebacks(CodeGenFunction &CGF,
                           const CallArgList &args) {
  for (CallArgList::writeback_iterator
         i = args.writeback_begin(), e = args.writeback_end(); i != e; ++i)
    emitWriteback(CGF, *i);
}

void CodeGenFunction::EmitCallArg(CallArgList &args, const Expr *E,
                                  Type type) {
//  if (type->isReferenceType())
//    return args.add(EmitReferenceBindingToExpr(E, /*InitializedDecl=*/0),
//                    type);

//  if (hasAggregateLLVMType(type) && !E->getType().hasComplexAttr() &&
//      isa<ImplicitCastExpr>(E) &&
//      cast<CastExpr>(E)->getCastKind() == CK_LValueToRValue) {
//    LValue L = EmitLValue(cast<CastExpr>(E)->getSubExpr());
//    assert(L.isSimple());
//    args.add(RValue::getAggregate(L.getAddress(), L.isVolatileQualified()),
//             type, /*NeedsCopy*/true);
//    return;
//  }

  args.add(EmitAnyExprToTemp(E), type);
}

/// Emits a call or invoke instruction to the given function, depending
/// on the current state of the EH stack.
llvm::CallSite
CodeGenFunction::EmitCallOrInvoke(llvm::Value *Callee,
		                              llvm::ArrayRef<llvm::Value *> Args,
                                  const llvm::Twine &Name) {
  llvm::BasicBlock *InvokeDest = getInvokeDest();
  if (!InvokeDest)
    return Builder.CreateCall(Callee, Args, Name);

  llvm::BasicBlock *ContBB = createBasicBlock("invoke.cont");
  llvm::InvokeInst *Invoke = Builder.CreateInvoke(Callee, ContBB, InvokeDest,
                                                  Args, Name);
  EmitBlock(ContBB);
  return Invoke;
}

RValue CodeGenFunction::EmitCall(const CGFunctionInfo &CallInfo,
                                 llvm::Value *Callee,
                                 ReturnValueSlot ReturnValue,
                                 const CallArgList &CallArgs,
                                 const Defn *TargetDecl,
                                 llvm::Instruction **callOrInvoke) {
  // FIXME: We no longer need the types from CallArgs; lift up and simplify.
  llvm::SmallVector<llvm::Value*, 16> Args;

  // Handle struct-return functions by passing a pointer to the
  // location that we would like to return into.
  Type RetTy = CallInfo.getReturnType();
  const ABIArgInfo &RetAI = CallInfo.getReturnInfo();


  // If the call returns a temporary with struct return, create a temporary
  // alloca to hold the result, unless one is given to us.
  if (CGM.ReturnTypeUsesSRet(CallInfo)) {
    llvm::Value *Value = ReturnValue.getValue();
    if (!Value)
      Value = CreateMemTemp(RetTy);
    Args.push_back(Value);
  }

  assert(CallInfo.arg_size() == CallArgs.size() &&
         "Mismatch between function signature & arguments.");
  CGFunctionInfo::const_arg_iterator info_it = CallInfo.arg_begin();
  for (CallArgList::const_iterator I = CallArgs.begin(), E = CallArgs.end();
       I != E; ++I, ++info_it) {
    const ABIArgInfo &ArgInfo = info_it->info;
    RValue RV = I->RV;

    unsigned TypeAlign =
      getContext().getTypeAlignInChars(I->Ty).getQuantity();
    switch (ArgInfo.getKind()) {
    case ABIArgInfo::Indirect: {
      if (RV.isScalar() || RV.isComplex()) {
        // Make a temporary alloca to pass the argument.
        llvm::AllocaInst *AI = CreateMemTemp(I->Ty);
        if (ArgInfo.getIndirectAlign() > AI->getAlignment())
          AI->setAlignment(ArgInfo.getIndirectAlign());
        Args.push_back(AI);
        if (RV.isScalar())
          EmitStoreOfScalar(RV.getScalarVal(), Args.back(), false,
                            TypeAlign, I->Ty);
        else
          StoreComplexToAddr(RV.getComplexVal(), Args.back(), false);
      } else {
        // We want to avoid creating an unnecessary temporary+copy here;
        // however, we need one in two cases:
        // 1. If the argument is not byval, and we are required to copy the
        //    source.  (This case doesn't occur on any common architecture.)
        // 2. If the argument is byval, RV is not sufficiently aligned, and
        //    we cannot force it to be sufficiently aligned.
        llvm::Value *Addr = RV.getAggregateAddr();
        unsigned Align = ArgInfo.getIndirectAlign();
        const llvm::TargetData *TD = &CGM.getTargetData();
        if ((!ArgInfo.getIndirectByVal() && I->NeedsCopy) ||
            (ArgInfo.getIndirectByVal() && TypeAlign < Align &&
             llvm::getOrEnforceKnownAlignment(Addr, Align, TD) < Align)) {
          // Create an aligned temporary, and copy to it.
          llvm::AllocaInst *AI = CreateMemTemp(I->Ty);
          if (Align > AI->getAlignment())
            AI->setAlignment(Align);
          Args.push_back(AI);
          EmitAggregateCopy(AI, Addr, I->Ty, RV.isVolatileQualified());
        } else {
          // Skip the extra memcpy call.
          Args.push_back(Addr);
        }
      }
      break;
    }

    case ABIArgInfo::Ignore:
      break;

    case ABIArgInfo::Extend:
    case ABIArgInfo::Direct: {
      if (!isa<llvm::StructType>(ArgInfo.getCoerceToType()) &&
          ArgInfo.getCoerceToType() == ConvertType(info_it->type) &&
          ArgInfo.getDirectOffset() == 0) {
        if (RV.isScalar())
          Args.push_back(RV.getScalarVal());
        else
          Args.push_back(Builder.CreateLoad(RV.getAggregateAddr()));
        break;
      }

      // FIXME: Avoid the conversion through memory if possible.
      llvm::Value *SrcPtr;
      if (RV.isScalar()) {
        SrcPtr = CreateMemTemp(I->Ty, "coerce");
        EmitStoreOfScalar(RV.getScalarVal(), SrcPtr, false, TypeAlign, I->Ty);
      } else if (RV.isComplex()) {
        SrcPtr = CreateMemTemp(I->Ty, "coerce");
        StoreComplexToAddr(RV.getComplexVal(), SrcPtr, false);
      } else
        SrcPtr = RV.getAggregateAddr();

      // If the value is offset in memory, apply the offset now.
      if (unsigned Offs = ArgInfo.getDirectOffset()) {
        SrcPtr = Builder.CreateBitCast(SrcPtr, Builder.getInt8PtrTy());
        SrcPtr = Builder.CreateConstGEP1_32(SrcPtr, Offs);
        SrcPtr = Builder.CreateBitCast(SrcPtr,
                       llvm::PointerType::getUnqual(ArgInfo.getCoerceToType()));

      }

      // If the coerce-to type is a first class aggregate, we flatten it and
      // pass the elements. Either way is semantically identical, but fast-isel
      // and the optimizer generally likes scalar values better than FCAs.
      if (llvm::StructType *STy =
            dyn_cast<llvm::StructType>(ArgInfo.getCoerceToType())) {
        SrcPtr = Builder.CreateBitCast(SrcPtr,
                                       llvm::PointerType::getUnqual(STy));
        for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i) {
          llvm::Value *EltPtr = Builder.CreateConstGEP2_32(SrcPtr, 0, i);
          llvm::LoadInst *LI = Builder.CreateLoad(EltPtr);
          // We don't know what we're loading from.
          LI->setAlignment(1);
          Args.push_back(LI);
        }
      } else {
        // In the simple case, just pass the coerced loaded value.
        Args.push_back(CreateCoercedLoad(SrcPtr, ArgInfo.getCoerceToType(),
                                         *this));
      }

      break;
    }

    case ABIArgInfo::Expand:
      ExpandTypeToArgs(I->Ty, RV, Args);
      break;
    }
  }

  // If the callee is a bitcast of a function to a varargs pointer to function
  // type, check to see if we can remove the bitcast.  This handles some cases
  // with unprototyped functions.
  if (llvm::ConstantExpr *CE = dyn_cast<llvm::ConstantExpr>(Callee))
    if (llvm::Function *CalleeF = dyn_cast<llvm::Function>(CE->getOperand(0))) {
      const llvm::PointerType *CurPT=cast<llvm::PointerType>(Callee->getType());
      const llvm::FunctionType *CurFT =
        cast<llvm::FunctionType>(CurPT->getElementType());
      const llvm::FunctionType *ActualFT = CalleeF->getFunctionType();

      if (CE->getOpcode() == llvm::Instruction::BitCast &&
          ActualFT->getReturnType() == CurFT->getReturnType() &&
          ActualFT->getNumParams() == CurFT->getNumParams() &&
          ActualFT->getNumParams() == Args.size() &&
          (CurFT->isVarArg() || !ActualFT->isVarArg())) {
        bool ArgsMatch = true;
        for (unsigned i = 0, e = ActualFT->getNumParams(); i != e; ++i)
          if (ActualFT->getParamType(i) != CurFT->getParamType(i)) {
            ArgsMatch = false;
            break;
          }

        // Strip the cast if we can get away with it.  This is a nice cleanup,
        // but also allows us to inline the function at -O0 if it is marked
        // always_inline.
        if (ArgsMatch)
          Callee = CalleeF;
      }
    }


  unsigned CallingConv;
  CodeGen::AttributeListType AttributeList;
  CGM.ConstructAttributeList(CallInfo, TargetDecl, AttributeList, CallingConv);
  llvm::AttrListPtr Attrs = llvm::AttrListPtr::get(AttributeList);

  llvm::BasicBlock *InvokeDest = 0;
  if (!(Attrs.getFnAttributes() & llvm::Attribute::NoUnwind))
    InvokeDest = getInvokeDest();

  llvm::CallSite CS;
  if (!InvokeDest) {
    CS = Builder.CreateCall(Callee, Args);
  } else {
    llvm::BasicBlock *Cont = createBasicBlock("invoke.cont");
    CS = Builder.CreateInvoke(Callee, Cont, InvokeDest, Args);
    EmitBlock(Cont);
  }
  if (callOrInvoke)
    *callOrInvoke = CS.getInstruction();

  CS.setAttributes(Attrs);
  CS.setCallingConv(static_cast<llvm::CallingConv::ID>(CallingConv));

  // If the call doesn't return, finish the basic block and clear the
  // insertion point; this allows the rest of IRgen to discard
  // unreachable code.
  if (CS.doesNotReturn()) {
    Builder.CreateUnreachable();
    Builder.ClearInsertionPoint();

    // FIXME: For now, emit a dummy basic block because expr emitters in
    // generally are not ready to handle emitting expressions at unreachable
    // points.
    EnsureInsertPoint();

    // Return a reasonable RValue.
    return GetUndefRValue(RetTy);
  }

  llvm::Instruction *CI = CS.getInstruction();
  if (Builder.isNamePreserving() && !CI->getType()->isVoidTy())
    CI->setName("call");

  // Emit any writebacks immediately.  Arguably this should happen
  // after any return-value munging.
  if (CallArgs.hasWritebacks())
    emitWritebacks(*this, CallArgs);

  switch (RetAI.getKind()) {
  case ABIArgInfo::Indirect: {
    unsigned Alignment = getContext().getTypeAlignInChars(RetTy).getQuantity();
    if (RetTy.hasComplexAttr())
      return RValue::getComplex(LoadComplexFromAddr(Args[0], false));
    if (CodeGenFunction::hasAggregateLLVMType(RetTy))
      return RValue::getAggregate(Args[0]);
    return RValue::get(EmitLoadOfScalar(Args[0], false, Alignment, RetTy));
  }

  case ABIArgInfo::Ignore:
    // If we are ignoring an argument that had a result, make sure to
    // construct the appropriate return value for our caller.
    return GetUndefRValue(RetTy);

  case ABIArgInfo::Extend:
  case ABIArgInfo::Direct: {
    if (RetAI.getCoerceToType() == ConvertType(RetTy) &&
        RetAI.getDirectOffset() == 0) {
      if (RetTy.hasComplexAttr()) {
        llvm::Value *Real = Builder.CreateExtractValue(CI, 0);
        llvm::Value *Imag = Builder.CreateExtractValue(CI, 1);
        return RValue::getComplex(std::make_pair(Real, Imag));
      }
      if (CodeGenFunction::hasAggregateLLVMType(RetTy)) {
        llvm::Value *DestPtr = ReturnValue.getValue();
        bool DestIsVolatile = ReturnValue.isVolatile();

        if (!DestPtr) {
          DestPtr = CreateMemTemp(RetTy, "agg.tmp");
          DestIsVolatile = false;
        }
        BuildAggStore(*this, CI, DestPtr, DestIsVolatile, false);
        return RValue::getAggregate(DestPtr);
      }
      return RValue::get(CI);
    }

    llvm::Value *DestPtr = ReturnValue.getValue();
    bool DestIsVolatile = ReturnValue.isVolatile();

    if (!DestPtr) {
      DestPtr = CreateMemTemp(RetTy, "coerce");
      DestIsVolatile = false;
    }

    // If the value is offset in memory, apply the offset now.
    llvm::Value *StorePtr = DestPtr;
    if (unsigned Offs = RetAI.getDirectOffset()) {
      StorePtr = Builder.CreateBitCast(StorePtr, Builder.getInt8PtrTy());
      StorePtr = Builder.CreateConstGEP1_32(StorePtr, Offs);
      StorePtr = Builder.CreateBitCast(StorePtr,
                         llvm::PointerType::getUnqual(RetAI.getCoerceToType()));
    }
    CreateCoercedStore(CI, StorePtr, DestIsVolatile, *this);

    unsigned Alignment = getContext().getTypeAlignInChars(RetTy).getQuantity();
    if (RetTy.hasComplexAttr())
      return RValue::getComplex(LoadComplexFromAddr(DestPtr, false));
    if (CodeGenFunction::hasAggregateLLVMType(RetTy))
      return RValue::getAggregate(DestPtr);
    return RValue::get(EmitLoadOfScalar(DestPtr, false, Alignment, RetTy));
  }

  case ABIArgInfo::Expand:
    assert(0 && "Invalid ABI kind for return argument");
  }

  assert(0 && "Unhandled ABIArgInfo::Kind");
  return RValue::get(0);
}

/* VarArg handling */

llvm::Value *CodeGenFunction::EmitVAArg(llvm::Value *VAListAddr, Type Ty) {
  return CGM.getTypes().getABIInfo().EmitVAArg(VAListAddr, Ty, *this);
}
