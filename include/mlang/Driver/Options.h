//===--- Options.h - Option info & table ------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_DRIVER_OPTIONS_H
#define MLANG_DRIVER_OPTIONS_H

namespace mlang {
namespace driver {
  class OptTable;

namespace options {
  enum ID {
    OPT_INVALID = 0, // This is not an option ID.
#define OPTION(NAME, ID, KIND, GROUP, ALIAS, FLAGS, PARAM, \
               HELPTEXT, METAVAR) OPT_##ID,
#include "mlang/Driver/Options.inc"
    LastOption
#undef OPTION
  };
}

  // implemented in DriverOptions.cpp
  OptTable *createDriverOptTable();
}
}

#endif
