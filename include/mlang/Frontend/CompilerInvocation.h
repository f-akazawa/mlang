//===--- CompilerInvocation.h - CompilerInvocation --------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines CompilerInvocation.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_COMPILER_INVOCATION_H_
#define MLANG_FRONTEND_COMPILER_INVOCATION_H_

#include "mlang/Basic/LangOptions.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Basic/TargetOptions.h"
#include "mlang/Basic/FileSystemOptions.h"
// #include "mlang/Frontend/AnalyzerOptions.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "mlang/Frontend/DependencyOutputOptions.h"
#include "mlang/Frontend/DiagnosticOptions.h"
#include "mlang/Frontend/FrontendOptions.h"
#include "mlang/Frontend/ImportSearchOptions.h"
#include "mlang/Frontend/LangStandard.h"
#include "mlang/Frontend/PreprocessorOptions.h"
#include "mlang/Frontend/PreprocessorOutputOptions.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"
#include <string>
#include <vector>

namespace llvm {
  template<typename T> class SmallVectorImpl;
}

namespace mlang {
class DiagnosticsEngine;

/// CompilerInvocation - Helper class for holding the data necessary to invoke
/// the compiler.
///
/// This class is designed to represent an abstract "invocation" of the
/// compiler, including data such as the include paths, the code generation
/// options, the warning flags, and so on.
class CompilerInvocation : public llvm::RefCountedBase<CompilerInvocation> {
  /// Options controlling the static analyzer.
  // AnalyzerOptions AnalyzerOpts;

  /// Options controlling IRgen and the backend.
  CodeGenOptions CodeGenOpts;

  /// Options controlling dependency output.
  DependencyOutputOptions DependencyOutputOpts;

  /// Options controlling the diagnostic engine.
  DiagnosticOptions DiagnosticOpts;

  /// Options controlling file system operations.
  FileSystemOptions FileSystemOpts;

  /// Options controlling the frontend itself.
  FrontendOptions FrontendOpts;

  /// Options controlling the #include directive.
  ImportSearchOptions ImportSearchOpts;

  /// Options controlling the language variant.
  LangOptions LangOpts;

  /// Options controlling the preprocessor (aside from #include handling).
  PreprocessorOptions PreprocessorOpts;

  /// Options controlling preprocessed output.
  PreprocessorOutputOptions PreprocessorOutputOpts;

  /// Options controlling the target.
  TargetOptions TargetOpts;

public:
  CompilerInvocation() {}

  /// @name Utility Methods
  /// @{

  /// CreateFromArgs - Create a compiler invocation from a list of input
  /// options.
  ///
  /// \param Res [out] - The resulting invocation.
  /// \param ArgBegin - The first element in the argument vector.
  /// \param ArgEnd - The last element in the argument vector.
  /// \param Diags - The diagnostic engine to use for errors.
  static void CreateFromArgs(CompilerInvocation &Res,
                             const char* const *ArgBegin,
                             const char* const *ArgEnd,
                             DiagnosticsEngine &Diags);

  /// GetBuiltinIncludePath - Get the directory where the compiler headers
  /// reside, relative to the compiler binary (found by the passed in
  /// arguments).
  ///
  /// \param Argv0 - The program path (from argv[0]), for finding the builtin
  /// compiler path.
  /// \param MainAddr - The address of main (or some other function in the main
  /// executable), for finding the builtin compiler path.
  static std::string GetResourcesPath(const char *Argv0, void *MainAddr);

  /// toArgs - Convert the CompilerInvocation to a list of strings suitable for
  /// passing to CreateFromArgs.
  void toArgs(std::vector<std::string> &Res);

  /// setLangDefaults - Set language defaults for the given input language and
  /// language standard in this CompilerInvocation.
  ///
  /// \param IK - The input language.
  /// \param LangStd - The input language standard.
//  void setLangDefaults(InputKind IK,
//                  LangStandard::Kind LangStd = LangStandard::lang_unspecified) {
//    setLangDefaults(LangOpts, IK, LangStd);
//  }

  /// setLangDefaults - Set language defaults for the given input language and
  /// language standard in the given LangOptions object.
  ///
  /// \param LangOpts - The LangOptions object to set up.
  /// \param IK - The input language.
  /// \param LangStd - The input language standard.
  static void setLangDefaults(LangOptions &Opts, InputKind IK,
                   LangStandard::Kind LangStd = LangStandard::lang_unspecified);

  /// @}
  /// @name Option Subgroups
  /// @{

//  AnalyzerOptions &getAnalyzerOpts() { return AnalyzerOpts; }
//  const AnalyzerOptions &getAnalyzerOpts() const {
//    return AnalyzerOpts;
//  }

  CodeGenOptions &getCodeGenOpts() { return CodeGenOpts; }
  const CodeGenOptions &getCodeGenOpts() const {
    return CodeGenOpts;
  }

  DependencyOutputOptions &getDependencyOutputOpts() {
    return DependencyOutputOpts;
  }
  const DependencyOutputOptions &getDependencyOutputOpts() const {
    return DependencyOutputOpts;
  }

  DiagnosticOptions &getDiagnosticOpts() { return DiagnosticOpts; }
  const DiagnosticOptions &getDiagnosticOpts() const { return DiagnosticOpts; }

  FileSystemOptions &getFileSystemOpts() { return FileSystemOpts; }
  const FileSystemOptions &getFileSystemOpts() const {
    return FileSystemOpts;
  }

  ImportSearchOptions &getImportSearchOpts() { return ImportSearchOpts; }
  const ImportSearchOptions &getImportSearchOpts() const {
    return ImportSearchOpts;
  }

  FrontendOptions &getFrontendOpts() { return FrontendOpts; }
  const FrontendOptions &getFrontendOpts() const {
    return FrontendOpts;
  }

  LangOptions &getLangOpts() { return LangOpts; }
  const LangOptions &getLangOpts() const { return LangOpts; }

  PreprocessorOptions &getPreprocessorOpts() { return PreprocessorOpts; }
  const PreprocessorOptions &getPreprocessorOpts() const {
    return PreprocessorOpts;
  }

  PreprocessorOutputOptions &getPreprocessorOutputOpts() {
    return PreprocessorOutputOpts;
  }
  const PreprocessorOutputOptions &getPreprocessorOutputOpts() const {
    return PreprocessorOutputOpts;
  }

  TargetOptions &getTargetOpts() { return TargetOpts; }
  const TargetOptions &getTargetOpts() const {
    return TargetOpts;
  }

  /// @}
};

} // end namespace mlang

#endif /* MLANG_FRONTEND_COMPILER_INVOCATION_H_ */
