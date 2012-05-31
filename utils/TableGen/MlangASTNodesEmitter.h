//===- MlangASTNodesEmitter.h - Generate Mlang AST node tables -*- C++ -*--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These tablegen backends emit Mlang AST node tables
//
//===----------------------------------------------------------------------===//

#ifndef MLANGAST_EMITTER_H
#define MLANGAST_EMITTER_H

#include "llvm/TableGen/TableGenBackend.h"
#include "llvm/TableGen/Record.h"
#include <string>
#include <cctype>
#include <map>

namespace llvm {

/// MlangASTNodesEmitter - The top-level class emits .inc files containing
///  declarations of Mlang statements.
///
class MlangASTNodesEmitter : public TableGenBackend {
  // A map from a node to each of its derived nodes.
  typedef std::multimap<Record*, Record*> ChildMap;
  typedef ChildMap::const_iterator ChildIterator;

  RecordKeeper &Records;
  Record Root;
  const std::string &BaseSuffix;

  // Create a macro-ized version of a name
  static std::string macroName(std::string S) {
    for (unsigned i = 0; i < S.size(); ++i)
      S[i] = std::toupper(S[i]);

    return S;
  }

  // Return the name to be printed in the base field. Normally this is
  // the record's name plus the base suffix, but if it is the root node and
  // the suffix is non-empty, it's just the suffix.
  std::string baseName(Record &R) {
    if (&R == &Root && !BaseSuffix.empty())
      return BaseSuffix;

    return R.getName() + BaseSuffix;
  }

  std::pair<Record *, Record *> EmitNode (const ChildMap &Tree, raw_ostream& OS,
                                          Record *Base);
public:
  explicit MlangASTNodesEmitter(RecordKeeper &R, const std::string &N,
                                const std::string &S)
    : Records(R), Root(N, SMLoc(), R), BaseSuffix(S)
    {}

  // run - Output the .inc file contents
  void run(raw_ostream &OS);
};

/// MlangDeclContextEmitter - Emits an addendum to a .inc file to enumerate the
/// mlang declaration contexts.
///
class MlangDeclContextEmitter : public TableGenBackend {
  RecordKeeper &Records;

public:
  explicit MlangDeclContextEmitter(RecordKeeper &R)
    : Records(R)
  {}

  // run - Output the .inc file contents
  void run(raw_ostream &OS);
};

} // End llvm namespace

#endif
