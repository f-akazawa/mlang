//===--- CompilerInvocation.cpp -------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "mlang/Frontend/CompilerInvocation.h"
#include "mlang/Diag/Diagnostic.h"
//#include "mlang/Basic/Version.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Driver/Arg.h"
#include "mlang/Driver/ArgList.h"
#include "mlang/Driver/CC1Options.h"
#include "mlang/Driver/DriverDiagnostic.h"
#include "mlang/Driver/OptTable.h"
#include "mlang/Driver/Option.h"
#include "mlang/Frontend/LangStandard.h"
#include "mlang/Serialization/ASTReader.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
using namespace mlang;

//===----------------------------------------------------------------------===//
// Serialization (to args)
//===----------------------------------------------------------------------===//

static void CodeGenOptsToArgs(const CodeGenOptions &Opts,
                              std::vector<std::string> &Res) {
  if (Opts.DebugInfo)
    Res.push_back("-g");
  if (Opts.DisableLLVMOpts)
    Res.push_back("-disable-llvm-optzns");
  if (Opts.DisableRedZone)
    Res.push_back("-disable-red-zone");
  if (!Opts.DwarfDebugFlags.empty()) {
    Res.push_back("-dwarf-debug-flags");
    Res.push_back(Opts.DwarfDebugFlags);
  }
//  if (Opts.EmitGcovArcs)
//    Res.push_back("-femit-coverage-data");
//  if (Opts.EmitGcovNotes)
//    Res.push_back("-femit-coverage-notes");
  if (!Opts.MergeAllConstants)
    Res.push_back("-fno-merge-all-constants");
  if (Opts.NoCommon)
    Res.push_back("-fno-common");
//  if (Opts.ForbidGuardVariables)
//    Res.push_back("-fforbid-guard-variables");
  if (Opts.NoImplicitFloat)
    Res.push_back("-no-implicit-float");
  if (Opts.OmitLeafFramePointer)
    Res.push_back("-momit-leaf-frame-pointer");
  if (Opts.OptimizeSize) {
    assert(Opts.OptimizationLevel == 2 && "Invalid options!");
    Opts.OptimizeSize == 1 ? Res.push_back("-Os") : Res.push_back("-Oz");
  } else if (Opts.OptimizationLevel != 0)
    Res.push_back("-O" + llvm::utostr(Opts.OptimizationLevel));
  if (!Opts.MainFileName.empty()) {
    Res.push_back("-main-file-name");
    Res.push_back(Opts.MainFileName);
  }
  // SimplifyLibCalls is only derived.
  // TimePasses is only derived.
  // UnitAtATime is unused.
  // Inlining is only derived.

  // UnrollLoops is derived, but also accepts an option, no
  // harm in pushing it back here.
  if (Opts.UnrollLoops)
    Res.push_back("-funroll-loops");
  if (Opts.DataSections)
    Res.push_back("-fdata-sections");
  if (Opts.FunctionSections)
    Res.push_back("-ffunction-sections");
  if (Opts.AsmVerbose)
    Res.push_back("-masm-verbose");
  if (!Opts.CodeModel.empty()) {
    Res.push_back("-mcode-model");
    Res.push_back(Opts.CodeModel);
  }
  if (!Opts.CXAAtExit)
    Res.push_back("-fno-use-cxa-atexit");
  if (Opts.CXXCtorDtorAliases)
    Res.push_back("-mconstructor-aliases");
  if (!Opts.DebugPass.empty()) {
    Res.push_back("-mdebug-pass");
    Res.push_back(Opts.DebugPass);
  }
  if (Opts.DisableFPElim)
    Res.push_back("-mdisable-fp-elim");
  if (!Opts.FloatABI.empty()) {
    Res.push_back("-mfloat-abi");
    Res.push_back(Opts.FloatABI);
  }
  if (!Opts.LimitFloatPrecision.empty()) {
    Res.push_back("-mlimit-float-precision");
    Res.push_back(Opts.LimitFloatPrecision);
  }
  if (Opts.NoZeroInitializedInBSS)
    Res.push_back("-mno-zero-initialized-bss");
//  if (Opts.NumRegisterParameters) {
//    Res.push_back("-mregparm");
//    Res.push_back(llvm::utostr(Opts.NumRegisterParameters));
//  }
  if (Opts.RelaxAll)
    Res.push_back("-mrelax-all");
//  if (Opts.NoDwarf2CFIAsm)
//    Res.push_back("-fno-dwarf2-cfi-asm");
  if (Opts.SoftFloat)
    Res.push_back("-msoft-float");
  if (Opts.UnwindTables)
    Res.push_back("-munwind-tables");
  if (Opts.RelocationModel != "pic") {
    Res.push_back("-mrelocation-model");
    Res.push_back(Opts.RelocationModel);
  }
  if (!Opts.VerifyModule)
    Res.push_back("-disable-llvm-verifier");
//  for (unsigned i = 0, e = Opts.BackendOptions.size(); i != e; ++i) {
//    Res.push_back("-backend-option");
//    Res.push_back(Opts.BackendOptions[i]);
//  }
}

static void DependencyOutputOptsToArgs(const DependencyOutputOptions &Opts,
                                       std::vector<std::string> &Res) {
  if (Opts.IncludeSystemHeaders)
    Res.push_back("-sys-header-deps");
  if (Opts.ShowHeaderIncludes)
    Res.push_back("-H");
  if (!Opts.HeaderIncludeOutputFile.empty()) {
    Res.push_back("-header-include-file");
    Res.push_back(Opts.HeaderIncludeOutputFile);
  }
  if (Opts.UsePhonyTargets)
    Res.push_back("-MP");
  if (!Opts.OutputFile.empty()) {
    Res.push_back("-dependency-file");
    Res.push_back(Opts.OutputFile);
  }
  for (unsigned i = 0, e = Opts.Targets.size(); i != e; ++i) {
    Res.push_back("-MT");
    Res.push_back(Opts.Targets[i]);
  }
}

