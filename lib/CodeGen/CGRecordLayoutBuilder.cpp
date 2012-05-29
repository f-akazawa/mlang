//===--- CGRecordLayoutBuilder.cpp - CGRecordLayout builder  ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Builder implementation for CGRecordLayout objects.
//
//===----------------------------------------------------------------------===//

#include "CGRecordLayout.h"
#include "mlang/AST/ASTContext.h"
//#include "mlang/AST/Attr.h"
//#include "mlang/AST/CXXInheritance.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "CodeGenTypes.h"
#include "CGOOPABI.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetData.h"
using namespace mlang;
using namespace CodeGen;

namespace {

class CGRecordLayoutBuilder {
public:
  /// FieldTypes - Holds the LLVM types that the struct is created from.
  /// 
  llvm::SmallVector<llvm::Type *, 16> FieldTypes;

  /// BaseSubobjectType - Holds the LLVM type for the non-virtual part
  /// of the struct. For example, consider:
  ///
  /// struct A { int i; };
  /// struct B { void *v; };
  /// struct C : virtual A, B { };
  ///
  /// The LLVM type of C will be
  /// %struct.C = type { i32 (...)**, %struct.A, i32, %struct.B }
  ///
  /// And the LLVM type of the non-virtual base struct will be
  /// %struct.C.base = type { i32 (...)**, %struct.A, i32 }
  ///
  /// This only gets initialized if the base subobject type is
  /// different from the complete-object type.
  llvm::StructType *BaseSubobjectType;

  /// FieldInfo - Holds a field and its corresponding LLVM field number.
  llvm::DenseMap<const MemberDefn *, unsigned> Fields;

  llvm::DenseMap<const UserClassDefn *, unsigned> NonVirtualBases;
  llvm::DenseMap<const UserClassDefn *, unsigned> VirtualBases;

  /// LaidOutVirtualBases - A set of all laid out virtual bases, used to avoid
  /// avoid laying out virtual bases more than once.
  llvm::SmallPtrSet<const UserClassDefn *, 4> LaidOutVirtualBases;
  
  /// IsZeroInitializable - Whether this struct can be C++
  /// zero-initialized with an LLVM zeroinitializer.
  bool IsZeroInitializable;
  bool IsZeroInitializableAsBase;

  /// Packed - Whether the resulting LLVM struct will be packed or not.
  bool Packed;
  
  /// IsMsStruct - Whether ms_struct is in effect or not
  bool IsMsStruct;

private:
  CodeGenTypes &Types;

  /// LastLaidOutBaseInfo - Contains the offset and non-virtual size of the
  /// last base laid out. Used so that we can replace the last laid out base
  /// type with an i8 array if needed.
  struct LastLaidOutBaseInfo {
    CharUnits Offset;
    CharUnits NonVirtualSize;

    bool isValid() const { return !NonVirtualSize.isZero(); }
    void invalidate() { NonVirtualSize = CharUnits::Zero(); }
  
  } LastLaidOutBase;

  /// Alignment - Contains the alignment of the RecordDecl.
  CharUnits Alignment;

  /// BitsAvailableInLastField - If a bit field spans only part of a LLVM field,
  /// this will have the number of bits still available in the field.
  char BitsAvailableInLastField;

  /// NextFieldOffset - Holds the next field offset.
  CharUnits NextFieldOffset;

  /// LayoutField - try to layout all fields in the record decl.
  /// Returns false if the operation failed because the struct is not packed.
  bool LayoutFields(const TypeDefn *D);

  /// Layout a single base, virtual or non-virtual
  void LayoutBase(const UserClassDefn *base,
                  const CGRecordLayout &baseLayout,
                  CharUnits baseOffset);

  /// LayoutVirtualBase - layout a single virtual base.
  void LayoutVirtualBase(const UserClassDefn *base,
                         CharUnits baseOffset);

  /// LayoutVirtualBases - layout the virtual bases of a record decl.
  void LayoutVirtualBases(const UserClassDefn *RD,
                          const ASTRecordLayout &Layout);
  
  /// LayoutNonVirtualBase - layout a single non-virtual base.
  void LayoutNonVirtualBase(const UserClassDefn *base,
                            CharUnits baseOffset);
  
  /// LayoutNonVirtualBases - layout the virtual bases of a record decl.
  void LayoutNonVirtualBases(const UserClassDefn *RD,
                             const ASTRecordLayout &Layout);

