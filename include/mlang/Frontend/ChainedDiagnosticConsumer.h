//===--- ChainedDiagnosticConsumer.h - Chain Diagnostic Clients -*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details..
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_CHAINEDDIAGNOSTICCONSUMER_H_
#define MLANG_FRONTEND_CHAINEDDIAGNOSTICCONSUMER_H_

#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/OwningPtr.h"

namespace mlang {
class LangOptions;

/// ChainedDiagnosticConsumer - Chain two diagnostic clients so that diagnostics
/// go to the first client and then the second. The first diagnostic client
/// should be the "primary" client, and will be used for computing whether the
/// diagnostics should be included in counts.
class ChainedDiagnosticConsumer : public DiagnosticConsumer {
  llvm::OwningPtr<DiagnosticConsumer> Primary;
  llvm::OwningPtr<DiagnosticConsumer> Secondary;

public:
  ChainedDiagnosticConsumer(DiagnosticConsumer *_Primary,
                            DiagnosticConsumer *_Secondary) {
    Primary.reset(_Primary);
    Secondary.reset(_Secondary);
  }

  virtual void BeginSourceFile(const LangOptions &LO,
                               const Preprocessor *PP) {
    Primary->BeginSourceFile(LO, PP);
    Secondary->BeginSourceFile(LO, PP);
  }

  virtual void EndSourceFile() {
    Secondary->EndSourceFile();
    Primary->EndSourceFile();
  }

  virtual bool IncludeInDiagnosticCounts() const {
    return Primary->IncludeInDiagnosticCounts();
  }

  virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                                const Diagnostic &Info) {
    // Default implementation (Warnings/errors count).
    DiagnosticConsumer::HandleDiagnostic(DiagLevel, Info);

    Primary->HandleDiagnostic(DiagLevel, Info);
    Secondary->HandleDiagnostic(DiagLevel, Info);
  }

  DiagnosticConsumer *clone(DiagnosticsEngine &Diags) const {
    return new ChainedDiagnosticConsumer(Primary->clone(Diags),
                                         Secondary->clone(Diags));
  }
};

} // end namspace mlang

#endif /* MLANG_FRONTEND_CHAINEDDIAGNOSTICCLIENT_H_ */
