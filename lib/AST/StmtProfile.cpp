//===---- StmtProfile.cpp - Profile implementation for Stmt ASTs ----------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements the Stmt::Profile method, which builds a unique bit
// representation that identifies a statement/expression.
//
//===----------------------------------------------------------------------===//
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/Defn.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/StmtVisitor.h"
#include "llvm/ADT/FoldingSet.h"
using namespace mlang;

namespace {
  class StmtProfiler : public StmtVisitor<StmtProfiler> {
    llvm::FoldingSetNodeID &ID;
    ASTContext &Context;
    bool Canonical;

  public:
    StmtProfiler(llvm::FoldingSetNodeID &ID, ASTContext &Context,
                 bool Canonical)
      : ID(ID), Context(Context), Canonical(Canonical) { }

    void VisitStmt(Stmt *S);

#define STMT(Node, Base) void Visit##Node(Node *S);
#include "mlang/AST/StmtNodes.inc"

    /// \brief Visit a definition that is referenced within an expression
    /// or statement.
    void VisitDefn(Defn *D);

    /// \brief Visit a type that is referenced within an expression or
    /// statement.
    void VisitType(Type T);

    /// \brief Visit a name that occurs within an expression or statement.
    void VisitName(DefinitionName Name);

    /// \brief Visit a nested-name-specifier that occurs within an expression
    /// or statement.
    void VisitNestedNameSpecifier(NestedNameSpecifier *NNS);

   };
}

void StmtProfiler::VisitStmt(Stmt *S) {
  ID.AddInteger(S->getStmtClass());
  for (Stmt::child_iterator C = S->child_begin(), CEnd = S->child_end();
       C != CEnd; ++C)
    Visit(*C);
}

void StmtProfiler::VisitDefnCmd(DefnCmd *S) {
  VisitStmt(S);
  for (DefnCmd::defn_iterator D = S->defn_begin(), DEnd = S->defn_end();
       D != DEnd; ++D)
    VisitDefn(*D);
}

void StmtProfiler::VisitBlockCmd(BlockCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitCaseCmd(CaseCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitOtherwiseCmd(OtherwiseCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitIfCmd(IfCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitSwitchCmd(SwitchCmd *S) {
  VisitStmt(S);
//  VisitDefn(S->getConditionVariable());
}

void StmtProfiler::VisitWhileCmd(WhileCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitForCmd(ForCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitContinueCmd(ContinueCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitBreakCmd(BreakCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitReturnCmd(ReturnCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitCatchCmd(CatchCmd *S) {
  VisitStmt(S);
  VisitType(S->getCaughtType());
}

void StmtProfiler::VisitTryCmd(TryCmd *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitExpr(Expr *S) {
  VisitStmt(S);
}

void StmtProfiler::VisitDefnRefExpr(DefnRefExpr *S) {
  VisitExpr(S);
  if (!Canonical)
    VisitNestedNameSpecifier(S->getQualifier());
  VisitDefn(S->getDefn());
}

void StmtProfiler::VisitIntegerLiteral(IntegerLiteral *S) {
  VisitExpr(S);
  S->getValue().Profile(ID);
}

void StmtProfiler::VisitCharacterLiteral(CharacterLiteral *S) {
  VisitExpr(S);
  ID.AddBoolean(false);
  ID.AddInteger(S->getValue());
}

void StmtProfiler::VisitFloatingLiteral(FloatingLiteral *S) {
  VisitExpr(S);
  S->getValue().Profile(ID);
  ID.AddBoolean(S->isExact());
}

void StmtProfiler::VisitImaginaryLiteral(ImaginaryLiteral *S) {
  VisitExpr(S);
}

void StmtProfiler::VisitStringLiteral(StringLiteral *S) {
  VisitExpr(S);
  ID.AddString(S->getString());
  ID.AddBoolean(false);
}

void StmtProfiler::VisitParenExpr(ParenExpr *S) {
  VisitExpr(S);
}

void StmtProfiler::VisitParenListExpr(ParenListExpr *S) {
  VisitExpr(S);
}

void StmtProfiler::VisitUnaryOperator(UnaryOperator *S) {
  VisitExpr(S);
  ID.AddInteger(S->getOpcode());
}

void StmtProfiler::VisitArrayIndex(ArrayIndex *S) {
  VisitExpr(S);
}

void StmtProfiler::VisitFunctionCall(FunctionCall *S) {
  VisitExpr(S);
}

void StmtProfiler::VisitMemberExpr(MemberExpr *S) {
  VisitExpr(S);
  VisitDefn(S->getMemberDefn());
  if (!Canonical)
    VisitNestedNameSpecifier(S->getQualifier());
  ID.AddBoolean(false);
}

void StmtProfiler::VisitBinaryOperator(BinaryOperator *S) {
  VisitExpr(S);
  ID.AddInteger(S->getOpcode());
}

void StmtProfiler::VisitCompoundAssignOperator(CompoundAssignOperator *S) {
  VisitBinaryOperator(S);
}

void StmtProfiler::VisitDefn(Defn *D) {
  ID.AddInteger(D? D->getKind() : 0);

  if (Canonical && D) {
    if (ParamVarDefn *Parm = dyn_cast<ParamVarDefn>(D)) {
      // The Itanium C++ ABI uses the type of a parameter when mangling
      // expressions that involve function parameters, so we will use the
      // parameter's type for establishing function parameter identity. That
      // way, our definition of "equivalent" (per C++ [temp.over.link])
      // matches the definition of "equivalent" used for name mangling.
      VisitType(Parm->getType());
      return;
    }
  }

  ID.AddPointer(D);
}

void StmtProfiler::VisitType(Type T) {
//  if (Canonical)
//    T = Context.getCanonicalType(T);

  ID.AddPointer(T.getAsOpaquePtr());
}

void StmtProfiler::VisitName(DefinitionName Name) {
  ID.AddPointer(Name.getAsOpaquePtr());
}

void StmtProfiler::VisitNestedNameSpecifier(NestedNameSpecifier *NNS) {
//  if (Canonical)
//    NNS = Context.getCanonicalNestedNameSpecifier(NNS);
  ID.AddPointer(NNS);
}

void Stmt::Profile(llvm::FoldingSetNodeID &ID, ASTContext &Context) {
  StmtProfiler Profiler(ID, Context, false);
  Profiler.Visit(this);
}
