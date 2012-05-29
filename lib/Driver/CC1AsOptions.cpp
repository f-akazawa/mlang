//===--- CC1AsOptions.cpp - Clang Assembler Options Table -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "mlang/Driver/CC1AsOptions.h"
#include "mlang/Driver/Option.h"
#include "mlang/Driver/OptTable.h"
using namespace mlang;
using namespace mlang::driver;
using namespace mlang::driver::options;
using namespace mlang::driver::cc1asoptions;

static const OptTable::Info CC1AsInfoTable[] = {
#define OPTION(NAME, ID, KIND, GROUP, ALIAS, FLAGS, PARAM, \
               HELPTEXT, METAVAR)   \
  { NAME, HELPTEXT, METAVAR, Option::KIND##Class, FLAGS, PARAM, \
    OPT_##GROUP, OPT_##ALIAS },
#include "mlang/Driver/CC1AsOptions.inc"
};

namespace {

class CC1AsOptTable : public OptTable {
public:
  CC1AsOptTable()
    : OptTable(CC1AsInfoTable,
               sizeof(CC1AsInfoTable) / sizeof(CC1AsInfoTable[0])) {}
};

}

OptTable *mlang::driver::createCC1AsOptTable() {
  return new CC1AsOptTable();
}
