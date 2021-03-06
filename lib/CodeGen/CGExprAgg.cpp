//===--- CGExprAgg.cpp - Emit LLVM Code from Aggregate Expressions --------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Aggregate Expr nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/StmtVisitor.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Intrinsics.h"
using namespace mlang;
using namespace CodeGen;

//===----------------------------------------------------------------------===//
//                        Aggregate Expression Emitter
//===----------------------------------------------------------------------===//

namespace  {
class AggExprEmitter : public StmtVisitor<AggExprEmitter> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  AggValueSlot Dest;
  bool IgnoreResult;

  ReturnValueSlot getReturnValueSlot() const {
    // If the destination slot requires garbage collection, we can't
    // use the real return value slot, because we have to use the GC
    // API.
    if (Dest.requiresGCollection()) return ReturnValueSlot();

    return ReturnValueSlot(Dest.getAddr(), Dest.isVolatile());
  }

  AggValueSlot EnsureSlot(Type T) {
    if (!Dest.isIgnored()) return Dest;
    return CGF.CreateAggTemp(T, "agg.tmp.ensured");
  }

public:
  AggExprEmitter(CodeGenFunction &cgf, AggValueSlot Dest,
                 bool ignore)
    : CGF(cgf), Builder(CGF.Builder), Dest(Dest),
      IgnoreResult(ignore) {
  }

  //===--------------------------------------------------------------------===//
  //                               Utilities
  //===--------------------------------------------------------------------===//

  /// EmitAggLoadOfLValue - Given an expression with aggregate type that
  /// represents a value lvalue, this method emits the address of the lvalue,
  /// then loads the result into DestPtr.
  void EmitAggLoadOfLValue(const Expr *E);

  /// EmitFinalDestCopy - Perform the final copy to DestPtr, if desired.
  void EmitFinalDestCopy(const Expr *E, LValue Src, bool Ignore = false);
  void EmitFinalDestCopy(const Expr *E, RValue Src, bool Ignore = false);

  void EmitGCMove(const Expr *E, RValue Src);

  bool TypeRequiresGCollection(Type T);

  //===--------------------------------------------------------------------===//
  //                            Visitor Methods
  //===--------------------------------------------------------------------===//

  void VisitStmt(Stmt *S) {
    CGF.ErrorUnsupported(S, "aggregate expression");
  }
  void VisitParenExpr(ParenExpr *PE) { Visit(PE->getSubExpr()); }
  void VisitUnaryExtension(UnaryOperator *E) { Visit(E->getSubExpr()); }

  // l-values.
  void VisitDefnRefExpr(DefnRefExpr *DRE) { EmitAggLoadOfLValue(DRE); }
  void VisitMemberExpr(MemberExpr *ME) { EmitAggLoadOfLValue(ME); }
  void VisitUnaryDeref(UnaryOperator *E) { EmitAggLoadOfLValue(E); }
  void VisitStringLiteral(StringLiteral *E) { EmitAggLoadOfLValue(E); }
  void VisitImaginaryLiteral(ImaginaryLiteral *E) {
    EmitAggLoadOfLValue(E);
  }
  void VisitArrayIndex(ArrayIndex *E) {
    EmitAggLoadOfLValue(E);
  }
  void VisitBlockDeclRefExpr(const DefnRefExpr *E) {
    EmitAggLoadOfLValue(E);
  }

  // Operators.
  void VisitFunctionCall(const FunctionCall *E);
  // void VisitStmtExpr(const StmtExpr *E);
  void VisitBinaryOperator(const BinaryOperator *BO);
//  void VisitPointerToDataMemberBinaryOperator(const BinaryOperator *BO);
  void VisitBinAssign(const BinaryOperator *E);
  void VisitBinComma(const BinaryOperator *E);


  // void VisitClassPropertyRefExpr(ObjCPropertyRefExpr *E);

  void EmitInitializationToLValue(Expr *E, LValue Address, Type T);
  void EmitNullInitializationToLValue(LValue Address, Type T);
};
}  // end anonymous namespace.

//===----------------------------------------------------------------------===//
//                                Utilities
//===----------------------------------------------------------------------===//

/// EmitAggLoadOfLValue - Given an expression with aggregate type that
/// represents a value lvalue, this method emits the address of the lvalue,
/// then loads the result into DestPtr.
void AggExprEmitter::EmitAggLoadOfLValue(const Expr *E) {
  LValue LV = CGF.EmitLValue(E);
  EmitFinalDestCopy(E, LV);
}

