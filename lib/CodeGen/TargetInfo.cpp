//===--- TargetInfo.cpp - Encapsulate target details ------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
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

#include "TargetInfo.h"
#include "ABIInfo.h"
#include "CodeGenFunction.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "llvm/Type.h"
#include "llvm/Target/TargetData.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/raw_ostream.h"
using namespace mlang;
using namespace CodeGen;

static void AssignToArrayRange(CodeGen::CGBuilderTy &Builder,
                               llvm::Value *Array,
                               llvm::Value *Value,
                               unsigned FirstIndex,
                               unsigned LastIndex) {
  // Alternatively, we could emit this as a loop in the source.
  for (unsigned I = FirstIndex; I <= LastIndex; ++I) {
    llvm::Value *Cell = Builder.CreateConstInBoundsGEP1_32(Array, I);
    Builder.CreateStore(Value, Cell);
  }
}

static bool isAggregateTypeForABI(Type T) {
  return CodeGenFunction::hasAggregateLLVMType(T) /*||
         T->isMemberFunctionPointerType()*/;
}

ABIInfo::~ABIInfo() {}

ASTContext &ABIInfo::getContext() const {
  return CGT.getContext();
}

llvm::LLVMContext &ABIInfo::getVMContext() const {
  return CGT.getLLVMContext();
}

const llvm::TargetData &ABIInfo::getTargetData() const {
  return CGT.getTargetData();
}

void ABIArgInfo::dump() const {
  llvm::raw_ostream &OS = llvm::errs();
  OS << "(ABIArgInfo Kind=";
  switch (TheKind) {
  case Direct:
    OS << "Direct Type=";
    if (llvm::Type *Ty = getCoerceToType())
      Ty->print(OS);
    else
      OS << "null";
    break;
  case Extend:
    OS << "Extend";
    break;
  case Ignore:
    OS << "Ignore";
    break;
  case Indirect:
    OS << "Indirect Align=" << getIndirectAlign()
       << " Byal=" << getIndirectByVal()
       << " Realign=" << getIndirectRealign();
    break;
  case Expand:
    OS << "Expand";
    break;
  }
  OS << ")\n";
}

TargetCodeGenInfo::~TargetCodeGenInfo() { delete Info; }

static bool isEmptyRecord(ASTContext &Context, Type T, bool AllowArrays);

/// isEmptyField - Return true iff a the field is "empty", that is it
/// is an unnamed bit-field or an (array of) empty record(s).
static bool isEmptyField(ASTContext &Context, const MemberDefn *FD,
                         bool AllowArrays) {
  Type FT = FD->getType();

    // Constant arrays of empty records count as empty, strip them off.
  const HeteroContainerType *RT = FT->getAs<HeteroContainerType>();
  if (!RT)
    return false;

  // C++ record fields are never empty, at least in the Itanium ABI.
  //
  // FIXME: We should use a predicate for whether this behavior is true in the
  // current ABI.
  if (isa<UserClassDefn>(RT->getDefn()))
    return false;

  return isEmptyRecord(Context, FT, AllowArrays);
}

/// isEmptyRecord - Return true iff a structure contains only empty
/// fields. Note that a structure with a flexible array member is not
/// considered empty.
static bool isEmptyRecord(ASTContext &Context, Type T, bool AllowArrays) {
  const HeteroContainerType *RT = T->getAs<HeteroContainerType>();
  if (!RT)
    return 0;
  const TypeDefn *RD = RT->getDefn();

  // If this is a C++ record, check the bases first.
  if (const UserClassDefn *CXXRD = dyn_cast<UserClassDefn>(RD))
    for (UserClassDefn::base_class_const_iterator i = CXXRD->bases_begin(),
           e = CXXRD->bases_end(); i != e; ++i)
      if (!isEmptyRecord(Context, i->getType(), true))
        return false;

  for (TypeDefn::member_iterator i = RD->member_begin(), e = RD->member_end();
         i != e; ++i)
    if (!isEmptyField(Context, *i, AllowArrays))
      return false;
  return true;
}