static void DiagnosticOptsToArgs(const DiagnosticOptions &Opts,
                                 std::vector<std::string> &Res) {
  if (Opts.IgnoreWarnings)
    Res.push_back("-w");
  if (Opts.NoRewriteMacros)
    Res.push_back("-Wno-rewrite-macros");
  if (Opts.Pedantic)
    Res.push_back("-pedantic");
  if (Opts.PedanticErrors)
    Res.push_back("-pedantic-errors");
  if (!Opts.ShowColumn)
    Res.push_back("-fno-show-column");
  if (!Opts.ShowLocation)
    Res.push_back("-fno-show-source-location");
  if (!Opts.ShowCarets)
    Res.push_back("-fno-caret-diagnostics");
  if (!Opts.ShowFixits)
    Res.push_back("-fno-diagnostics-fixit-info");
  if (Opts.ShowSourceRanges)
    Res.push_back("-fdiagnostics-print-source-range-info");
  if (Opts.ShowParseableFixits)
    Res.push_back("-fdiagnostics-parseable-fixits");
  if (Opts.ShowColors)
    Res.push_back("-fcolor-diagnostics");
  if (Opts.VerifyDiagnostics)
    Res.push_back("-verify");
//  if (Opts.ShowNames)
//    Res.push_back("-fdiagnostics-show-name");
  if (Opts.ShowOptionNames)
    Res.push_back("-fdiagnostics-show-option");
  if (Opts.ShowCategories == 1)
    Res.push_back("-fdiagnostics-show-category=id");
  else if (Opts.ShowCategories == 2)
    Res.push_back("-fdiagnostics-show-category=name");
  if (Opts.ErrorLimit) {
    Res.push_back("-ferror-limit");
    Res.push_back(llvm::utostr(Opts.ErrorLimit));
  }
//  if (!Opts.DiagnosticLogFile.empty()) {
//    Res.push_back("-diagnostic-log-file");
//    Res.push_back(Opts.DiagnosticLogFile);
//  }
  if (Opts.MacroBacktraceLimit
                        != DiagnosticOptions::DefaultMacroBacktraceLimit) {
    Res.push_back("-fmacro-backtrace-limit");
    Res.push_back(llvm::utostr(Opts.MacroBacktraceLimit));
  }
  if (Opts.TemplateBacktraceLimit
                        != DiagnosticOptions::DefaultTemplateBacktraceLimit) {
    Res.push_back("-ftemplate-backtrace-limit");
    Res.push_back(llvm::utostr(Opts.TemplateBacktraceLimit));
  }

  if (Opts.TabStop != DiagnosticOptions::DefaultTabStop) {
    Res.push_back("-ftabstop");
    Res.push_back(llvm::utostr(Opts.TabStop));
  }
  if (Opts.MessageLength) {
    Res.push_back("-fmessage-length");
    Res.push_back(llvm::utostr(Opts.MessageLength));
  }
  if (!Opts.DumpBuildInformation.empty()) {
    Res.push_back("-dump-build-information");
    Res.push_back(Opts.DumpBuildInformation);
  }
  for (unsigned i = 0, e = Opts.Warnings.size(); i != e; ++i)
    Res.push_back("-W" + Opts.Warnings[i]);
}

static const char *getInputKindName(InputKind Kind) {
  switch (Kind) {
  case IK_None:              break;
  case IK_AST:               return "ast";
  case IK_Asm:               return "assembler-with-cpp";
  case IK_C:                 return "c";
  case IK_CXX:               return "c++";
  case IK_LLVM_IR:           return "ir";
  case IK_ObjC:              return "objective-c";
  case IK_ObjCXX:            return "objective-c++";
  case IK_OpenCL:            return "cl";
  case IK_CUDA:              return "cuda";
  case IK_PreprocessedC:     return "cpp-output";
  case IK_PreprocessedCXX:   return "c++-cpp-output";
  case IK_PreprocessedObjC:  return "objective-c-cpp-output";
  case IK_PreprocessedObjCXX:return "objective-c++-cpp-output";
  }

  llvm_unreachable("Unexpected language kind!");
  return 0;
}

static const char *getActionName(frontend::ActionKind Kind) {
  switch (Kind) {
  case frontend::PluginAction:
    llvm_unreachable("Invalid kind!");

  case frontend::ASTDump:                return "-ast-dump";
  case frontend::ASTDumpXML:             return "-ast-dump-xml";
  case frontend::ASTPrint:               return "-ast-print";
  case frontend::ASTView:                return "-ast-view";
  case frontend::BoostCon:               return "-boostcon";
  case frontend::CreateModule:           return "-create-module";
  case frontend::DumpRawTokens:          return "-dump-raw-tokens";
  case frontend::DumpTokens:             return "-dump-tokens";
  case frontend::EmitAssembly:           return "-S";
  case frontend::EmitBC:                 return "-emit-llvm-bc";
  case frontend::EmitHTML:               return "-emit-html";
  case frontend::EmitLLVM:               return "-emit-llvm";
  case frontend::EmitLLVMOnly:           return "-emit-llvm-only";
  case frontend::EmitCodeGenOnly:        return "-emit-codegen-only";
  case frontend::EmitObj:                return "-emit-obj";
  case frontend::FixIt:                  return "-fixit";
  case frontend::GeneratePTH:            return "-emit-pth";
  case frontend::InitOnly:               return "-init-only";
  case frontend::ParseSyntaxOnly:        return "-fsyntax-only";
  case frontend::PrintDefnContext:       return "-print-decl-contexts";
  case frontend::PrintPreprocessedInput: return "-E";
  case frontend::RewriteTest:            return "-rewrite-test";
  case frontend::RunAnalysis:            return "-analyze";
  case frontend::RunPreprocessorOnly:    return "-Eonly";
  }

  llvm_unreachable("Unexpected language kind!");
  return 0;
}

static void FileSystemOptsToArgs(const FileSystemOptions &Opts,
                                 std::vector<std::string> &Res) {
  if (!Opts.WorkingDir.empty()) {
    Res.push_back("-working-directory");
    Res.push_back(Opts.WorkingDir);
  }
}