/// \brief True if the given aggregate type requires special GC API calls.
bool AggExprEmitter::TypeRequiresGCollection(Type T) {
//  // Only record types have members that might require garbage collection.
//  const HeteroContainerType *RecordTy = T->getAs<HeteroContainerType>();
//  if (!RecordTy) return false;
//
//  // Don't mess with non-trivial C++ types.
//  TypeDefn *Record = RecordTy->getDefn();
//  if (isa<UserClassDefn>(Record) &&
//      (!cast<UserClassDefn>(Record)->hasTrivialCopyConstructor() ||
//       !cast<UserClassDefn>(Record)->hasTrivialDestructor()))
//    return false;
//
//  // Check whether the type has an object member.
//  return Record->hasObjectMember();
	return false;
}

/// \brief Perform the final move to DestPtr if RequiresGCollection is set.
///
/// The idea is that you do something like this:
///   RValue Result = EmitSomething(..., getReturnValueSlot());
///   EmitGCMove(E, Result);
/// If GC doesn't interfere, this will cause the result to be emitted
/// directly into the return value slot.  If GC does interfere, a final
/// move will be performed.
void AggExprEmitter::EmitGCMove(const Expr *E, RValue Src) {
  if (Dest.requiresGCollection()) {
    std::pair<uint64_t, unsigned> TypeInfo = 
      CGF.getContext().getTypeSizeInfo(E->getType());
    unsigned long size = TypeInfo.first/8;
    llvm::Type *SizeTy = CGF.ConvertType(CGF.getContext()./*getSizeType()*/Int32Ty);
    llvm::Value *SizeVal = llvm::ConstantInt::get(SizeTy, size);
  }
}

/// EmitFinalDestCopy - Perform the final copy to DestPtr, if desired.
void AggExprEmitter::EmitFinalDestCopy(const Expr *E, RValue Src, bool Ignore) {
  assert(Src.isAggregate() && "value must be aggregate value!");

  // If Dest is ignored, then we're evaluating an aggregate expression
  // in a context (like an expression statement) that doesn't care
  // about the result.  C says that an lvalue-to-rvalue conversion is
  // performed in these cases; C++ says that it is not.  In either
  // case, we don't actually need to do anything unless the value is
  // volatile.
  if (Dest.isIgnored()) {
    if (!Src.isVolatileQualified() ||
        CGF.CGM.getLangOptions().OOP ||
        (IgnoreResult && Ignore))
      return;

    // If the source is volatile, we must read from it; to do that, we need
    // some place to put it.
    Dest = CGF.CreateAggTemp(E->getType(), "agg.tmp");
  }

  if (Dest.requiresGCollection()) {
    std::pair<uint64_t, unsigned> TypeInfo = 
    CGF.getContext().getTypeSizeInfo(E->getType());
    unsigned long size = TypeInfo.first/8;
    llvm::Type *SizeTy = CGF.ConvertType(CGF.getContext()./*getSizeType()*/Int32Ty);
    llvm::Value *SizeVal = llvm::ConstantInt::get(SizeTy, size);
    return;
  }
  // If the result of the assignment is used, copy the LHS there also.
  // FIXME: Pass VolatileDest as well.  I think we also need to merge volatile
  // from the source as well, as we can't eliminate it if either operand
  // is volatile, unless copy has volatile for both source and destination..
  CGF.EmitAggregateCopy(Dest.getAddr(), Src.getAggregateAddr(), E->getType(),
                        Dest.isVolatile()|Src.isVolatileQualified());
}

/// EmitFinalDestCopy - Perform the final copy to DestPtr, if desired.
void AggExprEmitter::EmitFinalDestCopy(const Expr *E, LValue Src, bool Ignore) {
  assert(Src.isSimple() && "Can't have aggregate bitfield, vector, etc");

  EmitFinalDestCopy(E, RValue::getAggregate(Src.getAddress(),
                                            Src.isVolatileQualified()),
                    Ignore);
}

//===----------------------------------------------------------------------===//
//                            Visitor Methods
//===----------------------------------------------------------------------===//

