//===--- NestedNameSpecifier.cpp - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the NestedNameSpecifier class, which represents
//  a nested-name-specifier.
//
//===----------------------------------------------------------------------===//
#include "mlang/AST/NestedNameSpecifier.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/PrettyPrinter.h"
#include "mlang/AST/Type.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>

using namespace mlang;

NestedNameSpecifier *
NestedNameSpecifier::FindOrInsert(ASTContext &Context,
                                  const NestedNameSpecifier &Mockup) {
  llvm::FoldingSetNodeID ID;
  Mockup.Profile(ID);

  void *InsertPos = 0;
  NestedNameSpecifier *NNS
    = Context.NestedNameSpecifiers.FindNodeOrInsertPos(ID, InsertPos);
  if (!NNS) {
    NNS = new (Context, 4) NestedNameSpecifier(Mockup);
    Context.NestedNameSpecifiers.InsertNode(NNS, InsertPos);
  }

  return NNS;
}

NestedNameSpecifier *
NestedNameSpecifier::Create(ASTContext &Context, NestedNameSpecifier *Prefix,
                            IdentifierInfo *II) {
  assert(II && "Identifier cannot be NULL");
  assert((!Prefix) && "Prefix must be dependent");

  NestedNameSpecifier Mockup;
  Mockup.Prefix.setPointer(Prefix);
  Mockup.Prefix.setInt(Identifier);
  Mockup.Specifier = II;
  return FindOrInsert(Context, Mockup);
}

NestedNameSpecifier *
NestedNameSpecifier::Create(ASTContext &Context, NestedNameSpecifier *Prefix,
                            NamespaceDefn *NS) {
  assert(NS && "Namespace cannot be NULL");
  assert((!Prefix ||
          (Prefix->getAsType() == 0 && Prefix->getAsIdentifier() == 0)) &&
         "Broken nested name specifier");
  NestedNameSpecifier Mockup;
  Mockup.Prefix.setPointer(Prefix);
  Mockup.Prefix.setInt(Namespace);
  Mockup.Specifier = NS;
  return FindOrInsert(Context, Mockup);
}

NestedNameSpecifier *
NestedNameSpecifier::Create(ASTContext &Context, NestedNameSpecifier *Prefix,
                            RawType *T) {
  assert(T && "Type cannot be NULL");
  NestedNameSpecifier Mockup;
  Mockup.Prefix.setPointer(Prefix);
  Mockup.Prefix.setInt(TypeSpec);
  Mockup.Specifier = T;
  return FindOrInsert(Context, Mockup);
}

NestedNameSpecifier *
NestedNameSpecifier::Create(ASTContext &Context, IdentifierInfo *II) {
  assert(II && "Identifier cannot be NULL");
  NestedNameSpecifier Mockup;
  Mockup.Prefix.setPointer(0);
  Mockup.Prefix.setInt(Identifier);
  Mockup.Specifier = II;
  return FindOrInsert(Context, Mockup);
}

/// \brief Print this nested name specifier to the given output
/// stream.
void
NestedNameSpecifier::print(llvm::raw_ostream &OS,
                           const PrintingPolicy &Policy) const {
  if (getPrefix())
    getPrefix()->print(OS, Policy);

  switch (getKind()) {
  case Identifier:
    OS << getAsIdentifier()->getName();
    break;

  case Namespace:
    OS << getAsNamespace()->getIdentifier()->getName();
    break;

  case Global:
    break;

  case TypeSpec: {
    std::string TypeStr;
    RawType *T = getAsType();

    PrintingPolicy InnerPolicy(Policy);
    InnerPolicy.SuppressScope = true;

    // Nested-name-specifiers are intended to contain minimally-qualified
    // types. An actual ElaboratedType will not occur, since we'll store
    // just the type that is referred to in the nested-name-specifier (e.g.,
    // a TypedefType, TagType, etc.). However, when we are dealing with
    // dependent template-id types (e.g., Outer<T>::template Inner<U>),
    // the type requires its own nested-name-specifier for uniqueness, so we
    // suppress that nested-name-specifier during printing.
//    assert(!isa<ElaboratedType>(T) &&
//           "Elaborated type in nested-name-specifier");
    // Print the type normally
    TypeStr = Type(T, 0).getAsString(InnerPolicy);

    OS << TypeStr;
    break;
  }
  }

  OS << ".";
}

void NestedNameSpecifier::dump(const LangOptions &LO) {
  print(llvm::errs(), PrintingPolicy(LO));
}
