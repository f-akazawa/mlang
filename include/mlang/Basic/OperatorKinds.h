//===--- OperatorKinds.h - OOP Overloaded Operators -------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines OOP overloaded operators.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_OPERATOR_KINDS_H_
#define MLANG_BASIC_OPERATOR_KINDS_H_

namespace mlang {

/// OverloadedOperatorKind - Enumeration specifying the different kinds of
/// C++ overloaded operators.
enum OverloadedOperatorKind {
  OO_None,                //< Not an overloaded operator
#define OVERLOADED_OPERATOR(Name,Spelling,Token,Unary,Binary,MemberOnly) \
  OO_##Name,
#include "mlang/Basic/OperatorKinds.def"
  NUM_OVERLOADED_OPERATORS
};

/// \brief Retrieve the spelling of the given overloaded operator, without 
/// the preceding "operator" keyword.
const char *getOperatorSpelling(OverloadedOperatorKind Operator);
  
} // end namespace mlang

#endif /* MLANG_BASIC_OPERATOR_KINDS_H_ */
