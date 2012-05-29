//===--- CodeGen/ModuleBuilder.h - Build LLVM from ASTs ---------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ModuleBuilder interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_MODULEBUILDER_H_
#define MLANG_CODEGEN_MODULEBUILDER_H_

#include "mlang/AST/ASTConsumer.h"
#include <string>

namespace llvm {
  class LLVMContext;
  class Module;
}

namespace mlang {
  class Diagnostic;
  class LangOptions;
  class CodeGenOptions;

  class CodeGenerator : public ASTConsumer {
  public:
    virtual llvm::Module* GetModule() = 0;
    virtual llvm::Module* ReleaseModule() = 0;
  };

  /// CreateLLVMCodeGen - Create a CodeGenerator instance.
  /// It is the responsibility of the caller to call delete on
  /// the allocated CodeGenerator instance.
  CodeGenerator *CreateLLVMCodeGen(Diagnostic &Diags,
                                   const std::string &ModuleName,
                                   const CodeGenOptions &CGO,
                                   llvm::LLVMContext& C);
}

#endif /* MLANG_CODEGEN_MODULEBUILDER_H_ */