//void AggExprEmitter::VisitCastExpr(CastExpr *E) {
//  if (Dest.isIgnored() && E->getCastKind() != CK_Dynamic) {
//    Visit(E->getSubExpr());
//    return;
//  }
//
//  switch (E->getCastKind()) {
//  case CK_Dynamic: {
//    assert(isa<CXXDynamicCastExpr>(E) && "CK_Dynamic without a dynamic_cast?");
//    LValue LV = CGF.EmitCheckedLValue(E->getSubExpr());
//    // FIXME: Do we also need to handle property references here?
//    if (LV.isSimple())
//      CGF.EmitDynamicCast(LV.getAddress(), cast<CXXDynamicCastExpr>(E));
//    else
//      CGF.CGM.ErrorUnsupported(E, "non-simple lvalue dynamic_cast");
//
//    if (!Dest.isIgnored())
//      CGF.CGM.ErrorUnsupported(E, "lvalue dynamic_cast with a destination");
//    break;
//  }
//
//  case CK_ToUnion: {
//    // GCC union extension
//    Type Ty = E->getSubExpr()->getType();
//    Type PtrTy = CGF.getContext().getPointerType(Ty);
//    llvm::Value *CastPtr = Builder.CreateBitCast(Dest.getAddr(),
//                                                 CGF.ConvertType(PtrTy));
//    EmitInitializationToLValue(E->getSubExpr(), CGF.MakeAddrLValue(CastPtr, Ty),
//                               Ty);
//    break;
//  }
//
//  case CK_DerivedToBase:
//  case CK_BaseToDerived:
//  case CK_UncheckedDerivedToBase: {
//    assert(0 && "cannot perform hierarchy conversion in EmitAggExpr: "
//                "should have been unpacked before we got here");
//    break;
//  }
//
//  case CK_GetObjCProperty: {
//    LValue LV = CGF.EmitLValue(E->getSubExpr());
//    assert(LV.isPropertyRef());
//    RValue RV = CGF.EmitLoadOfPropertyRefLValue(LV, getReturnValueSlot());
//    EmitGCMove(E, RV);
//    break;
//  }
//
//  case CK_LValueToRValue: // hope for downstream optimization
//  case CK_NoOp:
//  case CK_UserDefinedConversion:
//  case CK_ConstructorConversion:
//    assert(CGF.getContext().hasSameUnqualifiedType(E->getSubExpr()->getType(),
//                                                   E->getType()) &&
//           "Implicit cast types must be compatible");
//    Visit(E->getSubExpr());
//    break;
//
//  case CK_LValueBitCast:
//    llvm_unreachable("should not be emitting lvalue bitcast as rvalue");
//    break;
//
//  case CK_Dependent:
//  case CK_BitCast:
//  case CK_ArrayToPointerDecay:
//  case CK_FunctionToPointerDecay:
//  case CK_NullToPointer:
//  case CK_NullToMemberPointer:
//  case CK_BaseToDerivedMemberPointer:
//  case CK_DerivedToBaseMemberPointer:
//  case CK_MemberPointerToBoolean:
//  case CK_IntegralToPointer:
//  case CK_PointerToIntegral:
//  case CK_PointerToBoolean:
//  case CK_ToVoid:
//  case CK_VectorSplat:
//  case CK_IntegralCast:
//  case CK_IntegralToBoolean:
//  case CK_IntegralToFloating:
//  case CK_FloatingToIntegral:
//  case CK_FloatingToBoolean:
//  case CK_FloatingCast:
//  case CK_AnyPointerToObjCPointerCast:
//  case CK_AnyPointerToBlockPointerCast:
//  case CK_ObjCObjectLValueCast:
//  case CK_FloatingRealToComplex:
//  case CK_FloatingComplexToReal:
//  case CK_FloatingComplexToBoolean:
//  case CK_FloatingComplexCast:
//  case CK_FloatingComplexToIntegralComplex:
//  case CK_IntegralRealToComplex:
//  case CK_IntegralComplexToReal:
//  case CK_IntegralComplexToBoolean:
//  case CK_IntegralComplexCast:
//  case CK_IntegralComplexToFloatingComplex:
//    llvm_unreachable("cast kind invalid for aggregate types");
//  }
//}

void AggExprEmitter::VisitFunctionCall(const FunctionCall *E) {
  if (E->getCallReturnType()->isReferenceType()) {
    EmitAggLoadOfLValue(E);
    return;
  }

  RValue RV = CGF.EmitFunctionCall(E, getReturnValueSlot());
  EmitGCMove(E, RV);
}

//void AggExprEmitter::VisitClassPropertyRefExpr(ObjCPropertyRefExpr *E) {
//  llvm_unreachable("direct property access not surrounded by "
//                   "lvalue-to-rvalue cast");
//}

void AggExprEmitter::VisitBinComma(const BinaryOperator *E) {
  CGF.EmitIgnoredExpr(E->getLHS());
  Visit(E->getRHS());
}

