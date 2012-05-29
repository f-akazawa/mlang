//===--- Utils.h - Misc utilities for the front-end -------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This header contains miscellaneous utilities for various front-end actions
//  which were split from Frontend to minimise Frontend's dependencies.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTENDTOOL_UTILS_H_
#define MLANG_FRONTENDTOOL_UTILS_H_

namespace mlang {

class CompilerInstance;

/// ExecuteCompilerInvocation - Execute the given actions described by the
/// compiler invocation object in the given compiler instance.
///
/// \return - True on success.
bool ExecuteCompilerInvocation(CompilerInstance *Mlang);

}  // end namespace mlang

#endif /* MLANG_FRONTENDTOOL_UTILS_H_ */
