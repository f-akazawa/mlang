//===--- ASTCommon.cpp - Common stuff for ASTReader/ASTWriter----*- C++ -*-===//
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

#include "ASTCommon.h"
#include "mlang/Serialization/ASTDeserializationListener.h"
#include "mlang/Basic/IdentifierTable.h"
#include "llvm/ADT/StringExtras.h"

using namespace mlang;

// Give ASTDeserializationListener's VTable a home.
ASTDeserializationListener::~ASTDeserializationListener() { }

serialization::TypeIdx
serialization::TypeIdxFromBuiltin(const SimpleNumericType *BT) {
  unsigned ID = 0;
  switch (BT->getKind()) {
  case SimpleNumericType::Logical:    ID = PREDEF_TYPE_LOGICAL_ID;   break;
  case SimpleNumericType::Char:       ID = PREDEF_TYPE_CHAR_ID;      break;
  case SimpleNumericType::Int8:       ID = PREDEF_TYPE_INT8_ID;      break;
  case SimpleNumericType::Int16:      ID = PREDEF_TYPE_INT16_ID;     break;
  case SimpleNumericType::Int32:      ID = PREDEF_TYPE_INT32_ID;     break;
  case SimpleNumericType::Int64:      ID = PREDEF_TYPE_INT64_ID;     break;
  case SimpleNumericType::UInt8:      ID = PREDEF_TYPE_UINT8_ID;     break;
  case SimpleNumericType::UInt16:     ID = PREDEF_TYPE_UINT16_ID;    break;
  case SimpleNumericType::UInt32:     ID = PREDEF_TYPE_UINT32_ID;    break;
  case SimpleNumericType::UInt64:     ID = PREDEF_TYPE_UINT64_ID;    break;
  case SimpleNumericType::Single:     ID = PREDEF_TYPE_SINGLE_ID;    break;
  case SimpleNumericType::Double:     ID = PREDEF_TYPE_DOUBLE_ID;    break;
  }

  return TypeIdx(ID);
}
