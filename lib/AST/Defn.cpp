//===--- Defn.cpp - Definition AST Node Implementation  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements the Defn and DefnContext classes.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/Stmt.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
using namespace mlang;

//===----------------------------------------------------------------------===//
//  Statistics
//===----------------------------------------------------------------------===//

#define DEFN(DERIVED, BASE) static int n##DERIVED##s = 0;
#define ABSTRACT_DEFN(DEFN)
#include "mlang/AST/DefnNodes.inc"

static bool StatSwitch = false;

const char *Defn::getDefnKindName() const {
  switch (DefnKind) {
  default: assert(0 && "Definition not in DefnNodes.inc!");
#define DEFN(DERIVED, BASE) case DERIVED: return #DERIVED;
#define ABSTRACT_DEFN(DEFN)
#include "mlang/AST/DefnNodes.inc"
  }
}

void Defn::setInvalidDefn(bool Invalid) {
  InvalidDefn = Invalid;
  if (Invalid) {
    // Defensive maneuver for ill-formed code: we're likely not to make it to
    // a point where we set the access specifier, so default it to "public"
    // to avoid triggering asserts elsewhere in the front end.
    setAccess(AS_public);
  }
}

bool Defn::CollectingStats(bool Enable) {
  if (Enable) StatSwitch = true;
  return StatSwitch;
}

