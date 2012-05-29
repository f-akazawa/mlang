//===--- Stmt.cpp - Statement AST Node Implementation ---------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements the Stmt class and statement subclasses.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/Stmt.h"
#include "mlang/AST/CmdAll.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/Type.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/ASTDiagnostic.h"
#include "mlang/AST/DefnSub.h"
#include "mlang/Basic/TargetInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstdio>
using namespace mlang;

static struct StmtClassNameTable {
  const char *Name;
  unsigned Counter;
  unsigned Size;
} StmtClassInfo[Stmt::lastStmtConstant+1];

static StmtClassNameTable &getStmtInfoTableEntry(Stmt::StmtClass E) {
  static bool Initialized = false;
  if (Initialized)
    return StmtClassInfo[E];

  // Intialize the table on the first use.
  Initialized = true;
#define ABSTRACT_STMT(STMT)
#define STMT(CLASS, PARENT) \
  StmtClassInfo[(unsigned)Stmt::CLASS##Class].Name = #CLASS;    \
  StmtClassInfo[(unsigned)Stmt::CLASS##Class].Size = sizeof(CLASS);
#include "mlang/AST/StmtNodes.inc"

  return StmtClassInfo[E];
}

const char *Stmt::getStmtClassName() const {
  return getStmtInfoTableEntry((StmtClass) StmtBits.sClass).Name;
}

void Stmt::PrintStats() {
  // Ensure the table is primed.
  getStmtInfoTableEntry(Stmt::NullCmdClass);

  unsigned sum = 0;
  fprintf(stderr, "*** Stmt/Expr Stats:\n");
  for (int i = 0; i != Stmt::lastStmtConstant+1; i++) {
    if (StmtClassInfo[i].Name == 0) continue;
    sum += StmtClassInfo[i].Counter;
  }
  fprintf(stderr, "  %d stmts/exprs total.\n", sum);
  sum = 0;
  for (int i = 0; i != Stmt::lastStmtConstant+1; i++) {
    if (StmtClassInfo[i].Name == 0) continue;
    if (StmtClassInfo[i].Counter == 0) continue;
    fprintf(stderr, "    %d %s, %d each (%d bytes)\n",
            StmtClassInfo[i].Counter, StmtClassInfo[i].Name,
            StmtClassInfo[i].Size,
            StmtClassInfo[i].Counter*StmtClassInfo[i].Size);
    sum += StmtClassInfo[i].Counter*StmtClassInfo[i].Size;
  }
  fprintf(stderr, "Total bytes = %d\n", sum);
}

void Stmt::addStmtClass(StmtClass s) {
  ++getStmtInfoTableEntry(s).Counter;
}

static bool StatSwitch = false;

bool Stmt::CollectingStats(bool Enable) {
  if (Enable) StatSwitch = true;
  return StatSwitch;
}

namespace {
  struct good {};
  struct bad {};

  // These silly little functions have to be static inline to suppress
  // unused warnings, and they have to be defined to suppress other
  // warnings.
  static inline good is_good(good) { return good(); }

  typedef Stmt::child_range children_t();
  template <class T> good implements_children(children_t T::*) {
    return good();
  }
  static inline bad implements_children(children_t Stmt::*) {
    return bad();
  }

  typedef SourceRange getSourceRange_t() const;
  template <class T> good implements_getSourceRange(getSourceRange_t T::*) {
    return good();
  }
  static inline bad implements_getSourceRange(getSourceRange_t Stmt::*) {
    return bad();
  }

#define ASSERT_IMPLEMENTS_children(type) \
  (void) sizeof(is_good(implements_children(&type::children)))
#define ASSERT_IMPLEMENTS_getSourceRange(type) \
  (void) sizeof(is_good(implements_getSourceRange(&type::getSourceRange)))
}

/// Check whether the various Stmt classes implement their member
/// functions.
static inline void check_implementations() {
#define ABSTRACT_STMT(type)
#define STMT(type, base) \
  ASSERT_IMPLEMENTS_children(type); \
  ASSERT_IMPLEMENTS_getSourceRange(type);
#include "mlang/AST/StmtNodes.inc"
}

Stmt::child_range Stmt::children() {
  switch (getStmtClass()) {
  case Stmt::NoStmtClass: llvm_unreachable("statement without class");
#define ABSTRACT_STMT(type)
#define STMT(type, base) \
  case Stmt::type##Class: \
    return static_cast<type*>(this)->children();
#include "mlang/AST/StmtNodes.inc"
  }
  llvm_unreachable("unknown statement kind!");
  return child_range();
}

SourceRange Stmt::getSourceRange() const {
  switch (getStmtClass()) {
  case Stmt::NoStmtClass: llvm_unreachable("statement without class");
#define ABSTRACT_STMT(type)
#define STMT(type, base) \
  case Stmt::type##Class: \
    return static_cast<const type*>(this)->getSourceRange();
#include "mlang/AST/StmtNodes.inc"
  }
  llvm_unreachable("unknown statement kind!");
  return SourceRange();
}

void BlockCmd::setStmts(ASTContext &C, Stmt **Stmts, unsigned NumStmts) {
  if (this->Body)
    C.Deallocate(Body);
  this->BlockCmdBits.NumStmts = NumStmts;

  Body = new (C) Stmt*[NumStmts];
  memcpy(Body, Stmts, sizeof(Stmt *) * NumStmts);
}

bool Stmt::hasImplicitControlFlow() const {
  switch (StmtBits.sClass) {
    default:
      return false;

    case FunctionCallClass:
    case DefnCmdClass:
      return true;

    case Stmt::BinaryOperatorClass: {
      const BinaryOperator* B = cast<BinaryOperator>(this);
      if (B->isLogicalOp() || B->getOpcode() == BO_Comma)
        return true;
      else
        return false;
    }
  }
}

Type CatchCmd::getCaughtType() const {
  if (ExceptionVar)
    return ExceptionVar->getType();
  return Type();
}

//===----------------------------------------------------------------------===//
// Constructors
//===----------------------------------------------------------------------===//
ThenCmd::ThenCmd(ASTContext &C, SourceLocation SL, Stmt **thenblock,
		unsigned numStmts) :
		Cmd(ThenCmdClass), StartLoc(SL), NumStmts(numStmts) {
	SubStmts = new (C) Stmt*[numStmts];
	for (unsigned i = 0; i != numStmts; ++i) {
		SubStmts[i] = thenblock[i];
	}
}

ElseCmd::ElseCmd(ASTContext &C, SourceLocation EL, Stmt **elseblock,
		unsigned numStmts) :
		Cmd(ElseCmdClass), ElseLoc(EL), NumStmts(numStmts) {
	SubStmts = new (C) Stmt*[numStmts];
	for (unsigned i = 0; i != numStmts; ++i) {
		SubStmts[i] = elseblock[i];
	}
}

ElseIfCmd::ElseIfCmd(ASTContext &C, SourceLocation EL, Expr *cond,	Stmt **then,
		unsigned numStmts) :
		Cmd(ElseIfCmdClass), ElseIfLoc(EL), NumStmts(numStmts) {
	SubStmts = new (C) Stmt*[numStmts + STMT_START];
	SubStmts[COND] = reinterpret_cast<Stmt*>(cond);
	for (unsigned i = 0; i != numStmts; ++i) {
		SubStmts[i+STMT_START] = then[i];
	}
}

IfCmd::IfCmd(ASTContext &C, SourceLocation IL, SourceLocation EndL,
		Expr *cond,	Stmt *then, Stmt** ElseIfClauses, unsigned nElseIfClauses,
		SourceLocation ElseLoc, Stmt *elsev)
  : Cmd(IfCmdClass), IfLoc(IL), EndLoc(EndL), ElseLoc(ElseLoc),
    NumElseIfClauses(nElseIfClauses)
{
	SubStmts = new (C) Stmt*[nElseIfClauses+ELSEIF_START];
	SubStmts[COND] = reinterpret_cast<Stmt*>(cond);
	SubStmts[THEN] = then;
	SubStmts[ELSE] = elsev;
	for (unsigned i = 0; i != nElseIfClauses; ++i) {
		SubStmts[i+ELSEIF_START] = ElseIfClauses[i];
	}
}

ForCmd::ForCmd(ASTContext &C, Expr *condVar, Expr *range,
               Stmt *Body, SourceLocation FL, SourceLocation EL)
  : Cmd(ForCmdClass), ForLoc(FL), EndLoc(EL)
{
  // setLoopVariable(C, condVar);
	SubStmts[LOOPVAR] = reinterpret_cast<Stmt*>(condVar);
  SubStmts[RANGE] = reinterpret_cast<Stmt*>(range);
  SubStmts[BODY] = Body;
}

VarDefn *ForCmd::getLoopVariable() const {
  if (!SubStmts[LOOPVAR])
    return 0;
  
  DefnCmd *DS = cast<DefnCmd>(SubStmts[LOOPVAR]);
  return cast<VarDefn>(DS->getSingleDefn());
}

void ForCmd::setLoopVariable(ASTContext &C, VarDefn *V) {
  if (!V) {
    SubStmts[LOOPVAR] = 0;
    return;
  }
  
  SubStmts[LOOPVAR] = new (C) DefnCmd(DefnGroupRef(V),
                                      V->getSourceRange().getBegin(),
                                      V->getSourceRange().getEnd());
}

CaseCmd::CaseCmd(ASTContext &C, SourceLocation caseLoc, Expr *pat,
		Stmt **subStmt, unsigned numStmts) :
		Cmd(CaseCmdClass), CaseLoc(caseLoc), NumStmts(numStmts) {
	SubStmts = new (C) Stmt*[numStmts + STMT_START];
	SubStmts[PATTERN] = reinterpret_cast<Stmt*> (pat);
	for (unsigned i = 0; i != numStmts; ++i) {
		SubStmts[i+STMT_START] = subStmt[i];
	}
}

OtherwiseCmd::OtherwiseCmd(ASTContext &C, SourceLocation DL, Stmt **substmts,
		unsigned numStmts) :
		Cmd(OtherwiseCmdClass), OtherwiseLoc(DL),	NumStmts(numStmts) {
	SubStmts = new (C) Stmt*[numStmts];
	for (unsigned i = 0; i != numStmts; ++i) {
		SubStmts[i] = substmts[i];
	}
}

SwitchCmd::SwitchCmd(ASTContext &C, SourceLocation SLoc, SourceLocation ELoc,
    Expr *Var, Expr *Cond, Stmt** Cases,	unsigned numCases,
    Stmt *OtherwiseCmd)
  : Cmd(SwitchCmdClass), SwitchLoc(SLoc), EndLoc(ELoc),
    NumCases(numCases), AllEnumCasesCovered(0)
{
	SubStmts = new (C) Stmt*[numCases+CASE_START];
	SubStmts[VAR] = reinterpret_cast<Stmt*>(Var);
	SubStmts[COND] = reinterpret_cast<Stmt*>(Cond);
	SubStmts[OTHERWISE] = OtherwiseCmd;
	for (unsigned i = 0; i != numCases; ++i) {
		SubStmts[i+CASE_START] = Cases[i];
	}
}

WhileCmd::WhileCmd(ASTContext &C, Expr *cond, Stmt *body,
                   SourceLocation WL, SourceLocation EL) :
                   Cmd(WhileCmdClass) {
	SubStmts[COND] = reinterpret_cast<Stmt*>(cond);
	SubStmts[BODY] = body;
  WhileLoc = WL;
  EndLoc = EL;
}
