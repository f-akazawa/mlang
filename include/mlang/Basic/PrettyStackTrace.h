//===- mlang/Basic/PrettyStackTrace.h - Pretty Crash Handling --*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file defines the PrettyStackTraceEntry class, which is used to make
// crashes give more contextual information about what the program was doing
// when it crashed.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_PRETTYSTACKTRACE_H_
#define MLANG_BASIC_PRETTYSTACKTRACE_H_

#include "mlang/Basic/SourceLocation.h"
#include "llvm/Support/PrettyStackTrace.h"

namespace mlang {

  /// PrettyStackTraceLoc - If a crash happens while one of these objects are
  /// live, the message is printed out along with the specified source location.
  class PrettyStackTraceLoc : public llvm::PrettyStackTraceEntry {
    SourceManager &SM;
    SourceLocation Loc;
    const char *Message;
  public:
    PrettyStackTraceLoc(SourceManager &sm, SourceLocation L, const char *Msg)
      : SM(sm), Loc(L), Message(Msg) {}
    virtual void print(llvm::raw_ostream &OS) const;
  };
} // end namespace mlang

#endif /* MLANG_BASIC_PRETTYSTACKTRACE_H_ */
