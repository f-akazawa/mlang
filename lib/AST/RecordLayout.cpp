//===--- RecordLayout.cpp - RecordLayout Calculation ------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements RecordLayout interface.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/ASTContext.h"
#include "mlang/AST/RecordLayout.h"


using namespace mlang;

void ASTRecordLayout::Destroy(ASTContext &Ctx) {
  if (FieldOffsets)
    Ctx.Deallocate(FieldOffsets);
  if (ClassInfo) {
    Ctx.Deallocate(ClassInfo);
    ClassInfo->~UserClassLayoutInfo();
  }
  this->~ASTRecordLayout();
  Ctx.Deallocate(this);
}

ASTRecordLayout::ASTRecordLayout(ASTContext &Ctx, CharUnits size,
		CharUnits alignment, CharUnits datasize, const uint64_t *fieldoffsets,
		unsigned fieldcount) :
	Size(size), DataSize(datasize), FieldOffsets(0), Alignment(alignment),
			FieldCount(fieldcount), ClassInfo(0) {
	if (FieldCount > 0) {
		FieldOffsets = new (Ctx) uint64_t[FieldCount];
		memcpy(FieldOffsets, fieldoffsets, FieldCount * sizeof(*FieldOffsets));
	}
}

// Constructor for user-defined class records.
ASTRecordLayout::ASTRecordLayout(ASTContext &Ctx, CharUnits size,
		CharUnits alignment, CharUnits datasize, const uint64_t *fieldoffsets,
		unsigned fieldcount, CharUnits SizeOfLargestEmptySubobject,
		const UserClassDefn *PrimaryBase, const BaseOffsetsMapTy& BaseOffsets,
		const BaseOffsetsMapTy& VBaseOffsets) :
	Size(size), DataSize(datasize), FieldOffsets(0), Alignment(alignment),
			FieldCount(fieldcount), ClassInfo(new (Ctx, 8) UserClassLayoutInfo) {
	if (FieldCount > 0) {
		FieldOffsets = new (Ctx) uint64_t[FieldCount];
		memcpy(FieldOffsets, fieldoffsets, FieldCount * sizeof(*FieldOffsets));
	}

	ClassInfo->PrimaryBase.setPointer(PrimaryBase);
	ClassInfo->SizeOfLargestEmptySubobject = SizeOfLargestEmptySubobject;
	ClassInfo->BaseOffsets = BaseOffsets;

#ifndef NDEBUG
	if (const UserClassDefn *PrimaryBase = getPrimaryBase()) {
		assert(getBaseClassOffsetInBits(PrimaryBase) == 0 &&
				"Primary base must be at offset 0!");
	}
#endif
}
