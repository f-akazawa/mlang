//===--- PrettyDeclStackTraceEntry.h - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SEMA_PRETTY_DECL_STACK_TRACEENTRY_H_
#define MLANG_SEMA_PRETTY_DECL_STACK_TRACEENTRY_H_

#include "mlang/Basic/SourceLocation.h"
#include "llvm/Support/PrettyStackTrace.h"

namespace mlang {
class Defn;
class Sema;
class SourceManager;

/// PrettyDefnStackTraceEntry - If a crash occurs in the parser while
/// parsing something related to a declaration, include that
/// declaration in the stack trace.
class PrettyDefnStackTraceEntry : public llvm::PrettyStackTraceEntry {
  Sema &S;
  Defn *TheDefn;
  SourceLocation Loc;
  const char *Message;

public:
  PrettyDefnStackTraceEntry(Sema &S, Defn *D, SourceLocation Loc, const char *Msg)
    : S(S), TheDefn(D), Loc(Loc), Message(Msg) {}

  virtual void print(llvm::raw_ostream &OS) const;
};

} // end namespace mlang

#endif /* MLANG_SEMA_PRETTY_DECL_STACK_TRACEENTRY_H_ */
