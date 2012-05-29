//===--- DefnAll.cpp - Definition AST Node Implementation--------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements Defn subclass except OOP related.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/Stmt.h"
#include "mlang/AST/CmdBlock.h"
#include "mlang/Basic/Builtins.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlang;

//===--------------------------------------------------------------------===//
// TranslationUnitDefn
//===--------------------------------------------------------------------===//
TranslationUnitDefn * TranslationUnitDefn::Create(ASTContext & C) {
	return new (C,8) TranslationUnitDefn(C);
}

//===--------------------------------------------------------------------===//
// NamedDefn
//===--------------------------------------------------------------------===//
std::string NamedDefn::getQualifiedNameAsString() const {
	return getQualifiedNameAsString(getASTContext().getLangOptions());
}

std::string NamedDefn::getQualifiedNameAsString(const PrintingPolicy &Policy) const {
  const DefnContext *Ctx = getDefnContext();

	if (Ctx->isFunctionOrMethod())
		return getNameAsString();

	typedef llvm::SmallVector<const DefnContext *, 8> ContextsTy;
	ContextsTy Contexts;

	// Collect contexts.
	while (Ctx && isa<NamedDefn> (Ctx)) {
		Contexts.push_back(Ctx);
		Ctx = Ctx->getParent();
	};

	std::string QualName;
	llvm::raw_string_ostream OS(QualName);

	for (ContextsTy::reverse_iterator I = Contexts.rbegin(), E =
			Contexts.rend(); I != E; ++I) {
		if (const NamespaceDefn *ND = dyn_cast<NamespaceDefn>(*I)) {
			OS << ND;
		} else if (const ScriptDefn *SD = dyn_cast<ScriptDefn>(*I)) {
			OS << SD;
			continue;
		} else if (const UserClassDefn *RD = dyn_cast<UserClassDefn>(*I)) {
			OS << RD;
		} else if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(*I)) {
			const FunctionProtoType *FT = 0;
			FT = dyn_cast<FunctionProtoType> (
					FD->getType()->getAs<FunctionType> ());

			OS << FD << '(';
			if (FT) {
				unsigned NumParams = FD->getNumParams();
				for (unsigned i = 0; i < NumParams; ++i) {
					if (i)
						OS << ", ";
					std::string Param;
					FD->getParamDefn(i)->getType().getAsStringInternal(Param,
							Policy);
					OS << Param;
				}

				if (FT->isVariadic()) {
					if (NumParams > 0)
						OS << ", ";
					OS << "...";
				}
			}
			OS << ')';
		} else {
			OS << cast<NamedDefn> (*I);
		}
		OS << ".";
	}

	if (getDefnName())
		OS << this;

	return OS.str();
}

bool NamedDefn::definitionReplaces(NamedDefn *OldD) const {
  return false;
}

bool NamedDefn::hasLinkage() const {
  return true;
}

bool NamedDefn::isClassInstanceMember() const {
  return false;
}

Linkage NamedDefn::getLinkage() const {
	return static_cast<Linkage>(0);
}

NamedDefn::LinkageInfo NamedDefn::getLinkageAndVisibility() const {
 return LinkageInfo();
}

NamedDefn *NamedDefn::getUnderlyingDefn() {
	NamedDefn *ND = this;

	return ND;
}

//===--------------------------------------------------------------------===//
// TypeDefn
//===--------------------------------------------------------------------===//
TypeDefn::member_iterator TypeDefn::member_begin() const {
	return member_iterator(defn_iterator(FirstDefn));
}

//===--------------------------------------------------------------------===//
// VarDefn
//===--------------------------------------------------------------------===//
const char * VarDefn::getStorageClassSpecifierString(StorageClass SC) {
  const char * p = "storage information string should be returned.\n";
  return p;
}

VarDefn * VarDefn::Create(ASTContext &C, DefnContext *DC,
		                     SourceLocation L, IdentifierInfo *Id,
		                     Type T, StorageClass S,
		                     StorageClass SCAsWritten) {
	DefinitionName _ID(Id);
	return new (C,8) VarDefn(Var, DC, L, _ID, T, S, SCAsWritten);
}

void VarDefn::setStorageClass(StorageClass SC) {
	assert(isLegalForVariable(SC));
	if (getStorageClass() != SC)
		ClearLinkageCache();
	SClass = SC;
}

void VarDefn::setInit(Expr *I) {
	if (EvaluatedStmt *Eval = Init.dyn_cast<EvaluatedStmt *>()) {
		Eval->~EvaluatedStmt();
		getASTContext().Deallocate(Eval);
	}

	Init = (Stmt *)I;
}

SourceLocation VarDefn::getInnerLocStart() const {
	// FIXME : is this correct?
	return getLocation();
}

SourceRange VarDefn::getSourceRange() const {
  if (getInit())
    return SourceRange(getInnerLocStart(), getInit()->getLocEnd());
  return SourceRange(getInnerLocStart(), getLocation());
}