static void FrontendOptsToArgs(const FrontendOptions &Opts,
                               std::vector<std::string> &Res) {
  if (Opts.DisableFree)
    Res.push_back("-disable-free");
  if (Opts.ShowHelp)
    Res.push_back("-help");
  if (Opts.ShowMacrosInCodeCompletion)
    Res.push_back("-code-completion-macros");
  if (Opts.ShowCodePatternsInCodeCompletion)
    Res.push_back("-code-completion-patterns");
  if (!Opts.ShowGlobalSymbolsInCodeCompletion)
    Res.push_back("-no-code-completion-globals");
  if (Opts.ShowStats)
    Res.push_back("-print-stats");
  if (Opts.ShowTimers)
    Res.push_back("-ftime-report");
  if (Opts.ShowVersion)
    Res.push_back("-version");
  if (Opts.FixWhatYouCan)
    Res.push_back("-fix-what-you-can");

  bool NeedLang = false;
  for (unsigned i = 0, e = Opts.Inputs.size(); i != e; ++i)
    if (FrontendOptions::getInputKindForExtension(Opts.Inputs[i].second) !=
        Opts.Inputs[i].first)
      NeedLang = true;
  if (NeedLang) {
    Res.push_back("-x");
    Res.push_back(getInputKindName(Opts.Inputs[0].first));
  }
  for (unsigned i = 0, e = Opts.Inputs.size(); i != e; ++i) {
    assert((!NeedLang || Opts.Inputs[i].first == Opts.Inputs[0].first) &&
           "Unable to represent this input vector!");
    Res.push_back(Opts.Inputs[i].second);
  }

  if (!Opts.OutputFile.empty()) {
    Res.push_back("-o");
    Res.push_back(Opts.OutputFile);
  }
  if (!Opts.CodeCompletionAt.FileName.empty()) {
    Res.push_back("-code-completion-at");
    Res.push_back(Opts.CodeCompletionAt.FileName + ":" +
                  llvm::utostr(Opts.CodeCompletionAt.Line) + ":" +
                  llvm::utostr(Opts.CodeCompletionAt.Column));
  }
  if (Opts.ProgramAction != frontend::PluginAction)
    Res.push_back(getActionName(Opts.ProgramAction));
  if (!Opts.ActionName.empty()) {
    Res.push_back("-plugin");
    Res.push_back(Opts.ActionName);
    for(unsigned i = 0, e = Opts.PluginArgs.size(); i != e; ++i) {
      Res.push_back("-plugin-arg-" + Opts.ActionName);
      Res.push_back(Opts.PluginArgs[i]);
    }
  }
  for (unsigned i = 0, e = Opts.Plugins.size(); i != e; ++i) {
    Res.push_back("-load");
    Res.push_back(Opts.Plugins[i]);
  }
  for (unsigned i = 0, e = Opts.AddPluginActions.size(); i != e; ++i) {
    Res.push_back("-add-plugin");
    Res.push_back(Opts.AddPluginActions[i]);
    for(unsigned ai = 0, ae = Opts.AddPluginArgs.size(); ai != ae; ++ai) {
      Res.push_back("-plugin-arg-" + Opts.AddPluginActions[i]);
      Res.push_back(Opts.AddPluginArgs[i][ai]);
    }
  }
  for (unsigned i = 0, e = Opts.ASTMergeFiles.size(); i != e; ++i) {
    Res.push_back("-ast-merge");
    Res.push_back(Opts.ASTMergeFiles[i]);
  }
  for (unsigned i = 0, e = Opts.Modules.size(); i != e; ++i) {
    Res.push_back("-import-module");
    Res.push_back(Opts.Modules[i]);
  }
  for (unsigned i = 0, e = Opts.LLVMArgs.size(); i != e; ++i) {
    Res.push_back("-mllvm");
    Res.push_back(Opts.LLVMArgs[i]);
  }
}

static void ImportSearchOptsToArgs(const ImportSearchOptions &Opts,
                                   std::vector<std::string> &Res) {

}

static void LangOptsToArgs(const LangOptions &Opts,
                           std::vector<std::string> &Res) {
  LangOptions DefaultLangOpts;

  // FIXME: Need to set -std to get all the implicit options.

  // FIXME: We want to only pass options relative to the defaults, which
  // requires constructing a target. :(
  //
  // It would be better to push the all target specific choices into the driver,
  // so that everything below that was more uniform.

  
}

static void PreprocessorOptsToArgs(const PreprocessorOptions &Opts,
                                   std::vector<std::string> &Res) {
  for (unsigned i = 0, e = Opts.Imports.size(); i != e; ++i) {
    // FIXME: We need to avoid reincluding the implicit PCH and PTH includes.
    Res.push_back("-import");
    Res.push_back(Opts.Imports[i]);
  }
  if (!Opts.UsePredefines)
    Res.push_back("-undef");
  if (Opts.DetailedRecord)
    Res.push_back("-detailed-preprocessing-record");
  if (!Opts.ImplicitPTHInclude.empty()) {
    Res.push_back("-include-pth");
    Res.push_back(Opts.ImplicitPTHInclude);
  }
  if (!Opts.TokenCache.empty()) {
    if (Opts.ImplicitPTHInclude.empty()) {
      Res.push_back("-token-cache");
      Res.push_back(Opts.TokenCache);
    } else
      assert(Opts.ImplicitPTHInclude == Opts.TokenCache &&
             "Unsupported option combination!");
  }
  for (unsigned i = 0, e = Opts.ChainedImports.size(); i != e; ++i) {
    Res.push_back("-chain-include");
    Res.push_back(Opts.ChainedImports[i]);
  }
  for (unsigned i = 0, e = Opts.RemappedFiles.size(); i != e; ++i) {
    Res.push_back("-remap-file");
    Res.push_back(Opts.RemappedFiles[i].first + ";" +
                  Opts.RemappedFiles[i].second);
  }
}

static void PreprocessorOutputOptsToArgs(const PreprocessorOutputOptions &Opts,
                                         std::vector<std::string> &Res) {
  if (!Opts.ShowCPP)
    llvm::report_fatal_error("Invalid option combination!");

  if (Opts.ShowCPP)
    Res.push_back("-dD");

  if (!Opts.ShowLineMarkers)
    Res.push_back("-P");
  if (Opts.ShowComments)
    Res.push_back("-C");
}

static void TargetOptsToArgs(const TargetOptions &Opts,
                             std::vector<std::string> &Res) {
  Res.push_back("-triple");
  Res.push_back(Opts.Triple);
  if (!Opts.CPU.empty()) {
    Res.push_back("-target-cpu");
    Res.push_back(Opts.CPU);
  }
  if (!Opts.ABI.empty()) {
    Res.push_back("-target-abi");
    Res.push_back(Opts.ABI);
  }
  if (!Opts.LinkerVersion.empty()) {
    Res.push_back("-target-linker-version");
    Res.push_back(Opts.LinkerVersion);
  }
  if (!Opts.GmatABI.empty()) {
    Res.push_back("-gmat-abi");
    Res.push_back(Opts.GmatABI);
  }
  for (unsigned i = 0, e = Opts.Features.size(); i != e; ++i) {
    Res.push_back("-target-feature");
    Res.push_back(Opts.Features[i]);
  }
}