  /// ComputeNonVirtualBaseType - Compute the non-virtual base field types.
  bool ComputeNonVirtualBaseType(const UserClassDefn *RD);
  
  /// LayoutField - layout a single field. Returns false if the operation failed
  /// because the current struct is not packed.
  bool LayoutField(const MemberDefn *D, uint64_t FieldOffset);

  /// AppendField - Appends a field with the given offset and type.
  void AppendField(CharUnits fieldOffset, llvm::Type *FieldTy);

  /// AppendPadding - Appends enough padding bytes so that the total
  /// struct size is a multiple of the field alignment.
  void AppendPadding(CharUnits fieldOffset, CharUnits fieldAlignment);

  /// ResizeLastBaseFieldIfNecessary - Fields and bases can be laid out in the
  /// tail padding of a previous base. If this happens, the type of the previous
  /// base needs to be changed to an array of i8. Returns true if the last
  /// laid out base was resized.
  bool ResizeLastBaseFieldIfNecessary(CharUnits offset);

  /// getByteArrayType - Returns a byte array type with the given number of
  /// elements.
  llvm::Type *getByteArrayType(CharUnits NumBytes);
  
  /// AppendBytes - Append a given number of bytes to the record.
  void AppendBytes(CharUnits numBytes);

  /// AppendTailPadding - Append enough tail padding so that the type will have
  /// the passed size.
  void AppendTailPadding(CharUnits RecordSize);

  CharUnits getTypeAlignment(llvm::Type *Ty) const;

  /// getAlignmentAsLLVMStruct - Returns the maximum alignment of all the
  /// LLVM element types.
  CharUnits getAlignmentAsLLVMStruct() const;

  /// CheckZeroInitializable - Check if the given type contains a pointer
  /// to data member.
  void CheckZeroInitializable(Type T);

public:
  CGRecordLayoutBuilder(CodeGenTypes &Types)
    : BaseSubobjectType(0),
      IsZeroInitializable(true), IsZeroInitializableAsBase(true),
      Packed(false), IsMsStruct(false),
      Types(Types), BitsAvailableInLastField(0) { }

  /// Layout - Will layout a TypeDefn.
  void Layout(const TypeDefn *D);
};

}

void CGRecordLayoutBuilder::Layout(const TypeDefn *D) {
  Alignment = Types.getContext().getASTRecordLayout(D).getAlignment();

  if (LayoutFields(D))
    return;

  // We weren't able to layout the struct. Try again with a packed struct
  Packed = true;
  LastLaidOutBase.invalidate();
  NextFieldOffset = CharUnits::Zero();
  FieldTypes.clear();
  Fields.clear();
  NonVirtualBases.clear();
  VirtualBases.clear();

  LayoutFields(D);
}

bool CGRecordLayoutBuilder::LayoutField(const MemberDefn *D,
                                        uint64_t fieldOffset) {
  CheckZeroInitializable(D->getType());

  assert(fieldOffset % Types.getTarget().getCharWidth() == 0
         && "field offset is not on a byte boundary!");
  CharUnits fieldOffsetInBytes
    = Types.getContext().toCharUnitsFromBits(fieldOffset);

  llvm::Type *Ty = Types.ConvertTypeForMemRecursive(D->getType());
  CharUnits typeAlignment = getTypeAlignment(Ty);

  // If the type alignment is larger then the struct alignment, we must use
  // a packed struct.
  if (typeAlignment > Alignment) {
    assert(!Packed && "Alignment is wrong even with packed struct!");
    return false;
  }

  if (!Packed) {
    if (const HeteroContainerType *RT = D->getType()->getAs<HeteroContainerType>()) {
      const TypeDefn *RD = cast<TypeDefn>(RT->getDefn());
    }
  }

  // Round up the field offset to the alignment of the field type.
  CharUnits alignedNextFieldOffsetInBytes =
    NextFieldOffset.RoundUpToAlignment(typeAlignment);

  if (fieldOffsetInBytes < alignedNextFieldOffsetInBytes) {
    // Try to resize the last base field.
    if (ResizeLastBaseFieldIfNecessary(fieldOffsetInBytes)) {
      alignedNextFieldOffsetInBytes = 
        NextFieldOffset.RoundUpToAlignment(typeAlignment);
    }
  }

  if (fieldOffsetInBytes < alignedNextFieldOffsetInBytes) {
    assert(!Packed && "Could not place field even with packed struct!");
    return false;
  }

  AppendPadding(fieldOffsetInBytes, typeAlignment);

  // Now append the field.
  Fields[D] = FieldTypes.size();
  AppendField(fieldOffsetInBytes, Ty);

  LastLaidOutBase.invalidate();
  return true;
}