/// hasNonTrivialDestructorOrCopyConstructor - Determine if a type has either
/// a non-trivial destructor or a non-trivial copy constructor.
static bool hasNonTrivialDestructorOrCopyConstructor(const HeteroContainerType *RT) {
  const UserClassDefn *RD = dyn_cast<UserClassDefn>(RT->getDefn());
  if (!RD)
    return false;

  return /*!RD->hasTrivialDestructor() || !RD->hasTrivialCopyConstructor()*/false;
}

/// isRecordWithNonTrivialDestructorOrCopyConstructor - Determine if a type is
/// a record type with either a non-trivial destructor or a non-trivial copy
/// constructor.
static bool isRecordWithNonTrivialDestructorOrCopyConstructor(Type T) {
  const HeteroContainerType *RT = T->getAs<HeteroContainerType>();
  if (!RT)
    return false;

  return hasNonTrivialDestructorOrCopyConstructor(RT);
}

/// isSingleElementStruct - Determine if a structure is a "single
/// element struct", i.e. it has exactly one non-empty field or
/// exactly one field which is itself a single element
/// struct. Structures with flexible array members are never
/// considered single element structs.
///
/// \return The field declaration for the single non-empty field, if
/// it exists.
static const RawType *isSingleElementStruct(Type T, ASTContext &Context) {
  const HeteroContainerType *RT = T->getAsStructureType();
  if (!RT)
    return 0;

  const TypeDefn *RD = RT->getDefn();
//  if (RD->hasFlexibleArrayMember())
//    return 0;

  const RawType *Found = 0;

  // If this is a C++ record, check the bases first.
  if (const UserClassDefn *CXXRD = dyn_cast<UserClassDefn>(RD)) {
    for (UserClassDefn::base_class_const_iterator i = CXXRD->bases_begin(),
           e = CXXRD->bases_end(); i != e; ++i) {
      // Ignore empty records.
      if (isEmptyRecord(Context, i->getType(), true))
        continue;

      // If we already found an element then this isn't a single-element struct.
      if (Found)
        return 0;

      // If this is non-empty and not a single element struct, the composite
      // cannot be a single element struct.
      Found = isSingleElementStruct(i->getType(), Context);
      if (!Found)
        return 0;
    }
  }

  // Check for single element.
  for (TypeDefn::member_iterator i = RD->member_begin(), e = RD->member_end();
         i != e; ++i) {
    const MemberDefn *FD = *i;
    Type FT = FD->getType();

    // Ignore empty fields.
    if (isEmptyField(Context, FD, true))
      continue;

    // If we already found an element then this isn't a single-element
    // struct.
    if (Found)
      return 0;

    // Treat single element arrays as the element.
//    while (const ConstantArrayType *AT = Context.getAsConstantArrayType(FT)) {
//      if (AT->getSize().getZExtValue() != 1)
//        break;
//      FT = AT->getElementType();
//    }

    if (!isAggregateTypeForABI(FT)) {
      Found = FT.getRawTypePtr();
    } else {
      Found = isSingleElementStruct(FT, Context);
      if (!Found)
        return 0;
    }
  }

  return Found;
}

static bool is32Or64BitBasicType(Type Ty, ASTContext &Context) {
//  if (!Ty->getAs<SimpleNumericType>() && !Ty->hasPointerRepresentation() &&
//      !Ty->isAnyComplexType() && !Ty->isEnumeralType() &&
//      !Ty->isBlockPointerType())
//    return false;

  uint64_t Size = Context.getTypeSize(Ty);
  return Size == 32 || Size == 64;
}