void CompilerInvocation::toArgs(std::vector<std::string> &Res) {
//  AnalyzerOptsToArgs(getAnalyzerOpts(), Res);
  CodeGenOptsToArgs(getCodeGenOpts(), Res);
  DependencyOutputOptsToArgs(getDependencyOutputOpts(), Res);
  DiagnosticOptsToArgs(getDiagnosticOpts(), Res);
  FileSystemOptsToArgs(getFileSystemOpts(), Res);
  FrontendOptsToArgs(getFrontendOpts(), Res);
  ImportSearchOptsToArgs(getImportSearchOpts(), Res);
  LangOptsToArgs(getLangOpts(), Res);
  PreprocessorOptsToArgs(getPreprocessorOpts(), Res);
  PreprocessorOutputOptsToArgs(getPreprocessorOutputOpts(), Res);
  TargetOptsToArgs(getTargetOpts(), Res);
}

//===----------------------------------------------------------------------===//
// Deserialization (to args)
//===----------------------------------------------------------------------===//

using namespace mlang::driver;
using namespace mlang::driver::cc1options;

//

static unsigned getOptimizationLevel(ArgList &Args, InputKind IK,
                                     Diagnostic &Diags) {
  unsigned DefaultOpt = 0;
  if (IK == IK_OpenCL && !Args.hasArg(OPT_cl_opt_disable))
    DefaultOpt = 2;
  // -Os/-Oz implies -O2
  return (Args.hasArg(OPT_Os) || Args.hasArg (OPT_Oz)) ? 2 :
    Args.getLastArgIntValue(OPT_O, DefaultOpt, Diags);
}

static void ParseCodeGenArgs(CodeGenOptions &Opts, ArgList &Args, InputKind IK,
                             Diagnostic &Diags) {
  using namespace cc1options;

  Opts.OptimizationLevel = getOptimizationLevel(Args, IK, Diags);
  if (Opts.OptimizationLevel > 3) {
    Diags.Report(diag::err_drv_invalid_value)
      << Args.getLastArg(OPT_O)->getAsString(Args) << Opts.OptimizationLevel;
    Opts.OptimizationLevel = 3;
  }

  // We must always run at least the always inlining pass.
  Opts.Inlining = (Opts.OptimizationLevel > 1) ? CodeGenOptions::NormalInlining
    : CodeGenOptions::OnlyAlwaysInlining;

  Opts.DebugInfo = Args.hasArg(OPT_g);
  Opts.LimitDebugInfo = Args.hasArg(OPT_flimit_debug_info);
  Opts.DisableLLVMOpts = Args.hasArg(OPT_disable_llvm_optzns);
  Opts.DisableRedZone = Args.hasArg(OPT_disable_red_zone);
  Opts.RelaxedAliasing = Args.hasArg(OPT_relaxed_aliasing);
  Opts.DwarfDebugFlags = Args.getLastArgValue(OPT_dwarf_debug_flags);
  Opts.MergeAllConstants = !Args.hasArg(OPT_fno_merge_all_constants);
  Opts.NoCommon = Args.hasArg(OPT_fno_common);
  Opts.NoImplicitFloat = Args.hasArg(OPT_no_implicit_float);
  Opts.OptimizeSize = Args.hasArg(OPT_Os);
  Opts.OptimizeSize = Args.hasArg(OPT_Oz) ? 2 : Opts.OptimizeSize;
  Opts.SimplifyLibCalls = !(Args.hasArg(OPT_fno_builtin) ||
                            Args.hasArg(OPT_ffreestanding));
  Opts.UnrollLoops = Args.hasArg(OPT_funroll_loops) ||
                     (Opts.OptimizationLevel > 1 && !Opts.OptimizeSize);

  Opts.AsmVerbose = Args.hasArg(OPT_masm_verbose);
  Opts.CXAAtExit = !Args.hasArg(OPT_fno_use_cxa_atexit);
  Opts.CXXCtorDtorAliases = Args.hasArg(OPT_mconstructor_aliases);
  Opts.CodeModel = Args.getLastArgValue(OPT_mcode_model);
  Opts.DebugPass = Args.getLastArgValue(OPT_mdebug_pass);
  Opts.DisableFPElim = Args.hasArg(OPT_mdisable_fp_elim);
  Opts.FloatABI = Args.getLastArgValue(OPT_mfloat_abi);
  Opts.HiddenWeakVTables = Args.hasArg(OPT_fhidden_weak_vtables);
  Opts.LessPreciseFPMAD = Args.hasArg(OPT_cl_mad_enable);
  Opts.LimitFloatPrecision = Args.getLastArgValue(OPT_mlimit_float_precision);
  Opts.NoInfsFPMath = Opts.NoNaNsFPMath = Args.hasArg(OPT_cl_finite_math_only)||
                                          Args.hasArg(OPT_cl_fast_relaxed_math);
  Opts.NoZeroInitializedInBSS = Args.hasArg(OPT_mno_zero_initialized_in_bss);
  Opts.BackendOptions = Args.getAllArgValues(OPT_backend_option);
  Opts.NumRegisterParameters = Args.getLastArgIntValue(OPT_mregparm, 0, Diags);
  Opts.NoExecStack = Args.hasArg(OPT_mno_exec_stack);
  Opts.RelaxAll = Args.hasArg(OPT_mrelax_all);
  Opts.OmitLeafFramePointer = Args.hasArg(OPT_momit_leaf_frame_pointer);
  Opts.SoftFloat = Args.hasArg(OPT_msoft_float);
  Opts.UnsafeFPMath = Args.hasArg(OPT_cl_unsafe_math_optimizations) ||
                      Args.hasArg(OPT_cl_fast_relaxed_math);
  Opts.UnwindTables = Args.hasArg(OPT_munwind_tables);
  Opts.RelocationModel = Args.getLastArgValue(OPT_mrelocation_model, "pic");

  Opts.FunctionSections = Args.hasArg(OPT_ffunction_sections);
  Opts.DataSections = Args.hasArg(OPT_fdata_sections);

  Opts.MainFileName = Args.getLastArgValue(OPT_main_file_name);
  Opts.VerifyModule = !Args.hasArg(OPT_disable_llvm_verifier);

  Opts.InstrumentFunctions = Args.hasArg(OPT_finstrument_functions);
  Opts.InstrumentForProfiling = Args.hasArg(OPT_pg);
  Opts.EmitGcovArcs = Args.hasArg(OPT_femit_coverage_data);
  Opts.EmitGcovNotes = Args.hasArg(OPT_femit_coverage_notes);
  Opts.CoverageFile = Args.getLastArgValue(OPT_coverage_file);
}

static void ParseDependencyOutputArgs(DependencyOutputOptions &Opts,
                                      ArgList &Args) {
  using namespace cc1options;
  Opts.OutputFile = Args.getLastArgValue(OPT_dependency_file);
  Opts.Targets = Args.getAllArgValues(OPT_MT);
  Opts.IncludeSystemHeaders = Args.hasArg(OPT_sys_header_deps);
  Opts.UsePhonyTargets = Args.hasArg(OPT_MP);
  Opts.ShowHeaderIncludes = Args.hasArg(OPT_H);
  Opts.HeaderIncludeOutputFile = Args.getLastArgValue(OPT_header_include_file);
}

