//===--- ImportSearchOptions.h - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines ImportSearchOptions.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_IMPORTSEARCH_OPTIONS_H_
#define MLANG_FRONTEND_IMPORTSEARCH_OPTIONS_H_

namespace mlang {
class ImportSearchOptions {
public:
	std::string Sysroot;
	std::string ResourceDir;
	unsigned Verbose;
};
} // end namespace mlang

#endif /* MLANG_FRONTEND_IMPORTSEARCH_OPTIONS_H_ */
