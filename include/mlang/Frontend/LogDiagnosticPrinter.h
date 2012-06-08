//===--- LogDiagnosticPrinter.h - Log Diagnostic Client ---------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_LOG_DIAGNOSTIC_PRINTER_H_
#define MLANG_FRONTEND_LOG_DIAGNOSTIC_PRINTER_H_

#include "mlang/Diag/Diagnostic.h"
#include "mlang/Basic/SourceLocation.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"

namespace mlang {
class DiagnosticOptions;
class LangOptions;

class LogDiagnosticPrinter : public DiagnosticConsumer {
  struct DiagEntry {
    /// The primary message line of the diagnostic.
    std::string Message;

    /// The source file name, if available.
    std::string Filename;

    /// The source file line number, if available.
    unsigned Line;

    /// The source file column number, if available.
    unsigned Column;

    /// The ID of the diagnostic.
    unsigned DiagnosticID;

    /// The level of the diagnostic.
    DiagnosticsEngine::Level DiagnosticLevel;
  };

  llvm::raw_ostream &OS;
  const LangOptions *LangOpts;
  const DiagnosticOptions *DiagOpts;

  SourceLocation LastWarningLoc;
  FullSourceLoc LastLoc;
  unsigned OwnsOutputStream : 1;

  llvm::SmallVector<DiagEntry, 8> Entries;

  std::string MainFilename;
  std::string DwarfDebugFlags;

public:
  LogDiagnosticPrinter(llvm::raw_ostream &OS, const DiagnosticOptions &Diags,
                       bool OwnsOutputStream = false);
  virtual ~LogDiagnosticPrinter();

  void setDwarfDebugFlags(llvm::StringRef Value) {
    DwarfDebugFlags = Value;
  }

  void BeginSourceFile(const LangOptions &LO, const Preprocessor *PP) {
    LangOpts = &LO;
  }

  void EndSourceFile();

  virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                                const Diagnostic &Info);

  DiagnosticConsumer *clone(DiagnosticsEngine &Diags) const;
};

} // end namespace mlang

#endif /* MLANG_FRONTEND_LOG_DIAGNOSTIC_PRINTER_H_ */