static void ParseDiagnosticArgs(DiagnosticOptions &Opts, ArgList &Args,
                                Diagnostic &Diags) {
  using namespace cc1options;
  Opts.DiagnosticLogFile = Args.getLastArgValue(OPT_diagnostic_log_file);
  Opts.IgnoreWarnings = Args.hasArg(OPT_w);
  Opts.NoRewriteMacros = Args.hasArg(OPT_Wno_rewrite_macros);
  Opts.Pedantic = Args.hasArg(OPT_pedantic);
  Opts.PedanticErrors = Args.hasArg(OPT_pedantic_errors);
  Opts.ShowCarets = !Args.hasArg(OPT_fno_caret_diagnostics);
  Opts.ShowColors = Args.hasArg(OPT_fcolor_diagnostics);
  Opts.ShowColumn = Args.hasFlag(OPT_fshow_column,
                                 OPT_fno_show_column,
                                 /*Default=*/true);
  Opts.ShowFixits = !Args.hasArg(OPT_fno_diagnostics_fixit_info);
  Opts.ShowLocation = !Args.hasArg(OPT_fno_show_source_location);
  Opts.ShowNames = Args.hasArg(OPT_fdiagnostics_show_name);
  Opts.ShowOptionNames = Args.hasArg(OPT_fdiagnostics_show_option);

  llvm::StringRef ShowOverloads =
    Args.getLastArgValue(OPT_fshow_overloads_EQ, "all");
  if (ShowOverloads == "best")
    Opts.ShowOverloads = Diagnostic::Ovl_Best;
  else if (ShowOverloads == "all")
    Opts.ShowOverloads = Diagnostic::Ovl_All;
  else
    Diags.Report(diag::err_drv_invalid_value)
      << Args.getLastArg(OPT_fshow_overloads_EQ)->getAsString(Args)
      << ShowOverloads;

  llvm::StringRef ShowCategory =
    Args.getLastArgValue(OPT_fdiagnostics_show_category, "none");
  if (ShowCategory == "none")
    Opts.ShowCategories = 0;
  else if (ShowCategory == "id")
    Opts.ShowCategories = 1;
  else if (ShowCategory == "name")
    Opts.ShowCategories = 2;
  else
    Diags.Report(diag::err_drv_invalid_value)
      << Args.getLastArg(OPT_fdiagnostics_show_category)->getAsString(Args)
      << ShowCategory;

  Opts.ShowSourceRanges = Args.hasArg(OPT_fdiagnostics_print_source_range_info);
  Opts.ShowParseableFixits = Args.hasArg(OPT_fdiagnostics_parseable_fixits);
  Opts.VerifyDiagnostics = Args.hasArg(OPT_verify);
  Opts.ErrorLimit = Args.getLastArgIntValue(OPT_ferror_limit, 0, Diags);
  Opts.MacroBacktraceLimit
    = Args.getLastArgIntValue(OPT_fmacro_backtrace_limit,
                         DiagnosticOptions::DefaultMacroBacktraceLimit, Diags);
  Opts.TemplateBacktraceLimit
    = Args.getLastArgIntValue(OPT_ftemplate_backtrace_limit,
                         DiagnosticOptions::DefaultTemplateBacktraceLimit,
                         Diags);
  Opts.TabStop = Args.getLastArgIntValue(OPT_ftabstop,
                                    DiagnosticOptions::DefaultTabStop, Diags);
  if (Opts.TabStop == 0 || Opts.TabStop > DiagnosticOptions::MaxTabStop) {
    Diags.Report(diag::warn_ignoring_ftabstop_value)
      << Opts.TabStop << DiagnosticOptions::DefaultTabStop;
    Opts.TabStop = DiagnosticOptions::DefaultTabStop;
  }
  Opts.MessageLength = Args.getLastArgIntValue(OPT_fmessage_length, 0, Diags);
  Opts.DumpBuildInformation = Args.getLastArgValue(OPT_dump_build_information);
  Opts.Warnings = Args.getAllArgValues(OPT_W);
}

static void ParseFileSystemArgs(FileSystemOptions &Opts, ArgList &Args) {
  Opts.WorkingDir = Args.getLastArgValue(OPT_working_directory);
}

