//===--- CGExprConstant.cpp - Emit LLVM Code from Constant Expressions ----===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Constant Expr nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGOOPABI.h"
#include "CGRecordLayout.h"
#include "mlang/AST/APValue.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/AST/StmtVisitor.h"
#include "mlang/Basic/Builtins.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Target/TargetData.h"
using namespace mlang;
using namespace CodeGen;

//===----------------------------------------------------------------------===//
//                            ConstStructBuilder
//===----------------------------------------------------------------------===//

namespace {

class ConstStructBuilder {
  CodeGenModule &CGM;
  CodeGenFunction *CGF;

  bool Packed;
  CharUnits NextFieldOffsetInChars;
  CharUnits LLVMStructAlignment;
  std::vector<llvm::Constant *> Elements;
public:
  static llvm::Constant *BuildStruct(CodeGenModule &CGM, CodeGenFunction *CGF,
                                     InitListExpr *ILE);
  
private:  
  ConstStructBuilder(CodeGenModule &CGM, CodeGenFunction *CGF)
    : CGM(CGM), CGF(CGF), Packed(false),
      NextFieldOffsetInChars(CharUnits::Zero()),
      LLVMStructAlignment(CharUnits::One()) { }

  bool AppendField(const MemberDefn *Field, uint64_t FieldOffset,
                   llvm::Constant *InitExpr);

  void AppendPadding(CharUnits PadSize);

  void AppendTailPadding(CharUnits RecordSize);

  void ConvertStructToPacked();
                              
  bool Build(InitListExpr *ILE);

  CharUnits getAlignment(const llvm::Constant *C) const {
    if (Packed)  return CharUnits::One();
    return CharUnits::fromQuantity(
    		CGM.getTargetData().getABITypeAlignment(C->getType()));
  }

