//===--- StmtDumper.cpp - Dumping implementation for Stmt ASTs ------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements the Stmt::dump/Stmt::print methods, which dump out the
// AST in a form that exposes type details and other fields.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/StmtVisitor.h"
#include "mlang/AST/Defn.h"
#include "mlang/AST/PrettyPrinter.h"
#include "mlang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"
using namespace mlang;

//===----------------------------------------------------------------------===//
// StmtDumper Visitor
//===----------------------------------------------------------------------===//
namespace  {
  class StmtDumper : public StmtVisitor<StmtDumper> {
    SourceManager *SM;
    llvm::raw_ostream &OS;
    unsigned IndentLevel;

    /// MaxDepth - When doing a normal dump (not dumpAll) we only want to dump
    /// the first few levels of an AST.  This keeps track of how many ast levels
    /// are left.
    unsigned MaxDepth;

    /// LastLocFilename/LastLocLine - Keep track of the last location we print
    /// out so that we can print out deltas from then on out.
    const char *LastLocFilename;
    unsigned LastLocLine;

  public:
    StmtDumper(SourceManager *sm, llvm::raw_ostream &os, unsigned maxDepth)
      : SM(sm), OS(os), IndentLevel(0-1), MaxDepth(maxDepth) {
      LastLocFilename = "";
      LastLocLine = ~0U;
    }

    void DumpSubTree(Stmt *S) {
      // Prune the recursion if not using dump all.
      if (MaxDepth == 0) return;

      ++IndentLevel;
      if (S) {
        if (DefnCmd* DS = dyn_cast<DefnCmd>(S))
          VisitDefnCmd(DS);
        else {
          Visit(S);

          // Print out children.
          Stmt::child_iterator CI = S->child_begin(), CE = S->child_end();
          if (CI != CE) {
            while (CI != CE) {
              OS << '\n';
              DumpSubTree(*CI++);
            }
          }
        }
        OS << ')';
      } else {
        Indent();
        OS << "<<<NULL>>>";
      }
      --IndentLevel;
    }

    void DumpDeclarator(Defn *D);

    void Indent() const {
      for (int i = 0, e = IndentLevel; i < e; ++i)
        OS << "  ";
    }

    void DumpType(Type T) {
      SplitQualType T_split = T.split();
      OS << "'" << Type::getAsString(T_split) << "'";

//      if (!T.isNull()) {
//        // If the type is sugared, also dump a (shallow) desugared type.
//        SplitQualType D_split = T.getSplitDesugaredType();
//        if (T_split != D_split)
//          OS << ":'" << Type::getAsString(D_split) << "'";
//      }
    }

    void DumpDefnRef(Defn *node);
    void DumpStmt(const Stmt *Node) {
          Indent();
          OS << "(" << Node->getStmtClassName()
             << " " << (void*)Node;
          DumpSourceRange(Node);
        }

    void DumpCmd(const Cmd *Node) {
      Indent();
      OS << "(" << Node->getStmtClassName()
         << " " << (void*)Node;
      DumpSourceRange(Node);
    }
    void DumpValueKind(ExprValueKind K) {
      switch (K) {
      case VK_RValue: break;
      case VK_LValue: OS << " lvalue"; break;
      }
    }

    void DumpExpr(const Expr *Node) {
      DumpStmt(Node);
      OS << ' ';
      DumpType(Node->getType());
      DumpValueKind(Node->getValueKind());
    }
    void DumpSourceRange(const Stmt *Node);
    void DumpLocation(SourceLocation Loc);

    // Cmds.
    void VisitCmd(Cmd *Node);
    void VisitDefnCmd(DefnCmd *Node);

    // Exprs
    void VisitExpr(Expr *Node);
    void VisitDefnRefExpr(DefnRefExpr *Node);
    void VisitCharacterLiteral(CharacterLiteral *Node);
    void VisitIntegerLiteral(IntegerLiteral *Node);
    void VisitFloatingLiteral(FloatingLiteral *Node);
    void VisitStringLiteral(StringLiteral *Str);
    void VisitImaginaryLiteral(ImaginaryLiteral *Node);
    void VisitUnaryOperator(UnaryOperator *Node);
    void VisitMemberExpr(MemberExpr *Node);
    void VisitBinaryOperator(BinaryOperator *Node);
    void VisitCompoundAssignOperator(CompoundAssignOperator *Node);
    void VisitConcatExpr(ConcatExpr *Node);
    void VisitColonExpr(ColonExpr *Node);
    void VisitFunctionCall(FunctionCall *Node);
  };
}