static InputKind ParseFrontendArgs(FrontendOptions &Opts, ArgList &Args,
                                   Diagnostic &Diags) {
  using namespace cc1options;
  Opts.ProgramAction = frontend::ParseSyntaxOnly;
  if (const Arg *A = Args.getLastArg(OPT_Action_Group)) {
    switch (A->getOption().getID()) {
    default:
      assert(0 && "Invalid option in group!");
    case OPT_ast_dump:
      Opts.ProgramAction = frontend::ASTDump; break;
    case OPT_ast_dump_xml:
      Opts.ProgramAction = frontend::ASTDumpXML; break;
    case OPT_ast_print:
      Opts.ProgramAction = frontend::ASTPrint; break;
    case OPT_ast_view:
      Opts.ProgramAction = frontend::ASTView; break;
    case OPT_boostcon:
      Opts.ProgramAction = frontend::BoostCon; break;
    case OPT_dump_raw_tokens:
      Opts.ProgramAction = frontend::DumpRawTokens; break;
    case OPT_dump_tokens:
      Opts.ProgramAction = frontend::DumpTokens; break;
    case OPT_S:
      Opts.ProgramAction = frontend::EmitAssembly; break;
    case OPT_emit_llvm_bc:
      Opts.ProgramAction = frontend::EmitBC; break;
    case OPT_emit_html:
      Opts.ProgramAction = frontend::EmitHTML; break;
    case OPT_emit_llvm:
      Opts.ProgramAction = frontend::EmitLLVM; break;
    case OPT_emit_llvm_only:
      Opts.ProgramAction = frontend::EmitLLVMOnly; break;
    case OPT_emit_codegen_only:
      Opts.ProgramAction = frontend::EmitCodeGenOnly; break;
    case OPT_emit_obj:
      Opts.ProgramAction = frontend::EmitObj; break;
    case OPT_fixit_EQ:
      Opts.FixItSuffix = A->getValue(Args);
      // fall-through!
    case OPT_fixit:
      Opts.ProgramAction = frontend::FixIt; break;
    case OPT_emit_pth:
      Opts.ProgramAction = frontend::GeneratePTH; break;
    case OPT_init_only:
      Opts.ProgramAction = frontend::InitOnly; break;
    case OPT_fsyntax_only:
      Opts.ProgramAction = frontend::ParseSyntaxOnly; break;
    case OPT_print_decl_contexts:
      Opts.ProgramAction = frontend::PrintDefnContext; break;
    case OPT_E:
      Opts.ProgramAction = frontend::PrintPreprocessedInput; break;
    case OPT_rewrite_test:
      Opts.ProgramAction = frontend::RewriteTest; break;
    case OPT_analyze:
      Opts.ProgramAction = frontend::RunAnalysis; break;
    case OPT_Eonly:
      Opts.ProgramAction = frontend::RunPreprocessorOnly; break;
    case OPT_create_module:
      Opts.ProgramAction = frontend::CreateModule; break;
    }
  }

  if (const Arg* A = Args.getLastArg(OPT_plugin)) {
    Opts.Plugins.push_back(A->getValue(Args,0));
    Opts.ProgramAction = frontend::PluginAction;
    Opts.ActionName = A->getValue(Args);

    for (arg_iterator it = Args.filtered_begin(OPT_plugin_arg),
           end = Args.filtered_end(); it != end; ++it) {
      if ((*it)->getValue(Args, 0) == Opts.ActionName)
        Opts.PluginArgs.push_back((*it)->getValue(Args, 1));
    }
  }

  Opts.AddPluginActions = Args.getAllArgValues(OPT_add_plugin);
  Opts.AddPluginArgs.resize(Opts.AddPluginActions.size());
  for (int i = 0, e = Opts.AddPluginActions.size(); i != e; ++i) {
    for (arg_iterator it = Args.filtered_begin(OPT_plugin_arg),
           end = Args.filtered_end(); it != end; ++it) {
      if ((*it)->getValue(Args, 0) == Opts.AddPluginActions[i])
        Opts.AddPluginArgs[i].push_back((*it)->getValue(Args, 1));
    }
  }

  if (const Arg *A = Args.getLastArg(OPT_code_completion_at)) {
    Opts.CodeCompletionAt =
      ParsedSourceLocation::FromString(A->getValue(Args));
    if (Opts.CodeCompletionAt.FileName.empty())
      Diags.Report(diag::err_drv_invalid_value)
        << A->getAsString(Args) << A->getValue(Args);
  }
  Opts.DisableFree = Args.hasArg(OPT_disable_free);

  Opts.OutputFile = Args.getLastArgValue(OPT_o);
  Opts.Plugins = Args.getAllArgValues(OPT_load);
  Opts.ShowHelp = Args.hasArg(OPT_help);
  Opts.ShowMacrosInCodeCompletion = Args.hasArg(OPT_code_completion_macros);
  Opts.ShowCodePatternsInCodeCompletion
    = Args.hasArg(OPT_code_completion_patterns);
  Opts.ShowGlobalSymbolsInCodeCompletion
    = !Args.hasArg(OPT_no_code_completion_globals);
  Opts.ShowStats = Args.hasArg(OPT_print_stats);
  Opts.ShowTimers = Args.hasArg(OPT_ftime_report);
  Opts.ShowVersion = Args.hasArg(OPT_version);
  Opts.ASTMergeFiles = Args.getAllArgValues(OPT_ast_merge);
  Opts.LLVMArgs = Args.getAllArgValues(OPT_mllvm);
  Opts.FixWhatYouCan = Args.hasArg(OPT_fix_what_you_can);
  Opts.Modules = Args.getAllArgValues(OPT_import_module);

  // Opts.ARCMTAction = FrontendOptions::ARCMT_None;
//  if (const Arg *A = Args.getLastArg(OPT_arcmt_check,
//                                     OPT_arcmt_modify)) {
//    switch (A->getOption().getID()) {
//    default:
//      llvm_unreachable("missed a case");
//    case OPT_arcmt_check:
//      Opts.ARCMTAction = FrontendOptions::ARCMT_Check;
//      break;
//    case OPT_arcmt_modify:
//      Opts.ARCMTAction = FrontendOptions::ARCMT_Modify;
//      break;
//    }
//  }

  InputKind DashX = IK_None;
  if (const Arg *A = Args.getLastArg(OPT_x)) {
    DashX = llvm::StringSwitch<InputKind>(A->getValue(Args))
      .Case("c", IK_C)
      .Case("cl", IK_OpenCL)
      .Case("cuda", IK_CUDA)
      .Case("c++", IK_CXX)
      .Case("objective-c", IK_ObjC)
      .Case("objective-c++", IK_ObjCXX)
      .Case("cpp-output", IK_PreprocessedC)
      .Case("assembler-with-cpp", IK_Asm)
      .Case("c++-cpp-output", IK_PreprocessedCXX)
      .Case("objective-c-cpp-output", IK_PreprocessedObjC)
      .Case("objc-cpp-output", IK_PreprocessedObjC)
      .Case("objective-c++-cpp-output", IK_PreprocessedObjCXX)
      .Case("c-header", IK_C)
      .Case("objective-c-header", IK_ObjC)
      .Case("c++-header", IK_CXX)
      .Case("objective-c++-header", IK_ObjCXX)
      .Case("ast", IK_AST)
      .Case("ir", IK_LLVM_IR)
      .Default(IK_None);
    if (DashX == IK_None)
      Diags.Report(diag::err_drv_invalid_value)
        << A->getAsString(Args) << A->getValue(Args);
  }

  // '-' is the default input if none is given.
  std::vector<std::string> Inputs = Args.getAllArgValues(OPT_INPUT);
  Opts.Inputs.clear();
  if (Inputs.empty())
    Inputs.push_back("-");
  for (unsigned i = 0, e = Inputs.size(); i != e; ++i) {
    InputKind IK = DashX;
    if (IK == IK_None) {
      IK = FrontendOptions::getInputKindForExtension(
        llvm::StringRef(Inputs[i]).rsplit('.').second);
      // FIXME: Remove this hack.
      if (i == 0)
        DashX = IK;
    }
    Opts.Inputs.push_back(std::make_pair(IK, Inputs[i]));
  }

  return DashX;
}

std::string CompilerInvocation::GetResourcesPath(const char *Argv0,
                                                 void *MainAddr) {
  llvm::sys::Path P = llvm::sys::Path::GetMainExecutable(Argv0, MainAddr);

  if (!P.isEmpty()) {
    P.eraseComponent();  // Remove /clang from foo/bin/clang
    P.eraseComponent();  // Remove /bin   from foo/bin

    // Get foo/lib/clang/<version>/include
    P.appendComponent("lib");
    P.appendComponent("mlang");
    P.appendComponent("1.0");
  }

  return P.str();
}

