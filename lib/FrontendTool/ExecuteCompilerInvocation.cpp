//===--- ExecuteCompilerInvocation.cpp ------------------------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file holds ExecuteCompilerInvocation(). It is split into its own file to
// minimize the impact of pulling in essentially everything else in Clang.
//
//===----------------------------------------------------------------------===//

#include "mlang/FrontendTool/Utils.h"
//#include "mlang/StaticAnalyzer/Frontend/FrontendActions.h"
#include "mlang/CodeGen/CodeGenAction.h"
#include "mlang/Driver/CC1Options.h"
#include "mlang/Driver/OptTable.h"
#include "mlang/Frontend/CompilerInvocation.h"
#include "mlang/Frontend/CompilerInstance.h"
#include "mlang/Frontend/FrontendActions.h"
#include "mlang/Frontend/FrontendDiagnostic.h"
//#include "clang/Frontend/FrontendPluginRegistry.h"
//#include "clang/Rewrite/FrontendActions.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/DynamicLibrary.h"
using namespace mlang;

static FrontendAction *CreateFrontendBaseAction(CompilerInstance &CI) {
  using namespace mlang::frontend;

  switch (CI.getFrontendOpts().ProgramAction) {
  default:
    llvm_unreachable("Invalid program action!");

  case ASTDump:                return new ASTDumpAction();
  case ASTDumpXML:             return new ASTDumpXMLAction();
  case ASTPrint:               return new ASTPrintAction();
  case ASTView:                return new ASTViewAction();
  case BoostCon:               return new BoostConAction();
  case CreateModule:           return 0;
  case DumpRawTokens:          return new DumpRawTokensAction();
  case DumpTokens:             return new DumpTokensAction();
  case EmitAssembly:           return new EmitAssemblyAction();
  case EmitBC:                 return new EmitBCAction();
//  case EmitHTML:               return new HTMLPrintAction();
  case EmitLLVM:               return new EmitLLVMAction();
  case EmitLLVMOnly:           return new EmitLLVMOnlyAction();
  case EmitCodeGenOnly:        return new EmitCodeGenOnlyAction();
  case EmitObj:                return new EmitObjAction();
//  case FixIt:                  return new FixItAction();
  case GeneratePTH:            return new GeneratePTHAction();
  case InitOnly:               return new InitOnlyAction();
  case ParseSyntaxOnly:        return new SyntaxOnlyAction();

  case PluginAction: {
//    for (FrontendPluginRegistry::iterator it =
//           FrontendPluginRegistry::begin(), ie = FrontendPluginRegistry::end();
//         it != ie; ++it) {
//      if (it->getName() == CI.getFrontendOpts().ActionName) {
//        llvm::OwningPtr<PluginASTAction> P(it->instantiate());
//        if (!P->ParseArgs(CI, CI.getFrontendOpts().PluginArgs))
//          return 0;
//        return P.take();
//      }
//    }

    CI.getDiagnostics().Report(diag::err_fe_invalid_plugin_name)
      << CI.getFrontendOpts().ActionName;
    return 0;
  }

  case PrintDefnContext:       return new DefnContextPrintAction();
  case PrintPreprocessedInput: return new PrintPreprocessedAction();
//  case RewriteTest:            return new RewriteTestAction();
//  case RunAnalysis:            return new ento::AnalysisAction();
  case RunPreprocessorOnly:    return new PreprocessOnlyAction();
  }
}

static FrontendAction *CreateFrontendAction(CompilerInstance &CI) {
  // Create the underlying action.
  FrontendAction *Act = CreateFrontendBaseAction(CI);
  if (!Act)
    return 0;

  // If there are any AST files to merge, create a frontend action
  // adaptor to perform the merge.
  if (!CI.getFrontendOpts().ASTMergeFiles.empty())
    Act = new ASTMergeAction(Act, &CI.getFrontendOpts().ASTMergeFiles[0],
                             CI.getFrontendOpts().ASTMergeFiles.size());

  return Act;
}

bool mlang::ExecuteCompilerInvocation(CompilerInstance *Mlang) {
  // Honor -help.
  if (Mlang->getFrontendOpts().ShowHelp) {
    llvm::OwningPtr<driver::OptTable> Opts(driver::createCC1OptTable());
    Opts->PrintHelp(llvm::outs(), "mlang -cc1",
                    "'Mlang' Compiler: http://mlang.gpumath.org");
    return 0;
  }
//
//  // Honor -analyzer-checker-help.
//  if (Mlang->getAnalyzerOpts().ShowCheckerHelp) {
//    ento::printCheckerHelp(llvm::outs());
//    return 0;
//  }

  // Honor -version.
  //
  // FIXME: Use a better -version message?
  if (Mlang->getFrontendOpts().ShowVersion) {
    llvm::cl::PrintVersionMessage();
    return 0;
  }

  // Honor -mllvm.
  //
  // FIXME: Remove this, one day.
  if (!Mlang->getFrontendOpts().LLVMArgs.empty()) {
    unsigned NumArgs = Mlang->getFrontendOpts().LLVMArgs.size();
    const char **Args = new const char*[NumArgs + 2];
    Args[0] = "mlang (LLVM option parsing)";
    for (unsigned i = 0; i != NumArgs; ++i)
      Args[i + 1] = Mlang->getFrontendOpts().LLVMArgs[i].c_str();
    Args[NumArgs + 1] = 0;
    llvm::cl::ParseCommandLineOptions(NumArgs + 1, const_cast<char **>(Args));
  }

  // Load any requested plugins.
  for (unsigned i = 0,
         e = Mlang->getFrontendOpts().Plugins.size(); i != e; ++i) {
    const std::string &Path = Mlang->getFrontendOpts().Plugins[i];
    std::string Error;
    if (llvm::sys::DynamicLibrary::LoadLibraryPermanently(Path.c_str(), &Error))
      Mlang->getDiagnostics().Report(diag::err_fe_unable_to_load_plugin)
        << Path << Error;
  }

  // If there were errors in processing arguments, don't do anything else.
  bool Success = false;
  if (!Mlang->getDiagnostics().hasErrorOccurred()) {
    // Create and execute the frontend action.
    llvm::OwningPtr<FrontendAction> Act(CreateFrontendAction(*Mlang));
    if (Act) {
      Success = Mlang->ExecuteAction(*Act);
      if (Mlang->getFrontendOpts().DisableFree)
        Act.take();
    }
  }

  return Success;
}
