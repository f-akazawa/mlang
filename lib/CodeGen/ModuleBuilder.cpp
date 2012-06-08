//===--- ModuleBuilder.cpp - Emit LLVM Code from ASTs ---------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This builds an AST and converts it to LLVM Code.
//
//===----------------------------------------------------------------------===//

#include "mlang/CodeGen/ModuleBuilder.h"
#include "CodeGenModule.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Expr.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Diag/Diagnostic.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Target/TargetData.h"
#include "llvm/ADT/OwningPtr.h"
using namespace mlang;

namespace {
  class CodeGeneratorImpl : public CodeGenerator {
    DiagnosticsEngine &Diags;
    llvm::OwningPtr<const llvm::TargetData> TD;
    ASTContext *Ctx;
    const CodeGenOptions CodeGenOpts;  // Intentionally copied in.
  protected:
    llvm::OwningPtr<llvm::Module> M;
    llvm::OwningPtr<CodeGen::CodeGenModule> Builder;
  public:
    CodeGeneratorImpl(DiagnosticsEngine &diags, const std::string& ModuleName,
                      const CodeGenOptions &CGO, llvm::LLVMContext& C)
      : Diags(diags), CodeGenOpts(CGO), M(new llvm::Module(ModuleName, C)) {}

    virtual ~CodeGeneratorImpl() {}

    virtual llvm::Module* GetModule() {
      return M.get();
    }

    virtual llvm::Module* ReleaseModule() {
      return M.take();
    }

    virtual void Initialize(ASTContext &Context) {
      Ctx = &Context;

      M->setTargetTriple(Ctx->Target.getTriple().getTriple());
      M->setDataLayout(Ctx->Target.getTargetDescription());
      TD.reset(new llvm::TargetData(Ctx->Target.getTargetDescription()));
      Builder.reset(new CodeGen::CodeGenModule(Context, CodeGenOpts,
                                               *M, *TD, Diags));
    }

    virtual void HandleTopLevelDefn(DefnGroupRef DG) {
      // Make sure to emit all elements of a Defn.
      for (DefnGroupRef::iterator I = DG.begin(), E = DG.end(); I != E; ++I)
        Builder->EmitTopLevelDefn(*I);
    }

    /// HandleTagDefnDefinition - This callback is invoked each time a TagDecl
    /// to (e.g. struct, union, enum, class) is completed. This allows the
    /// client hack on the type, which can occur at any point in the file
    /// (because these can be defined in declspecs).
    virtual void HandleTypeDefnDefinition(TypeDefn *D) {
      Builder->UpdateCompletedType(D);
      
      // In C++, we may have member functions that need to be emitted at this 
      // point.
      if (Ctx->getLangOptions().OOP) {
        for (DefnContext::defn_iterator M = D->defns_begin(),
                                     MEnd = D->defns_end();
             M != MEnd; ++M)
          if (ClassMethodDefn *Method = dyn_cast<ClassMethodDefn>(*M))
//            if (Method->hasAttr<UsedAttr>() ||
//                 Method->hasAttr<ConstructorAttr>())
              Builder->EmitTopLevelDefn(Method);
      }
    }

    virtual void HandleTranslationUnit(ASTContext &Ctx) {
      if (Diags.hasErrorOccurred()) {
        M.reset();
        return;
      }

      if (Builder)
        Builder->Release();
    }

    virtual void CompleteTentativeDefinition(VarDefn *D) {
      if (Diags.hasErrorOccurred())
        return;

      Builder->EmitTentativeDefinition(D);
    }

//    virtual void HandleVTable(UserClassDefn *RD, bool DefinitionRequired) {
//      if (Diags.hasErrorOccurred())
//        return;
//
//      Builder->EmitVTable(RD, DefinitionRequired);
//    }
  };
}

CodeGenerator *mlang::CreateLLVMCodeGen(DiagnosticsEngine &Diags,
                                        const std::string& ModuleName,
                                        const CodeGenOptions &CGO,
                                        llvm::LLVMContext& C) {
  return new CodeGeneratorImpl(Diags, ModuleName, CGO, C);
}
