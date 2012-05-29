//===--- CGRecordLayout.h - LLVM Record Layout Information ------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CGRECORDLAYOUT_H_
#define MLANG_CODEGEN_CGRECORDLAYOUT_H_

#include "llvm/ADT/DenseMap.h"
#include "llvm/DerivedTypes.h"
#include "mlang/AST/DefnSub.h"
namespace llvm {
  class raw_ostream;
  class StructType;
}

namespace mlang {
namespace CodeGen {

/// CGRecordLayout - This class handles struct and union layout info while
/// lowering AST types to LLVM types.
///
/// These layout objects are only created on demand as IR generation requires.
class CGRecordLayout {
  friend class CodeGenTypes;

  CGRecordLayout(const CGRecordLayout&); // DO NOT IMPLEMENT
  void operator=(const CGRecordLayout&); // DO NOT IMPLEMENT

private:
  /// The LLVM type corresponding to this record layout; used when
  /// laying it out as a complete object.
  llvm::StructType *CompleteObjectType;

  /// The LLVM type for the non-virtual part of this record layout;
  /// used when laying it out as a base subobject.
  llvm::StructType *BaseSubobjectType;

  /// Map from (non-bit-field) struct field to the corresponding llvm struct
  /// type field no. This info is populated by record builder.
  llvm::DenseMap<const MemberDefn *, unsigned> FieldInfo;

  // FIXME: Maybe we could use a CXXBaseSpecifier as the key and use a single
  // map for both virtual and non virtual bases.
  llvm::DenseMap<const UserClassDefn *, unsigned> NonVirtualBases;

  /// Map from virtual bases to their field index in the complete object.
  llvm::DenseMap<const UserClassDefn *, unsigned> CompleteObjectVirtualBases;


  /// Whether one of the fields in this record layout is a pointer to data
  /// member, or a struct that contains pointer to data member.
  bool IsZeroInitializable : 1;

  /// False if any direct or indirect subobject of this class, when
  /// considered as a base subobject, requires a non-zero bitpattern
  /// when zero-initialized.
  bool IsZeroInitializableAsBase : 1;

public:
  CGRecordLayout(llvm::StructType *CompleteObjectType,
                   llvm::StructType *BaseSubobjectType,
                   bool IsZeroInitializable,
                   bool IsZeroInitializableAsBase)
      : CompleteObjectType(CompleteObjectType),
        BaseSubobjectType(BaseSubobjectType),
        IsZeroInitializable(IsZeroInitializable),
        IsZeroInitializableAsBase(IsZeroInitializableAsBase) {}

  /// \brief Return the LLVM type associated with this record.
  llvm::StructType *getLLVMType() const {
    return CompleteObjectType;
  }

  /// \brief Return the "base subobject" LLVM type associated with
  /// this record.
  llvm::StructType *getBaseSubobjectLLVMType() const {
	  return BaseSubobjectType;
  }

  /// \brief Check whether this struct can be C++ zero-initialized
  /// with a zeroinitializer.
  bool isZeroInitializable() const {
    return IsZeroInitializable;
  }

  /// \brief Check whether this struct can be C++ zero-initialized
  /// with a zeroinitializer when considered as a base subobject.
  bool isZeroInitializableAsBase() const {
    return IsZeroInitializableAsBase;
  }

  /// \brief Return llvm::StructType element number that corresponds to the
  /// field FD.
  unsigned getLLVMFieldNo(const MemberDefn *FD) const {
    assert(FieldInfo.count(FD) && "Invalid field for record!");
    return FieldInfo.lookup(FD);
  }

  unsigned getNonVirtualBaseLLVMFieldNo(const UserClassDefn *RD) const {
	  assert(NonVirtualBases.count(RD) && "Invalid non-virtual base!");
	  return NonVirtualBases.lookup(RD);
  }

  /// \brief Return the LLVM field index corresponding to the given
  /// virtual base.  Only valid when operating on the complete object.
  unsigned getVirtualBaseIndex(const UserClassDefn *base) const {
    assert(CompleteObjectVirtualBases.count(base) && "Invalid virtual base!");
    return CompleteObjectVirtualBases.lookup(base);
  }

  void print(llvm::raw_ostream &OS) const;
  void dump() const;
};

}  // end namespace CodeGen
}  // end namespace mlang

#endif /* MLANG_CODEGEN_CGRECORDLAYOUT_H_ */
