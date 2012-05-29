//===--- SemaDefnOOP.cpp - Semantical Operation for OOP ---------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements Semantical Operation for OOP Definition
//  in Sema class.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
#include "mlang/AST/DefnOOP.h"

using namespace mlang;
using namespace sema;

Defn * Sema::ActOnStartLinkageSpecification(Scope *S, SourceLocation ExternLoc,
		SourceLocation LangLoc, llvm::StringRef Lang, SourceLocation LBraceLoc) {
  return NULL; //FIXME yabin
}

Defn * Sema::ActOnFinishLinkageSpecification(Scope *S, Defn *LinkageSpec,
		SourceLocation RBraceLoc) {
	return NULL; //FIXME yabin
}

/// MarkBaseAndMemberDestructorsReferenced - Given a record decl,
/// mark all the non-trivial destructors of its members and bases as
/// referenced.
void Sema::MarkBaseAndMemberDestructorsReferenced(SourceLocation Loc,
		UserClassDefn *Record) {

}

bool Sema::RequireNonAbstractType(SourceLocation Loc, Type T,
		const PartialDiagnostic &PD) {
	return false; //FIXME yabin
}

bool Sema::RequireNonAbstractType(SourceLocation Loc, Type T, unsigned DiagID,
		AbstractDiagSelID SelID) {
	return false; //FIXME yabin
}

DefnResult
Sema::ActOnExplicitInstantiation(Scope *S, SourceLocation ExternLoc,
		SourceLocation TemplateLoc, unsigned TagSpec, SourceLocation KWLoc/*,
		AttributeList *Attr*/) {
	return DefnResult(true); //FIXME yabin
}

DefnResult
Sema::ActOnExplicitInstantiation(Scope *S, SourceLocation ExternLoc,
		unsigned TagSpec, SourceLocation KWLoc, IdentifierInfo *Name,
		SourceLocation NameLoc/*, AttributeList *Attr*/) {
	return DefnResult(true); //FIXME yabin
}

DefnResult Sema::ActOnExplicitInstantiation(Scope *S, SourceLocation ExternLoc) {
	return DefnResult(true); //FIXME yabin
}

ExprResult Sema::RebuildExprInCurrentInstantiation(Expr *E) {
	return ExprError(); //FIXME yabin
}

void Sema::PrintInstantiationStack() {

}

void Sema::PerformPendingInstantiations(bool LocalOnly) {

}

// TODO yabin TypeSourceInfo * Sema::SubstType(SourceLocation Loc, DefinitionName Entity);

Type Sema::SubstType(Type T, SourceLocation Loc, DefinitionName Entity) {
  return Type();
}

// TODO yabin TypeSourceInfo * Sema::SubstFunctionDefnType(SourceLocation Loc,
//                                      DefinitionName Entity) {}

ParamVarDefn * Sema::SubstParamVarDefn(ParamVarDefn *D){
  return NULL;
}

ExprResult Sema::SubstExpr(Expr *E) {
	return ExprError();
}

StmtResult Sema::SubstStmt(Stmt *S) {
	return StmtError();
}

Defn * Sema::SubstDefn(Defn *D, DefnContext *Owner) {
  return NULL; //FIXME yabin
}

bool Sema::SubstBaseSpecifiers(UserClassDefn *Instantiation, UserClassDefn *Pattern) {
  return false; //FIXME yabin
}

bool Sema::InstantiateClass(SourceLocation PointOfInstantiation,
		UserClassDefn *Instantiation, UserClassDefn *Pattern, bool Complain) {
  return false; //FIXME yabin
}

void Sema::InstantiateAttrs(Defn *Pattern, Defn *Inst) {

}

void Sema::InstantiateClassMembers(SourceLocation PointOfInstantiation,
		UserClassDefn *Instantiation) {

}

void Sema::InstantiateFunctionDefinition(SourceLocation PointOfInstantiation,
		FunctionDefn *Function, bool Recursive,	bool DefinitionRequired) {

}