  CharUnits getSizeInChars(const llvm::Constant *C) const {
    return CharUnits::fromQuantity(
    		CGM.getTargetData().getTypeAllocSize(C->getType()));
  }
};

bool ConstStructBuilder::
AppendField(const MemberDefn *Field, uint64_t FieldOffset,
            llvm::Constant *InitCst) {
	const ASTContext &Context = CGM.getContext();

	CharUnits FieldOffsetInChars = Context.toCharUnitsFromBits(FieldOffset);

	assert(NextFieldOffsetInChars <= FieldOffsetInChars
			&& "Field offset mismatch!");

	CharUnits FieldAlignment = getAlignment(InitCst);

	// Round up the field offset to the alignment of the field type.
	CharUnits AlignedNextFieldOffsetInChars =
			NextFieldOffsetInChars.RoundUpToAlignment(FieldAlignment);

	if (AlignedNextFieldOffsetInChars > FieldOffsetInChars) {
		assert(!Packed && "Alignment is wrong even with a packed struct!");

		// Convert the struct to a packed struct.
		ConvertStructToPacked();

		AlignedNextFieldOffsetInChars = NextFieldOffsetInChars;
	}

	if (AlignedNextFieldOffsetInChars < FieldOffsetInChars) {
		// We need to append padding.
		AppendPadding(FieldOffsetInChars - NextFieldOffsetInChars);

		assert(NextFieldOffsetInChars == FieldOffsetInChars &&
				"Did not add enough padding!");

		AlignedNextFieldOffsetInChars = NextFieldOffsetInChars;
	}

	// Add the field.
	Elements.push_back(InitCst);
	NextFieldOffsetInChars = AlignedNextFieldOffsetInChars + getSizeInChars(
			InitCst);

	if (Packed)
		assert(LLVMStructAlignment == CharUnits::One() &&
				"Packed struct not byte-aligned!");
	else
		LLVMStructAlignment = std::max(LLVMStructAlignment, FieldAlignment);

	return true;
}

void ConstStructBuilder::AppendPadding(CharUnits PadSize) {
  if (PadSize.isZero())
    return;

  llvm::Type *Ty = llvm::Type::getInt8Ty(CGM.getLLVMContext());
  if (PadSize > CharUnits::One())
    Ty = llvm::ArrayType::get(Ty, PadSize.getQuantity());

  llvm::Constant *C = llvm::UndefValue::get(Ty);
  Elements.push_back(C);
  assert(getAlignment(C) == CharUnits::One() &&
         "Padding must have 1 byte alignment!");

  NextFieldOffsetInChars += getSizeInChars(C);
}

void ConstStructBuilder::AppendTailPadding(CharUnits RecordSize) {
  assert(NextFieldOffsetInChars <= RecordSize &&
         "Size mismatch!");

  AppendPadding(RecordSize - NextFieldOffsetInChars);
}

void ConstStructBuilder::ConvertStructToPacked() {
  std::vector<llvm::Constant *> PackedElements;
  CharUnits ElementOffsetInChars = CharUnits::Zero();

  for (unsigned i = 0, e = Elements.size(); i != e; ++i) {
    llvm::Constant *C = Elements[i];

    CharUnits ElementAlign = CharUnits::fromQuantity(
      CGM.getTargetData().getABITypeAlignment(C->getType()));
    CharUnits AlignedElementOffsetInChars =
      ElementOffsetInChars.RoundUpToAlignment(ElementAlign);

    if (AlignedElementOffsetInChars > ElementOffsetInChars) {
      // We need some padding.
      CharUnits NumChars =
        AlignedElementOffsetInChars - ElementOffsetInChars;

      llvm::Type *Ty = llvm::Type::getInt8Ty(CGM.getLLVMContext());
      if (NumChars > CharUnits::One())
        Ty = llvm::ArrayType::get(Ty, NumChars.getQuantity());

      llvm::Constant *Padding = llvm::UndefValue::get(Ty);
      PackedElements.push_back(Padding);
      ElementOffsetInChars += getSizeInChars(Padding);
    }

    PackedElements.push_back(C);
    ElementOffsetInChars += getSizeInChars(C);
  }

  assert(ElementOffsetInChars == NextFieldOffsetInChars &&
         "Packing the struct changed its size!");

  Elements = PackedElements;
  LLVMStructAlignment = CharUnits::One();
  Packed = true;
}
                            
bool ConstStructBuilder::Build(InitListExpr *ILE) {
  StructDefn *RD = ILE->getType()->getAs<StructType>()->getDefn();
  const ASTRecordLayout &Layout = CGM.getContext().getASTRecordLayout(RD);

  unsigned FieldNo = 0;
  unsigned ElementNo = 0;

  for (StructDefn::member_iterator Field = RD->member_begin(),
       FieldEnd = RD->member_end(); Field != FieldEnd; ++Field, ++FieldNo) {
    // Get the initializer.  A struct can include fields without initializers,
    // we just use explicit null values for them.
    llvm::Constant *EltInit;
    if (ElementNo < ILE->getNumInits())
      EltInit = CGM.EmitConstantExpr(ILE->getInit(ElementNo++),
                                     Field->getType(), CGF);
    else
      EltInit = CGM.EmitNullConstant(Field->getType());

    if (!EltInit)
      return false;

    // Handle members.
    if (!AppendField(*Field, Layout.getFieldOffset(FieldNo), EltInit))
    	return false;
  }

  CharUnits LayoutSizeInChars = Layout.getSize();

  if (NextFieldOffsetInChars > LayoutSizeInChars) {
    // If the struct is bigger than the size of the record type,
    // we must have a flexible array member at the end.
//    assert(RD->hasFlexibleArrayMember() &&
//           "Must have flexible array member if struct is bigger than type!");

    // No tail padding is necessary.
    return true;
  }

  CharUnits LLVMSizeInChars =
    NextFieldOffsetInChars.RoundUpToAlignment(LLVMStructAlignment);

  // Check if we need to convert the struct to a packed struct.
  if (NextFieldOffsetInChars <= LayoutSizeInChars &&
      LLVMSizeInChars > LayoutSizeInChars) {
    assert(!Packed && "Size mismatch!");

    ConvertStructToPacked();
    assert(NextFieldOffsetInChars <= LayoutSizeInChars &&
           "Converting to packed did not help!");
  }

  // Append tail padding if necessary.
  AppendTailPadding(LayoutSizeInChars);

  assert(LayoutSizeInChars == NextFieldOffsetInChars &&
         "Tail padding mismatch!");

  return true;
}

llvm::Constant *ConstStructBuilder::
  BuildStruct(CodeGenModule &CGM, CodeGenFunction *CGF, InitListExpr *ILE) {
	ConstStructBuilder Builder(CGM, CGF);

	if (!Builder.Build(ILE))
		return 0;

	// Pick the type to use.  If the type is layout identical to the ConvertType
	// type then use it, otherwise use whatever the builder produced for us.
	llvm::StructType *STy = llvm::ConstantStruct::getTypeForElements(
			CGM.getLLVMContext(), Builder.Elements, Builder.Packed);
	llvm::Type *ILETy = CGM.getTypes().ConvertType(ILE->getType());
	if (llvm::StructType *ILESTy = dyn_cast<llvm::StructType>(ILETy)) {
		if (ILESTy->isLayoutIdentical(STy))
			STy = ILESTy;
	}

	llvm::Constant *Result = llvm::ConstantStruct::get(STy, Builder.Elements);

	assert(Builder.NextFieldOffsetInChars.RoundUpToAlignment(
					Builder.getAlignment(Result)) ==
			Builder.getSizeInChars(Result) && "Size mismatch!");

	return Result;
}

  
//===----------------------------------------------------------------------===//
//                             ConstExprEmitter
//===----------------------------------------------------------------------===//
  
class ConstExprEmitter :
  public StmtVisitor<ConstExprEmitter, llvm::Constant*> {
  CodeGenModule &CGM;
  CodeGenFunction *CGF;
  llvm::LLVMContext &VMContext;
public:
  ConstExprEmitter(CodeGenModule &cgm, CodeGenFunction *cgf)
    : CGM(cgm), CGF(cgf), VMContext(cgm.getLLVMContext()) {
  }

  //===--------------------------------------------------------------------===//
  //                            Visitor Methods
  //===--------------------------------------------------------------------===//

  llvm::Constant *VisitStmt(Stmt *S) {
    return 0;
  }

  llvm::Constant *VisitParenExpr(ParenExpr *PE) {
    return Visit(PE->getSubExpr());
  }

  llvm::Constant *VisitImaginaryLiteralExpr(ImaginaryLiteral *E) {
    return Visit(E->getSubExpr());
  }
    
  llvm::Constant *VisitBinSub(BinaryOperator *E) {
    llvm::Constant *LHS = CGM.EmitConstantExpr(E->getLHS(),
                                               E->getLHS()->getType(), CGF);
    llvm::Constant *RHS = CGM.EmitConstantExpr(E->getRHS(),
                                               E->getRHS()->getType(), CGF);

    llvm::Type *ResultType = ConvertType(E->getType());
    LHS = llvm::ConstantExpr::getPtrToInt(LHS, ResultType);
    RHS = llvm::ConstantExpr::getPtrToInt(RHS, ResultType);
        
    // No need to divide by element size, since addr of label is always void*,
    // which has size 1 in GNUish.
    return llvm::ConstantExpr::getSub(LHS, RHS);
  }
    
//  llvm::Constant *VisitCastExpr(CastExpr* E) {
//    switch (E->getCastKind()) {
//    case CK_ToUnion: {
//      // GCC cast to union extension
//      assert(E->getType()->isUnionType() &&
//             "Destination type is not union type!");
//      llvm::Type *Ty = ConvertType(E->getType());
//      Expr *SubExpr = E->getSubExpr();
//
//      llvm::Constant *C =
//        CGM.EmitConstantExpr(SubExpr, SubExpr->getType(), CGF);
//      if (!C)
//        return 0;
//
//      // Build a struct with the union sub-element as the first member,
//      // and padded to the appropriate size
//      std::vector<llvm::Constant*> Elts;
//      std::vector<llvm::Type*> Types;
//      Elts.push_back(C);
//      Types.push_back(C->getType());
//      unsigned CurSize = CGM.getTargetData().getTypeAllocSize(C->getType());
//      unsigned TotalSize = CGM.getTargetData().getTypeAllocSize(Ty);
//
//      assert(CurSize <= TotalSize && "Union size mismatch!");
//      if (unsigned NumPadBytes = TotalSize - CurSize) {
//        llvm::Type *Ty = llvm::Type::getInt8Ty(VMContext);
//        if (NumPadBytes > 1)
//          Ty = llvm::ArrayType::get(Ty, NumPadBytes);
//
//        Elts.push_back(llvm::UndefValue::get(Ty));
//        Types.push_back(Ty);
//      }
//
//      llvm::StructType* STy =
//        llvm::StructType::get(C->getType()->getContext(), Types, false);
//      return llvm::ConstantStruct::get(STy, Elts);
//    }
//    case CK_NullToMemberPointer: {
//      const MemberPointerType *MPT = E->getType()->getAs<MemberPointerType>();
//      return CGM.getOOPABI().EmitNullMemberPointer(MPT);
//    }
//
//    case CK_BaseToDerivedMemberPointer: {
//      Expr *SubExpr = E->getSubExpr();
//      llvm::Constant *C =
//        CGM.EmitConstantExpr(SubExpr, SubExpr->getType(), CGF);
//      if (!C) return 0;
//
//      return CGM.getOOPABI().EmitMemberPointerConversion(C, E);
//    }
//
//    case CK_BitCast:
//      // This must be a member function pointer cast.
//      return Visit(E->getSubExpr());
//
//    default: {
//      // FIXME: This should be handled by the CK_NoOp cast kind.
//      // Explicit and implicit no-op casts
//      Type Ty = E->getType(), SubTy = E->getSubExpr()->getType();
//      if (CGM.getContext().hasSameUnqualifiedType(Ty, SubTy))
//        return Visit(E->getSubExpr());
//
//      // Handle integer->integer casts for address-of-label differences.
//      if (Ty->isIntegerType() && SubTy->isIntegerType() &&
//          CGF) {
//        llvm::Value *Src = Visit(E->getSubExpr());
//        if (Src == 0) return 0;
//
//        // Use EmitScalarConversion to perform the conversion.
//        return cast<llvm::Constant>(CGF->EmitScalarConversion(Src, SubTy, Ty));
//      }
//
//      return 0;
//    }
//    }
//  }

  llvm::Constant *EmitArrayInitialization(InitListExpr *ILE) {
    unsigned NumInitElements = ILE->getNumInits();
    if (NumInitElements == 1 &&
        (isa<StringLiteral>(ILE->getInit(0)) ))
      return Visit(ILE->getInit(0));

    std::vector<llvm::Constant*> Elts;
    llvm::ArrayType *AType =
        cast<llvm::ArrayType>(ConvertType(ILE->getType()));
    llvm::Type *ElemTy = AType->getElementType();
    unsigned NumElements = AType->getNumElements();

    // Initialising an array requires us to automatically
    // initialise any elements that have not been initialised explicitly
    unsigned NumInitableElts = std::min(NumInitElements, NumElements);

    // Copy initializer elements.
    unsigned i = 0;
    bool RewriteType = false;
    for (; i < NumInitableElts; ++i) {
      Expr *Init = ILE->getInit(i);
      llvm::Constant *C = CGM.EmitConstantExpr(Init, Init->getType(), CGF);
      if (!C)
        return 0;
      RewriteType |= (C->getType() != ElemTy);
      Elts.push_back(C);
    }

    // Initialize remaining array elements.
    // FIXME: This doesn't handle member pointers correctly!
    for (; i < NumElements; ++i)
      Elts.push_back(llvm::Constant::getNullValue(ElemTy));

    if (RewriteType) {
      // FIXME: Try to avoid packing the array
      std::vector<llvm::Type*> Types;
      for (unsigned i = 0; i < Elts.size(); ++i)
        Types.push_back(Elts[i]->getType());
      llvm::StructType *SType = llvm::StructType::get(AType->getContext(),
                                                            Types, true);
      return llvm::ConstantStruct::get(SType, Elts);
    }

    return llvm::ConstantArray::get(AType, Elts);
  }

  llvm::Constant *EmitStructInitialization(InitListExpr *ILE) {
    return ConstStructBuilder::BuildStruct(CGM, CGF, ILE);
  }

  llvm::Constant *VisitInitListExpr(InitListExpr *ILE) {
	  if (ILE->getType()->isArrayType())
		  return EmitArrayInitialization(ILE);
	  else {
      // We have a scalar in braces. Just use the first element.
      if (ILE->getNumInits() > 0) {
    	  Expr *Init = ILE->getInit(0);
    	  return CGM.EmitConstantExpr(Init, Init->getType(), CGF);
      }
      return CGM.EmitNullConstant(ILE->getType());
    }

    if (ILE->getType()->isHeteroContainerType())
      return EmitStructInitialization(ILE);

    // If ILE was a constant vector, we would have handled it already.
    if (ILE->getType()->isVectorType())
      return 0;

    assert(0 && "Unable to handle InitListExpr");
    // Get rid of control reaches end of void function warning.
    // Not reached.
    return 0;
  }

  llvm::Constant *VisitStringLiteral(StringLiteral *E) {
    return CGM.GetConstantArrayFromStringLiteral(E);
  }




  // Utility methods
  llvm::Type *ConvertType(Type T) {
    return CGM.getTypes().ConvertType(T);
  }

public:
  llvm::Constant *EmitLValue(Expr *E) {
    switch (E->getStmtClass()) {
    default: break;
    case Expr::DefnRefExprClass: {
      ValueDefn *Defn = cast<DefnRefExpr>(E)->getDefn();
//      if (Decl->hasAttr<WeakRefAttr>())
//        return CGM.GetWeakRefReference(Decl);
//      if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(Decl))
//        return CGM.GetAddrOfFunction(FD);
      if (const VarDefn* VD = dyn_cast<VarDefn>(Defn)) {
        // We can never refer to a variable with local storage.
//        if (!VD->hasLocalStorage()) {
//          if (VD->isFileVarDecl() || VD->hasExternalStorage())
//            return CGM.GetAddrOfGlobalVar(VD);
//          else if (VD->isLocalVarDecl()) {
//            assert(CGF && "Can't access static local vars without CGF");
//            return CGF->GetAddrOfStaticLocalVar(VD);
//          }
//        }
        return 0;
      }
      break;
    }
    case Expr::StringLiteralClass:
      return CGM.GetAddrOfConstantStringFromLiteral(cast<StringLiteral>(E));
    case Expr::FunctionCallClass: {
    	FunctionCall* CE = cast<FunctionCall>(E);
      const Expr *Arg = CE->getArg(0)->IgnoreParenCasts();
      const StringLiteral *Literal = cast<StringLiteral>(Arg);
      // FIXME: need to deal with UCN conversion issues.
      //FIXME huyabin
//      return CGM.GetAddrOfConstantCFString(Literal);
      return NULL;
    }
    case Expr::BlockCmdClass: {
      std::string FunctionName;
      if (CGF)
        FunctionName = CGF->CurFn->getName();
      else
        FunctionName = "global";

      return CGM.GetAddrOfGlobalBlock(cast<BlockCmd>(E), FunctionName.c_str());
    }
    }

    return 0;
  }
};

}  // end anonymous namespace.