void AggExprEmitter::VisitBinaryOperator(const BinaryOperator *E) {
	CGF.ErrorUnsupported(E, "aggregate binary expression");
}

void AggExprEmitter::VisitBinAssign(const BinaryOperator *E) {
  // For an assignment to work, the value on the right has
  // to be compatible with the value on the left.
//  assert(CGF.getContext().hasSameUnqualifiedType(E->getLHS()->getType(),
//                                                 E->getRHS()->getType())
//         && "Invalid assignment");

  // FIXME:  __block variables need the RHS evaluated first!
  LValue LHS = CGF.EmitLValue(E->getLHS());

  // We have to special case property setters, otherwise we must have
  // a simple lvalue (no aggregates inside vectors, bitfields).
  if (LHS.isPropertyRef()) {
    AggValueSlot Slot = EnsureSlot(E->getRHS()->getType());
    CGF.EmitAggExpr(E->getRHS(), Slot);
//    CGF.EmitStoreThroughPropertyRefLValue(Slot.asRValue(), LHS);
  } else {
    bool GCollection = false;
    if (CGF.getContext().getLangOptions().getGCMode())
      GCollection = TypeRequiresGCollection(E->getLHS()->getType());

    // Codegen the RHS so that it stores directly into the LHS.
    AggValueSlot LHSSlot = AggValueSlot::forLValue(LHS, true, 
                                                   GCollection);
    CGF.EmitAggExpr(E->getRHS(), LHSSlot, false);
    EmitFinalDestCopy(E, LHS, true);
  }
}



//void
//AggExprEmitter::VisitCXXConstructExpr(const CXXConstructExpr *E) {
//  AggValueSlot Slot = EnsureSlot(E->getType());
//  CGF.EmitCXXConstructExpr(E, Slot);
//}

//void AggExprEmitter::VisitExprWithCleanups(ExprWithCleanups *E) {
//  CGF.EmitExprWithCleanups(E, Dest);
//}

//void AggExprEmitter::VisitCXXScalarValueInitExpr(CXXScalarValueInitExpr *E) {
//  Type T = E->getType();
//  AggValueSlot Slot = EnsureSlot(T);
//  EmitNullInitializationToLValue(CGF.MakeAddrLValue(Slot.getAddr(), T), T);
//}
//
//void AggExprEmitter::VisitImplicitValueInitExpr(ImplicitValueInitExpr *E) {
//  Type T = E->getType();
//  AggValueSlot Slot = EnsureSlot(T);
//  EmitNullInitializationToLValue(CGF.MakeAddrLValue(Slot.getAddr(), T), T);
//}

/// isSimpleZero - If emitting this value will obviously just cause a store of
/// zero to memory, return true.  This can return false if uncertain, so it just
/// handles simple cases.
static bool isSimpleZero(const Expr *E, CodeGenFunction &CGF) {
  // (0)
  if (const ParenExpr *PE = dyn_cast<ParenExpr>(E))
    return isSimpleZero(PE->getSubExpr(), CGF);
  // 0
  if (const IntegerLiteral *IL = dyn_cast<IntegerLiteral>(E))
    return IL->getValue() == 0;
  // +0.0
  if (const FloatingLiteral *FL = dyn_cast<FloatingLiteral>(E))
    return FL->getValue().isPosZero();
//  // int()
//  if ((isa<ImplicitValueInitExpr>(E) || isa<CXXScalarValueInitExpr>(E)) &&
//      CGF.getTypes().isZeroInitializable(E->getType()))
//    return true;
  // (int*)0 - Null pointer expressions.
//  if (const CastExpr *ICE = dyn_cast<CastExpr>(E))
//    return ICE->getCastKind() == CK_NullToPointer;
  // '\0'
  if (const CharacterLiteral *CL = dyn_cast<CharacterLiteral>(E))
    return CL->getValue() == 0;
  
  // Otherwise, hard case: conservatively return false.
  return false;
}


