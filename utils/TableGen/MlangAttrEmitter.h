//===- MlangAttrEmitter.h - Generate Mlang attribute handling =-*- C++ -*--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These tablegen backends emit Mlang attribute processing code
//
//===----------------------------------------------------------------------===//

#ifndef MLANGATTR_EMITTER_H
#define MLANGATTR_EMITTER_H

#include "llvm/TableGen/TableGenBackend.h"

namespace llvm {

/// MlangAttrClassEmitter - class emits the class defintions for attributes for
///   mlang.
class MlangAttrClassEmitter : public TableGenBackend {
  RecordKeeper &Records;

 public:
  explicit MlangAttrClassEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

/// MlangAttrImplEmitter - class emits the class method defintions for
///   attributes for mlang.
class MlangAttrImplEmitter : public TableGenBackend {
  RecordKeeper &Records;

 public:
  explicit MlangAttrImplEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

/// MlangAttrListEmitter - class emits the enumeration list for attributes for
///   mlang.
class MlangAttrListEmitter : public TableGenBackend {
  RecordKeeper &Records;

 public:
  explicit MlangAttrListEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

/// MlangAttrPCHReadEmitter - class emits the code to read an attribute from
///   a mlang precompiled header.
class MlangAttrPCHReadEmitter : public TableGenBackend {
  RecordKeeper &Records;

public:
  explicit MlangAttrPCHReadEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

/// MlangAttrPCHWriteEmitter - class emits the code to read an attribute from
///   a mlang precompiled header.
class MlangAttrPCHWriteEmitter : public TableGenBackend {
  RecordKeeper &Records;

public:
  explicit MlangAttrPCHWriteEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

/// MlangAttrSpellingListEmitter - class emits the list of spellings for
//  attributes for mlang.
class MlangAttrSpellingListEmitter : public TableGenBackend {
  RecordKeeper &Records;

 public:
  explicit MlangAttrSpellingListEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

/// MlangAttrLateParsedListEmitter emits the LateParsed property for attributes
/// for mlang.
class MlangAttrLateParsedListEmitter : public TableGenBackend {
  RecordKeeper &Records;

 public:
  explicit MlangAttrLateParsedListEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

/// MlangAttrTemplateInstantiateEmitter emits code to instantiate dependent
/// attributes on templates.
class MlangAttrTemplateInstantiateEmitter : public TableGenBackend {
  RecordKeeper &Records;

 public:
  explicit MlangAttrTemplateInstantiateEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

/// MlangAttrParsedAttrListEmitter emits the list of parsed attributes
/// for mlang.
class MlangAttrParsedAttrListEmitter : public TableGenBackend {
  RecordKeeper &Records;

public:
  explicit MlangAttrParsedAttrListEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

/// MlangAttrParsedAttrKindsEmitter emits the kind list of parsed attributes
/// for mlang.
class MlangAttrParsedAttrKindsEmitter : public TableGenBackend {
  RecordKeeper &Records;

public:
  explicit MlangAttrParsedAttrKindsEmitter(RecordKeeper &R)
    : Records(R)
    {}

  void run(raw_ostream &OS);
};

}

#endif
