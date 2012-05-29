//===--- RecordLayout.h - Layout information for a struct/union -*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the RecordLayout interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_LAYOUT_INFO_H_
#define MLANG_AST_LAYOUT_INFO_H_

#include "llvm/Support/DataTypes.h"
#include "llvm/ADT/DenseMap.h"
#include "mlang/AST/CharUnits.h"
#include "mlang/AST/DefnOOP.h"

namespace mlang {
  class ASTContext;
  class UserClassDefn;

/// ASTRecordLayout -
/// This class contains layout information for one UserClassDefn,
/// which is a user defined class.  The defn represented must be a definition,
/// not a forward declaration.
///
/// These objects are managed by ASTContext.
class ASTRecordLayout {
	/// Size - Size of record in characters.
	CharUnits Size;

	/// DataSize - Size of record in characters without tail padding.
	CharUnits DataSize;

	/// FieldOffsets - Array of field offsets in bits.
	uint64_t *FieldOffsets;

	// Alignment - Alignment of record in characters.
	CharUnits Alignment;

	// FieldCount - Number of fields.
	unsigned FieldCount;

  struct UserClassLayoutInfo {
	  /// SizeOfLargestEmptySubobject - The size of the largest empty subobject
	  /// (either a base or a member). Will be zero if the class doesn't contain
	  /// any empty subobjects.
	  CharUnits SizeOfLargestEmptySubobject;

	  /// PrimaryBase - The primary base info for this record.
	  llvm::PointerIntPair<const UserClassDefn *, 1, bool> PrimaryBase;

	  /// FIXME: This should really use a SmallPtrMap, once we have one in LLVM :)
	  typedef llvm::DenseMap<const UserClassDefn *, CharUnits> BaseOffsetsMapTy;

	  /// BaseOffsets - Contains a map from base classes to their offset.
	  BaseOffsetsMapTy BaseOffsets;
  };

  /// ClassInfo - If the record layout is for a user defined class, this
  /// will have specific information about the record.
  UserClassLayoutInfo *ClassInfo;

  friend class ASTContext;

  ASTRecordLayout(ASTContext &Ctx, CharUnits size, CharUnits alignment,
		              CharUnits datasize, const uint64_t *fieldoffsets,
                  unsigned fieldcount);

  // Constructor for user-defined class records.
  typedef UserClassLayoutInfo::BaseOffsetsMapTy BaseOffsetsMapTy;
  ASTRecordLayout(ASTContext &Ctx, CharUnits size, CharUnits alignment,
		              CharUnits datasize,
		              const uint64_t *fieldoffsets,
		              unsigned fieldcount,
		              CharUnits SizeOfLargestEmptySubobject,
		              const UserClassDefn *PrimaryBase,
		              const BaseOffsetsMapTy& BaseOffsets,
		              const BaseOffsetsMapTy& VBaseOffsets);

  ~ASTRecordLayout() {}

  void Destroy(ASTContext &Ctx);
  
  ASTRecordLayout(const ASTRecordLayout&);   // DO NOT IMPLEMENT
  void operator=(const ASTRecordLayout&); // DO NOT IMPLEMENT

public:

  /// getAlignment - Get the record alignment in bits.
  CharUnits getAlignment() const { return Alignment; }

  /// getSize - Get the record size in bits.
  CharUnits getSize() const { return Size; }

  /// getFieldCount - Get the number of fields in the layout.
  unsigned getFieldCount() const { return FieldCount; }

  /// getFieldOffset - Get the offset of the given field index, in
  /// bits.
  uint64_t getFieldOffset(unsigned FieldNo) const {
    assert (FieldNo < FieldCount && "Invalid Field No");
    return FieldOffsets[FieldNo];
  }

  /// getDataSize() - Get the record data size, which is the record size
  /// without tail padding, in bits.
  CharUnits getDataSize() const {
    return DataSize;
  }

  /// getPrimaryBase - Get the primary base for this record.
	const UserClassDefn *getPrimaryBase() const {
		assert(ClassInfo && "Record layout does not have C++ specific info!");

		return ClassInfo->PrimaryBase.getPointer();
	}

	/// getBaseClassOffset - Get the offset, in chars, for the given base class.
	CharUnits getBaseClassOffset(const UserClassDefn *Base) const {
		assert(ClassInfo && "Record layout does not have C++ specific info!");
		assert(ClassInfo->BaseOffsets.count(Base) && "Did not find base!");

		return ClassInfo->BaseOffsets[Base];
	}

	/// getBaseClassOffsetInBits - Get the offset, in bits, for the given
	/// base class.
	uint64_t getBaseClassOffsetInBits(const UserClassDefn *Base) const {
		assert(ClassInfo && "Record layout does not have class specific info!");
		assert(ClassInfo->BaseOffsets.count(Base) && "Did not find base!");

		return getBaseClassOffset(Base).getQuantity()
				* Base->getASTContext().getCharWidth();
	}

	CharUnits getSizeOfLargestEmptySubobject() const {
		assert(ClassInfo && "Record layout does not have class specific info!");
		return ClassInfo->SizeOfLargestEmptySubobject;
	}
};

}  // end namespace mlang

#endif /* MLANG_AST_LAYOUT_INFO_H_ */