void CGRecordLayoutBuilder::LayoutBase(const UserClassDefn *base,
                                       const CGRecordLayout &baseLayout,
                                       CharUnits baseOffset) {
  ResizeLastBaseFieldIfNecessary(baseOffset);

  AppendPadding(baseOffset, CharUnits::One());

  const ASTRecordLayout &baseASTLayout
    = Types.getContext().getASTRecordLayout(base);

  LastLaidOutBase.Offset = NextFieldOffset;
  LastLaidOutBase.NonVirtualSize = /*baseASTLayout.getNonVirtualSize()*/
		  CharUnits::Zero();

  // Fields and bases can be laid out in the tail padding of previous
  // bases.  If this happens, we need to allocate the base as an i8
  // array; otherwise, we can use the subobject type.  However,
  // actually doing that would require knowledge of what immediately
  // follows this base in the layout, so instead we do a conservative
  // approximation, which is to use the base subobject type if it
  // has the same LLVM storage size as the nvsize.

  llvm::StructType *subobjectType = baseLayout.getBaseSubobjectLLVMType();
  AppendField(baseOffset, subobjectType);

  Types.addBaseSubobjectTypeName(base, baseLayout);
}

void CGRecordLayoutBuilder::LayoutNonVirtualBase(const UserClassDefn *base,
                                                 CharUnits baseOffset) {
  // Ignore empty bases.
  if (base->isEmpty()) return;

  const CGRecordLayout &baseLayout = Types.getCGRecordLayout(base);
  if (IsZeroInitializableAsBase) {
    assert(IsZeroInitializable &&
           "class zero-initializable as base but not as complete object");

    IsZeroInitializable = IsZeroInitializableAsBase =
      baseLayout.isZeroInitializableAsBase();
  }

  LayoutBase(base, baseLayout, baseOffset);
  NonVirtualBases[base] = (FieldTypes.size() - 1);
}

void
CGRecordLayoutBuilder::LayoutVirtualBase(const UserClassDefn *base,
                                         CharUnits baseOffset) {
  // Ignore empty bases.
  if (base->isEmpty()) return;

  const CGRecordLayout &baseLayout = Types.getCGRecordLayout(base);
  if (IsZeroInitializable)
    IsZeroInitializable = baseLayout.isZeroInitializableAsBase();

  LayoutBase(base, baseLayout, baseOffset);
  VirtualBases[base] = (FieldTypes.size() - 1);
}

/// LayoutVirtualBases - layout the non-virtual bases of a record decl.
void
CGRecordLayoutBuilder::LayoutVirtualBases(const UserClassDefn *RD,
                                          const ASTRecordLayout &Layout) {
  for (UserClassDefn::base_class_const_iterator I = RD->bases_begin(),
       E = RD->bases_end(); I != E; ++I) {
    const UserClassDefn *BaseDecl =
      cast<UserClassDefn>(I->getType()->getAs<ClassdefType>()->getDefn());

    // We only want to lay out virtual bases that aren't indirect primary bases
    // of some other base.
//    if (I->isVirtual() && !IndirectPrimaryBases.count(BaseDecl)) {
//      // Only lay out the base once.
//      if (!LaidOutVirtualBases.insert(BaseDecl))
//        continue;
//
//      CharUnits vbaseOffset = Layout.getVBaseClassOffset(BaseDecl);
//      LayoutVirtualBase(BaseDecl, vbaseOffset);
//    }
//
//    if (!BaseDecl->getNumVBases()) {
//      // This base isn't interesting since it doesn't have any virtual bases.
//      continue;
//    }
    
    LayoutVirtualBases(BaseDecl, Layout);
  }
}

