//===-- CGBuilder.h - Choose IRBuilder implementation  ----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CGBUILDER_H_
#define MLANG_CODEGEN_CGBUILDER_H_

#include "llvm/Support/IRBuilder.h"

namespace mlang {
namespace CodeGen {

// Don't preserve names on values in an optimized build.
#ifdef NDEBUG
typedef llvm::IRBuilder<false> CGBuilderTy;
#else
typedef llvm::IRBuilder<> CGBuilderTy;
#endif

}  // end namespace CodeGen
}  // end namespace mlang

#endif /* MLANG_CODEGEN_CGBUILDER_H_ */
