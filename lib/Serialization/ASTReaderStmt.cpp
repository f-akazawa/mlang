//===--- ASTReaderStmt.cpp - Stmt/Expr Deserialization ----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Statement/expression deserialization.  This implements the
// ASTReader::ReadStmt method.
//
//===----------------------------------------------------------------------===//

#include "mlang/Serialization/ASTReader.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/StmtVisitor.h"
#include "llvm/Support/ErrorHandling.h"

using namespace mlang;
using namespace mlang::serialization;

namespace mlang {

  class ASTStmtReader : public StmtVisitor<ASTStmtReader> {
    ASTReader &Reader;
    ASTReader::PerFileData &F;
    llvm::BitstreamCursor &DeclsCursor;
    const ASTReader::RecordData &Record;
    unsigned &Idx;

    SourceLocation ReadSourceLocation(const ASTReader::RecordData &R,
                                      unsigned &I) {
      return Reader.ReadSourceLocation(F, R, I);
    }
    SourceRange ReadSourceRange(const ASTReader::RecordData &R, unsigned &I) {
      return Reader.ReadSourceRange(F, R, I);
    }
    void ReadDefinitionNameInfo(DefinitionNameInfo &NameInfo,
                                const ASTReader::RecordData &R, unsigned &I) {
      Reader.ReadDefinitionNameInfo(F, NameInfo, R, I);
    }

  public:
    ASTStmtReader(ASTReader &Reader, ASTReader::PerFileData &F,
                  llvm::BitstreamCursor &Cursor,
                  const ASTReader::RecordData &Record, unsigned &Idx)
      : Reader(Reader), F(F), DeclsCursor(Cursor), Record(Record), Idx(Idx) { }

    /// \brief The number of record fields required for the Stmt class
    /// itself.
    static const unsigned NumStmtFields = 0;

    /// \brief The number of record fields required for the Expr class
    /// itself.
    static const unsigned NumExprFields = NumStmtFields + 6;
    
    void VisitStmt(Stmt *S);

    void VisitCmd(Cmd *C);
    void VisitNullCmd(NullCmd *S);
    void VisitBlockCmd(BlockCmd *S);
    void VisitCaseCmd(CaseCmd *S);
    void VisitOtherwiseCmd(OtherwiseCmd *S);
    void VisitIfCmd(IfCmd *S);
    void VisitSwitchCmd(SwitchCmd *S);
    void VisitWhileCmd(WhileCmd *S);
    void VisitForCmd(ForCmd *S);
    void VisitContinueCmd(ContinueCmd *S);
    void VisitBreakCmd(BreakCmd *S);
    void VisitReturnCmd(ReturnCmd *S);
    void VisitDefnCmd(DefnCmd *S);
    void VisitExpr(Expr *E);
    void VisitDefnRefExpr(DefnRefExpr *E);
    void VisitIntegerLiteral(IntegerLiteral *E);
    void VisitFloatingLiteral(FloatingLiteral *E);
    void VisitImaginaryLiteral(ImaginaryLiteral *E);
    void VisitStringLiteral(StringLiteral *E);
    void VisitCharacterLiteral(CharacterLiteral *E);
    void VisitParenExpr(ParenExpr *E);
    void VisitParenListExpr(ParenListExpr *E);
    void VisitUnaryOperator(UnaryOperator *E);
    void VisitArrayIndex(ArrayIndex *E);
    void VisitFunctionCall(FunctionCall *E);
    void VisitMemberExpr(MemberExpr *E);
    void VisitBinaryOperator(BinaryOperator *E);
    void VisitCompoundAssignOperator(CompoundAssignOperator *E);
    void VisitInitListExpr(InitListExpr *E);

    // OOP Statements
    void VisitCatchCmd(CatchCmd *S);
    void VisitTryCmd(TryCmd *S);
  };
}

void ASTStmtReader::VisitStmt(Stmt *S) {
  assert(Idx == NumStmtFields && "Incorrect statement field count");
}

void ASTStmtReader::VisitCmd(Cmd *C) {
  assert(Idx == NumStmtFields && "Incorrect statement field count");
}

void ASTStmtReader::VisitNullCmd(NullCmd *S) {
  VisitStmt(S);
  S->setSemiLoc(ReadSourceLocation(Record, Idx));
//  S->LeadingEmptyMacro = ReadSourceLocation(Record, Idx);
}