//===----------------------------------------------------------------------===//
//  Utilities
//===----------------------------------------------------------------------===//
void StmtDumper::DumpLocation(SourceLocation Loc) {
  SourceLocation SpellingLoc = SM->getSpellingLoc(Loc);

  // The general format we print out is filename:line:col, but we drop pieces
  // that haven't changed since the last loc printed.
  PresumedLoc PLoc = SM->getPresumedLoc(SpellingLoc);

  if (PLoc.isInvalid()) {
    OS << "<invalid sloc>";
    return;
  }

  if (strcmp(PLoc.getFilename(), LastLocFilename) != 0) {
    OS << PLoc.getFilename() << ':' << PLoc.getLine()
       << ':' << PLoc.getColumn();
    LastLocFilename = PLoc.getFilename();
    LastLocLine = PLoc.getLine();
  } else if (PLoc.getLine() != LastLocLine) {
    OS << "line" << ':' << PLoc.getLine()
       << ':' << PLoc.getColumn();
    LastLocLine = PLoc.getLine();
  } else {
    OS << "col" << ':' << PLoc.getColumn();
  }
}

void StmtDumper::DumpSourceRange(const Stmt *Node) {
  // Can't translate locations if a SourceManager isn't available.
  if (SM == 0) return;

  // TODO: If the parent expression is available, we can print a delta vs its
  // location.
  SourceRange R = Node->getSourceRange();

  OS << " <";
  DumpLocation(R.getBegin());
  if (R.getBegin() != R.getEnd()) {
    OS << ", ";
    DumpLocation(R.getEnd());
  }
  OS << ">";

  // <t2.c:123:421[blah], t2.c:412:321>

}


//===----------------------------------------------------------------------===//
//  Stmt printing methods.
//===----------------------------------------------------------------------===//

void StmtDumper::VisitCmd(Cmd *Node) {
  DumpCmd(Node);
}

void StmtDumper::DumpDeclarator(Defn *D) {

}

void StmtDumper::VisitDefnCmd(DefnCmd *Node) {
  DumpCmd(Node);
  OS << "\n";
  for (DefnCmd::defn_iterator DI = Node->defn_begin(), DE = Node->defn_end();
       DI != DE; ++DI) {
    Defn* D = *DI;
    ++IndentLevel;
    Indent();
    OS << (void*) D << " ";
    DumpDeclarator(D);
    if (DI+1 != DE)
      OS << "\n";
    --IndentLevel;
  }
}

//===----------------------------------------------------------------------===//
//  Expr printing methods.
//===----------------------------------------------------------------------===//
void StmtDumper::VisitExpr(Expr *Node) {
  DumpExpr(Node);
}

//void StmtDumper::VisitDefnRefExpr(DefnRefExpr *Node) {
//  DumpExpr(Node);
//
//  OS << " ";
//  switch (Node->getDefn()->getKind()) {
//  default: OS << "Decl"; break;
//  case Defn::Function: OS << "FunctionDecl"; break;
//  case Defn::Var: OS << "Var"; break;
//  case Defn::ParmVar: OS << "ParmVar"; break;
//  case Defn::EnumConstant: OS << "EnumConstant"; break;
//  case Defn::Typedef: OS << "Typedef"; break;
//  case Defn::Record: OS << "Record"; break;
//  case Defn::Enum: OS << "Enum"; break;
//  case Defn::CXXRecord: OS << "CXXRecord"; break;
//  case Defn::ObjCInterface: OS << "ObjCInterface"; break;
//  case Defn::ObjCClass: OS << "ObjCClass"; break;
//  }
//
//  OS << "='" << Node->getDefn() << "' " << (void*)Node->getDefn();
//}

//void StmtDumper::VisitUnresolvedLookupExpr(UnresolvedLookupExpr *Node) {
//  DumpExpr(Node);
//  OS << " (";
//  if (!Node->requiresADL()) OS << "no ";
//  OS << "ADL) = '" << Node->getName() << '\'';
//
//  UnresolvedLookupExpr::Defns_iterator
//    I = Node->Defns_begin(), E = Node->Defns_end();
//  if (I == E) OS << " empty";
//  for (; I != E; ++I)
//    OS << " " << (void*) *I;
//}

void StmtDumper::VisitCharacterLiteral(CharacterLiteral *Node) {
  DumpExpr(Node);
  OS << Node->getValue();
}

void StmtDumper::VisitIntegerLiteral(IntegerLiteral *Node) {
  DumpExpr(Node);

  bool isSigned = Node->getType()->isSignedIntegerType();
  OS << " " << Node->getValue().toString(10, isSigned);
}

void StmtDumper::VisitFloatingLiteral(FloatingLiteral *Node) {
  DumpExpr(Node);
  OS << " " << Node->getValueAsApproximateDouble();
}