void 
AggExprEmitter::EmitInitializationToLValue(Expr* E, LValue LV, Type T) {
  // FIXME: Ignore result?
  // FIXME: Are initializers affected by volatile?
  if (Dest.isZeroed() && isSimpleZero(E, CGF)) {
    // Storing "i32 0" to a zero'd memory location is a noop.
  } /*else if (isa<ImplicitValueInitExpr>(E)) {
    EmitNullInitializationToLValue(LV, T);
  }*/ else if (T->isReferenceType()) {
//    RValue RV = CGF.EmitReferenceBindingToExpr(E, /*InitializedDecl=*/0);
//    CGF.EmitStoreThroughLValue(RV, LV);
  } else if (T.hasComplexAttr()) {
    CGF.EmitComplexExprIntoAddr(E, LV.getAddress(), false);
  } else if (CGF.hasAggregateLLVMType(T)) {
    CGF.EmitAggExpr(E, AggValueSlot::forAddr(LV.getAddress(), false, true,
                                             false, Dest.isZeroed()));
  } else {
    CGF.EmitStoreThroughLValue(RValue::get(CGF.EmitScalarExpr(E)), LV);
  }
}

void AggExprEmitter::EmitNullInitializationToLValue(LValue LV, Type T) {
  // If the destination slot is already zeroed out before the aggregate is
  // copied into it, we don't have to emit any zeros here.
  if (Dest.isZeroed() && CGF.getTypes().isZeroInitializable(T))
    return;
  
  if (!CGF.hasAggregateLLVMType(T)) {
    // For non-aggregates, we can store zero
    llvm::Value *Null = llvm::Constant::getNullValue(CGF.ConvertType(T));
    CGF.EmitStoreThroughLValue(RValue::get(Null), LV);
  } else {
    // There's a potential optimization opportunity in combining
    // memsets; that would be easy for arrays, but relatively
    // difficult for structures with the current code.
    CGF.EmitNullInitialization(LV.getAddress(), T);
  }
}