void ASTStmtReader::VisitBlockCmd(BlockCmd *S) {
  VisitStmt(S);
  llvm::SmallVector<Stmt *, 16> Stmts;
  unsigned NumStmts = Record[Idx++];
  while (NumStmts--)
    Stmts.push_back(Reader.ReadSubStmt());
  S->setStmts(*Reader.getContext(), Stmts.data(), Stmts.size());
//  S->setLBracLoc(ReadSourceLocation(Record, Idx));
//  S->setRBracLoc(ReadSourceLocation(Record, Idx));
}

void ASTStmtReader::VisitCaseCmd(CaseCmd *S) {
  VisitStmt(S);

}

void ASTStmtReader::VisitOtherwiseCmd(OtherwiseCmd *S) {
	VisitStmt(S);

}

void ASTStmtReader::VisitIfCmd(IfCmd *S) {
  VisitStmt(S);

}

void ASTStmtReader::VisitSwitchCmd(SwitchCmd *S) {
  VisitStmt(S);

}

void ASTStmtReader::VisitWhileCmd(WhileCmd *S) {
  VisitStmt(S);

}

void ASTStmtReader::VisitForCmd(ForCmd *S) {
  VisitStmt(S);

}

void ASTStmtReader::VisitContinueCmd(ContinueCmd *S) {
  VisitStmt(S);
  S->setContinueLoc(ReadSourceLocation(Record, Idx));
}

void ASTStmtReader::VisitBreakCmd(BreakCmd *S) {
  VisitStmt(S);
  S->setBreakLoc(ReadSourceLocation(Record, Idx));
}

void ASTStmtReader::VisitReturnCmd(ReturnCmd *S) {
  VisitStmt(S);

}

void ASTStmtReader::VisitDefnCmd(DefnCmd *S) {
  VisitStmt(S);
  S->setStartLoc(ReadSourceLocation(Record, Idx));
  S->setEndLoc(ReadSourceLocation(Record, Idx));

  if (Idx + 1 == Record.size()) {
    // Single declaration
    S->setDefnGroup(DefnGroupRef(Reader.GetDefn(Record[Idx++])));
  } else {
    llvm::SmallVector<Defn *, 16> Defns;
    Defns.reserve(Record.size() - Idx);
    for (unsigned N = Record.size(); Idx != N; ++Idx)
      Defns.push_back(Reader.GetDefn(Record[Idx]));
    S->setDefnGroup(DefnGroupRef(DefnGroup::Create(*Reader.getContext(),
                                                   Defns.data(),
                                                   Defns.size())));
  }
}

void ASTStmtReader::VisitCatchCmd(CatchCmd *S) {
	VisitStmt(S);
}

void ASTStmtReader::VisitTryCmd(TryCmd *S) {
	VisitStmt(S);
}

void ASTStmtReader::VisitExpr(Expr *E) {
  VisitStmt(E);
  E->setType(Reader.GetType(Record[Idx++]));
  assert(Idx == NumExprFields && "Incorrect expression field count");
}

void ASTStmtReader::VisitDefnRefExpr(DefnRefExpr *E) {
  VisitExpr(E);

 }

void ASTStmtReader::VisitIntegerLiteral(IntegerLiteral *E) {
  VisitExpr(E);
  E->setLocation(ReadSourceLocation(Record, Idx));
  E->setValue(*Reader.getContext(), Reader.ReadAPInt(Record, Idx));
}

void ASTStmtReader::VisitFloatingLiteral(FloatingLiteral *E) {
  VisitExpr(E);
  E->setValue(*Reader.getContext(), Reader.ReadAPFloat(Record, Idx));
  E->setExact(Record[Idx++]);
  E->setLocation(ReadSourceLocation(Record, Idx));
}

void ASTStmtReader::VisitImaginaryLiteral(ImaginaryLiteral *E) {
  VisitExpr(E);
  E->setSubExpr(Reader.ReadSubExpr());
}

void ASTStmtReader::VisitStringLiteral(StringLiteral *E) {
  VisitExpr(E);
  unsigned Len = Record[Idx++];
  assert(Record[Idx] == E->getNumConcatenated() &&
         "Wrong number of concatenated tokens!");
  ++Idx;

  // Read string data
  llvm::SmallString<16> Str(&Record[Idx], &Record[Idx] + Len);
  E->setString(*Reader.getContext(), Str.str());
  Idx += Len;

  // Read source locations
  for (unsigned I = 0, N = E->getNumConcatenated(); I != N; ++I)
    E->setStrTokenLoc(I, ReadSourceLocation(Record, Idx));
}