llvm::Constant *CodeGenModule::EmitConstantExpr(const Expr *E,
                                                Type DestType,
                                                CodeGenFunction *CGF) {
  Expr::EvalResult Result;

  bool Success = false;

  if (DestType->isReferenceType())
    Success = E->EvaluateAsLValue(Result, Context);
  else
    Success = E->Evaluate(Result, Context);

  if (Success && !Result.HasSideEffects) {
    switch (Result.Val.getKind()) {
    case APValue::Uninitialized:
      assert(0 && "Constant expressions should be initialized.");
      return 0;
    case APValue::LValue: {
      llvm::Type *DestTy = getTypes().ConvertTypeForMem(DestType);
      llvm::Constant *Offset =
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(VMContext),
                               Result.Val.getLValueOffset().getQuantity());

      llvm::Constant *C;
      if (const Expr *LVBase = Result.Val.getLValueBase()) {
        C = ConstExprEmitter(*this, CGF).EmitLValue(const_cast<Expr*>(LVBase));

        // Apply offset if necessary.
        if (!Offset->isNullValue()) {
          llvm::Type *Type = llvm::Type::getInt8PtrTy(VMContext);
          llvm::Constant *Casted = llvm::ConstantExpr::getBitCast(C, Type);
          Casted = llvm::ConstantExpr::getGetElementPtr(Casted, Offset);
          C = llvm::ConstantExpr::getBitCast(Casted, C->getType());
        }

        // Convert to the appropriate type; this could be an lvalue for
        // an integer.
        if (isa<llvm::PointerType>(DestTy))
          return llvm::ConstantExpr::getBitCast(C, DestTy);

        return llvm::ConstantExpr::getPtrToInt(C, DestTy);
      } else {
        C = Offset;

        // Convert to the appropriate type; this could be an lvalue for
        // an integer.
        if (isa<llvm::PointerType>(DestTy))
          return llvm::ConstantExpr::getIntToPtr(C, DestTy);

        // If the types don't match this should only be a truncate.
        if (C->getType() != DestTy)
          return llvm::ConstantExpr::getTrunc(C, DestTy);

        return C;
      }
    }
    case APValue::Int: {
      llvm::Constant *C = llvm::ConstantInt::get(VMContext,
                                                 Result.Val.getInt());

      if (C->getType()->isIntegerTy(1)) {
        llvm::Type *BoolTy = getTypes().ConvertTypeForMem(E->getType());
        C = llvm::ConstantExpr::getZExt(C, BoolTy);
      }
      return C;
    }
    case APValue::ComplexInt: {
    	llvm::Constant *Complex[2];

    	Complex[0] = llvm::ConstantInt::get(VMContext,
    			Result.Val.getComplexIntReal());
    	Complex[1] = llvm::ConstantInt::get(VMContext,
    			Result.Val.getComplexIntImag());

    	// FIXME: the target may want to specify that this is packed.
    	llvm::StructType *STy = llvm::StructType::get(Complex[0]->getType(),
    			Complex[1]->getType(), NULL);
    	return llvm::ConstantStruct::get(STy, Complex);
    }
    case APValue::Float:
      return llvm::ConstantFP::get(VMContext, Result.Val.getFloat());
    case APValue::ComplexFloat: {
    	llvm::Constant *Complex[2];
    	Complex[0] = llvm::ConstantFP::get(VMContext,
    			Result.Val.getComplexFloatReal());
    	Complex[1] = llvm::ConstantFP::get(VMContext,
    			Result.Val.getComplexFloatImag());

    	// FIXME: the target may want to specify that this is packed.
    	llvm::StructType *STy = llvm::StructType::get(Complex[0]->getType(),
    			Complex[1]->getType(), NULL);
    	return llvm::ConstantStruct::get(STy, Complex);
    }
    case APValue::Vector: {
      llvm::SmallVector<llvm::Constant *, 4> Inits;
      unsigned NumElts = Result.Val.getVectorLength();

      for (unsigned i = 0; i != NumElts; ++i) {
        APValue &Elt = Result.Val.getVectorElt(i);
        if (Elt.isInt())
          Inits.push_back(llvm::ConstantInt::get(VMContext, Elt.getInt()));
        else
          Inits.push_back(llvm::ConstantFP::get(VMContext, Elt.getFloat()));
      }
      return llvm::ConstantVector::get(Inits);
    }
    }
  }

  llvm::Constant* C = ConstExprEmitter(*this, CGF).Visit(const_cast<Expr*>(E));
  if (C && C->getType()->isIntegerTy(1)) {
    llvm::Type *BoolTy = getTypes().ConvertTypeForMem(E->getType());
    C = llvm::ConstantExpr::getZExt(C, BoolTy);
  }
  return C;
}