void
CGRecordLayoutBuilder::LayoutNonVirtualBases(const UserClassDefn *RD,
                                             const ASTRecordLayout &Layout) {
  const UserClassDefn *PrimaryBase = Layout.getPrimaryBase();

  // Check if we need to add a vtable pointer.
//  if (RD->isDynamicClass()) {
//    if (!PrimaryBase) {
//      llvm::Type *FunctionType =
//        llvm::FunctionType::get(llvm::Type::getInt32Ty(Types.getLLVMContext()),
//                                /*isVarArg=*/true);
//      llvm::Type *VTableTy = FunctionType->getPointerTo();
//
//      assert(NextFieldOffset.isZero() &&
//             "VTable pointer must come first!");
//      AppendField(CharUnits::Zero(), VTableTy->getPointerTo());
//    } else {
//      if (!Layout.isPrimaryBaseVirtual())
//        LayoutNonVirtualBase(PrimaryBase, CharUnits::Zero());
//      else
//        LayoutVirtualBase(PrimaryBase, CharUnits::Zero());
//    }
//  }

  // Layout the non-virtual bases.
  for (UserClassDefn::base_class_const_iterator I = RD->bases_begin(),
       E = RD->bases_end(); I != E; ++I) {
    if (I->isVirtual())
      continue;

    const UserClassDefn *BaseDecl =
      cast<UserClassDefn>(I->getType()->getAs<ClassdefType>()->getDefn());

    // We've already laid out the primary base.
    if (BaseDecl == PrimaryBase /*&& !Layout.isPrimaryBaseVirtual()*/)
      continue;

    LayoutNonVirtualBase(BaseDecl, Layout.getBaseClassOffset(BaseDecl));
  }
}

bool
CGRecordLayoutBuilder::ComputeNonVirtualBaseType(const UserClassDefn *RD) {
  const ASTRecordLayout &Layout = Types.getContext().getASTRecordLayout(RD);

  CharUnits NonVirtualSize  = /*Layout.getNonVirtualSize()*/CharUnits::Zero();
  CharUnits NonVirtualAlign = /*Layout.getNonVirtualAlign()*/CharUnits::Zero();
  CharUnits AlignedNonVirtualTypeSize =
    NonVirtualSize.RoundUpToAlignment(NonVirtualAlign);
  
  // First check if we can use the same fields as for the complete class.
  CharUnits RecordSize = Layout.getSize();
  if (AlignedNonVirtualTypeSize == RecordSize)
    return true;

  // Check if we need padding.
  CharUnits AlignedNextFieldOffset =
    NextFieldOffset.RoundUpToAlignment(getAlignmentAsLLVMStruct());

  if (AlignedNextFieldOffset > AlignedNonVirtualTypeSize) {
    assert(!Packed && "cannot layout even as packed struct");
    return false; // Needs packing.
  }

  bool needsPadding = (AlignedNonVirtualTypeSize != AlignedNextFieldOffset);
  if (needsPadding) {
    CharUnits NumBytes = AlignedNonVirtualTypeSize - AlignedNextFieldOffset;
    FieldTypes.push_back(getByteArrayType(NumBytes));
  }

  BaseSubobjectType = llvm::StructType::get(Types.getLLVMContext(),
                                            FieldTypes, Packed);

  if (needsPadding) {
    // Pull the padding back off.
    FieldTypes.pop_back();
  }

  return true;
}

bool CGRecordLayoutBuilder::LayoutFields(const TypeDefn *D) {
  assert(!Alignment.isZero() && "Did not set alignment!");

  const ASTRecordLayout &Layout = Types.getContext().getASTRecordLayout(D);

  const UserClassDefn *RD = dyn_cast<UserClassDefn>(D);
  if (RD)
    LayoutNonVirtualBases(RD, Layout);

  unsigned FieldNo = 0;
  const MemberDefn *LastFD = 0;
  
  for (TypeDefn::member_iterator Field = D->member_begin(),
       FieldEnd = D->member_end(); Field != FieldEnd; ++Field, ++FieldNo) {
    if (IsMsStruct) {
      // Zero-length bitfields following non-bitfield members are
      // ignored:
      const MemberDefn *FD =  (*Field);
//      if (Types.getContext().ZeroBitfieldFollowsNonBitfield(FD, LastFD)) {
//        --FieldNo;
//        continue;
//      }
      LastFD = FD;
    }
    
    if (!LayoutField(*Field, Layout.getFieldOffset(FieldNo))) {
      assert(!Packed &&
             "Could not layout fields even with a packed LLVM struct!");
      return false;
    }
  }

  if (RD) {
    // We've laid out the non-virtual bases and the fields, now compute the
    // non-virtual base field types.
    if (!ComputeNonVirtualBaseType(RD)) {
      assert(!Packed && "Could not layout even with a packed LLVM struct!");
      return false;
    }

    // And lay out the virtual bases.
//    RD->getIndirectPrimaryBases(IndirectPrimaryBases);
//    if (Layout.isPrimaryBaseVirtual())
//      IndirectPrimaryBases.insert(Layout.getPrimaryBase());
    LayoutVirtualBases(RD, Layout);
  }
  
  // Append tail padding if necessary.
  AppendTailPadding(Layout.getSize());

  return true;
}

