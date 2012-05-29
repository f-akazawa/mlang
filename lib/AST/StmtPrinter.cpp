//===--- StmtPrinter.cpp - Printing implementation for Stmt ASTs ----------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements the Stmt::dumpPretty/Stmt::printPretty methods, which
// pretty print the AST back out to C code.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/StmtVisitor.h"
#include "mlang/AST/Defn.h"
#include "mlang/AST/PrettyPrinter.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Format.h"
#include "mlang/AST/CmdAll.h"
#include "mlang/AST/ExprAll.h"

using namespace mlang;

//===----------------------------------------------------------------------===//
// StmtPrinter Visitor
//===----------------------------------------------------------------------===//

namespace  {
  class StmtPrinter : public StmtVisitor<StmtPrinter> {
    llvm::raw_ostream &OS;
    ASTContext &Context;
    unsigned IndentLevel;
    mlang::PrinterHelper* Helper;
    PrintingPolicy Policy;

  public:
    StmtPrinter(llvm::raw_ostream &os, ASTContext &C, PrinterHelper* helper,
                const PrintingPolicy &Policy,
                unsigned Indentation = 0)
      : OS(os), Context(C), IndentLevel(Indentation), Helper(helper),
        Policy(Policy) {}

    void PrintStmt(Stmt *S) {
      PrintStmt(S, Policy.Indentation);
    }

    void PrintStmt(Stmt *S, int SubIndent) {
      IndentLevel += SubIndent;
      if (S && isa<Expr>(S)) {
        // If this is an expr used in a stmt context, indent and newline it.
        Indent();
        Visit(S);
        OS << ";\n";
      } else if (S) {
        Visit(S);
      } else {
        Indent() << "<<<NULL STATEMENT>>>\n";
      }
      IndentLevel -= SubIndent;
    }

    void PrintRawBlockCmd(BlockCmd *S);
    void PrintRawDefn(Defn *D);
    void PrintRawDefnCmd(DefnCmd *S);
    void PrintRawIfCmd(IfCmd *If);
    void PrintRawCatchCmd(CatchCmd *Catch);

    void PrintExpr(Expr *E) {
      if (E)
        Visit(E);
      else
        OS << "<null expr>";
    }

    llvm::raw_ostream &Indent(int Delta = 0) {
      for (int i = 0, e = IndentLevel+Delta; i < e; ++i)
        OS << "  ";
      return OS;
    }

    void Visit(Stmt* S) {
      if (Helper && Helper->handledStmt(S,OS))
          return;
      else StmtVisitor<StmtPrinter>::Visit(S);
    }
    
    void VisitCmd(Cmd *Node) LLVM_ATTRIBUTE_UNUSED {
      Indent() << "<<unknown stmt type>>\n";
    }
    void VisitExpr(Expr *Node) LLVM_ATTRIBUTE_UNUSED {
      OS << "<<unknown expr type>>";
    }

#define ABSTRACT_STMT(CLASS)
#define STMT(CLASS, PARENT) \
    void Visit##CLASS(CLASS *Node);
#include "mlang/AST/StmtNodes.inc"
  };
}

//===----------------------------------------------------------------------===//
//  Stmt printing methods.
//===----------------------------------------------------------------------===//

/// PrintRawBlockCmd - Print a compound stmt without indenting the {, and
/// with no newline after the }.
void StmtPrinter::PrintRawBlockCmd(BlockCmd *Node) {
  OS << "{\n";
  for (BlockCmd::body_iterator I = Node->body_begin(), E = Node->body_end();
       I != E; ++I)
    PrintStmt(*I);

  Indent() << "}";
}

void StmtPrinter::PrintRawDefn(Defn *D) {
  D->print(OS, Policy, IndentLevel);
}

void StmtPrinter::PrintRawDefnCmd(DefnCmd *S) {
	DefnCmd::defn_iterator Begin = S->defn_begin(), End = S->defn_end();
  llvm::SmallVector<Defn*, 2> Defns;
  for ( ; Begin != End; ++Begin)
    Defns.push_back(*Begin);

  Defn::printGroup(Defns.data(), Defns.size(), OS, Policy, IndentLevel);
}

void StmtPrinter::VisitNullCmd(NullCmd *Node) {
  Indent() << ";\n";
}

void StmtPrinter::VisitDefnCmd(DefnCmd *Node) {
  Indent();
  PrintRawDefnCmd(Node);
  OS << ";\n";
}

