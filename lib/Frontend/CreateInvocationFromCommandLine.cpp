//===--- CreateInvocationFromCommandLine.cpp - CompilerInvocation from Args ==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Construct a compiler invocation object for command line driver arguments
//
//===----------------------------------------------------------------------===//

#include "mlang/Frontend/Utils.h"
#include "mlang/Frontend/CompilerInstance.h"
#include "mlang/Frontend/DiagnosticOptions.h"
#include "mlang/Frontend/FrontendDiagnostic.h"
#include "mlang/Driver/Compilation.h"
#include "mlang/Driver/Driver.h"
#include "mlang/Driver/ArgList.h"
#include "mlang/Driver/Options.h"
#include "mlang/Driver/Tool.h"
#include "mlang/Driver/Job.h"
#include "llvm/Support/Host.h"
using namespace mlang;

/// createInvocationFromCommandLine - Construct a compiler invocation object for
/// a command line argument vector.
///
/// \return A CompilerInvocation, or 0 if none was built for the given
/// argument vector.
CompilerInvocation *
mlang::createInvocationFromCommandLine(llvm::ArrayRef<const char *> ArgList,
                                   llvm::IntrusiveRefCntPtr<Diagnostic> Diags) {
  if (!Diags.getPtr()) {
    // No diagnostics engine was provided, so create our own diagnostics object
    // with the default options.
    DiagnosticOptions DiagOpts;
    Diags = CompilerInstance::createDiagnostics(DiagOpts, ArgList.size(),
                                                ArgList.begin());
  }

  llvm::SmallVector<const char *, 16> Args;
  Args.push_back("<mlang>"); // FIXME: Remove dummy argument.
  Args.insert(Args.end(), ArgList.begin(), ArgList.end());

  // FIXME: Find a cleaner way to force the driver into restricted modes. We
  // also want to force it to use mlang.
  Args.push_back("-fsyntax-only");

  // FIXME: We shouldn't have to pass in the path info.
  driver::Driver TheDriver("mlang", llvm::sys::getHostTriple(),
                           "a.out", false, false, *Diags);

  // Don't check that inputs exist, they may have been remapped.
  TheDriver.setCheckInputsExist(false);

  llvm::OwningPtr<driver::Compilation> C(TheDriver.BuildCompilation(Args));

  // Just print the cc1 options if -### was present.
  if (C->getArgs().hasArg(driver::options::OPT__HASH_HASH_HASH)) {
    C->PrintJob(llvm::errs(), C->getJobs(), "\n", true);
    return 0;
  }

  // We expect to get back exactly one command job, if we didn't something
  // failed.
  const driver::JobList &Jobs = C->getJobs();
  if (Jobs.size() != 1 || !isa<driver::Command>(*Jobs.begin())) {
    llvm::SmallString<256> Msg;
    llvm::raw_svector_ostream OS(Msg);
    C->PrintJob(OS, C->getJobs(), "; ", true);
    Diags->Report(diag::err_fe_expected_compiler_job) << OS.str();
    return 0;
  }

  const driver::Command *Cmd = cast<driver::Command>(*Jobs.begin());
  if (llvm::StringRef(Cmd->getCreator().getName()) != "mlang") {
    Diags->Report(diag::err_fe_expected_clang_command);
    return 0;
  }

  const driver::ArgStringList &CCArgs = Cmd->getArguments();
  CompilerInvocation *CI = new CompilerInvocation();
  CompilerInvocation::CreateFromArgs(*CI,
                                     const_cast<const char **>(CCArgs.data()),
                                     const_cast<const char **>(CCArgs.data()) +
                                     CCArgs.size(),
                                     *Diags);
  return CI;
}
