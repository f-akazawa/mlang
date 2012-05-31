//===- TableGen.cpp - Top-Level TableGen implementation for Mlang ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the main function for Mlang's TableGen.
//
//===----------------------------------------------------------------------===//

#include "MlangASTNodesEmitter.h"
#include "MlangAttrEmitter.h"
#include "MlangDiagnosticsEmitter.h"
#include "OptParserEmitter.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Main.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenAction.h"

using namespace llvm;

enum ActionType {
  GenMlangAttrClasses,
  GenMlangAttrImpl,
  GenMlangAttrList,
  GenMlangAttrPCHRead,
  GenMlangAttrPCHWrite,
  GenMlangAttrSpellingList,
  GenMlangAttrLateParsedList,
  GenMlangAttrTemplateInstantiate,
  GenMlangAttrParsedAttrList,
  GenMlangAttrParsedAttrKinds,
  GenMlangDiagsDefs,
  GenMlangDiagGroups,
  GenMlangDiagsIndexName,
  GenMlangDeclNodes,
  GenMlangStmtNodes,
  GenOptParserDefs,
  GenOptParserImpl,
};

namespace {
  cl::opt<ActionType>
  Action(cl::desc("Action to perform:"),
         cl::values(clEnumValN(GenOptParserDefs, "gen-opt-parser-defs",
                               "Generate option definitions"),
                    clEnumValN(GenOptParserImpl, "gen-opt-parser-impl",
                               "Generate option parser implementation"),
                    clEnumValN(GenMlangAttrClasses, "gen-mlang-attr-classes",
                               "Generate mlang attribute clases"),
                    clEnumValN(GenMlangAttrImpl, "gen-mlang-attr-impl",
                               "Generate mlang attribute implementations"),
                    clEnumValN(GenMlangAttrList, "gen-mlang-attr-list",
                               "Generate a mlang attribute list"),
                    clEnumValN(GenMlangAttrPCHRead, "gen-mlang-attr-pch-read",
                               "Generate mlang PCH attribute reader"),
                    clEnumValN(GenMlangAttrPCHWrite, "gen-mlang-attr-pch-write",
                               "Generate mlang PCH attribute writer"),
                    clEnumValN(GenMlangAttrSpellingList,
                               "gen-mlang-attr-spelling-list",
                               "Generate a mlang attribute spelling list"),
                    clEnumValN(GenMlangAttrLateParsedList,
                               "gen-mlang-attr-late-parsed-list",
                               "Generate a mlang attribute LateParsed list"),
                    clEnumValN(GenMlangAttrTemplateInstantiate,
                               "gen-mlang-attr-template-instantiate",
                               "Generate a mlang template instantiate code"),
                    clEnumValN(GenMlangAttrParsedAttrList,
                               "gen-mlang-attr-parsed-attr-list",
                               "Generate a mlang parsed attribute list"),
                    clEnumValN(GenMlangAttrParsedAttrKinds,
                               "gen-mlang-attr-parsed-attr-kinds",
                               "Generate a mlang parsed attribute kinds"),
                    clEnumValN(GenMlangDiagsDefs, "gen-mlang-diags-defs",
                               "Generate Mlang diagnostics definitions"),
                    clEnumValN(GenMlangDiagGroups, "gen-mlang-diag-groups",
                               "Generate Mlang diagnostic groups"),
                    clEnumValN(GenMlangDiagsIndexName,
                               "gen-mlang-diags-index-name",
                               "Generate Mlang diagnostic name index"),
                    clEnumValN(GenMlangDeclNodes, "gen-mlang-decl-nodes",
                               "Generate Mlang AST declaration nodes"),
                    clEnumValN(GenMlangStmtNodes, "gen-mlang-stmt-nodes",
                               "Generate Mlang AST statement nodes"),
                    clEnumValEnd));

  cl::opt<std::string>
  MlangComponent("mlang-component",
                 cl::desc("Only use warnings from specified component"),
                 cl::value_desc("component"), cl::Hidden);

class MlangTableGenAction : public TableGenAction {
public:
  bool operator()(raw_ostream &OS, RecordKeeper &Records) {
    switch (Action) {
    case GenMlangAttrClasses:
      MlangAttrClassEmitter(Records).run(OS);
      break;
    case GenMlangAttrImpl:
      MlangAttrImplEmitter(Records).run(OS);
      break;
    case GenMlangAttrList:
      MlangAttrListEmitter(Records).run(OS);
      break;
    case GenMlangAttrPCHRead:
      MlangAttrPCHReadEmitter(Records).run(OS);
      break;
    case GenMlangAttrPCHWrite:
      MlangAttrPCHWriteEmitter(Records).run(OS);
      break;
    case GenMlangAttrSpellingList:
      MlangAttrSpellingListEmitter(Records).run(OS);
      break;
    case GenMlangAttrLateParsedList:
      MlangAttrLateParsedListEmitter(Records).run(OS);
      break;
    case GenMlangAttrTemplateInstantiate:
      MlangAttrTemplateInstantiateEmitter(Records).run(OS);
      break;
    case GenMlangAttrParsedAttrList:
      MlangAttrParsedAttrListEmitter(Records).run(OS);
      break;
    case GenMlangAttrParsedAttrKinds:
      MlangAttrParsedAttrKindsEmitter(Records).run(OS);
      break;
    case GenMlangDiagsDefs:
      MlangDiagsDefsEmitter(Records, MlangComponent).run(OS);
      break;
    case GenMlangDiagGroups:
      MlangDiagGroupsEmitter(Records).run(OS);
      break;
    case GenMlangDiagsIndexName:
      MlangDiagsIndexNameEmitter(Records).run(OS);
      break;
    case GenMlangDeclNodes:
      MlangASTNodesEmitter(Records, "Decl", "Decl").run(OS);
      MlangDeclContextEmitter(Records).run(OS);
      break;
    case GenMlangStmtNodes:
      MlangASTNodesEmitter(Records, "Stmt", "").run(OS);
      break;
    case GenOptParserDefs:
      OptParserEmitter(Records, true).run(OS);
      break;
    case GenOptParserImpl:
      OptParserEmitter(Records, false).run(OS);
      break;
    }

    return false;
  }
};
}

int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv);

  MlangTableGenAction Action;
  return TableGenMain(argv[0], Action);
}