/// canExpandIndirectArgument - Test whether an argument type which is to be
/// passed indirectly (on the stack) would have the equivalent layout if it was
/// expanded into separate arguments. If so, we prefer to do the latter to avoid
/// inhibiting optimizations.
///
// FIXME: This predicate is missing many cases, currently it just follows
// llvm-gcc (checks that all fields are 32-bit or 64-bit primitive types). We
// should probably make this smarter, or better yet make the LLVM backend
// capable of handling it.
static bool canExpandIndirectArgument(Type Ty, ASTContext &Context) {
  // We can only expand structure types.
  const HeteroContainerType *RT = Ty->getAs<HeteroContainerType>();
  if (!RT)
    return false;

  // We can only expand (C) structures.
  //
  // FIXME: This needs to be generalized to handle classes as well.
  const TypeDefn *RD = RT->getDefn();
  if (!RD->isStruct() || isa<UserClassDefn>(RD))
    return false;

  for (TypeDefn::member_iterator i = RD->member_begin(), e = RD->member_end();
         i != e; ++i) {
    const MemberDefn *FD = *i;

    if (!is32Or64BitBasicType(FD->getType(), Context))
      return false;

    // FIXME: Reject bit-fields wholesale; there are two problems, we don't know
    // how to expand them yet, and the predicate for telling if a bitfield still
    // counts as "basic" is more complicated than what we were doing previously.
//    if (FD->isBitField())
//      return false;
  }

  return true;
}

//===--------------------------------------------------------------------===//
//            X86 ABI
//===--------------------------------------------------------------------===//
namespace {
/// DefaultABIInfo - The default implementation for ABI specific
/// details. This implementation provides information which results in
/// self-consistent and sensible LLVM IR generation, but does not
/// conform to any particular ABI.
class DefaultABIInfo : public ABIInfo {
public:
  DefaultABIInfo(CodeGen::CodeGenTypes &CGT) : ABIInfo(CGT) {}

  ABIArgInfo classifyReturnType(Type RetTy) const;
  ABIArgInfo classifyArgumentType(Type RetTy) const;

  virtual void computeInfo(CGFunctionInfo &FI) const {
    FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    for (CGFunctionInfo::arg_iterator it = FI.arg_begin(), ie = FI.arg_end();
         it != ie; ++it)
      it->info = classifyArgumentType(it->type);
  }

  virtual llvm::Value *EmitVAArg(llvm::Value *VAListAddr, Type Ty,
                                 CodeGenFunction &CGF) const;
};

class DefaultTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  DefaultTargetCodeGenInfo(CodeGen::CodeGenTypes &CGT)
    : TargetCodeGenInfo(new DefaultABIInfo(CGT)) {}
};

llvm::Value *DefaultABIInfo::EmitVAArg(llvm::Value *VAListAddr, Type Ty,
                                       CodeGenFunction &CGF) const {
  return 0;
}

ABIArgInfo DefaultABIInfo::classifyArgumentType(Type Ty) const {
  if (isAggregateTypeForABI(Ty))
    return ABIArgInfo::getIndirect(0);

  return (Ty->isPromotableIntegerType() ?
          ABIArgInfo::getExtend() : ABIArgInfo::getDirect());
}

ABIArgInfo DefaultABIInfo::classifyReturnType(Type RetTy) const {
  if (isAggregateTypeForABI(RetTy))
    return ABIArgInfo::getIndirect(0);

  return (RetTy->isPromotableIntegerType() ?
          ABIArgInfo::getExtend() : ABIArgInfo::getDirect());
}

/// UseX86_MMXType - Return true if this is an MMX type that should use the special
/// x86_mmx type.
bool UseX86_MMXType(llvm::Type *IRType) {
  // If the type is an MMX type <2 x i32>, <4 x i16>, or <8 x i8>, use the
  // special x86_mmx type.
  return IRType->isVectorTy() && IRType->getPrimitiveSizeInBits() == 64 &&
    cast<llvm::VectorType>(IRType)->getElementType()->isIntegerTy() &&
    IRType->getScalarSizeInBits() != 64;
}

static llvm::Type* X86AdjustInlineAsmType(CodeGen::CodeGenFunction &CGF,
                                                llvm::StringRef Constraint,
                                                llvm::Type* Ty) {
  if ((Constraint == "y" || Constraint == "&y") && Ty->isVectorTy())
    return llvm::Type::getX86_MMXTy(CGF.getLLVMContext());
  return Ty;
}

//===----------------------------------------------------------------------===//
// X86-32 ABI Implementation
//===----------------------------------------------------------------------===//

/// X86_32ABIInfo - The X86-32 ABI information.
class X86_32ABIInfo : public ABIInfo {
  static const unsigned MinABIStackAlignInBytes = 4;