void StmtPrinter::VisitBlockCmd(BlockCmd *Node) {
  Indent();
  PrintRawBlockCmd(Node);
  OS << "\n";
}

void StmtPrinter::VisitCaseCmd(CaseCmd *Node) {
  Indent(-1) << "case ";
  PrintExpr(Node->getPattern());
//  if (Node->getSubStmt()) {
//    OS << " ... ";
//    PrintStmt(Node->getSubStmt());
//  }
//  OS << ":\n";
//
//  PrintStmt(Node->getSubStmt(), 0);
}

void StmtPrinter::VisitOtherwiseCmd(OtherwiseCmd *Node) {
  Indent(-1) << "default:\n";
//  PrintStmt(Node->getSubStmt(), 0);
}

void StmtPrinter::PrintRawIfCmd(IfCmd *If) {
  OS << "if (";
  PrintExpr(If->getCond());
  OS << ')';

  if (BlockCmd *CS = dyn_cast<BlockCmd>(If->getThen())) {
    OS << ' ';
    PrintRawBlockCmd(CS);
    OS << (If->getElse() ? ' ' : '\n');
  } else {
    OS << '\n';
    PrintStmt(If->getThen());
    if (If->getElse()) Indent();
  }

  if (Stmt *Else = If->getElse()) {
    OS << "else";

    if (BlockCmd *CS = dyn_cast<BlockCmd>(Else)) {
      OS << ' ';
      PrintRawBlockCmd(CS);
      OS << '\n';
    } else if (IfCmd *ElseIf = dyn_cast<IfCmd>(Else)) {
      OS << ' ';
      PrintRawIfCmd(ElseIf);
    } else {
      OS << '\n';
      PrintStmt(If->getElse());
    }
  }
}

void StmtPrinter::VisitIfCmd(IfCmd *If) {
  Indent();
  PrintRawIfCmd(If);
}

void StmtPrinter::VisitSwitchCmd(SwitchCmd *Node) {
  Indent() << "switch (";
  PrintExpr(Node->getCondition());
  OS << ")";

  // Pretty print compoundstmt bodies (very common).
//  if (BlockCmd *CS = dyn_cast<BlockCmd>(Node->getBody())) {
//    OS << " ";
//    PrintRawBlockCmd(CS);
//    OS << "\n";
//  } else {
//    OS << "\n";
//    PrintStmt(Node->getBody());
//  }
}

void StmtPrinter::VisitWhileCmd(WhileCmd *Node) {
  Indent() << "while (";
  PrintExpr(Node->getCond());
  OS << ")\n";
  PrintStmt(Node->getBody());
}

//void StmtPrinter::VisitDoStmt(DoStmt *Node) {
//  Indent() << "do ";
//  if (BlockCmd *CS = dyn_cast<BlockCmd>(Node->getBody())) {
//    PrintRawBlockCmd(CS);
//    OS << " ";
//  } else {
//    OS << "\n";
//    PrintStmt(Node->getBody());
//    Indent();
//  }
//
//  OS << "while (";
//  PrintExpr(Node->getCond());
//  OS << ");\n";
//}

void StmtPrinter::VisitForCmd(ForCmd *Node) {
  Indent() << "for (";
//  if (Node->getInit()) {
//    if (DefnCmd *DS = dyn_cast<DefnCmd>(Node->getInit()))
//      PrintRawDefnCmd(DS);
//    else
//      PrintExpr(cast<Expr>(Node->getInit()));
//  }
//  OS << ";";
//  if (Node->getCond()) {
//    OS << " ";
//    PrintExpr(Node->getCond());
//  }
//  OS << ";";
//  if (Node->getInc()) {
//    OS << " ";
//    PrintExpr(Node->getInc());
//  }
//  OS << ") ";

  if (BlockCmd *CS = dyn_cast<BlockCmd>(Node->getBody())) {
    PrintRawBlockCmd(CS);
    OS << "\n";
  } else {
    OS << "\n";
    PrintStmt(Node->getBody());
  }
}

void StmtPrinter::VisitContinueCmd(ContinueCmd *Node) {
  Indent() << "continue;\n";
}

void StmtPrinter::VisitBreakCmd(BreakCmd *Node) {
  Indent() << "break;\n";
}