static void
FillInNullDataMemberPointers(CodeGenModule &CGM, Type T,
                             std::vector<llvm::Constant *> &Elements,
                             uint64_t StartOffset) {
  assert(StartOffset % 8 == 0 && "StartOffset not byte aligned!");

  if (CGM.getTypes().isZeroInitializable(T))
    return;

//  if (const ConstantArrayType *CAT =
//        CGM.getContext().getAsConstantArrayType(T)) {
//    Type ElementTy = CAT->getElementType();
//    uint64_t ElementSize = CGM.getContext().getTypeSize(ElementTy);
//
//    for (uint64_t I = 0, E = CAT->getSize().getZExtValue(); I != E; ++I) {
//      FillInNullDataMemberPointers(CGM, ElementTy, Elements,
//                                   StartOffset + I * ElementSize);
//    }
/*  } else*/ if (const HeteroContainerType *RT = T->getAs<HeteroContainerType>()) {
    const UserClassDefn *RD = cast<UserClassDefn>(RT->getDefn());
    const ASTRecordLayout &Layout = CGM.getContext().getASTRecordLayout(RD);

    // Go through all bases and fill in any null pointer to data members.
//    for (UserClassDefn::base_class_const_iterator I = RD->bases_begin(),
//         E = RD->bases_end(); I != E; ++I) {
//      if (I->isVirtual()) {
//        // Ignore virtual bases.
//        continue;
//      }
//
//      const UserClassDefn *BaseDecl =
//      cast<UserClassDefn>(I->getType()->getAs<HeteroContainerType>()->getDefn());
//
////      // Ignore empty bases.
////      if (BaseDecl->isEmpty())
////        continue;
//
//      // Ignore bases that don't have any pointer to data members.
//      if (CGM.getTypes().isZeroInitializable(BaseDecl))
//        continue;
//
//      uint64_t BaseOffset = Layout.getBaseClassOffsetInBits(BaseDecl);
//      FillInNullDataMemberPointers(CGM, I->getType(),
//                                   Elements, StartOffset + BaseOffset);
//    }
    
    // Visit all fields.
    unsigned FieldNo = 0;
    for (TypeDefn::member_iterator I = RD->member_begin(),
         E = RD->member_end(); I != E; ++I, ++FieldNo) {
      Type FieldType = I->getType();
      
      if (CGM.getTypes().isZeroInitializable(FieldType))
        continue;

      uint64_t FieldOffset = StartOffset + Layout.getFieldOffset(FieldNo);
      FillInNullDataMemberPointers(CGM, FieldType, Elements, FieldOffset);
    }
  } else {
//    assert(T->isMemberPointerType() && "Should only see member pointers here!");
//    assert(!T->getAs<MemberPointerType>()->getPointeeType()->isFunctionType() &&
//           "Should only see pointers to data members here!");
  
    uint64_t StartIndex = StartOffset / 8;
    uint64_t EndIndex = StartIndex + CGM.getContext().getTypeSize(T) / 8;

    llvm::Constant *NegativeOne =
      llvm::ConstantInt::get(llvm::Type::getInt8Ty(CGM.getLLVMContext()),
                             -1ULL, /*isSigned=*/true);

    // Fill in the null data member pointer.
    for (uint64_t I = StartIndex; I != EndIndex; ++I)
      Elements[I] = NegativeOne;
  }
}

