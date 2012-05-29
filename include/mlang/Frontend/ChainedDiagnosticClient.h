//===--- ChainedDiagnosticClient.h - Chain Diagnostic Clients ---*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details..
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_CHAINEDDIAGNOSTICCLIENT_H_
#define MLANG_FRONTEND_CHAINEDDIAGNOSTICCLIENT_H_

#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/OwningPtr.h"

namespace mlang {
class LangOptions;

/// ChainedDiagnosticClient - Chain two diagnostic clients so that diagnostics
/// go to the first client and then the second. The first diagnostic client
/// should be the "primary" client, and will be used for computing whether the
/// diagnostics should be included in counts.
class ChainedDiagnosticClient : public DiagnosticClient {
  llvm::OwningPtr<DiagnosticClient> Primary;
  llvm::OwningPtr<DiagnosticClient> Secondary;

public:
  ChainedDiagnosticClient(DiagnosticClient *_Primary,
                          DiagnosticClient *_Secondary) {
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

  virtual void HandleDiagnostic(Diagnostic::Level DiagLevel,
                                const DiagnosticInfo &Info) {
    // Default implementation (Warnings/errors count).
    DiagnosticClient::HandleDiagnostic(DiagLevel, Info);

    Primary->HandleDiagnostic(DiagLevel, Info);
    Secondary->HandleDiagnostic(DiagLevel, Info);
  }
};

} // end namspace mlang

#endif /* MLANG_FRONTEND_CHAINEDDIAGNOSTICCLIENT_H_ */
