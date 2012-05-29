//===--- LangStandards.cpp - Language Standard Definitions ----------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "mlang/Frontend/LangStandard.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"
using namespace mlang;
using namespace mlang::frontend;

#define LANGSTANDARD(id, name, desc, features) \
  static const LangStandard Lang_##id = { name, desc, features };
#include "mlang/Frontend/LangStandards.def"

const LangStandard &LangStandard::getLangStandardForKind(Kind K) {
  switch (K) {
  default:
    llvm_unreachable("Invalid language kind!");
  case lang_unspecified:
    llvm::report_fatal_error("getLangStandardForKind() on unspecified kind");
#define LANGSTANDARD(id, name, desc, features) \
    case lang_##id: return Lang_##id;
#include "mlang/Frontend/LangStandards.def"
  }
}

const LangStandard *LangStandard::getLangStandardForName(llvm::StringRef Name) {
  Kind K = llvm::StringSwitch<Kind>(Name)
#define LANGSTANDARD(id, name, desc, features) \
    .Case(name, lang_##id)
#include "mlang/Frontend/LangStandards.def"
    .Default(lang_unspecified);
  if (K == lang_unspecified)
    return 0;

  return &getLangStandardForKind(K);
}