static void ParseImportSearchArgs(ImportSearchOptions &Opts, ArgList &Args) {
//  using namespace cc1options;
  Opts.Sysroot = Args.getLastArgValue(OPT_isysroot, "/");
  Opts.Verbose = Args.hasArg(OPT_v);
  Opts.ResourceDir = Args.getLastArgValue(OPT_resource_dir);
}

void CompilerInvocation::setLangDefaults(LangOptions &Opts, InputKind IK,
                                         LangStandard::Kind LangStd) {
  // Set some properties which depend solely on the input kind; it would be nice
  // to move these to the language standard, and have the driver resolve the
  // input kind + language standard.
  if (IK == IK_Asm) {
    Opts.AsmPreprocessor = 1;
  }

  if (LangStd == LangStandard::lang_unspecified) {
    // Based on the base language, pick one.
    switch (IK) {
    case IK_None:
    case IK_AST:
    case IK_LLVM_IR:
      assert(0 && "Invalid input kind!");
    case IK_OpenCL:
      LangStd = LangStandard::lang_opencl;
      break;
    case IK_CUDA:
      LangStd = LangStandard::lang_cuda;
      break;
    case IK_Asm:
    case IK_C:
    case IK_PreprocessedC:
    case IK_ObjC:
    case IK_PreprocessedObjC:
      LangStd = LangStandard::lang_gnu99;
      break;
    case IK_CXX:
    case IK_PreprocessedCXX:
    case IK_ObjCXX:
    case IK_PreprocessedObjCXX:
      LangStd = LangStandard::lang_gnucxx98;
      break;
    }
  }

  const LangStandard &Std = LangStandard::getLangStandardForKind(LangStd);
  Opts.BCPLComment = Std.hasBCPLComments();

  // OpenCL has some additional defaults.
  if (LangStd == LangStandard::lang_opencl) {
    Opts.OpenCL = 1;
    Opts.AltiVec = 1;
  }

  if (LangStd == LangStandard::lang_cuda)
    Opts.CUDA = 1;
}