//void AggExprEmitter::VisitInitListExpr(InitListExpr *E) {
//#if 0
//  // FIXME: Assess perf here?  Figure out what cases are worth optimizing here
//  // (Length of globals? Chunks of zeroed-out space?).
//  //
//  // If we can, prefer a copy from a global; this is a lot less code for long
//  // globals, and it's easier for the current optimizers to analyze.
//  if (llvm::Constant* C = CGF.CGM.EmitConstantExpr(E, E->getType(), &CGF)) {
//    llvm::GlobalVariable* GV =
//    new llvm::GlobalVariable(CGF.CGM.getModule(), C->getType(), true,
//                             llvm::GlobalValue::InternalLinkage, C, "");
//    EmitFinalDestCopy(E, CGF.MakeAddrLValue(GV, E->getType()));
//    return;
//  }
//#endif
//  if (E->hadArrayRangeDesignator())
//    CGF.ErrorUnsupported(E, "GNU array range designator extension");
//
//  llvm::Value *DestPtr = Dest.getAddr();
//
//  // Handle initialization of an array.
//  if (E->getType()->isArrayType()) {
//    const llvm::PointerType *APType =
//      cast<llvm::PointerType>(DestPtr->getType());
//    const llvm::ArrayType *AType =
//      cast<llvm::ArrayType>(APType->getElementType());
//
//    uint64_t NumInitElements = E->getNumInits();
//
//    if (E->getNumInits() > 0) {
//      Type T1 = E->getType();
//      Type T2 = E->getInit(0)->getType();
//      if (CGF.getContext().hasSameUnqualifiedType(T1, T2)) {
//        EmitAggLoadOfLValue(E->getInit(0));
//        return;
//      }
//    }
//
//    uint64_t NumArrayElements = AType->getNumElements();
//    Type ElementType = CGF.getContext().getCanonicalType(E->getType());
//    ElementType = CGF.getContext().getAsArrayType(ElementType)->getElementType();
//
//    // FIXME: were we intentionally ignoring address spaces and GC attributes?
//
//    for (uint64_t i = 0; i != NumArrayElements; ++i) {
//      // If we're done emitting initializers and the destination is known-zeroed
//      // then we're done.
//      if (i == NumInitElements &&
//          Dest.isZeroed() &&
//          CGF.getTypes().isZeroInitializable(ElementType))
//        break;
//
//      llvm::Value *NextVal = Builder.CreateStructGEP(DestPtr, i, ".array");
//      LValue LV = CGF.MakeAddrLValue(NextVal, ElementType);
//
//      if (i < NumInitElements)
//        EmitInitializationToLValue(E->getInit(i), LV, ElementType);
//      else
//        EmitNullInitializationToLValue(LV, ElementType);
//
//      // If the GEP didn't get used because of a dead zero init or something
//      // else, clean it up for -O0 builds and general tidiness.
//      if (llvm::GetElementPtrInst *GEP =
//            dyn_cast<llvm::GetElementPtrInst>(NextVal))
//        if (GEP->use_empty())
//          GEP->eraseFromParent();
//    }
//    return;
//  }
//
//  assert(E->getType()->isRecordType() && "Only support structs/unions here!");
//
//  // Do struct initialization; this code just sets each individual member
//  // to the approprate value.  This makes bitfield support automatic;
//  // the disadvantage is that the generated code is more difficult for
//  // the optimizer, especially with bitfields.
//  unsigned NumInitElements = E->getNumInits();
//  TypeDefn *SD = E->getType()->getAs<HeteroContainerType>()->getDecl();
//
//  if (E->getType()->isUnionType()) {
//    // Only initialize one field of a union. The field itself is
//    // specified by the initializer list.
//    if (!E->getInitializedFieldInUnion()) {
//      // Empty union; we have nothing to do.
//
//#ifndef NDEBUG
//      // Make sure that it's really an empty and not a failure of
//      // semantic analysis.
//      for (TypeDefn::field_iterator Field = SD->field_begin(),
//                                   FieldEnd = SD->field_end();
//           Field != FieldEnd; ++Field)
//        assert(Field->isUnnamedBitfield() && "Only unnamed bitfields allowed");
//#endif
//      return;
//    }
//
//    // FIXME: volatility
//    MemberDefn *Field = E->getInitializedFieldInUnion();
//
//    LValue FieldLoc = CGF.EmitLValueForFieldInitialization(DestPtr, Field, 0);
//    if (NumInitElements) {
//      // Store the initializer into the field
//      EmitInitializationToLValue(E->getInit(0), FieldLoc, Field->getType());
//    } else {
//      // Default-initialize to null.
//      EmitNullInitializationToLValue(FieldLoc, Field->getType());
//    }
//
//    return;
//  }
//
//  // Here we iterate over the fields; this makes it simpler to both
//  // default-initialize fields and skip over unnamed fields.
//  unsigned CurInitVal = 0;
//  for (TypeDefn::field_iterator Field = SD->field_begin(),
//                               FieldEnd = SD->field_end();
//       Field != FieldEnd; ++Field) {
//    // We're done once we hit the flexible array member
//    if (Field->getType()->isIncompleteArrayType())
//      break;
//
//    if (Field->isUnnamedBitfield())
//      continue;
//
//    // Don't emit GEP before a noop store of zero.
//    if (CurInitVal == NumInitElements && Dest.isZeroed() &&
//        CGF.getTypes().isZeroInitializable(E->getType()))
//      break;
//
//    // FIXME: volatility
//    LValue FieldLoc = CGF.EmitLValueForFieldInitialization(DestPtr, *Field, 0);
//    // We never generate write-barries for initialized fields.
//    FieldLoc.setNonGC(true);
//
//    if (CurInitVal < NumInitElements) {
//      // Store the initializer into the field.
//      EmitInitializationToLValue(E->getInit(CurInitVal++), FieldLoc,
//                                 Field->getType());
//    } else {
//      // We're out of initalizers; default-initialize to null
//      EmitNullInitializationToLValue(FieldLoc, Field->getType());
//    }
//
//    // If the GEP didn't get used because of a dead zero init or something
//    // else, clean it up for -O0 builds and general tidiness.
//    if (FieldLoc.isSimple())
//      if (llvm::GetElementPtrInst *GEP =
//            dyn_cast<llvm::GetElementPtrInst>(FieldLoc.getAddress()))
//        if (GEP->use_empty())
//          GEP->eraseFromParent();
//  }
//}

//===----------------------------------------------------------------------===//
//                        Entry Points into this File
//===----------------------------------------------------------------------===//

