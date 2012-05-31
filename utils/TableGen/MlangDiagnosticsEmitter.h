//===- MlangDiagnosticsEmitter.h - Generate Mlang diagnostics tables -*- C++ -*-
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These tablegen backends emit Mlang diagnostics tables.
//
//===----------------------------------------------------------------------===//

#ifndef MLANGDIAGS_EMITTER_H
#define MLANGDIAGS_EMITTER_H

#include "llvm/TableGen/TableGenBackend.h"

namespace llvm {

/// MlangDiagsDefsEmitter - The top-level class emits .def files containing
///  declarations of Mlang diagnostics.
///
class MlangDiagsDefsEmitter : public TableGenBackend {
  RecordKeeper &Records;
  const std::string& Component;
public:
  explicit MlangDiagsDefsEmitter(RecordKeeper &R, const std::string& component)
    : Records(R), Component(component) {}

  // run - Output the .def file contents
  void run(raw_ostream &OS);
};

class MlangDiagGroupsEmitter : public TableGenBackend {
  RecordKeeper &Records;
public:
  explicit MlangDiagGroupsEmitter(RecordKeeper &R) : Records(R) {}

  void run(raw_ostream &OS);
};

class MlangDiagsIndexNameEmitter : public TableGenBackend {
  RecordKeeper &Records;
public:
  explicit MlangDiagsIndexNameEmitter(RecordKeeper &R) : Records(R) {}

  void run(raw_ostream &OS);
};


} // End llvm namespace

#endif