void CGRecordLayoutBuilder::AppendTailPadding(CharUnits RecordSize) {
  ResizeLastBaseFieldIfNecessary(RecordSize);

  assert(NextFieldOffset <= RecordSize && "Size mismatch!");

  CharUnits AlignedNextFieldOffset =
    NextFieldOffset.RoundUpToAlignment(getAlignmentAsLLVMStruct());

  if (AlignedNextFieldOffset == RecordSize) {
    // We don't need any padding.
    return;
  }

  CharUnits NumPadBytes = RecordSize - NextFieldOffset;
  AppendBytes(NumPadBytes);
}

void CGRecordLayoutBuilder::AppendField(CharUnits fieldOffset,
                                        llvm::Type *fieldType) {
  CharUnits fieldSize =
    CharUnits::fromQuantity(Types.getTargetData().getTypeAllocSize(fieldType));

  FieldTypes.push_back(fieldType);

  NextFieldOffset = fieldOffset + fieldSize;
  BitsAvailableInLastField = 0;
}

void CGRecordLayoutBuilder::AppendPadding(CharUnits fieldOffset,
                                          CharUnits fieldAlignment) {
  assert(NextFieldOffset <= fieldOffset &&
         "Incorrect field layout!");

  // Round up the field offset to the alignment of the field type.
  CharUnits alignedNextFieldOffset =
    NextFieldOffset.RoundUpToAlignment(fieldAlignment);

  if (alignedNextFieldOffset < fieldOffset) {
    // Even with alignment, the field offset is not at the right place,
    // insert padding.
    CharUnits padding = fieldOffset - NextFieldOffset;

    AppendBytes(padding);
  }
}

bool CGRecordLayoutBuilder::ResizeLastBaseFieldIfNecessary(CharUnits offset) {
  // Check if we have a base to resize.
  if (!LastLaidOutBase.isValid())
    return false;

  // This offset does not overlap with the tail padding.
  if (offset >= NextFieldOffset)
    return false;

  // Restore the field offset and append an i8 array instead.
  FieldTypes.pop_back();
  NextFieldOffset = LastLaidOutBase.Offset;
  AppendBytes(LastLaidOutBase.NonVirtualSize);
  LastLaidOutBase.invalidate();

  return true;
}

llvm::Type *CGRecordLayoutBuilder::getByteArrayType(CharUnits numBytes) {
  assert(!numBytes.isZero() && "Empty byte arrays aren't allowed.");

  llvm::Type *Ty = llvm::Type::getInt8Ty(Types.getLLVMContext());
  if (numBytes > CharUnits::One())
    Ty = llvm::ArrayType::get(Ty, numBytes.getQuantity());

  return Ty;
}

void CGRecordLayoutBuilder::AppendBytes(CharUnits numBytes) {
  if (numBytes.isZero())
    return;

  // Append the padding field
  AppendField(NextFieldOffset, getByteArrayType(numBytes));
}

CharUnits CGRecordLayoutBuilder::getTypeAlignment(llvm::Type *Ty) const {
  if (Packed)
    return CharUnits::One();

  return CharUnits::fromQuantity(Types.getTargetData().getABITypeAlignment(Ty));
}

CharUnits CGRecordLayoutBuilder::getAlignmentAsLLVMStruct() const {
  if (Packed)
    return CharUnits::One();

  CharUnits maxAlignment = CharUnits::One();
  for (size_t i = 0; i != FieldTypes.size(); ++i)
    maxAlignment = std::max(maxAlignment, getTypeAlignment(FieldTypes[i]));

  return maxAlignment;
}