/// GetNumNonZeroBytesInInit - Get an approximate count of the number of
/// non-zero bytes that will be stored when outputting the initializer for the
/// specified initializer expression.
static uint64_t GetNumNonZeroBytesInInit(const Expr *E, CodeGenFunction &CGF) {
  if (const ParenExpr *PE = dyn_cast<ParenExpr>(E))
    return GetNumNonZeroBytesInInit(PE->getSubExpr(), CGF);

  // 0 and 0.0 won't require any non-zero stores!
  if (isSimpleZero(E, CGF)) return 0;

  // If this is an initlist expr, sum up the size of sizes of the (present)
  // elements.  If this is something weird, assume the whole thing is non-zero.
//  const InitListExpr *ILE = dyn_cast<InitListExpr>(E);
//  if (ILE == 0 || !CGF.getTypes().isZeroInitializable(ILE->getType()))
//    return CGF.getContext().getTypeSize(E->getType())/8;
//
//  // InitListExprs for structs have to be handled carefully.  If there are
//  // reference members, we need to consider the size of the reference, not the
//  // referencee.  InitListExprs for unions and arrays can't have references.
//  if (const HeteroContainerType *RT = E->getType()->getAs<HeteroContainerType>()) {
//    if (!RT->isUnionType()) {
//      TypeDefn *SD = E->getType()->getAs<HeteroContainerType>()->getDecl();
//      uint64_t NumNonZeroBytes = 0;
//
//      unsigned ILEElement = 0;
//      for (TypeDefn::field_iterator Field = SD->field_begin(),
//           FieldEnd = SD->field_end(); Field != FieldEnd; ++Field) {
//        // We're done once we hit the flexible array member or run out of
//        // InitListExpr elements.
//        if (Field->getType()->isIncompleteArrayType() ||
//            ILEElement == ILE->getNumInits())
//          break;
//        if (Field->isUnnamedBitfield())
//          continue;
//
//        const Expr *E = ILE->getInit(ILEElement++);
//
//        // Reference values are always non-null and have the width of a pointer.
//        if (Field->getType()->isReferenceType())
//          NumNonZeroBytes += CGF.getContext().Target.getPointerWidth(0);
//        else
//          NumNonZeroBytes += GetNumNonZeroBytesInInit(E, CGF);
//      }
//
//      return NumNonZeroBytes;
//    }
//  }
//
//
//  uint64_t NumNonZeroBytes = 0;
//  for (unsigned i = 0, e = ILE->getNumInits(); i != e; ++i)
//    NumNonZeroBytes += GetNumNonZeroBytesInInit(ILE->getInit(i), CGF);
//  return NumNonZeroBytes;
  
  return 0;
}

/// CheckAggExprForMemSetUse - If the initializer is large and has a lot of
/// zeros in it, emit a memset and avoid storing the individual zeros.
///
static void CheckAggExprForMemSetUse(AggValueSlot &Slot, const Expr *E,
                                     CodeGenFunction &CGF) {
  // If the slot is already known to be zeroed, nothing to do.  Don't mess with
  // volatile stores.
  if (Slot.isZeroed() || Slot.isVolatile() || Slot.getAddr() == 0) return;
  
  // If the type is 16-bytes or smaller, prefer individual stores over memset.
  std::pair<uint64_t, unsigned> TypeInfo =
    CGF.getContext().getTypeSizeInfo(E->getType());
  if (TypeInfo.first/8 <= 16)
    return;

  // Check to see if over 3/4 of the initializer are known to be zero.  If so,
  // we prefer to emit memset + individual stores for the rest.
  uint64_t NumNonZeroBytes = GetNumNonZeroBytesInInit(E, CGF);
  if (NumNonZeroBytes*4 > TypeInfo.first/8)
    return;
  
  // Okay, it seems like a good idea to use an initial memset, emit the call.
  llvm::Constant *SizeVal = CGF.Builder.getInt64(TypeInfo.first/8);
  unsigned Align = TypeInfo.second/8;

  llvm::Value *Loc = Slot.getAddr();
  llvm::Type *BP = llvm::Type::getInt8PtrTy(CGF.getLLVMContext());
  
  Loc = CGF.Builder.CreateBitCast(Loc, BP);
  CGF.Builder.CreateMemSet(Loc, CGF.Builder.getInt8(0), SizeVal, Align, false);
  
  // Tell the AggExprEmitter that the slot is known zero.
  Slot.setZeroed();
}




/// EmitAggExpr - Emit the computation of the specified expression of aggregate
/// type.  The result is computed into DestPtr.  Note that if DestPtr is null,
/// the value of the aggregate expression is not needed.  If VolatileDest is
/// true, DestPtr cannot be 0.
///
/// \param IsInitializer - true if this evaluation is initializing an
/// object whose lifetime is already being managed.
//
// FIXME: Take Qualifiers object.
void CodeGenFunction::EmitAggExpr(const Expr *E, AggValueSlot Slot,
                                  bool IgnoreResult) {
  assert(E && hasAggregateLLVMType(E->getType()) &&
         "Invalid aggregate expression to emit");
  assert((Slot.getAddr() != 0 || Slot.isIgnored()) &&
         "slot has bits but no address");

  // Optimize the slot if possible.
  CheckAggExprForMemSetUse(Slot, E, *this);
 
  AggExprEmitter(*this, Slot, IgnoreResult).Visit(const_cast<Expr*>(E));
}