  bool IsDarwinVectorABI;
  bool IsSmallStructInRegABI;

  static bool isRegisterSize(unsigned Size) {
    return (Size == 8 || Size == 16 || Size == 32 || Size == 64);
  }

  static bool shouldReturnTypeInRegister(Type Ty, ASTContext &Context);

  /// getIndirectResult - Give a source type \arg Ty, return a suitable result
  /// such that the argument will be passed in memory.
  ABIArgInfo getIndirectResult(Type Ty, bool ByVal = true) const;

  /// \brief Return the alignment to use for the given type on the stack.
  unsigned getTypeStackAlignInBytes(Type Ty, unsigned Align) const;

public:

  ABIArgInfo classifyReturnType(Type RetTy) const;
  ABIArgInfo classifyArgumentType(Type RetTy) const;

  virtual void computeInfo(CGFunctionInfo &FI) const {
    FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    for (CGFunctionInfo::arg_iterator it = FI.arg_begin(), ie = FI.arg_end();
         it != ie; ++it)
      it->info = classifyArgumentType(it->type);
  }

  virtual llvm::Value *EmitVAArg(llvm::Value *VAListAddr, Type Ty,
                                 CodeGenFunction &CGF) const;

  X86_32ABIInfo(CodeGen::CodeGenTypes &CGT, bool d, bool p)
    : ABIInfo(CGT), IsDarwinVectorABI(d), IsSmallStructInRegABI(p) {}
};

class X86_32TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  X86_32TargetCodeGenInfo(CodeGen::CodeGenTypes &CGT, bool d, bool p)
    :TargetCodeGenInfo(new X86_32ABIInfo(CGT, d, p)) {}

  void SetTargetAttributes(const Defn *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &CGM) const;

  int getDwarfEHStackPointer(CodeGen::CodeGenModule &CGM) const {
    // Darwin uses different dwarf register numbers for EH.
    if (CGM.isTargetDarwin()) return 5;

    return 4;
  }

  bool initDwarfEHRegSizeTable(CodeGen::CodeGenFunction &CGF,
                               llvm::Value *Address) const;

  llvm::Type* adjustInlineAsmType(CodeGen::CodeGenFunction &CGF,
                                        llvm::StringRef Constraint,
                                        llvm::Type* Ty) const {
    return X86AdjustInlineAsmType(CGF, Constraint, Ty);
  }

};

} //end anonymous namespace

/// shouldReturnTypeInRegister - Determine if the given type should be
/// passed in a register (for the Darwin ABI).
bool X86_32ABIInfo::shouldReturnTypeInRegister(Type Ty,
                                               ASTContext &Context) {
  uint64_t Size = Context.getTypeSize(Ty);

  // Type must be register sized.
  if (!isRegisterSize(Size))
    return false;

  if (Ty->isVectorType()) {
    // 64- and 128- bit vectors inside structures are not returned in
    // registers.
    if (Size == 64 || Size == 128)
      return false;

    return true;
  }

  // If this is a builtin, pointer, enum, complex type, member pointer, or
  // member function pointer it is ok.
//  if (Ty->getAs<SimpleNumericType>() || Ty->hasPointerRepresentation() ||
//      Ty->isAnyComplexType() || Ty->isEnumeralType() ||
//      Ty->isBlockPointerType() || Ty->isMemberPointerType())
//    return true;
//
//  // Arrays are treated like records.
//  if (const ConstantArrayType *AT = Context.getAsConstantArrayType(Ty))
//    return shouldReturnTypeInRegister(AT->getElementType(), Context);

  // Otherwise, it must be a record type.
  const HeteroContainerType *RT = Ty->getAs<HeteroContainerType>();
  if (!RT) return false;

  // FIXME: Traverse bases here too.

  // Structure types are passed in register if all fields would be
  // passed in a register.
  for (TypeDefn::member_iterator i = RT->getDefn()->member_begin(),
         e = RT->getDefn()->member_end(); i != e; ++i) {
    const MemberDefn *FD = *i;

    // Empty fields are ignored.
    if (isEmptyField(Context, FD, true))
      continue;

    // Check fields recursively.
    if (!shouldReturnTypeInRegister(FD->getType(), Context))
      return false;
  }

  return true;
}