void StmtPrinter::VisitReturnCmd(ReturnCmd *Node) {
  Indent() << "return";
//  if (Node->getRetValue()) {
//    OS << " ";
//    PrintExpr(Node->getRetValue());
//  }
  OS << ";\n";
}

void StmtPrinter::PrintRawCatchCmd(CatchCmd *Node) {
  OS << "catch (";
//  if (Defn *ExDefn = Node->getExceptionDefn())
//    PrintRawDefn(ExDefn);
//  else
    OS << "...";
  OS << ") ";
  PrintRawBlockCmd(cast<BlockCmd>(Node->getHandlerBlock()));
}

void StmtPrinter::VisitCatchCmd(CatchCmd *Node) {
  Indent();
  PrintRawCatchCmd(Node);
  OS << "\n";
}

void StmtPrinter::VisitTryCmd(TryCmd *Node) {
  Indent() << "try ";
  PrintRawBlockCmd(Node->getTryBlock());
//  for (unsigned i = 0, e = Node->getNumHandlers(); i < e; ++i) {
//    OS << " ";
//    PrintRawCatchCmd(Node->getHandler(i));
//  }
  OS << "\n";
}

void StmtPrinter::VisitElseIfCmd(ElseIfCmd *Node) {

}

void StmtPrinter::VisitElseCmd(ElseCmd *Node) {

}

void StmtPrinter::VisitThenCmd(ThenCmd *Node) {

}

void StmtPrinter::VisitScopeDefnCmd(ScopeDefnCmd *Node) {

}

//===----------------------------------------------------------------------===//
//  Expr printing methods.
//===----------------------------------------------------------------------===//

void StmtPrinter::VisitDefnRefExpr(DefnRefExpr *Node) {
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  OS << Node->getNameInfo();
}

void StmtPrinter::VisitCharacterLiteral(CharacterLiteral *Node) {
  unsigned value = Node->getValue();
  switch (value) {
  case '\\':
    OS << "'\\\\'";
    break;
  case '\'':
    OS << "'\\''";
    break;
  case '\a':
    // TODO: K&R: the meaning of '\\a' is different in traditional C
    OS << "'\\a'";
    break;
  case '\b':
    OS << "'\\b'";
    break;
  // Nonstandard escape sequence.
  /*case '\e':
    OS << "'\\e'";
    break;*/
  case '\f':
    OS << "'\\f'";
    break;
  case '\n':
    OS << "'\\n'";
    break;
  case '\r':
    OS << "'\\r'";
    break;
  case '\t':
    OS << "'\\t'";
    break;
  case '\v':
    OS << "'\\v'";
    break;
  default:
    if (value < 256 && isprint(value)) {
      OS << "'" << (char)value << "'";
    } else if (value < 256) {
      OS << "'\\x" << llvm::format("%x", value) << "'";
    } else {
      // FIXME what to really do here?
      OS << value;
    }
  }
}

void StmtPrinter::VisitIntegerLiteral(IntegerLiteral *Node) {
  bool isSigned = Node->getType()->isSignedIntegerType();
  OS << Node->getValue().toString(10, isSigned);
}

void StmtPrinter::VisitFloatingLiteral(FloatingLiteral *Node) {
  // FIXME: print value more precisely.
  OS << Node->getValueAsApproximateDouble();
}

void StmtPrinter::VisitImaginaryLiteral(ImaginaryLiteral *Node) {
  PrintExpr(Node->getSubExpr());
  OS << "i";
}

void StmtPrinter::VisitStringLiteral(StringLiteral *Str) {
  OS << '"';

  // FIXME: this doesn't print wstrings right.
  llvm::StringRef StrData = Str->getString();
  for (llvm::StringRef::iterator I = StrData.begin(), E = StrData.end(); 
                                                             I != E; ++I) {
    unsigned char Char = *I;

    switch (Char) {
    default:
      if (isprint(Char))
        OS << (char)Char;
      else  // Output anything hard as an octal escape.
        OS << '\\'
        << (char)('0'+ ((Char >> 6) & 7))
        << (char)('0'+ ((Char >> 3) & 7))
        << (char)('0'+ ((Char >> 0) & 7));
      break;
    // Handle some common non-printable cases to make dumps prettier.
    case '\\': OS << "\\\\"; break;
    case '"': OS << "\\\""; break;
    case '\n': OS << "\\n"; break;
    case '\t': OS << "\\t"; break;
    case '\a': OS << "\\a"; break;
    case '\b': OS << "\\b"; break;
    }
  }
  OS << '"';
}