void ASTStmtReader::VisitCharacterLiteral(CharacterLiteral *E) {
  VisitExpr(E);
  E->setValue(Record[Idx++]);
  E->setLocation(ReadSourceLocation(Record, Idx));
}

void ASTStmtReader::VisitParenExpr(ParenExpr *E) {
  VisitExpr(E);
  E->setLParen(ReadSourceLocation(Record, Idx));
  E->setRParen(ReadSourceLocation(Record, Idx));
  E->setSubExpr(Reader.ReadSubExpr());
}

void ASTStmtReader::VisitParenListExpr(ParenListExpr *E) {
  VisitExpr(E);

}

void ASTStmtReader::VisitUnaryOperator(UnaryOperator *E) {
  VisitExpr(E);
  E->setSubExpr(Reader.ReadSubExpr());
  E->setOpcode((UnaryOperator::Opcode)Record[Idx++]);
  E->setOperatorLoc(ReadSourceLocation(Record, Idx));
}

void ASTStmtReader::VisitArrayIndex(ArrayIndex *E) {
  VisitExpr(E);

}

void ASTStmtReader::VisitFunctionCall(FunctionCall *E) {
  VisitExpr(E);
  E->setNumArgs(*Reader.getContext(), Record[Idx++]);
  E->setRParenLoc(ReadSourceLocation(Record, Idx));
  E->setCallee(Reader.ReadSubExpr());
  for (unsigned I = 0, N = E->getNumArgs(); I != N; ++I)
    E->setArg(I, Reader.ReadSubExpr());
}

void ASTStmtReader::VisitMemberExpr(MemberExpr *E) {
  // Don't call VisitExpr, this is fully initialized at creation.
  assert(E->getStmtClass() == Stmt::MemberExprClass &&
         "It's a subclass, we must advance Idx!");
}

void ASTStmtReader::VisitBinaryOperator(BinaryOperator *E) {
  VisitExpr(E);
  E->setLHS(Reader.ReadSubExpr());
  E->setRHS(Reader.ReadSubExpr());
  E->setOpcode((BinaryOperator::Opcode)Record[Idx++]);
  E->setOperatorLoc(ReadSourceLocation(Record, Idx));
}

void ASTStmtReader::VisitCompoundAssignOperator(CompoundAssignOperator *E) {
  VisitBinaryOperator(E);
  E->setComputationLHSType(Reader.GetType(Record[Idx++]));
  E->setComputationResultType(Reader.GetType(Record[Idx++]));
}

void ASTStmtReader::VisitInitListExpr(InitListExpr *E) {
  VisitExpr(E);
  E->setSyntacticForm(cast_or_null<InitListExpr>(Reader.ReadSubStmt()));
  E->setLBraceLoc(ReadSourceLocation(Record, Idx));
  E->setRBraceLoc(ReadSourceLocation(Record, Idx));
}

Stmt *ASTReader::ReadStmt(PerFileData &F) {
  switch (ReadingKind) {
  case Read_Defn:
  case Read_Type:
    return ReadStmtFromStream(F);
  case Read_Stmt:
    return ReadSubStmt();
  }

  llvm_unreachable("ReadingKind not set ?");
  return 0;
}

Expr *ASTReader::ReadExpr(PerFileData &F) {
  return cast_or_null<Expr>(ReadStmt(F));
}

Expr *ASTReader::ReadSubExpr() {
  return cast_or_null<Expr>(ReadSubStmt());
}