static void ParseLangArgs(LangOptions &Opts, ArgList &Args, InputKind IK,
                          Diagnostic &Diags) {
  // FIXME: Cleanup per-file based stuff.
  LangStandard::Kind LangStd = LangStandard::lang_unspecified;
  if (const Arg *A = Args.getLastArg(OPT_std_EQ)) {
    LangStd = llvm::StringSwitch<LangStandard::Kind>(A->getValue(Args))
#define LANGSTANDARD(id, name, desc, features) \
      .Case(name, LangStandard::lang_##id)
#include "mlang/Frontend/LangStandards.def"
      .Default(LangStandard::lang_unspecified);
    if (LangStd == LangStandard::lang_unspecified)
      Diags.Report(diag::err_drv_invalid_value)
        << A->getAsString(Args) << A->getValue(Args);
    else {
      // Valid standard, check to make sure language and standard are compatable.
      const LangStandard &Std = LangStandard::getLangStandardForKind(LangStd);
      switch (IK) {
      case IK_C:
      case IK_ObjC:
      case IK_PreprocessedC:
      case IK_PreprocessedObjC:
        if (!(Std.isC89() || Std.isC99()))
          Diags.Report(diag::err_drv_argument_not_allowed_with)
            << A->getAsString(Args) << "C/ObjC";
        break;
      case IK_CXX:
      case IK_ObjCXX:
      case IK_PreprocessedCXX:
      case IK_PreprocessedObjCXX:
        if (!Std.isCPlusPlus())
          Diags.Report(diag::err_drv_argument_not_allowed_with)
            << A->getAsString(Args) << "C++/ObjC++";
        break;
      case IK_OpenCL:
        if (!Std.isC99())
          Diags.Report(diag::err_drv_argument_not_allowed_with)
            << A->getAsString(Args) << "OpenCL";
        break;
      case IK_CUDA:
        if (!Std.isCPlusPlus())
          Diags.Report(diag::err_drv_argument_not_allowed_with)
            << A->getAsString(Args) << "CUDA";
        break;
      default:
        break;
      }
    }
  }

  if (const Arg *A = Args.getLastArg(OPT_cl_std_EQ)) {
    if (strcmp(A->getValue(Args), "CL1.1") != 0) {
      Diags.Report(diag::err_drv_invalid_value)
        << A->getAsString(Args) << A->getValue(Args);
    }
  }

  CompilerInvocation::setLangDefaults(Opts, IK, LangStd);

  if (Args.hasArg(OPT_faltivec))
    Opts.AltiVec = 1;

  if (Args.hasArg(OPT_pthread))
    Opts.POSIXThreads = 1;

//  llvm::StringRef Vis = Args.getLastArgValue(OPT_fvisibility, "default");
//  if (Vis == "default")
//    Opts.setVisibilityMode(DefaultVisibility);
//  else if (Vis == "hidden")
//    Opts.setVisibilityMode(HiddenVisibility);
//  else if (Vis == "protected")
//    Opts.setVisibilityMode(ProtectedVisibility);
//  else
//    Diags.Report(diag::err_drv_invalid_value)
//      << Args.getLastArg(OPT_fvisibility)->getAsString(Args) << Vis;

  if (Args.hasArg(OPT_ftrapv)) {
    Opts.setSignedOverflowBehavior(LangOptions::SOB_Trapping);
    // Set the handler, if one is specified.
    Opts.OverflowHandler =
        Args.getLastArgValue(OPT_ftrapv_handler);
  }
  else if (Args.hasArg(OPT_fwrapv))
    Opts.setSignedOverflowBehavior(LangOptions::SOB_Defined);

  if (Args.hasArg(OPT_fno_threadsafe_statics))
    Opts.ThreadsafeStatics = 0;

  Opts.CharIsSigned = !Args.hasArg(OPT_fno_signed_char);
  Opts.ShortWChar = Args.hasArg(OPT_fshort_wchar);
  Opts.ShortEnums = Args.hasArg(OPT_fshort_enums);
  Opts.NoBuiltin = Args.hasArg(OPT_fno_builtin);
  Opts.AccessControl = !Args.hasArg(OPT_fno_access_control);
  Opts.MathErrno = Args.hasArg(OPT_fmath_errno);
  Opts.InstantiationDepth = Args.getLastArgIntValue(OPT_ftemplate_depth, 1024,
                                               Diags);
  Opts.NumLargeByValueCopy = Args.getLastArgIntValue(OPT_Wlarge_by_value_copy,
                                                    0, Diags);
  Opts.Static = Args.hasArg(OPT_static_define);
  Opts.DumpRecordLayouts = Args.hasArg(OPT_fdump_record_layouts);
  Opts.OptimizeSize = 0;

  // FIXME: Eliminate this dependency.
  unsigned Opt = getOptimizationLevel(Args, IK, Diags);
  Opts.Optimize = Opt != 0;

  unsigned SSP = Args.getLastArgIntValue(OPT_stack_protector, 0, Diags);
  switch (SSP) {
  default:
    Diags.Report(diag::err_drv_invalid_value)
      << Args.getLastArg(OPT_stack_protector)->getAsString(Args) << SSP;
    break;
  case 0: Opts.setStackProtectorMode(LangOptions::SSPOff); break;
  case 1: Opts.setStackProtectorMode(LangOptions::SSPOn);  break;
  case 2: Opts.setStackProtectorMode(LangOptions::SSPReq); break;
  }
}

static void ParsePreprocessorArgs(PreprocessorOptions &Opts, ArgList &Args,
                                  FileManager &FileMgr,
                                  Diagnostic &Diags) {
  using namespace cc1options;
  Opts.ImplicitPTHInclude = Args.getLastArgValue(OPT_include_pth);
  if (const Arg *A = Args.getLastArg(OPT_token_cache))
      Opts.TokenCache = A->getValue(Args);
  else
    Opts.TokenCache = Opts.ImplicitPTHInclude;
  Opts.UsePredefines = !Args.hasArg(OPT_undef);
  Opts.DetailedRecord = Args.hasArg(OPT_detailed_preprocessing_record);

//  // Include 'altivec.h' if -faltivec option present
//  if (Args.hasArg(OPT_faltivec))
//    Opts.Includes.push_back("altivec.h");

  for (arg_iterator it = Args.filtered_begin(OPT_remap_file),
         ie = Args.filtered_end(); it != ie; ++it) {
    const Arg *A = *it;
    std::pair<llvm::StringRef,llvm::StringRef> Split =
      llvm::StringRef(A->getValue(Args)).split(';');

    if (Split.second.empty()) {
      Diags.Report(diag::err_drv_invalid_remap_file) << A->getAsString(Args);
      continue;
    }

    Opts.addRemappedFile(Split.first, Split.second);
  }

//  if (Arg *A = Args.getLastArg(OPT_fobjc_arc_cxxlib_EQ)) {
//    llvm::StringRef Name = A->getValue(Args);
//    unsigned Library = llvm::StringSwitch<unsigned>(Name)
//      .Case("libc++", ARCXX_libcxx)
//      .Case("libstdc++", ARCXX_libstdcxx)
//      .Case("none", ARCXX_nolib)
//      .Default(~0U);
//    if (Library == ~0U)
//      Diags.Report(diag::err_drv_invalid_value) << A->getAsString(Args) << Name;
//    else
//      Opts.ObjCXXARCStandardLibrary = (ObjCXXARCStandardLibraryKind)Library;
//  }
}

static void ParsePreprocessorOutputArgs(PreprocessorOutputOptions &Opts,
                                        ArgList &Args) {
  using namespace cc1options;
  Opts.ShowCPP = !Args.hasArg(OPT_dM);
  Opts.ShowComments = Args.hasArg(OPT_C);
  Opts.ShowLineMarkers = !Args.hasArg(OPT_P);
}

static void ParseTargetArgs(TargetOptions &Opts, ArgList &Args) {
  using namespace cc1options;
  Opts.ABI = Args.getLastArgValue(OPT_target_abi);
  //Opts.CXXABI = Args.getLastArgValue(OPT_cxx_abi);
  Opts.CPU = Args.getLastArgValue(OPT_target_cpu);
  Opts.Features = Args.getAllArgValues(OPT_target_feature);
  Opts.LinkerVersion = Args.getLastArgValue(OPT_target_linker_version);
  Opts.Triple = llvm::Triple::normalize(Args.getLastArgValue(OPT_triple));

  // Use the host triple if unspecified.
  if (Opts.Triple.empty())
    Opts.Triple = llvm::sys::getHostTriple();
}
//

void CompilerInvocation::CreateFromArgs(CompilerInvocation &Res,
		const char * const *ArgBegin, const char * const *ArgEnd,
		Diagnostic &Diags) {
	// Parse the arguments.
	llvm::OwningPtr<OptTable> Opts(createCC1OptTable());
	unsigned MissingArgIndex, MissingArgCount;
	llvm::OwningPtr<InputArgList> Args(Opts->ParseArgs(ArgBegin, ArgEnd,
			MissingArgIndex, MissingArgCount));

	// Check for missing argument error.
	if (MissingArgCount)
		Diags.Report(diag::err_drv_missing_argument) << Args->getArgString(
				MissingArgIndex) << MissingArgCount;

	// Issue errors on unknown arguments.
	for (arg_iterator it = Args->filtered_begin(OPT_UNKNOWN), ie =
			Args->filtered_end(); it != ie; ++it)
		Diags.Report(diag::err_drv_unknown_argument) << (*it)->getAsString(
				*Args);

	//ParseAnalyzerArgs(Res.getAnalyzerOpts(), *Args, Diags);
	ParseDependencyOutputArgs(Res.getDependencyOutputOpts(), *Args);
	ParseDiagnosticArgs(Res.getDiagnosticOpts(), *Args, Diags);
	ParseFileSystemArgs(Res.getFileSystemOpts(), *Args);
	// FIXME: We shouldn't have to pass the DashX option around here
	InputKind DashX = ParseFrontendArgs(Res.getFrontendOpts(), *Args, Diags);
	ParseCodeGenArgs(Res.getCodeGenOpts(), *Args, DashX, Diags);
	ParseImportSearchArgs(Res.getImportSearchOpts(), *Args);
	if (DashX != IK_AST && DashX != IK_LLVM_IR) {
		ParseLangArgs(Res.getLangOpts(), *Args, DashX, Diags);
	}
	// FIXME: ParsePreprocessorArgs uses the FileManager to read the contents of
	// PCH file and find the original header name. Remove the need to do that in
	// ParsePreprocessorArgs and remove the FileManager
	// parameters from the function and the "FileManager.h" #include.
	FileManager FileMgr(Res.getFileSystemOpts());
	ParsePreprocessorArgs(Res.getPreprocessorOpts(), *Args, FileMgr, Diags);
	ParsePreprocessorOutputArgs(Res.getPreprocessorOutputOpts(), *Args);
	ParseTargetArgs(Res.getTargetOpts(), *Args);
}
