//===--- CC1AsOptions.h - Clang Assembler Options Table ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_DRIVER_CC1ASOPTIONS_H
#define MLANG_DRIVER_CC1ASOPTIONS_H

namespace mlang {
namespace driver {
  class OptTable;

namespace cc1asoptions {
  enum ID {
    OPT_INVALID = 0, // This is not an option ID.
#define OPTION(NAME, ID, KIND, GROUP, ALIAS, FLAGS, PARAM, \
               HELPTEXT, METAVAR) OPT_##ID,
#include "mlang/Driver/CC1AsOptions.inc"
    LastOption
#undef OPTION
  };
}

  OptTable *createCC1AsOptTable();
}
}

#endif
