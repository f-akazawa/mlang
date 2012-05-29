//===- ASTCommon.h - Common stuff for ASTReader/ASTWriter -*- C++ -*-=========//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines common functions that both ASTReader and ASTWriter use.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SERIALIZATION_LIB_AST_COMMON_H_
#define MLANG_SERIALIZATION_LIB_AST_COMMON_H_

#include "mlang/Serialization/ASTBitCodes.h"

namespace mlang {

namespace serialization {

TypeIdx TypeIdxFromBuiltin(const SimpleNumericType *BT);

template <typename IdxForTypeTy>
TypeID MakeTypeID(Type T, IdxForTypeTy IdxForType) {
  if (T.isNull())
    return PREDEF_TYPE_NULL_ID;

  unsigned FastQuals = T.getFastTypeInfo();
  T.removeFastTypeInfo();

  if (T.hasNonFastTypeInfo())
    return IdxForType(T).asTypeID(FastQuals);

  assert(!T.hasTypeInfo());

  if (const SimpleNumericType *BT = dyn_cast<SimpleNumericType>(T.getRawTypePtr()))
    return TypeIdxFromBuiltin(BT).asTypeID(FastQuals);

  return IdxForType(T).asTypeID(FastQuals);
}

} // namespace serialization

} // namespace mlang

#endif /* MLANG_SERIALIZATION_LIB_AST_COMMON_H_ */
