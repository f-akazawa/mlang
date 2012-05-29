//===--- StmtViz.cpp - Graphviz visualization for Stmt ASTs -----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements Stmt::viewAST, which generates a Graphviz DOT file
//  that depicts the AST and then calls Graphviz/dot+gv on it.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/StmtGraphTraits.h"
#include "llvm/Support/GraphWriter.h"

using namespace mlang;

void Stmt::viewAST() const {
#ifndef NDEBUG
  llvm::ViewGraph(this,"AST");
#else
  llvm::errs() << "Stmt::viewAST is only available in debug builds on "
               << "systems with Graphviz or gv!\n";
#endif
}

namespace llvm {
template<>
struct DOTGraphTraits<const Stmt*> : public DefaultDOTGraphTraits {
  DOTGraphTraits (bool isSimple=false) : DefaultDOTGraphTraits(isSimple) {}

  static std::string getNodeLabel(const Stmt* Node, const Stmt* Graph) {

#ifndef NDEBUG
    std::string OutSStr;
    llvm::raw_string_ostream Out(OutSStr);

    if (Node)
      Out << Node->getStmtClassName();
    else
      Out << "<NULL>";

    std::string OutStr = Out.str();
    if (OutStr[0] == '\n') OutStr.erase(OutStr.begin());

    // Process string output to make it nicer...
    for (unsigned i = 0; i != OutStr.length(); ++i)
      if (OutStr[i] == '\n') {                            // Left justify
        OutStr[i] = '\\';
        OutStr.insert(OutStr.begin()+i+1, 'l');
      }

    return OutStr;
#else
    return "";
#endif
  }
};
} // end namespace llvm
