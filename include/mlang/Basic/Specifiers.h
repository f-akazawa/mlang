//===--- Specifiers.h - Type Specifiers for Mlang ---------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines Type Specifiers.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_STORAGE_CLASS_H_
#define MLANG_BASIC_STORAGE_CLASS_H_

namespace mlang {

/// AccessSpecifier - A object-orient programming access specifier (public,
/// private,protected), plus the special value "none" which means
/// different things in different contexts.
enum AccessSpecifier {
	AS_public, AS_protected, AS_private, AS_none
};

/// ExprValueKind - The categorization of expression values
enum ExprValueKind {
	/// An r-value expression (a gr-value in the C++0x taxonomy)
	/// produces a temporary value.
	VK_RValue,

	/// An l-value expression is a reference to an object with
	/// independent storage.
	VK_LValue
};

/// \brief Storage classes.
enum StorageClass {
	SC_None = 0x0,

	/// generic variable
	SC_Local = 0x01,

	// varargin, argn, .nargin., .nargout.
	// (FIXME -- is this really used now?)
	SC_Automatic = 0x02,

	// formal parameter
	SC_Formal = 0x04,

	// not listed or cleared (.nargin., .nargout.)
	SC_Hidden = 0x08,

	// inherited from parent scope; not cleared at function exit
	SC_Inherited = 0x10,

	// global (redirects to global scope)
	SC_Global = 0x20,

	// not cleared at function exit
	SC_Persistent = 0x40,

	// temporary variables forced into symbol table for parsing
	SC_Forced = 0x80
};

/// Checks whether the given storage class is legal for functions.
inline bool isLegalForFunction(StorageClass SC) {
  // return SC <= SC_PrivateExtern;
	return true;
}

/// Checks whether the given storage class is legal for variables.
inline bool isLegalForVariable(StorageClass SC) {
  return true;
}
} // end namespace mlang

#endif /* MLANG_BASIC_STORAGE_CLASS_H_ */
