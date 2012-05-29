//===-- CGException.h - Classes for exceptions IR generation ----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// These classes support the generation of LLVM IR for exceptions in
// C++ and Objective C.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CGEXCEPTION_H_
#define MLANG_CODEGEN_CGEXCEPTION_H_

/// EHScopeStack is defined in CodeGenFunction.h, but its
/// implementation is in this file and in CGException.cpp.
#include "CodeGenFunction.h"

namespace llvm {
  class Value;
  class BasicBlock;
}

namespace mlang {
namespace CodeGen {

/// The exceptions personality for a function.  When 
class EHPersonality {
  llvm::StringRef PersonalityFn;

  // If this is non-null, this personality requires a non-standard
  // function for rethrowing an exception after a catchall cleanup.
  // This function must have prototype void(void*).
  llvm::StringRef CatchallRethrowFn;

  EHPersonality(llvm::StringRef PersonalityFn,
                llvm::StringRef CatchallRethrowFn = llvm::StringRef())
    : PersonalityFn(PersonalityFn),
      CatchallRethrowFn(CatchallRethrowFn) {}

public:
  static const EHPersonality &get(const LangOptions &Lang);
  static const EHPersonality GNU_C;
  static const EHPersonality GNU_C_SJLJ;
  static const EHPersonality GNU_ObjC;
  static const EHPersonality NeXT_ObjC;
  static const EHPersonality GNU_CPlusPlus;
  static const EHPersonality GNU_CPlusPlus_SJLJ;

  llvm::StringRef getPersonalityFnName() const { return PersonalityFn; }
  llvm::StringRef getCatchallRethrowFnName() const { return CatchallRethrowFn; }
};

} // end namespace CodeGen
} // end namespace mlang

#endif /* MLANG_CODEGEN_CGEXCEPTION_H_ */