static llvm::Constant *EmitNullConstant(CodeGenModule &CGM,
                                        const UserClassDefn *RD) {
  Type T = CGM.getContext().getTypeDefnType(RD);

  llvm::StructType *STy =
    cast<llvm::StructType>(CGM.getTypes().ConvertTypeForMem(T));
  unsigned NumElements = STy->getNumElements();
  std::vector<llvm::Constant *> Elements(NumElements);

  const CGRecordLayout &Layout = CGM.getTypes().getCGRecordLayout(RD);

  for (UserClassDefn::base_class_const_iterator I = RD->bases_begin(),
       E = RD->bases_end(); I != E; ++I) {
    if (I->isVirtual()) {
      // Ignore virtual bases.
      continue;
    }

    const UserClassDefn *BaseDefn =
      cast<UserClassDefn>(I->getType()->getAs<HeteroContainerType>()->getDefn());

    // Ignore empty bases.
//    if (BaseDefn->isEmpty())
//      continue;
    
    // Ignore bases that don't have any pointer to data members.
    if (CGM.getTypes().isZeroInitializable(BaseDefn))
      continue;

    unsigned BaseFieldNo = Layout.getNonVirtualBaseLLVMFieldNo(BaseDefn);
    llvm::Type *BaseTy = STy->getElementType(BaseFieldNo);

    if (isa<llvm::StructType>(BaseTy)) {
      // We can just emit the base as a null constant.
      Elements[BaseFieldNo] = EmitNullConstant(CGM, BaseDefn);
      continue;
    }

    // Some bases are represented as arrays of i8 if the size of the
    // base is smaller than its corresponding LLVM type.
    // Figure out how many elements this base array has.
    llvm::ArrayType *BaseArrayTy =  cast<llvm::ArrayType>(BaseTy);
    unsigned NumBaseElements = BaseArrayTy->getNumElements();

    // Fill in null data member pointers.
    std::vector<llvm::Constant *> BaseElements(NumBaseElements);
    FillInNullDataMemberPointers(CGM, I->getType(), BaseElements, 0);

    // Now go through all other elements and zero them out.
    if (NumBaseElements) {
      llvm::Type* Int8Ty = llvm::Type::getInt8Ty(CGM.getLLVMContext());
      llvm::Constant *Zero = llvm::Constant::getNullValue(Int8Ty);
      for (unsigned I = 0; I != NumBaseElements; ++I) {
        if (!BaseElements[I])
          BaseElements[I] = Zero;
      }
    }
      
    Elements[BaseFieldNo] = llvm::ConstantArray::get(BaseArrayTy, BaseElements);
  }

  // Visit all fields.
  for (TypeDefn::member_iterator I = RD->member_begin(), E = RD->member_end();
       I != E; ++I) {
    const MemberDefn *FD = *I;
    
    unsigned FieldNo = Layout.getLLVMFieldNo(FD);
    Elements[FieldNo] = CGM.EmitNullConstant(FD->getType());
  }

  // Now go through all other fields and zero them out.
  for (unsigned i = 0; i != NumElements; ++i) {
    if (!Elements[i])
      Elements[i] = llvm::Constant::getNullValue(STy->getElementType(i));
  }
  
  return llvm::ConstantStruct::get(STy, Elements);
}

