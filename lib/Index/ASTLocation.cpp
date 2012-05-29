//===--- ASTLocation.cpp - A <Decl, Stmt> pair ------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  ASTLocation is Decl or a Stmt and its immediate Decl parent.
//
//===----------------------------------------------------------------------===//

#include "mlang/Index/ASTLocation.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Stmt.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/Basic/SourceLocation.h"

using namespace mlang;
using namespace idx;

static Defn *getDefnFromExpr(Stmt *E) {
  if (DefnRefExpr *RefExpr = dyn_cast<DefnRefExpr>(E))
    return RefExpr->getDefn();
  if (MemberExpr *ME = dyn_cast<MemberExpr>(E))
    return ME->getMemberDefn();
  if (FunctionCall *CE = dyn_cast<FunctionCall>(E))
    return getDefnFromExpr(CE->getCallee());

  return 0;
}

Defn *ASTLocation::getReferencedDecl() {
  if (isInvalid())
    return 0;

  switch (getKind()) {
  default: assert(0 && "Invalid Kind");
  case N_Type:
    return 0;
  case N_Decl:
    return D;
  case N_NamedRef:
    return NDRef.ND;
  case N_Stmt:
    return getDefnFromExpr(Stm);
  }
  
  return 0;
}

SourceRange ASTLocation::getSourceRange() const {
  if (isInvalid())
    return SourceRange();

  switch (getKind()) {
  default: assert(0 && "Invalid Kind");
    return SourceRange();
  case N_Decl:
    return D->getSourceRange();
  case N_Stmt:
    return Stm->getSourceRange();
  case N_NamedRef:
    return SourceRange(AsNamedRef().Loc, AsNamedRef().Loc);
  case N_Type:
    break;
  }
  
  return SourceRange();
}

void ASTLocation::print(llvm::raw_ostream &OS) const {
  if (isInvalid()) {
    OS << "<< Invalid ASTLocation >>\n";
    return;
  }
  
  ASTContext &Ctx = getParentDecl()->getASTContext();

  switch (getKind()) {
  case N_Decl:
    OS << "[Decl: " << AsDecl()->getDefnKindName() << " ";
    if (const NamedDefn *ND = dyn_cast<NamedDefn>(AsDecl()))
      OS << ND;
    break;

  case N_Stmt:
    OS << "[Stmt: " << AsStmt()->getStmtClassName() << " ";
    AsStmt()->printPretty(OS, Ctx, 0, PrintingPolicy(Ctx.getLangOptions()));
    break;
    
  case N_NamedRef:
    OS << "[NamedRef: " << AsNamedRef().ND->getDefnKindName() << " ";
    OS << AsNamedRef().ND;
    break;
    
  case N_Type: {
    Type T = *AsType();
    OS << "[Type: " << T->getTypeClassName() << " " << T.getAsString();
  }
  }

  OS << "] <";

  SourceRange Range = getSourceRange();
  SourceManager &SourceMgr = Ctx.getSourceManager();
  Range.getBegin().print(OS, SourceMgr);
  OS << ", ";
  Range.getEnd().print(OS, SourceMgr);
  OS << ">\n";
}
