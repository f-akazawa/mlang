//===--- TextDiagnosticBuffer.cpp - Buffer Text Diagnostics ---------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a concrete diagnostic client, which buffers the diagnostic messages.
//
//===----------------------------------------------------------------------===//

#include "mlang/Frontend/TextDiagnosticBuffer.h"
#include "llvm/ADT/SmallString.h"
using namespace mlang;

/// HandleDiagnostic - Store the errors, warnings, and notes that are
/// reported.
///
void TextDiagnosticBuffer::HandleDiagnostic(DiagnosticsEngine::Level Level,
                                            const Diagnostic &Info) {
  // Default implementation (Warnings/errors count).
  DiagnosticConsumer::HandleDiagnostic(Level, Info);

  llvm::SmallString<100> Buf;
  Info.FormatDiagnostic(Buf);
  switch (Level) {
  default: assert(0 && "Diagnostic not handled during diagnostic buffering!");
  case DiagnosticsEngine::Note:
    Notes.push_back(std::make_pair(Info.getLocation(), Buf.str()));
    break;
  case DiagnosticsEngine::Warning:
    Warnings.push_back(std::make_pair(Info.getLocation(), Buf.str()));
    break;
  case DiagnosticsEngine::Error:
  case DiagnosticsEngine::Fatal:
    Errors.push_back(std::make_pair(Info.getLocation(), Buf.str()));
    break;
  }
}

void TextDiagnosticBuffer::FlushDiagnostics(DiagnosticsEngine &Diags) const {
  // FIXME: Flush the diagnostics in order.
  for (const_iterator it = err_begin(), ie = err_end(); it != ie; ++it)
    Diags.Report(Diags.getCustomDiagID(DiagnosticsEngine::Error,
                                       it->second.c_str()));
  for (const_iterator it = warn_begin(), ie = warn_end(); it != ie; ++it)
    Diags.Report(Diags.getCustomDiagID(DiagnosticsEngine::Warning,
                                       it->second.c_str()));
  for (const_iterator it = note_begin(), ie = note_end(); it != ie; ++it)
    Diags.Report(Diags.getCustomDiagID(DiagnosticsEngine::Note,
                                       it->second.c_str()));
}

DiagnosticConsumer *TextDiagnosticBuffer::clone(DiagnosticsEngine &) const {
  return new TextDiagnosticBuffer();
}