/// Merge in whether a field of the given type is zero-initializable.
void CGRecordLayoutBuilder::CheckZeroInitializable(Type T) {
  // This record already contains a member pointer.
  if (!IsZeroInitializableAsBase)
    return;

  // Can only have member pointers if we're compiling C++.
  if (!Types.getContext().getLangOptions().OOP)
    return;

  const RawType *elementType = T.getRawTypePtr();

  if (const ClassdefType *RT = elementType->getAs<ClassdefType>()) {
    const UserClassDefn *RD = cast<UserClassDefn>(RT->getDefn());
    const CGRecordLayout &Layout = Types.getCGRecordLayout(RD);
    if (!Layout.isZeroInitializable())
      IsZeroInitializable = IsZeroInitializableAsBase = false;
  }
}

CGRecordLayout *CodeGenTypes::ComputeRecordLayout(const TypeDefn *D) {
  CGRecordLayoutBuilder Builder(*this);

  Builder.Layout(D);

  llvm::StructType *Ty = llvm::StructType::get(getLLVMContext(),
                                                     Builder.FieldTypes,
                                                     Builder.Packed);

  // If we're in C++, compute the base subobject type.
  llvm::StructType *BaseTy = 0;
  if (isa<UserClassDefn>(D)) {
    BaseTy = Builder.BaseSubobjectType;
    if (!BaseTy) BaseTy = Ty;
  }

  CGRecordLayout *RL =
    new CGRecordLayout(Ty, BaseTy, Builder.IsZeroInitializable,
                       Builder.IsZeroInitializableAsBase);

  RL->NonVirtualBases.swap(Builder.NonVirtualBases);
  RL->CompleteObjectVirtualBases.swap(Builder.VirtualBases);

  // Add all the field numbers.
  RL->FieldInfo.swap(Builder.Fields);

  // Dump the layout, if requested.
  if (getContext().getLangOptions().DumpRecordLayouts) {
    llvm::errs() << "\n*** Dumping IRgen Record Layout\n";
    llvm::errs() << "Record: ";
    D->dump();
    llvm::errs() << "\nLayout: ";
    RL->dump();
  }

#ifndef NDEBUG
  // Verify that the computed LLVM struct size matches the AST layout size.
  const ASTRecordLayout &Layout = getContext().getASTRecordLayout(D);

  uint64_t TypeSizeInBits = getContext().toBits(Layout.getSize());
  assert(TypeSizeInBits == getTargetData().getTypeAllocSizeInBits(Ty) &&
         "Type size mismatch!");

  if (BaseTy) {
    CharUnits NonVirtualSize  = /*Layout.getNonVirtualSize()*/CharUnits::Zero();
    CharUnits NonVirtualAlign = /*Layout.getNonVirtualAlign()*/CharUnits::Zero();
    CharUnits AlignedNonVirtualTypeSize = 
      NonVirtualSize.RoundUpToAlignment(NonVirtualAlign);

    uint64_t AlignedNonVirtualTypeSizeInBits = 
      getContext().toBits(AlignedNonVirtualTypeSize);

    assert(AlignedNonVirtualTypeSizeInBits == 
           getTargetData().getTypeAllocSizeInBits(BaseTy) &&
           "Type size mismatch!");
  }
                                     
  // Verify that the LLVM and AST field offsets agree.
  llvm::StructType *ST =
    dyn_cast<llvm::StructType>(RL->getLLVMType());
  const llvm::StructLayout *SL = getTargetData().getStructLayout(ST);

  const ASTRecordLayout &AST_RL = getContext().getASTRecordLayout(D);
  TypeDefn::member_iterator it = D->member_begin();
  const MemberDefn *LastFD = 0;

  for (unsigned i = 0, e = AST_RL.getFieldCount(); i != e; ++i, ++it) {
    const MemberDefn *FD = *it;

    // For non-bit-fields, just check that the LLVM struct offset matches the
    // AST offset.
    unsigned FieldNo = RL->getLLVMFieldNo(FD);
    assert(AST_RL.getFieldOffset(i) == SL->getElementOffsetInBits(FieldNo) &&
             "Invalid field offset!");
    LastFD = FD;
  }
#endif

  return RL;
}

void CGRecordLayout::print(llvm::raw_ostream &OS) const {
  OS << "<CGRecordLayout\n";
  OS << "  LLVMType:" << *CompleteObjectType << "\n";
  if (BaseSubobjectType)
    OS << "  NonVirtualBaseLLVMType:" << *BaseSubobjectType << "\n"; 
  OS << "  IsZeroInitializable:" << IsZeroInitializable << "\n";
}

void CGRecordLayout::dump() const {
  print(llvm::errs());
}
