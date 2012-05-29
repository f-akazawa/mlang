//===--- CC1Options.cpp - Clang CC1 Options Table -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "mlang/Driver/CC1Options.h"
#include "mlang/Driver/Option.h"
#include "mlang/Driver/OptTable.h"
using namespace mlang;
using namespace mlang::driver;
using namespace mlang::driver::options;
using namespace mlang::driver::cc1options;

static const OptTable::Info CC1InfoTable[] = {
#define OPTION(NAME, ID, KIND, GROUP, ALIAS, FLAGS, PARAM, \
               HELPTEXT, METAVAR)   \
  { NAME, HELPTEXT, METAVAR, Option::KIND##Class, FLAGS, PARAM, \
    OPT_##GROUP, OPT_##ALIAS },
#include "mlang/Driver/CC1Options.inc"
};

namespace {

class CC1OptTable : public OptTable {
public:
  CC1OptTable()
    : OptTable(CC1InfoTable, sizeof(CC1InfoTable) / sizeof(CC1InfoTable[0])) {}
};

}

OptTable *mlang::driver::createCC1OptTable() {
  return new CC1OptTable();
}