ABIArgInfo X86_32ABIInfo::classifyReturnType(Type RetTy) const {
//  if (RetTy->isVoidType())
//    return ABIArgInfo::getIgnore();

  if (const VectorType *VT = RetTy->getAs<VectorType>()) {
    // On Darwin, some vectors are returned in registers.
    if (IsDarwinVectorABI) {
      uint64_t Size = getContext().getTypeSize(RetTy);

      // 128-bit vectors are a special case; they are returned in
      // registers and we need to make sure to pick a type the LLVM
      // backend will like.
      if (Size == 128)
        return ABIArgInfo::getDirect(llvm::VectorType::get(
                  llvm::Type::getInt64Ty(getVMContext()), 2));

      // Always return in register if it fits in a general purpose
      // register, or if it is 64 bits and has a single element.
      if ((Size == 8 || Size == 16 || Size == 32) ||
          (Size == 64 && VT->getNumElements() == 1))
        return ABIArgInfo::getDirect(llvm::IntegerType::get(getVMContext(),
                                                            Size));

      return ABIArgInfo::getIndirect(0);
    }

    return ABIArgInfo::getDirect();
  }

  if (isAggregateTypeForABI(RetTy)) {
    if (const HeteroContainerType *RT = RetTy->getAs<HeteroContainerType>()) {
      // Structures with either a non-trivial destructor or a non-trivial
      // copy constructor are always indirect.
      if (hasNonTrivialDestructorOrCopyConstructor(RT))
        return ABIArgInfo::getIndirect(0, /*ByVal=*/false);

      // Structures with flexible arrays are always indirect.
//      if (RT->getDefn()->hasFlexibleArrayMember())
//        return ABIArgInfo::getIndirect(0);
    }

    // If specified, structs and unions are always indirect.
    if (!IsSmallStructInRegABI && !RetTy.hasComplexAttr())
      return ABIArgInfo::getIndirect(0);

    // Classify "single element" structs as their element type.
    if (const RawType *SeltTy = isSingleElementStruct(RetTy, getContext())) {
      if (const SimpleNumericType *BT = SeltTy->getAs<SimpleNumericType>()) {
        if (BT->isIntegerType()) {
          // We need to use the size of the structure, padding
          // bit-fields can adjust that to be larger than the single
          // element type.
          uint64_t Size = getContext().getTypeSize(RetTy);
          return ABIArgInfo::getDirect(
            llvm::IntegerType::get(getVMContext(), (unsigned)Size));
        }

        if (BT->getKind() == SimpleNumericType::Single) {
          assert(getContext().getTypeSize(RetTy) ==
                 getContext().getTypeSize(SeltTy) &&
                 "Unexpect single element structure size!");
          return ABIArgInfo::getDirect(llvm::Type::getFloatTy(getVMContext()));
        }

        if (BT->getKind() == SimpleNumericType::Double) {
          assert(getContext().getTypeSize(RetTy) ==
                 getContext().getTypeSize(SeltTy) &&
                 "Unexpect single element structure size!");
          return ABIArgInfo::getDirect(llvm::Type::getDoubleTy(getVMContext()));
        }
      } else if (SeltTy->isVectorType()) {
        // 64- and 128-bit vectors are never returned in a
        // register when inside a structure.
        uint64_t Size = getContext().getTypeSize(RetTy);
        if (Size == 64 || Size == 128)
          return ABIArgInfo::getIndirect(0);

        return classifyReturnType(Type(SeltTy, 0));
      }
    }

    // Small structures which are register sized are generally returned
    // in a register.
    if (X86_32ABIInfo::shouldReturnTypeInRegister(RetTy, getContext())) {
      uint64_t Size = getContext().getTypeSize(RetTy);
      return ABIArgInfo::getDirect(llvm::IntegerType::get(getVMContext(),Size));
    }

    return ABIArgInfo::getIndirect(0);
  }

  return (RetTy->isPromotableIntegerType() ?
          ABIArgInfo::getExtend() : ABIArgInfo::getDirect());
}

