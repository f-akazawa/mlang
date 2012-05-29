//===--- CodeGenAction.h - LLVM Code Generation Frontend Action -*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CODE_GEN_ACTION_H_
#define MLANG_CODEGEN_CODE_GEN_ACTION_H_

#include "mlang/Frontend/FrontendAction.h"
#include "llvm/ADT/OwningPtr.h"

namespace llvm {
  class LLVMContext;
  class Module;
}

namespace mlang {

class CodeGenAction : public ASTFrontendAction {
private:
  unsigned Act;
  llvm::OwningPtr<llvm::Module> TheModule;
  llvm::LLVMContext *VMContext;
  bool OwnsVMContext;

protected:
  /// Create a new code generation action.  If the optional \arg _VMContext
  /// parameter is supplied, the action uses it without taking ownership,
  /// otherwise it creates a fresh LLVM context and takes ownership.
  CodeGenAction(unsigned _Act, llvm::LLVMContext *_VMContext = 0);

  virtual bool hasIRSupport() const;

  virtual ASTConsumer *CreateASTConsumer(CompilerInstance &CI,
                                         llvm::StringRef InFile);

  virtual void ExecuteAction();

  virtual void EndSourceFileAction();

public:
  ~CodeGenAction();

  /// takeModule - Take the generated LLVM module, for use after the action has
  /// been run. The result may be null on failure.
  llvm::Module *takeModule();
};

class EmitAssemblyAction : public CodeGenAction {
public:
  EmitAssemblyAction();
};

class EmitBCAction : public CodeGenAction {
public:
  EmitBCAction();
};

class EmitLLVMAction : public CodeGenAction {
public:
  EmitLLVMAction();
};

class EmitLLVMOnlyAction : public CodeGenAction {
public:
  EmitLLVMOnlyAction();
};

class EmitCodeGenOnlyAction : public CodeGenAction {
public:
  EmitCodeGenOnlyAction();
};

class EmitObjAction : public CodeGenAction {
public:
  EmitObjAction();
};

}

#endif /* MLANG_CODEGEN_CODE_GEN_ACTION_H_ */