LValue CodeGenFunction::EmitAggExprToLValue(const Expr *E) {
  assert(hasAggregateLLVMType(E->getType()) && "Invalid argument!");
  llvm::Value *Temp = CreateMemTemp(E->getType());
  LValue LV = MakeAddrLValue(Temp, E->getType());
  EmitAggExpr(E, AggValueSlot::forAddr(Temp, LV.isVolatileQualified(), false));
  return LV;
}

void CodeGenFunction::EmitAggregateCopy(llvm::Value *DestPtr,
                                        llvm::Value *SrcPtr, Type Ty,
                                        bool isVolatile) {
#if 0
	assert(!Ty.hasComplexAttr() && "Shouldn't happen for complex");

  if (getContext().getLangOptions().OOP) {
    if (const HeteroContainerType *RT = Ty->getAs<HeteroContainerType>()) {
      UserClassDefn *Record = cast<UserClassDefn>(RT->getDecl());
      assert((Record->hasTrivialCopyConstructor() || 
              Record->hasTrivialCopyAssignment()) &&
             "Trying to aggregate-copy a type without a trivial copy "
             "constructor or assignment operator");
      // Ignore empty classes in C++.
      if (Record->isEmpty())
        return;
    }
  }
  
  // Aggregate assignment turns into llvm.memcpy.  This is almost valid per
  // C99 6.5.16.1p3, which states "If the value being stored in an object is
  // read from another object that overlaps in anyway the storage of the first
  // object, then the overlap shall be exact and the two objects shall have
  // qualified or unqualified versions of a compatible type."
  //
  // memcpy is not defined if the source and destination pointers are exactly
  // equal, but other compilers do this optimization, and almost every memcpy
  // implementation handles this case safely.  If there is a libc that does not
  // safely handle this, we can add a target hook.

  // Get size and alignment info for this aggregate.
  std::pair<uint64_t, unsigned> TypeInfo = getContext().getTypeInfo(Ty);

  // FIXME: Handle variable sized types.

  // FIXME: If we have a volatile struct, the optimizer can remove what might
  // appear to be `extra' memory ops:
  //
  // volatile struct { int i; } a, b;
  //
  // int main() {
  //   a = b;
  //   a = b;
  // }
  //
  // we need to use a different call here.  We use isVolatile to indicate when
  // either the source or the destination is volatile.

  const llvm::PointerType *DPT = cast<llvm::PointerType>(DestPtr->getType());
  llvm::Type *DBP =
    llvm::Type::getInt8PtrTy(VMContext, DPT->getAddressSpace());
  DestPtr = Builder.CreateBitCast(DestPtr, DBP, "tmp");

  const llvm::PointerType *SPT = cast<llvm::PointerType>(SrcPtr->getType());
  llvm::Type *SBP =
    llvm::Type::getInt8PtrTy(VMContext, SPT->getAddressSpace());
  SrcPtr = Builder.CreateBitCast(SrcPtr, SBP, "tmp");

  if (const HeteroContainerType *RecordTy = Ty->getAs<HeteroContainerType>()) {
    TypeDefn *Record = RecordTy->getDecl();
    if (Record->hasObjectMember()) {
      unsigned long size = TypeInfo.first/8;
      llvm::Type *SizeTy = ConvertType(getContext().getSizeType());
      llvm::Value *SizeVal = llvm::ConstantInt::get(SizeTy, size);
      CGM.getObjCRuntime().EmitGCMemmoveCollectable(*this, DestPtr, SrcPtr, 
                                                    SizeVal);
      return;
    }
  } else if (getContext().getAsArrayType(Ty)) {
    Type BaseType = getContext().getBaseElementType(Ty);
    if (const HeteroContainerType *RecordTy = BaseType->getAs<HeteroContainerType>()) {
      if (RecordTy->getDecl()->hasObjectMember()) {
        unsigned long size = TypeInfo.first/8;
        llvm::Type *SizeTy = ConvertType(getContext().getSizeType());
        llvm::Value *SizeVal = llvm::ConstantInt::get(SizeTy, size);
        CGM.getObjCRuntime().EmitGCMemmoveCollectable(*this, DestPtr, SrcPtr, 
                                                      SizeVal);
        return;
      }
    }
  }
  
  Builder.CreateMemCpy(DestPtr, SrcPtr,
                       llvm::ConstantInt::get(IntPtrTy, TypeInfo.first/8),
                       TypeInfo.second/8, isVolatile);
#endif
}