void Sema::InstantiateStaticDataMemberDefinition(SourceLocation PointOfInstantiation,
		VarDefn *Var, bool Recursive, bool DefinitionRequired) {

}

Defn * Sema::ActOnStartClassInterface(SourceLocation AtInterfaceLoc,
		IdentifierInfo *ClassName, SourceLocation ClassLoc,
		IdentifierInfo *SuperName, SourceLocation SuperLoc,
		Defn * const *ProtoRefs, unsigned NumProtoRefs,
		const SourceLocation *ProtoLocs, SourceLocation EndProtoLoc/*,
		AttributeList *AttrList*/) {
  return NULL;
}

Defn * Sema::ActOnCompatiblityAlias(SourceLocation AtCompatibilityAliasLoc,
		IdentifierInfo *AliasName, SourceLocation AliasLocation,
		IdentifierInfo *ClassName, SourceLocation ClassLocation) {
	return NULL;
}

Defn * Sema::ActOnStartProtocolInterface(SourceLocation AtProtoInterfaceLoc,
		IdentifierInfo *ProtocolName, SourceLocation ProtocolLoc,
		Defn * const *ProtoRefNames, unsigned NumProtoRefs,
		const SourceLocation *ProtoLocs, SourceLocation EndProtoLoc/*,
		AttributeList *AttrList*/) {
	return NULL;
}

Defn * Sema::ActOnStartCategoryInterface(SourceLocation AtInterfaceLoc,
		IdentifierInfo *ClassName, SourceLocation ClassLoc,
		IdentifierInfo *CategoryName, SourceLocation CategoryLoc,
		Defn * const *ProtoRefs, unsigned NumProtoRefs,
		const SourceLocation *ProtoLocs, SourceLocation EndProtoLoc) {
	return NULL;
}

Defn * Sema::ActOnStartClassImplementation(SourceLocation AtClassImplLoc,
		IdentifierInfo *ClassName, SourceLocation ClassLoc,
		IdentifierInfo *SuperClassname, SourceLocation SuperClassLoc) {
	return NULL;
}

Defn * Sema::ActOnStartCategoryImplementation(SourceLocation AtCatImplLoc,
		IdentifierInfo *ClassName, SourceLocation ClassLoc,
		IdentifierInfo *CatName, SourceLocation CatLoc) {
	return NULL;
}

Defn * Sema::ActOnForwardClassDeclaration(SourceLocation Loc,
				IdentifierInfo **IdentList, SourceLocation *IdentLocs,
				unsigned NumElts) {
	return NULL;
}

Defn * Sema::ActOnForwardProtocolDeclaration(SourceLocation AtProtoclLoc,
		const IdentifierLocPair *IdentList, unsigned NumElts/*,
		AttributeList *attrList*/) {
	return NULL;
}

void Sema::FindProtocolDeclaration(bool WarnOnDefinitions,
		const IdentifierLocPair *ProtocolId, unsigned NumProtocols,
		llvm::SmallVectorImpl<Defn *> &Protocols) {

}

void Sema::CompareProperties(Defn *CDefn, Defn *MergeProtocols) {

}

//TODO yabin  void Sema::ActOnAtEnd(Scope *S, SourceRange AtEnd, Defn *classDefn,
//                  Defn **allMethods = 0, unsigned allNum = 0,
//                  Defn **allProperties = 0, unsigned pNum = 0,
//                  DefnGroupPtrTy *allTUVars = 0, unsigned tuvNum = 0);

Defn * Sema::ActOnProperty(Scope *S, SourceLocation AtLoc, Defn *ClassCategory,
		DefnContext *lexicalDC) {
	return NULL;
}

Defn * Sema::ActOnPropertyImplDefn(Scope *S, SourceLocation AtLoc,
		SourceLocation PropertyLoc, bool ImplKind, Defn *ClassImplDefn,
		IdentifierInfo *PropertyId, IdentifierInfo *PropertyIvar) {
	return NULL;
}

void Sema::DiagnoseReturnInConstructorExceptionHandler(TryCmd *TryBlock) {

}