llvm::Constant *CodeGenModule::EmitNullConstant(Type T) {
  if (getTypes().isZeroInitializable(T))
    return llvm::Constant::getNullValue(getTypes().ConvertTypeForMem(T));
    
//  if (const ConstantArrayType *CAT = Context.getAsConstantArrayType(T)) {
//
//    Type ElementTy = CAT->getElementType();
//
//    llvm::Constant *Element = EmitNullConstant(ElementTy);
//    unsigned NumElements = CAT->getSize().getZExtValue();
//    std::vector<llvm::Constant *> Array(NumElements);
//    for (unsigned i = 0; i != NumElements; ++i)
//      Array[i] = Element;
//
//    const llvm::ArrayType *ATy =
//      cast<llvm::ArrayType>(getTypes().ConvertTypeForMem(T));
//    return llvm::ConstantArray::get(ATy, Array);
//  }

  if (const HeteroContainerType *RT = T->getAs<HeteroContainerType>()) {
    const UserClassDefn *RD = cast<UserClassDefn>(RT->getDefn());
    return ::EmitNullConstant(*this, RD);
  }

//  assert(T->isMemberPointerType() && "Should only see member pointers here!");
//  assert(!T->getAs<MemberPointerType>()->getPointeeType()->isFunctionType() &&
//         "Should only see pointers to data members here!");
  
  // Itanium C++ ABI 2.3:
  //   A NULL pointer is represented as -1.
  return llvm::ConstantInt::get(getTypes().ConvertTypeForMem(T), -1ULL, 
                                /*isSigned=*/true);
}