void StmtDumper::VisitStringLiteral(StringLiteral *Str) {
  DumpExpr(Str);
  // FIXME: this doesn't print wstrings right.
  OS << " ";
  OS << '"';
  OS.write_escaped(Str->getString());
  OS << '"';
}

void StmtDumper::VisitImaginaryLiteral(ImaginaryLiteral *Node) {
  DumpExpr(Node);
  Type ty = Node->getSubExpr()->getType();
  if(ty->isIntegerType()) {
  bool isSigned = Node->getType()->isSignedIntegerType();
  OS << " " << cast<IntegerLiteral>(Node->getSubExpr())
		  ->getValue().toString(10, isSigned);
  } else if (ty->isFloatingPointType()) {
	  OS << " " << cast<FloatingLiteral>(Node->getSubExpr())
	  		  ->getValueAsApproximateDouble();
  }
  OS << "i";
}

void StmtDumper::VisitUnaryOperator(UnaryOperator *Node) {
  DumpExpr(Node);
  OS << " " << (Node->isPostfix() ? "postfix" : "prefix")
     << " '" << UnaryOperator::getOpcodeStr(Node->getOpcode()) << "'";
}

void StmtDumper::VisitMemberExpr(MemberExpr *Node) {
  DumpExpr(Node);
  OS << " " << "."
     << Node->getMemberDefn() << ' '
     << (void*)Node->getMemberDefn();
}

void StmtDumper::VisitBinaryOperator(BinaryOperator *Node) {
  DumpExpr(Node);
  OS << " '" << BinaryOperator::getOpcodeStr(Node->getOpcode()) << "'";
}

void StmtDumper::VisitCompoundAssignOperator(CompoundAssignOperator *Node) {
  DumpExpr(Node);
  OS << " '" << BinaryOperator::getOpcodeStr(Node->getOpcode())
     << "' ComputeLHSTy=";
  DumpType(Node->getComputationLHSType());
  OS << " ComputeResultTy=";
  DumpType(Node->getComputationResultType());
}

void StmtDumper::VisitDefnRefExpr(DefnRefExpr *Node) {
  DumpExpr(Node);

  OS << " ";
  DumpDefnRef(Node->getDefn());
}

void StmtDumper::DumpDefnRef(Defn *d) {
  OS << d->getDefnKindName() << ' ' << (void*) d;

  if (NamedDefn *nd = dyn_cast<NamedDefn>(d)) {
    OS << " '";
    nd->getDefnName().printName(OS);
    OS << "'";
  }

  if (ValueDefn *vd = dyn_cast<ValueDefn>(d)) {
    OS << ' '; DumpType(vd->getType());
  }
}

void StmtDumper::VisitConcatExpr(ConcatExpr *Node) {
  DumpExpr(Node);
  OS << " '" <<  "Yabin'";
}

void StmtDumper::VisitColonExpr(ColonExpr *Node) {
  DumpExpr(Node);

  if(Node->IsBinaryOp()) {
	  OS << " 'Is Binary Operator'";
  } else if (Node->IsMagicColon())
	  OS << " 'Is Magic Colon'";
}

void StmtDumper::VisitFunctionCall(FunctionCall *Node) {
	DumpExpr(Node);
}
//===----------------------------------------------------------------------===//
// Stmt method implementations
//===----------------------------------------------------------------------===//

/// dump - This does a local dump of the specified AST fragment.  It dumps the
/// specified node and a few nodes underneath it, but not the whole subtree.
/// This is useful in a debugger.
void Stmt::dump(SourceManager &SM) const {
  dump(llvm::errs(), SM);
}

void Stmt::dump(llvm::raw_ostream &OS, SourceManager &SM) const {
  StmtDumper P(&SM, OS, 4);
  P.DumpSubTree(const_cast<Stmt*>(this));
  OS << "\n";
}

/// dump - This does a local dump of the specified AST fragment.  It dumps the
/// specified node and a few nodes underneath it, but not the whole subtree.
/// This is useful in a debugger.
void Stmt::dump() const {
  StmtDumper P(0, llvm::errs(), 4);
  P.DumpSubTree(const_cast<Stmt*>(this));
  llvm::errs() << "\n";
}

/// dumpAll - This does a dump of the specified AST fragment and all subtrees.
void Stmt::dumpAll(SourceManager &SM) const {
  StmtDumper P(&SM, llvm::errs(), ~0U);
  P.DumpSubTree(const_cast<Stmt*>(this));
  llvm::errs() << "\n";
}

/// dumpAll - This does a dump of the specified AST fragment and all subtrees.
void Stmt::dumpAll() const {
  StmtDumper P(0, llvm::errs(), ~0U);
  P.DumpSubTree(const_cast<Stmt*>(this));
  llvm::errs() << "\n";
}
