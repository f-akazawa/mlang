//===--- DefinitionName.cpp - Declaration names implementation --*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements the DefinitionName and DefinitionNameTable
// classes.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/DefinitionName.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/Type.h"
#include "mlang/AST/TypeOrdering.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlang;

// Construct a definition name from the name of a user class constructor,
// destructor.
DefinitionName::DefinitionName(ClassSpecialName *Name)
  : Ptr(reinterpret_cast<uintptr_t>(Name)) {
	assert((Ptr & NK_Mask) == 0 && "Improperly aligned ClassSpecialName");
	Ptr |= Name->getKind();
}

std::string DefinitionName::getAsString() const {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  printName(OS);
  return OS.str();
}

void DefinitionName::printName(llvm::raw_ostream &OS) const {
  switch (getNameKind()) {
  case NK_Identifier:
    if (const IdentifierInfo *II = getAsIdentifierInfo())
      OS << II->getName();
    return;

  case NK_ConstructorName: {
    Type ClassType = getClassNameType();
    if (const ClassdefType *ClassRec = ClassType->getAs<ClassdefType>())
      OS << ClassRec->getDefn();
    else
      OS << ClassType.getAsString();
    return;
  }

  case NK_DestructorName: {
    OS << '~';
    Type Ty = getClassNameType();
    if (const ClassdefType *Rec = Ty->getAs<ClassdefType>())
      OS << Rec->getDefn();
    else
      OS << Ty.getAsString();
    return;
  }
  }

  assert(false && "Unexpected definition name kind");
}

int DefinitionName::compare(DefinitionName LHS, DefinitionName RHS) {
  if (LHS.getNameKind() != RHS.getNameKind())
    return (LHS.getNameKind() < RHS.getNameKind() ? -1 : 1);

  switch (LHS.getNameKind()) {
  case DefinitionName::NK_Identifier: {
    IdentifierInfo *LII = LHS.getAsIdentifierInfo();
    IdentifierInfo *RII = RHS.getAsIdentifierInfo();
    if (!LII) return RII ? -1 : 0;
    if (!RII) return 1;

    return LII->getName().compare(RII->getName());
  }

  case DefinitionName::NK_ConstructorName:
  case DefinitionName::NK_DestructorName:
    if (TypeOrdering()(LHS.getClassNameType(), RHS.getClassNameType()))
      return -1;
    if (TypeOrdering()(RHS.getClassNameType(), LHS.getClassNameType()))
      return 1;
  default:
    return 0;
  }
}

Type DefinitionName::getClassNameType() const {
  if (ClassSpecialName *ClassName = getAsClassSpecialName())
    return ClassName->getType();
  else
    return Type();
}

void *DefinitionName::getFETokenInfoAsVoid() const {
  switch (getNameKind()) {
  case NK_Identifier:
    return getAsIdentifierInfo()->getFETokenInfo<void>();

  case NK_ConstructorName:
  case NK_DestructorName:
    return getAsClassSpecialName()->getFETokenInfo();

  default:
    assert(false && "Declaration name has no FETokenInfo");
  }
  return 0;
}

void DefinitionName::setFETokenInfo(void *T) {
  switch (getNameKind()) {
  case NK_Identifier:
    getAsIdentifierInfo()->setFETokenInfo(T);
    break;

  case NK_ConstructorName:
  case NK_DestructorName:
    getAsClassSpecialName()->setFETokenInfo(T);
    break;

  default:
    assert(false && "Declaration name has no FETokenInfo");
  }
}

void DefinitionName::dump() const {
  printName(llvm::errs());
  llvm::errs() << '\n';
}

DefinitionNameTable::DefinitionNameTable(ASTContext &C) : Ctx(C) {
  ClassSpecialNamesImpl = new llvm::FoldingSet<ClassSpecialName>;
}

DefinitionNameTable::~DefinitionNameTable() {
  llvm::FoldingSet<ClassSpecialName> *SpecialNames =
    static_cast<llvm::FoldingSet<ClassSpecialName>*>(ClassSpecialNamesImpl);

  delete SpecialNames;
}

DefinitionName
DefinitionNameTable::getClassSpecialName(DefinitionName::NameKind Kind,
                                         Type Ty) {
  assert((Kind == DefinitionName::NK_ConstructorName ||
		      Kind == DefinitionName::NK_DestructorName) &&
         "Kind must be a class special name kind");
  llvm::FoldingSet<ClassSpecialName> *SpecialNames
    = static_cast<llvm::FoldingSet<ClassSpecialName>*>(ClassSpecialNamesImpl);

  // Unique selector, to guarantee there is one per name.
  llvm::FoldingSetNodeID ID;
  ID.AddInteger(Kind);
  ID.AddPointer(Ty.getAsOpaquePtr());

  void *InsertPos = 0;
  if (ClassSpecialName *Name = SpecialNames->FindNodeOrInsertPos(ID, InsertPos))
    return DefinitionName(Name);

  ClassSpecialName *SpecialName = new (Ctx,4) ClassSpecialName;
  SpecialName->setType(Ty);
  SpecialName->setFETokenInfo(0);
  SpecialNames->InsertNode(SpecialName, InsertPos);
  return DefinitionName(SpecialName);
}

unsigned
llvm::DenseMapInfo<mlang::DefinitionName>::getHashValue(mlang::DefinitionName N) {
  return DenseMapInfo<void*>::getHashValue(N.getAsOpaquePtr());
}

std::string DefinitionNameInfo::getAsString() const {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  printName(OS);
  return OS.str();
}

void DefinitionNameInfo::printName(llvm::raw_ostream &OS) const {
  switch (Name.getNameKind()) {
  case DefinitionName::NK_Identifier:
    Name.printName(OS);
    return;

  case DefinitionName::NK_ConstructorName:
  case DefinitionName::NK_DestructorName:
    if(Name.getNameKind() == DefinitionName::NK_DestructorName)
      OS << '~';
    else
      Name.printName(OS);
    return;
  }
  assert(false && "Unexpected declaration name kind");
}

DefinitionName
DefinitionNameTable::getOperatorName(OverloadedOperatorKind Op) {
  return DefinitionName();
}

DefinitionName
DefinitionNameTable::getLiteralOperatorName(IdentifierInfo *II) {
  return DefinitionName();
}

// TODO yabin
SourceLocation DefinitionNameInfo::getEndLoc() const {
  return NameLoc;
}