static bool isRecordWithSSEVectorType(ASTContext &Context, Type Ty) {
  const HeteroContainerType *RT = Ty->getAs<HeteroContainerType>();
  if (!RT)
    return 0;
  const TypeDefn *RD = RT->getDefn();

  // If this is a C++ record, check the bases first.
  if (const UserClassDefn *CXXRD = dyn_cast<UserClassDefn>(RD))
    for (UserClassDefn::base_class_const_iterator i = CXXRD->bases_begin(),
           e = CXXRD->bases_end(); i != e; ++i)
      if (!isRecordWithSSEVectorType(Context, i->getType()))
        return false;

  for (TypeDefn::member_iterator i = RD->member_begin(), e = RD->member_end();
       i != e; ++i) {
    Type FT = i->getType();

    if (FT->getAs<VectorType>() && Context.getTypeSize(Ty) == 128)
      return true;

    if (isRecordWithSSEVectorType(Context, FT))
      return true;
  }

  return false;
}

unsigned X86_32ABIInfo::getTypeStackAlignInBytes(Type Ty,
                                                 unsigned Align) const {
  // Otherwise, if the alignment is less than or equal to the minimum ABI
  // alignment, just use the default; the backend will handle this.
  if (Align <= MinABIStackAlignInBytes)
    return 0; // Use default alignment.

  // On non-Darwin, the stack type alignment is always 4.
  if (!IsDarwinVectorABI) {
    // Set explicit alignment, since we may need to realign the top.
    return MinABIStackAlignInBytes;
  }

  // Otherwise, if the type contains an SSE vector type, the alignment is 16.
  if (isRecordWithSSEVectorType(getContext(), Ty))
    return 16;

  return MinABIStackAlignInBytes;
}

ABIArgInfo X86_32ABIInfo::getIndirectResult(Type Ty, bool ByVal) const {
  if (!ByVal)
    return ABIArgInfo::getIndirect(0, false);

  // Compute the byval alignment.
  unsigned TypeAlign = getContext().getTypeAlign(Ty) / 8;
  unsigned StackAlign = getTypeStackAlignInBytes(Ty, TypeAlign);
  if (StackAlign == 0)
    return ABIArgInfo::getIndirect(4);

  // If the stack alignment is less than the type alignment, realign the
  // argument.
  if (StackAlign < TypeAlign)
    return ABIArgInfo::getIndirect(StackAlign, /*ByVal=*/true,
                                   /*Realign=*/true);

  return ABIArgInfo::getIndirect(StackAlign);
}

ABIArgInfo X86_32ABIInfo::classifyArgumentType(Type Ty) const {
  // FIXME: Set alignment on indirect arguments.
  if (isAggregateTypeForABI(Ty)) {
    // Structures with flexible arrays are always indirect.
    if (const HeteroContainerType *RT = Ty->getAs<HeteroContainerType>()) {
      // Structures with either a non-trivial destructor or a non-trivial
      // copy constructor are always indirect.
      if (hasNonTrivialDestructorOrCopyConstructor(RT))
        return getIndirectResult(Ty, /*ByVal=*/false);

//      if (RT->getDefn()->hasFlexibleArrayMember())
//        return getIndirectResult(Ty);
    }

    // Ignore empty structs.
//    if (Ty->isStructureType() && getContext().getTypeSize(Ty) == 0)
//      return ABIArgInfo::getIgnore();

    // Expand small (<= 128-bit) record types when we know that the stack layout
    // of those arguments will match the struct. This is important because the
    // LLVM backend isn't smart enough to remove byval, which inhibits many
    // optimizations.
    if (getContext().getTypeSize(Ty) <= 4*32 &&
        canExpandIndirectArgument(Ty, getContext()))
      return ABIArgInfo::getExpand();

    return getIndirectResult(Ty);
  }

  if (const VectorType *VT = Ty->getAs<VectorType>()) {
    // On Darwin, some vectors are passed in memory, we handle this by passing
    // it as an i8/i16/i32/i64.
    if (IsDarwinVectorABI) {
      uint64_t Size = getContext().getTypeSize(Ty);
      if ((Size == 8 || Size == 16 || Size == 32) ||
          (Size == 64 && VT->getNumElements() == 1))
        return ABIArgInfo::getDirect(llvm::IntegerType::get(getVMContext(),
                                                            Size));
    }

    llvm::Type *IRType = CGT.ConvertTypeRecursive(Ty);
    if (UseX86_MMXType(IRType)) {
      ABIArgInfo AAI = ABIArgInfo::getDirect(IRType);
      AAI.setCoerceToType(llvm::Type::getX86_MMXTy(getVMContext()));
      return AAI;
    }

    return ABIArgInfo::getDirect();
  }

  return (Ty->isPromotableIntegerType() ?
          ABIArgInfo::getExtend() : ABIArgInfo::getDirect());
}