// Within the bitstream, expressions are stored in Reverse Polish
// Notation, with each of the subexpressions preceding the
// expression they are stored in. Subexpressions are stored from last to first.
// To evaluate expressions, we continue reading expressions and placing them on
// the stack, with expressions having operands removing those operands from the
// stack. Evaluation terminates when we see a STMT_STOP record, and
// the single remaining expression on the stack is our result.
Stmt *ASTReader::ReadStmtFromStream(PerFileData &F) {

  ReadingKindTracker ReadingKind(Read_Stmt, *this);
  llvm::BitstreamCursor &Cursor = F.DefnsCursor;

#ifndef NDEBUG
  unsigned PrevNumStmts = StmtStack.size();
#endif

  RecordData Record;
  unsigned Idx;
  ASTStmtReader Reader(*this, F, Cursor, Record, Idx);
  Stmt::EmptyShell Empty;

  while (true) {
    unsigned Code = Cursor.ReadCode();
    if (Code == llvm::bitc::END_BLOCK) {
      if (Cursor.ReadBlockEnd()) {
        Error("error at end of block in AST file");
        return 0;
      }
      break;
    }

    if (Code == llvm::bitc::ENTER_SUBBLOCK) {
      // No known subblocks, always skip them.
      Cursor.ReadSubBlockID();
      if (Cursor.SkipBlock()) {
        Error("malformed block record in AST file");
        return 0;
      }
      continue;
    }

    if (Code == llvm::bitc::DEFINE_ABBREV) {
      Cursor.ReadAbbrevRecord();
      continue;
    }

    Stmt *S = 0;
    Idx = 0;
    Record.clear();
    bool Finished = false;
    switch ((StmtCode)Cursor.ReadRecord(Code, Record)) {
    case STMT_STOP:
      Finished = true;
      break;

    case STMT_NULL_PTR:
      S = 0;
      break;

    case STMT_NULL:
      S = new (Context) NullCmd(Empty);
      break;

    case STMT_COMPOUND:
      S = new (Context) BlockCmd(Empty);
      break;

    case STMT_CASE:
      S = new (Context) CaseCmd(Empty);
      break;

    case STMT_OTHERWISE:
      S = new (Context) OtherwiseCmd(Empty);
      break;

    case STMT_IF:
      S = new (Context) IfCmd(Empty);
      break;

    case STMT_SWITCH:
      S = new (Context) SwitchCmd(Empty);
      break;

    case STMT_WHILE:
      S = new (Context) WhileCmd(Empty);
      break;

    case STMT_FOR:
      S = new (Context) ForCmd(Empty);
      break;

    case STMT_CONTINUE:
      S = new (Context) ContinueCmd(Empty);
      break;

    case STMT_BREAK:
      S = new (Context) BreakCmd(Empty);
      break;

    case STMT_RETURN:
      S = new (Context) ReturnCmd(Empty);
      break;

    case STMT_DEFN:
      S = new (Context) DefnCmd(Empty);
      break;

    case EXPR_DEFN_REF:
      S = DefnRefExpr::CreateEmpty(
        *Context,
        /*HasQualifier=*/Record[ASTStmtReader::NumExprFields]);
      break;

    case EXPR_INTEGER_LITERAL:
      S = IntegerLiteral::Create(*Context, Empty);
      break;

    case EXPR_FLOATING_LITERAL:
      S = FloatingLiteral::Create(*Context, Empty);
      break;

    case EXPR_STRING_LITERAL:
      S = StringLiteral::CreateEmpty(*Context,
                                     Record[ASTStmtReader::NumExprFields + 1]);
      break;

    case EXPR_CHARACTER_LITERAL:
      S = new (Context) CharacterLiteral(Empty);
      break;

    case EXPR_PAREN:
      S = new (Context) ParenExpr(Empty);
      break;

    case EXPR_PAREN_LIST:
      S = new (Context) ParenListExpr(Empty);
      break;

    case EXPR_UNARY_OPERATOR:
      S = new (Context) UnaryOperator(Empty);
      break;

    case EXPR_ARRAY_SUBSCRIPT:
      S = new (Context) ArrayIndex(*Context, Stmt::ArrayIndexClass, Empty);
      break;

    case EXPR_CALL:
      S = new (Context) FunctionCall(*Context, Stmt::FunctionCallClass, Empty);
      break;

    case EXPR_MEMBER: {
      
      break;
    }

    case EXPR_BINARY_OPERATOR:
      S = new (Context) BinaryOperator(Empty);
      break;

    case EXPR_COMPOUND_ASSIGN_OPERATOR:
      S = new (Context) CompoundAssignOperator(Empty);
      break;

    case STMT_CXX_CATCH:
      S = new (Context) CatchCmd(Empty);
      break;

    case STMT_CXX_TRY:
      S = new (Context) TryCmd(Empty);
      break;

    }
    
    // We hit a STMT_STOP, so we're done with this expression.
    if (Finished)
      break;

    ++NumStatementsRead;

    if (S)
      Reader.Visit(S);

    assert(Idx == Record.size() && "Invalid deserialization of statement");
    StmtStack.push_back(S);
  }

#ifndef NDEBUG
  assert(StmtStack.size() > PrevNumStmts && "Read too many sub stmts!");
  assert(StmtStack.size() == PrevNumStmts + 1 && "Extra expressions on stack!");
#endif

  return StmtStack.pop_back_val();
}