const Expr *VarDefn::getAnyInitializer(const VarDefn *&D) const {
  return NULL;
}
//===--------------------------------------------------------------------===//
// MemberDefn
//===--------------------------------------------------------------------===//


//===--------------------------------------------------------------------===//
// ParamVarDefn
//===--------------------------------------------------------------------===//
ParamVarDefn *ParamVarDefn::Create(ASTContext &C, DefnContext *DC,
                              SourceLocation L,IdentifierInfo *Id,
                              Type T, StorageClass S,
                              StorageClass SCAsWritten, Expr *DefArg) {
	return new (C,8) ParamVarDefn(ParamVar, DC, L, Id, T,
			S, SCAsWritten, DefArg);
}

Expr * ParamVarDefn::getDefaultArg() {
  // FIXME : we don not support default arguments currently.
	return NULL;
}

//===--------------------------------------------------------------------===//
// FunctionDefn
//===--------------------------------------------------------------------===//
FunctionDefn *FunctionDefn::Create(ASTContext &C, DefnContext *DC,
                            const DefinitionNameInfo &NameInfo,
                            Type T, StorageClass S,
                            StorageClass SCAsWritten,
                            bool isInlineSpecified,
                            bool hasWrittenPrototype) {
	return new (C,8) FunctionDefn(Defn::Function, C, DC, NameInfo, T, S,
			SCAsWritten, isInlineSpecified, hasWrittenPrototype);
}

void FunctionDefn::setParams(ASTContext &C, ParamVarDefn **NewParamInfo,
		unsigned NumParams) {

}

unsigned FunctionDefn::getNumParams() const {
	const FunctionType *FT = getType()->getAs<FunctionType>();
	if (isa<FunctionNoProtoType>(FT))
		return 0;
	return cast<FunctionProtoType>(FT)->getNumArgs();
}

void FunctionDefn::setBody(BlockCmd *C) {
	Body = C;
	if (C)
		EndRangeLoc = C->getLocEnd();
}

bool FunctionDefn::isVariadic() const {
	if (const FunctionProtoType *FT = getType()->getAs<FunctionProtoType>())
		return FT->isVariadic();
	return false;
}

unsigned FunctionDefn::getBuiltinID() const {
	ASTContext &Context = getASTContext();
	if (!getIdentifier() || !getIdentifier()->getBuiltinID())
		return 0;

	unsigned BuiltinID = getIdentifier()->getBuiltinID();
	if (!Context.BuiltinInfo.isPredefinedLibFunction(BuiltinID))
		return BuiltinID;

	// If this is a static function, it's not a builtin.
	if (getStorageClass() == SC_Persistent)
		return 0;

	// Not a builtin
	return 0;
}

void FunctionDefn::setStorageClass(StorageClass SC) {
	assert(isLegalForFunction(SC));
	  if (getStorageClass() != SC)
	    ClearLinkageCache();

	  SClass = SC;
}

bool FunctionDefn::isInlined() const {
  if (IsInline)
	  return true;

  if (isa<ClassMethodDefn>(this)) {
	  if (isInlineSpecified())
		  return true;
  }

  return false;
}

//===--------------------------------------------------------------------===//
// TypeDefn
//===--------------------------------------------------------------------===//



//===--------------------------------------------------------------------===//
// StructDefn
//===--------------------------------------------------------------------===//


//===--------------------------------------------------------------------===//
// CellDefn
//===--------------------------------------------------------------------===//



//===--------------------------------------------------------------------===//
// ScriptDefn
//===--------------------------------------------------------------------===//
ScriptDefn *ScriptDefn::Create(ASTContext &C, DefnContext *DC) {
	return new (C,8) ScriptDefn(DC);
}

void ScriptDefn::setCaptures(ASTContext &Context,
                            const Capture *begin,
                            const Capture *end) {
  if (begin == end) {
    NumCaptures = 0;
    Captures = 0;
    return;
  }

  NumCaptures = end - begin;

  // Avoid new Capture[] because we don't want to provide a default
  // constructor.
  size_t allocationSize = NumCaptures * sizeof(Capture);
  void *buffer = Context.Allocate(allocationSize, /*alignment*/sizeof(void*));
  memcpy(buffer, begin, allocationSize);
  Captures = static_cast<Capture*>(buffer);
}

bool ScriptDefn::capturesVariable(const VarDefn *variable) const {
  for (capture_const_iterator
         i = capture_begin(), e = capture_end(); i != e; ++i)
    // Only auto vars can be captured, so no redeclaration worries.
    if (i->getVariable() == variable)
      return true;

  return false;
}

SourceRange ScriptDefn::getSourceRange() const {
  return SourceRange(getLocation(), Body? Body->getLocEnd() : getLocation());
}

//===--------------------------------------------------------------------===//
// NamespaceDefn
//===--------------------------------------------------------------------===//