void Defn::PrintStats() {
  fprintf(stderr, "*** Defn Stats:\n");

  int totalDefns = 0;
#define DEFN(DERIVED, BASE) totalDefns += n##DERIVED##s;
#define ABSTRACT_DEFN(DEFN)
#include "mlang/AST/DefnNodes.inc"
  fprintf(stderr, "  %d defns total.\n", totalDefns);

  int totalBytes = 0;
#define DEFN(DERIVED, BASE)                                             \
  if (n##DERIVED##s > 0) {                                              \
    totalBytes += (int)(n##DERIVED##s * sizeof(DERIVED##Defn));         \
    fprintf(stderr, "    %d " #DERIVED " decls, %d each (%d bytes)\n",  \
            n##DERIVED##s, (int)sizeof(DERIVED##Defn),                  \
            (int)(n##DERIVED##s * sizeof(DERIVED##Defn)));              \
  }
#define ABSTRACT_DEFN(DEFN)
#include "mlang/AST/DefnNodes.inc"

  fprintf(stderr, "Total bytes = %d\n", totalBytes);
}

void Defn::add(Kind k) {
  switch (k) {
  default: assert(0 && "Definition not in DefnNodes.inc!");
#define DEFN(DERIVED, BASE) case DERIVED: ++n##DERIVED##s; break;
#define ABSTRACT_DEFN(DEFN)
#include "mlang/AST/DefnNodes.inc"
  }
}

//===----------------------------------------------------------------------===//
// PrettyStackTraceDefn Implementation
//===----------------------------------------------------------------------===//

void PrettyStackTraceDefn::print(llvm::raw_ostream &OS) const {
  SourceLocation TheLoc = Loc;
  if (TheLoc.isInvalid() && TheDefn)
    TheLoc = TheDefn->getLocation();

  if (TheLoc.isValid()) {
    TheLoc.print(OS, SM);
    OS << ": ";
  }

  OS << Message;

  if (const NamedDefn *DN = dyn_cast_or_null<NamedDefn>(TheDefn))
    OS << " '" << DN->getNameAsString() << '\'';
  OS << '\n';
}

//===----------------------------------------------------------------------===//
// Defn Implementation
//===----------------------------------------------------------------------===//

// Out-of-line virtual method providing a home for Defn.
Defn::~Defn() { }

void Defn::setDefnContext(DefnContext *DC) {
  DefnCtx = DC;
}

TranslationUnitDefn *Defn::getTranslationUnitDefn() {
  if (TranslationUnitDefn *TUD = dyn_cast<TranslationUnitDefn>(this))
    return TUD;

  DefnContext *DC = getDefnContext();
  assert(DC && "This defn is not contained in a translation unit!");

  while (!DC->isTranslationUnit()) {
    DC = DC->getParent();
    assert(DC && "This defn is not contained in a translation unit!");
  }

  return cast<TranslationUnitDefn>(DC);
}

ASTContext &Defn::getASTContext() const {
  return getTranslationUnitDefn()->getASTContext();
}

void Defn::dumpXML() const
{
}

void Defn::dumpXML(llvm::raw_ostream & OS) const
{
}

ASTMutationListener *Defn::getASTMutationListener() const {
  return getASTContext().getASTMutationListener();
}

unsigned Defn::getIdentifierNamespaceForKind(Kind DefnKind) {
  switch (DefnKind) {
	case Struct:
	case Cell:
	case UserClass:
		return IDNS_Type;
	case Function:
	case Var:
	case ParamVar:
	case ClassMethod:
	case ClassConstructor:
	case ClassDestructor:
	case ClassAttributes:
		return IDNS_Ordinary;
	case Member:
		return IDNS_Member;
	case Script:
		return IDNS_Script;
	default:
		return 0;
	}
}

Defn *Defn::castFromDefnContext (const DefnContext *D) {
  Defn::Kind DK = D->getDefnKind();
  switch(DK) {
#define DEFN(NAME, BASE)
#define DEFN_CONTEXT(NAME) \
    case Defn::NAME:       \
      return static_cast<NAME##Defn*>(const_cast<DefnContext*>(D));
#define DEFN_CONTEXT_BASE(NAME)
#include "mlang/AST/DefnNodes.inc"
    default:
#define DEFN(NAME, BASE)
#define DEFN_CONTEXT_BASE(NAME)                  \
      if (DK >= first##NAME && DK <= last##NAME) \
        return static_cast<NAME##Defn*>(const_cast<DefnContext*>(D));
#include "mlang/AST/DefnNodes.inc"
      assert(false && "a defn that inherits DefnContext isn't handled");
      return 0;
  }
}

DefnContext *Defn::castToDefnContext(const Defn *D) {
  Defn::Kind DK = D->getKind();
  switch(DK) {
#define DEFN(NAME, BASE)
#define DEFN_CONTEXT(NAME) \
    case Defn::NAME:       \
      return static_cast<NAME##Defn*>(const_cast<Defn*>(D));
#define DEFN_CONTEXT_BASE(NAME)
#include "mlang/AST/DefnNodes.inc"
    default:
#define DEFN(NAME, BASE)
#define DEFN_CONTEXT_BASE(NAME)                                   \
      if (DK >= first##NAME && DK <= last##NAME)                  \
        return static_cast<NAME##Defn*>(const_cast<Defn*>(D));
#include "mlang/AST/DefnNodes.inc"
      assert(false && "a defn that inherits DefnContext isn't handled");
      return 0;
  }
}

SourceLocation Defn::getBodyEnd() const {
  // Special handling of FunctionDefn to avoid de-serializing the body from PCH.
  // FunctionDefn stores EndRangeLoc for this purpose.
  if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(this)) {
	  return FD->getSourceRange().getEnd();
  }

  if (Stmt *Body = getBody())
    return Body->getSourceRange().getEnd();

  return SourceLocation();
}

void Defn::CheckAccessDefnContext() const {
#ifndef NDEBUG
  // Suppress this check if any of the following hold:
  // 1. this is the translation unit (and thus has no parent)
  // 2. this is a template parameter (and thus doesn't belong to its context)
  // 3. this is a non-type template parameter
  // 4. the context is not a record
  // 5. it's invalid
  // 6. it's a C++0x static_assert.
  if (isa<TranslationUnitDefn>(this) ||
      !isa<UserClassDefn>(getDefnContext()) ||
      isInvalidDefn() ||
      // FIXME: a ParmVarDecl can have ClassTemplateSpecialization
      // as DefnContext (?).
      isa<ParamVarDefn>(this) ||
      // FIXME: a ClassTemplateSpecialization or UserClassDefn can have
      // AS_none as access specifier.
      isa<UserObjectDefn>(this))
    return;

  assert(Access != AS_none &&
         "Access specifier is AS_none inside a record decl");
#endif
}
