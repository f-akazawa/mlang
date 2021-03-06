//===--- Utils.h - Misc utilities for the front-end -------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This header contains miscellaneous utilities for various front-end actions.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_UTILS_H_
#define MLANG_FRONTEND_UTILS_H_

#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
class Triple;
}

namespace mlang {
class ASTConsumer;
class CompilerInstance;
class CompilerInvocation;
class Defn;
class DependencyOutputOptions;
class DiagnosticsEngine;
class DiagnosticOptions;
class FileManager;
class ImportSearch;
class ImportSearchOptions;
class IdentifierTable;
class LangOptions;
class Preprocessor;
class PreprocessorOptions;
class PreprocessorOutputOptions;
class SourceManager;
class Stmt;
class TargetInfo;
class FrontendOptions;

/// Apply the header search options to get given ImportSearch object.
void ApplyImportSearchOptions(ImportSearch &HS,
                              const ImportSearchOptions &HSOpts,
                              const LangOptions &Lang,
                              const llvm::Triple &triple);

/// InitializePreprocessor - Initialize the preprocessor getting it and the
/// environment ready to process a single file.
void InitializePreprocessor(Preprocessor &PP,
                            const PreprocessorOptions &PPOpts,
                            const ImportSearchOptions &HSOpts,
                            const FrontendOptions &FEOpts);

/// ProcessWarningOptions - Initialize the diagnostic client and process the
/// warning options specified on the command line.
void ProcessWarningOptions(DiagnosticsEngine &Diags,
                           const DiagnosticOptions &Opts);

/// DoPrintPreprocessedInput - Implement -E mode.
void DoPrintPreprocessedInput(Preprocessor &PP, llvm::raw_ostream* OS,
                              const PreprocessorOutputOptions &Opts);

/// CheckDiagnostics - Gather the expected diagnostics and check them.
bool CheckDiagnostics(Preprocessor &PP);

/// AttachDependencyFileGen - Create a dependency file generator, and attach
/// it to the given preprocessor.  This takes ownership of the output stream.
void AttachDependencyFileGen(Preprocessor &PP,
                             const DependencyOutputOptions &Opts);

/// AttachHeaderIncludeGen - Create a header include list generator, and attach
/// it to the given preprocessor.
///
/// \param ShowAllHeaders - If true, show all header information instead of just
/// headers following the predefines buffer. This is useful for making sure
/// includes mentioned on the command line are also reported, but differs from
/// the default behavior used by -H.
/// \param OutputPath - If non-empty, a path to write the header include
/// information to, instead of writing to stderr.
void AttachHeaderIncludeGen(Preprocessor &PP, bool ShowAllHeaders = false,
                            llvm::StringRef OutputPath = "",
                            bool ShowDepth = true);

/// CacheTokens - Cache tokens for use with PCH. Note that this requires
/// a seekable stream.
void CacheTokens(Preprocessor &PP, llvm::raw_fd_ostream* OS);

/// createInvocationFromCommandLine - Construct a compiler invocation object for
/// a command line argument vector.
///
/// \return A CompilerInvocation, or 0 if none was built for the given
/// argument vector.
CompilerInvocation *
createInvocationFromCommandLine(llvm::ArrayRef<const char *> Args,
                             llvm::IntrusiveRefCntPtr<DiagnosticsEngine> Diags =
                                llvm::IntrusiveRefCntPtr<DiagnosticsEngine>());

}  // end namespace mlang

#endif /* MLANG_FRONTEND_UTILS_H_ */
