//===--- Type.cpp - Type representation and manipulation ------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements type-related functionality.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/Type.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/CharUnits.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/PrettyPrinter.h"
#include "mlang/AST/TypeVisitor.h"
#include "mlang/Basic/Specifiers.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"
#include <algorithm>
using namespace mlang;

//===----------------------------------------------------------------------===//
// RawType
//===----------------------------------------------------------------------===//
/// isCharType - Return true if this is a char type.
bool RawType::isCharType() const {
	if (const SimpleNumericType *IT = dyn_cast<SimpleNumericType>(this)) {
		return IT->isChar();
	}

	return false;
}

/// isLogicalType - Return true if this is a boolean type.
bool RawType::isLogicalType() const {
	if (const SimpleNumericType *IT = dyn_cast<SimpleNumericType>(this)) {
		return IT->isLogical();
	}

	return false;
}

/// isNumericType - Return true if this is a char type.
bool RawType::isNumericType() const {
	if (const SimpleNumericType *IT = dyn_cast<SimpleNumericType>(this)) {
		return IT->isInteger() || IT->isFloatingPoint();
	}

	return false;
}

/// isIntegerType - Return true if this is an integer type.
bool RawType::isIntegerType() const {
	if (const SimpleNumericType *IT = dyn_cast<SimpleNumericType>(this)) {
		return IT->isInteger();
	}

	return false;
}

bool RawType::hasIntegerRepresentation() const {
  if (const VectorType *VT = dyn_cast<VectorType>(this))
    return VT->getElementType()->isIntegerType();
  else
    return isIntegerType();
}

/// isSignedIntegerType - Return true if this is an integer type that is
/// signed.
bool RawType::isSignedIntegerType() const {
  return isIntegerType() && !isUnsignedIntegerType();
}

bool RawType::hasSignedIntegerRepresentation() const {
  if (const VectorType *VT = dyn_cast<VectorType>(this))
    return VT->getElementType()->isSignedIntegerType();
  else
    return isSignedIntegerType();
}

/// isUnsignedIntegerType - Return true if this is an integer type that is
/// unsigned.
bool RawType::isUnsignedIntegerType() const {
	if (const SimpleNumericType *IT = dyn_cast<SimpleNumericType>(this)) {
		return IT->isUnsignedInteger();
	}

	return false;
}

bool RawType::hasUnsignedIntegerRepresentation() const {
  if (const VectorType *VT = dyn_cast<VectorType>(this))
    return VT->getElementType()->isUnsignedIntegerType();
  else
    return isUnsignedIntegerType();
}

/// isFloatingPointType - Return true if this is an float type.
bool RawType::isFloatingPointType() const {
	if (const SimpleNumericType *IT = dyn_cast<SimpleNumericType>(this)) {
		return IT->isFloatingPoint();
	}

	return false;
}

bool RawType::hasFloatingRepresentation() const {
  if (const VectorType *VT = dyn_cast<VectorType>(this))
    return VT->getElementType()->isFloatingPointType();
  else
    return isFloatingPointType();
}

bool RawType::isSpecifierType() const {
  // Note that this intentionally does not use the canonical type.
  switch (getTypeClass()) {
  case Struct:
  case Classdef:
    return true;
  default:
    return false;
  }
}

bool RawType::isPromotableIntegerType() const {
  if (const SimpleNumericType *BT = getAs<SimpleNumericType>())
    switch (BT->getKind()) {
    case SimpleNumericType::Logical:
    case SimpleNumericType::Char:
    case SimpleNumericType::Int8:
    case SimpleNumericType::UInt8:
    case SimpleNumericType::Int16:
    case SimpleNumericType::UInt16:
      return true;
    default:
      return false;
    }

  return false;
}

const StructType *RawType::getAsStructureType() const {
  // If this is directly a structure type, return it.
  if (const StructType *RT = dyn_cast<StructType>(this)) {
    if (isa<StructDefn>(RT->getDefn()))
      return RT;
  }

  return 0;
}
//===----------------------------------------------------------------------===//
// Type, TypeInfo
//===----------------------------------------------------------------------===//
const char *RawType::getTypeClassName() const {
	switch (TypeBits.TC) {
	#define ABSTRACT_TYPE(Derived, Base)
	#define TYPE(Derived, Base) case Derived: return #Derived;
	#include "mlang/AST/TypeNodes.def"
	}

	llvm_unreachable("Invalid type class.");
	return 0;
}

std::string Type::getAsString() const {
	std::string str = "return its type name";
	return str;
}

//===----------------------------------------------------------------------===//
// FunctionType, FunctionProtoType, FunctionNoProtoType
//===----------------------------------------------------------------------===//
void FunctionProtoType::InitFuncArguments(unsigned  numArgs, const Type *args, const ExtProtoInfo & epi)
{
    Type *argSlot = reinterpret_cast<Type*>(this + 1);
    for(unsigned i = 0;i != numArgs;++i){
        argSlot[i] = args[i];
    }
    Type *exnSlot = argSlot + numArgs;
    for(unsigned i = 0, e = epi.NumExceptions;i != e;++i){
        exnSlot[i] = epi.Exceptions[i];
    }
}

FunctionProtoType::FunctionProtoType(Type result, const Type *args,
		unsigned numArgs, const ExtProtoInfo &epi)
  : FunctionType(FunctionProto, result, epi.Variadic, epi.MultiOutput,
                epi.ExtInfo),
   NumArgs(numArgs), NumExceptions(epi.NumExceptions),
   HasExceptionSpec(epi.HasExceptionSpec),
   HasAnyExceptionSpec(epi.HasAnyExceptionSpec)
{
	InitFuncArguments(numArgs, args, epi);
}

void FunctionProtoType::Profile(llvm::FoldingSetNodeID &ID, Type Result,
        arg_type_iterator ArgTys, unsigned NumArgs,
        const ExtProtoInfo &epi, const ASTContext &Ctx) {
	ID.AddPointer(Result.getAsOpaquePtr());

	for (unsigned i = 0; i != NumArgs; ++i)
		ID.AddPointer(ArgTys[i].getAsOpaquePtr());

	ID.AddBoolean(epi.Variadic);
	ID.AddInteger(epi.MultiOutput);

	if (epi.HasExceptionSpec) {
		ID.AddBoolean(epi.HasAnyExceptionSpec);
		for (unsigned i = 0; i != epi.NumExceptions; ++i)
			ID.AddPointer(epi.Exceptions[i].getAsOpaquePtr());
	}
	epi.ExtInfo.Profile(ID);
}

void FunctionProtoType::Profile(llvm::FoldingSetNodeID &ID,
		const ASTContext &Ctx) {
	Profile(ID, getResultType(), arg_type_begin(), NumArgs,
			    getExtProtoInfo(), Ctx);
}


//===----------------------------------------------------------------------===//
// HeteroContainerType, StructType, CellType, ClassdefType
//===----------------------------------------------------------------------===//
bool HeteroContainerType::isBeingDefined() const {
	return defn->isBeingDefined();
}

bool StructType::classof(const HeteroContainerType *TT) {
  return isa<StructDefn>(TT->getDefn());
}

bool CellType::classof(const HeteroContainerType *TT) {
  return isa<CellDefn>(TT->getDefn());
}

bool ClassdefType::classof(const HeteroContainerType *TT) {
  return isa<UserClassDefn>(TT->getDefn());
}