llvm::Value *X86_32ABIInfo::EmitVAArg(llvm::Value *VAListAddr, Type Ty,
                                      CodeGenFunction &CGF) const {
  llvm::Type *BP = llvm::Type::getInt8PtrTy(CGF.getLLVMContext());
  llvm::Type *BPP = llvm::PointerType::getUnqual(BP);

  CGBuilderTy &Builder = CGF.Builder;
  llvm::Value *VAListAddrAsBPP = Builder.CreateBitCast(VAListAddr, BPP,
                                                       "ap");
  llvm::Value *Addr = Builder.CreateLoad(VAListAddrAsBPP, "ap.cur");
  llvm::Type *PTy =
    llvm::PointerType::getUnqual(CGF.ConvertType(Ty));
  llvm::Value *AddrTyped = Builder.CreateBitCast(Addr, PTy);

  uint64_t Offset =
    llvm::RoundUpToAlignment(CGF.getContext().getTypeSize(Ty) / 8, 4);
  llvm::Value *NextAddr =
    Builder.CreateGEP(Addr, llvm::ConstantInt::get(CGF.Int32Ty, Offset),
                      "ap.next");
  Builder.CreateStore(NextAddr, VAListAddrAsBPP);

  return AddrTyped;
}

void X86_32TargetCodeGenInfo::SetTargetAttributes(const Defn *D,
                                                  llvm::GlobalValue *GV,
                                            CodeGen::CodeGenModule &CGM) const {
  if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(D)) {
//    if (FD->hasAttr<X86ForceAlignArgPointerAttr>()) {
//      // Get the LLVM function.
//      llvm::Function *Fn = cast<llvm::Function>(GV);
//
//      // Now add the 'alignstack' attribute with a value of 16.
//      Fn->addFnAttr(llvm::Attribute::constructStackAlignmentFromInt(16));
//    }
  }
}

bool X86_32TargetCodeGenInfo::initDwarfEHRegSizeTable(
                                               CodeGen::CodeGenFunction &CGF,
                                               llvm::Value *Address) const {
  CodeGen::CGBuilderTy &Builder = CGF.Builder;
  llvm::LLVMContext &Context = CGF.getLLVMContext();

  llvm::IntegerType *i8 = llvm::Type::getInt8Ty(Context);
  llvm::Value *Four8 = llvm::ConstantInt::get(i8, 4);

  // 0-7 are the eight integer registers;  the order is different
  //   on Darwin (for EH), but the range is the same.
  // 8 is %eip.
  AssignToArrayRange(Builder, Address, Four8, 0, 8);

  if (CGF.CGM.isTargetDarwin()) {
    // 12-16 are st(0..4).  Not sure why we stop at 4.
    // These have size 16, which is sizeof(long double) on
    // platforms with 8-byte alignment for that type.
    llvm::Value *Sixteen8 = llvm::ConstantInt::get(i8, 16);
    AssignToArrayRange(Builder, Address, Sixteen8, 12, 16);

  } else {
    // 9 is %eflags, which doesn't get a size on Darwin for some
    // reason.
    Builder.CreateStore(Four8, Builder.CreateConstInBoundsGEP1_32(Address, 9));

    // 11-16 are st(0..5).  Not sure why we stop at 5.
    // These have size 12, which is sizeof(long double) on
    // platforms with 4-byte alignment for that type.
    llvm::Value *Twelve8 = llvm::ConstantInt::get(i8, 12);
    AssignToArrayRange(Builder, Address, Twelve8, 11, 16);
  }

  return false;
}