void StmtPrinter::VisitParenExpr(ParenExpr *Node) {
  OS << "(";
  PrintExpr(Node->getSubExpr());
  OS << ")";
}

void StmtPrinter::VisitUnaryOperator(UnaryOperator *Node) {
  if (!Node->isPostfix()) {
    OS << UnaryOperator::getOpcodeStr(Node->getOpcode());

    // Print a space if this is an "identifier operator" like __real, or if
    // it might be concatenated incorrectly like '+'.
    switch (Node->getOpcode()) {
    default: break;
    case UO_Plus:
    case UO_Minus:
      if (isa<UnaryOperator>(Node->getSubExpr()))
        OS << ' ';
      break;
    }
  }
  PrintExpr(Node->getSubExpr());

  if (Node->isPostfix())
    OS << UnaryOperator::getOpcodeStr(Node->getOpcode());
}

void StmtPrinter::VisitArrayIndex(ArrayIndex *Node) {
  PrintExpr(Node->getBase());
  OS << "[";
  PrintExpr(Node->getIdx(0));
  OS << "]";
}

void StmtPrinter::VisitFunctionCall(FunctionCall *Call) {
  PrintExpr(Call->getCallee());
  OS << "(";
  for (unsigned i = 0, e = Call->getNumArgs(); i != e; ++i) {
    if (i) OS << ", ";
    PrintExpr(Call->getArg(i));
  }
  OS << ")";
}

void StmtPrinter::VisitMemberExpr(MemberExpr *Node) {
  // FIXME: Suppress printing implicit bases (like "this")
  PrintExpr(Node->getBase());
  OS << ".";
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);

//  OS << Node->getMemberNameInfo();
}

void StmtPrinter::VisitBinaryOperator(BinaryOperator *Node) {
  PrintExpr(Node->getLHS());
  OS << " " << BinaryOperator::getOpcodeStr(Node->getOpcode()) << " ";
  PrintExpr(Node->getRHS());
}

void StmtPrinter::VisitCompoundAssignOperator(CompoundAssignOperator *Node) {
  PrintExpr(Node->getLHS());
  OS << " " << BinaryOperator::getOpcodeStr(Node->getOpcode()) << " ";
  PrintExpr(Node->getRHS());
}

void StmtPrinter::VisitParenListExpr(ParenListExpr* Node) {
  OS << "( ";
  for (unsigned i = 0, e = Node->getNumExprs(); i != e; ++i) {
    if (i) OS << ", ";
    PrintExpr(Node->getExpr(i));
  }
  OS << " )";
}

void StmtPrinter::VisitInitListExpr(InitListExpr* Node) {

}

void StmtPrinter::VisitMultiOutput(MultiOutput* Node) {

}

void StmtPrinter::VisitRowVectorExpr(RowVectorExpr* Node) {

}

void StmtPrinter::VisitColonExpr(ColonExpr* Node) {

}

void StmtPrinter::VisitConcatExpr(ConcatExpr* Node) {

}

void StmtPrinter::VisitCellConcatExpr(CellConcatExpr* Node) {

}

void StmtPrinter::VisitFuncHandle(FuncHandle* Node) {

}

void StmtPrinter::VisitAnonFuncHandle(AnonFuncHandle* Node) {

}

void StmtPrinter::VisitOpaqueValueExpr(OpaqueValueExpr* Node) {

}
//===----------------------------------------------------------------------===//
// Stmt method implementations
//===----------------------------------------------------------------------===//

void Stmt::dumpPretty(ASTContext& Context) const {
  printPretty(llvm::errs(), Context, 0,
              PrintingPolicy(Context.getLangOptions()));
}

void Stmt::printPretty(llvm::raw_ostream &OS, ASTContext& Context,
                       PrinterHelper* Helper,
                       const PrintingPolicy &Policy,
                       unsigned Indentation) const {
  if (this == 0) {
    OS << "<NULL>";
    return;
  }

  if (Policy.Dump && &Context) {
    dump(OS, Context.getSourceManager());
    return;
  }

  StmtPrinter P(OS, Context, Helper, Policy, Indentation);
  P.Visit(const_cast<Stmt*>(this));
}

//===----------------------------------------------------------------------===//
// PrinterHelper
//===----------------------------------------------------------------------===//

// Implement virtual destructor.
PrinterHelper::~PrinterHelper() {}