//===--------------------------------------------------------------------===//
//               PTX ABI
//===--------------------------------------------------------------------===//
namespace {

class PTXABIInfo : public ABIInfo {
public:
  PTXABIInfo(CodeGenTypes &CGT) : ABIInfo(CGT) {}

  ABIArgInfo classifyReturnType(Type RetTy) const;
  ABIArgInfo classifyArgumentType(Type Ty) const;

  virtual void computeInfo(CGFunctionInfo &FI) const;
  virtual llvm::Value *EmitVAArg(llvm::Value *VAListAddr, Type Ty,
                                 CodeGenFunction &CFG) const;
};

class PTXTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  PTXTargetCodeGenInfo(CodeGenTypes &CGT)
    : TargetCodeGenInfo(new PTXABIInfo(CGT)) {}
};

ABIArgInfo PTXABIInfo::classifyReturnType(Type RetTy) const {
//  if (RetTy->isVoidType())
//    return ABIArgInfo::getIgnore();
  if (isAggregateTypeForABI(RetTy))
    return ABIArgInfo::getIndirect(0);
  return ABIArgInfo::getDirect();
}

ABIArgInfo PTXABIInfo::classifyArgumentType(Type Ty) const {
  if (isAggregateTypeForABI(Ty))
    return ABIArgInfo::getIndirect(0);

  return ABIArgInfo::getDirect();
}

void PTXABIInfo::computeInfo(CGFunctionInfo &FI) const {
  FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
  for (CGFunctionInfo::arg_iterator it = FI.arg_begin(), ie = FI.arg_end();
       it != ie; ++it)
    it->info = classifyArgumentType(it->type);

  // Always honor user-specified calling convention.
  if (FI.getCallingConvention() != llvm::CallingConv::C)
    return;

  // Calling convention as default by an ABI.
  llvm::CallingConv::ID DefaultCC;
  llvm::StringRef Env = getContext().Target.getTriple().getEnvironmentName();
  if (Env == "device")
    DefaultCC = llvm::CallingConv::PTX_Device;
  else
    DefaultCC = llvm::CallingConv::PTX_Kernel;

  FI.setEffectiveCallingConvention(DefaultCC);
}

llvm::Value *PTXABIInfo::EmitVAArg(llvm::Value *VAListAddr, Type Ty,
                                   CodeGenFunction &CFG) const {
  llvm_unreachable("PTX does not support varargs");
  return 0;
}

}  // end anonymous namespace

const TargetCodeGenInfo &CodeGenModule::getTargetCodeGenInfo() {
  if (TheTargetCodeGenInfo)
    return *TheTargetCodeGenInfo;

  // For now we just cache the TargetCodeGenInfo in CodeGenModule and don't
  // free it.

  const llvm::Triple &Triple = getContext().Target.getTriple();
  switch (Triple.getArch()) {
  default:
    return *(TheTargetCodeGenInfo = new DefaultTargetCodeGenInfo(Types));

  case llvm::Triple::ptx32:
  case llvm::Triple::ptx64:
    return *(TheTargetCodeGenInfo = new PTXTargetCodeGenInfo(Types));


  case llvm::Triple::x86:
    if (Triple.isOSDarwin())
      return *(TheTargetCodeGenInfo =
               new X86_32TargetCodeGenInfo(Types, true, true));

    switch (Triple.getOS()) {
    case llvm::Triple::Cygwin:
    case llvm::Triple::MinGW32:
    case llvm::Triple::AuroraUX:
    case llvm::Triple::DragonFly:
    case llvm::Triple::FreeBSD:
    case llvm::Triple::OpenBSD:
    case llvm::Triple::NetBSD:
      return *(TheTargetCodeGenInfo =
               new X86_32TargetCodeGenInfo(Types, false, true));

    default:
      return *(TheTargetCodeGenInfo =
               new X86_32TargetCodeGenInfo(Types, false, false));
    }
  }
}
