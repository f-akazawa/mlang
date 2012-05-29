//===--- SemaDefn.cpp - Semantic Analysis for Declarations ----------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for declarations.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
//#include "mlang/Sema/Initialization.h"
#include "mlang/Sema/Lookup.h"
//#include "mlang/Sema/ClassFieldCollector.h"
#include "mlang/Sema/Scope.h"
#include "mlang/Sema/ScopeInfo.h"
#include "mlang/Sema/SemaDiagnostic.h"
#include "mlang/AST/APValue.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/AST/ASTContext.h"
//#include "mlang/AST/CXXInheritance.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/CmdAll.h"
#include "mlang/AST/CharUnits.h"
//#include "clang/Sema/DeclSpec.h"
#include "mlang/Parse/ParseDiagnostic.h"
#include "mlang/Diag/PartialDiagnostic.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/TargetInfo.h"
// FIXME: layering (ideally, Sema shouldn't be dependent on Lex API's)
#include "mlang/Lex/Preprocessor.h"
#include "llvm/ADT/Triple.h"
#include <algorithm>
#include <cstring>
#include <functional>
using namespace mlang;
using namespace sema;

Sema::DefnGroupPtrTy Sema::ConvertDefnToDefnGroup(Defn *Ptr) {
  return DefnGroupPtrTy::make(DefnGroupRef(Ptr));
}

/// \brief If the identifier refers to a type name within this scope,
/// return the declaration of that type.
///
/// This routine performs ordinary name lookup of the identifier II
/// within the given scope, with optional OOP scope specifier SS, to
/// determine whether the name refers to a type. If so, returns an
/// opaque pointer (actually a Type) corresponding to that
/// type. Otherwise, returns NULL.
///
/// If name lookup results in an ambiguity, this routine will complain
/// and then return NULL.
ParsedType Sema::getTypeName(IdentifierInfo &II, SourceLocation NameLoc,
                             Scope *S,
                             bool isClassName,
                             ParsedType ObjectTypePtr) {
#if 0
  // Determine where we will perform name lookup.
  DefnContext *LookupCtx = 0;
  if (ObjectTypePtr) {
    Type ObjectType = ObjectTypePtr.get();
    if (ObjectType->isRecordType())
      LookupCtx = computeDeclContext(ObjectType);
  } else if (SS && SS->isNotEmpty()) {
    LookupCtx = computeDefnContext(*SS, false);

    if (!LookupCtx) {
      if (isDependentScopeSpecifier(*SS)) {
        // C++ [temp.res]p3:
        //   A qualified-id that refers to a type and in which the
        //   nested-name-specifier depends on a template-parameter (14.6.2)
        //   shall be prefixed by the keyword typename to indicate that the
        //   qualified-id denotes a type, forming an
        //   elaborated-type-specifier (7.1.5.3).
        //
        // We therefore do not perform any name lookup if the result would
        // refer to a member of an unknown specialization.
        if (!isClassName)
          return ParsedType();
        
        // We know from the grammar that this name refers to a type,
        // so build a dependent node to describe the type.
        Type T =
          CheckTypenameType(ETK_None, SS->getScopeRep(), II,
                            SourceLocation(), SS->getRange(), NameLoc);
        return ParsedType::make(T);
      }
      
      return ParsedType();
    }
    
    if (!LookupCtx->isDependentContext() &&
        RequireCompleteDeclContext(*SS, LookupCtx))
      return ParsedType();
  }

  // FIXME: LookupNestedNameSpecifierName isn't the right kind of
  // lookup for class-names.
  LookupNameKind Kind = isClassName ? LookupNestedNameSpecifierName :
                                      LookupOrdinaryName;
  LookupResult Result(*this, &II, NameLoc, Kind);
  if (LookupCtx) {
    // Perform "qualified" name lookup into the declaration context we
    // computed, which is either the type of the base of a member access
    // expression or the declaration context associated with a prior
    // nested-name-specifier.
    LookupQualifiedName(Result, LookupCtx);

    if (ObjectTypePtr && Result.empty()) {
      // C++ [basic.lookup.classref]p3:
      //   If the unqualified-id is ~type-name, the type-name is looked up
      //   in the context of the entire postfix-expression. If the type T of 
      //   the object expression is of a class type C, the type-name is also
      //   looked up in the scope of class C. At least one of the lookups shall
      //   find a name that refers to (possibly cv-qualified) T.
      LookupName(Result, S);
    }
  } else {
    // Perform unqualified name lookup.
    LookupName(Result, S);
  }
  
  NamedDecl *IIDecl = 0;
  switch (Result.getResultKind()) {
  case LookupResult::NotFound:
  case LookupResult::NotFoundInCurrentInstantiation:
  case LookupResult::FoundOverloaded:
  case LookupResult::FoundUnresolvedValue:
    Result.suppressDiagnostics();
    return ParsedType();

  case LookupResult::Ambiguous:
    // Recover from type-hiding ambiguities by hiding the type.  We'll
    // do the lookup again when looking for an object, and we can
    // diagnose the error then.  If we don't do this, then the error
    // about hiding the type will be immediately followed by an error
    // that only makes sense if the identifier was treated like a type.
    if (Result.getAmbiguityKind() == LookupResult::AmbiguousTagHiding) {
      Result.suppressDiagnostics();
      return ParsedType();
    }

    // Look to see if we have a type anywhere in the list of results.
    for (LookupResult::iterator Res = Result.begin(), ResEnd = Result.end();
         Res != ResEnd; ++Res) {
      if (isa<TypeDecl>(*Res) || isa<ObjCInterfaceDecl>(*Res)) {
        if (!IIDecl ||
            (*Res)->getLocation().getRawEncoding() <
              IIDecl->getLocation().getRawEncoding())
          IIDecl = *Res;
      }
    }

    if (!IIDecl) {
      // None of the entities we found is a type, so there is no way
      // to even assume that the result is a type. In this case, don't
      // complain about the ambiguity. The parser will either try to
      // perform this lookup again (e.g., as an object name), which
      // will produce the ambiguity, or will complain that it expected
      // a type name.
      Result.suppressDiagnostics();
      return ParsedType();
    }

    // We found a type within the ambiguous lookup; diagnose the
    // ambiguity and then return that type. This might be the right
    // answer, or it might not be, but it suppresses any attempt to
    // perform the name lookup again.
    break;

  case LookupResult::Found:
    IIDecl = Result.getFoundDecl();
    break;
  }

  assert(IIDecl && "Didn't find decl");

  Type T;
  if (TypeDecl *TD = dyn_cast<TypeDecl>(IIDecl)) {
    DiagnoseUseOfDecl(IIDecl, NameLoc);

    if (T.isNull())
      T = Context.getTypeDeclType(TD);
    
    if (SS)
      T = getElaboratedType(ETK_None, *SS, T);
    
  } else if (ObjCInterfaceDecl *IDecl = dyn_cast<ObjCInterfaceDecl>(IIDecl)) {
    T = Context.getObjCInterfaceType(IDecl);
  } else {
    // If it's not plausibly a type, suppress diagnostics.
    Result.suppressDiagnostics();
    return ParsedType();
  }

  return ParsedType::make(T);
#endif
  return ParsedType();
}

/// isTagName() - This method is called *for error recovery purposes only*
/// to determine if the specified name is a valid tag name ("struct foo").  If
/// so, this returns the TST for the tag corresponding to it (TST_enum,
/// TST_union, TST_struct, TST_class).  This is used to diagnose cases in C
/// where the user forgot to specify the tag.
//DeclSpec::TST Sema::isTagName(IdentifierInfo &II, Scope *S) {
//  // Do a tag name lookup in this scope.
//  LookupResult R(*this, &II, SourceLocation(), LookupTypeName);
//  LookupName(R, S, false);
//  R.suppressDiagnostics();
//  if (R.getResultKind() == LookupResult::Found)
//    if (const TypeDefn *TD = R.getAsSingle<TypeDefn>()) {
//      switch (TD->getTagKind()) {
//      default:         return DeclSpec::TST_unspecified;
//      case TTK_Struct: return DeclSpec::TST_struct;
//      case TTK_Union:  return DeclSpec::TST_union;
//      case TTK_Class:  return DeclSpec::TST_class;
//      case TTK_Enum:   return DeclSpec::TST_enum;
//      }
//    }
//
//  return DeclSpec::TST_unspecified;
//}

bool Sema::DiagnoseUnknownTypeName(const IdentifierInfo &II, 
                                   SourceLocation IILoc,
                                   Scope *S,
                                   ParsedType &SuggestedType) {
#if 0
	// We don't have anything to suggest (yet).
  SuggestedType = ParsedType();
  
  // There may have been a typo in the name of the type. Look up typo
  // results, in case we have something that we can suggest.
  LookupResult Lookup(*this, &II, IILoc, LookupOrdinaryName, 
                      NotForRedefinition);

  if (DeclarationName Corrected = CorrectTypo(Lookup, S, SS, 0, 0, CTC_Type)) {
    if (NamedDecl *Result = Lookup.getAsSingle<NamedDecl>()) {
      if ((isa<TypeDecl>(Result) || isa<ObjCInterfaceDecl>(Result)) &&
          !Result->isInvalidDecl()) {
        // We found a similarly-named type or interface; suggest that.
        if (!SS || !SS->isSet())
          Diag(IILoc, diag::err_unknown_typename_suggest)
            << &II << Lookup.getLookupName()
            << FixItHint::CreateReplacement(SourceRange(IILoc),
                                            Result->getNameAsString());
        else if (DeclContext *DC = computeDeclContext(*SS, false))
          Diag(IILoc, diag::err_unknown_nested_typename_suggest) 
            << &II << DC << Lookup.getLookupName() << SS->getRange()
            << FixItHint::CreateReplacement(SourceRange(IILoc),
                                            Result->getNameAsString());
        else
          llvm_unreachable("could not have corrected a typo here");

        Diag(Result->getLocation(), diag::note_previous_decl)
          << Result->getDeclName();
        
        SuggestedType = getTypeName(*Result->getIdentifier(), IILoc, S, SS);
        return true;
      }
    } else if (Lookup.empty()) {
      // We corrected to a keyword.
      // FIXME: Actually recover with the keyword we suggest, and emit a fix-it.
      Diag(IILoc, diag::err_unknown_typename_suggest)
        << &II << Corrected;
      return true;      
    }
  }

  if (getLangOptions().CPlusPlus) {
    // See if II is a class template that the user forgot to pass arguments to.
    UnqualifiedId Name;
    Name.setIdentifier(&II, IILoc);
    CXXScopeSpec EmptySS;
    TemplateTy TemplateResult;
    bool MemberOfUnknownSpecialization;
    if (isTemplateName(S, SS ? *SS : EmptySS, /*hasTemplateKeyword=*/false,
                       Name, ParsedType(), true, TemplateResult,
                       MemberOfUnknownSpecialization) == TNK_Type_template) {
      TemplateName TplName = TemplateResult.getAsVal<TemplateName>();
      Diag(IILoc, diag::err_template_missing_args) << TplName;
      if (TemplateDecl *TplDecl = TplName.getAsTemplateDecl()) {
        Diag(TplDecl->getLocation(), diag::note_template_decl_here)
          << TplDecl->getTemplateParameters()->getSourceRange();
      }
      return true;
    }
  }

  // FIXME: Should we move the logic that tries to recover from a missing tag
  // (struct, union, enum) from Parser::ParseImplicitInt here, instead?
  
  if (!SS || (!SS->isSet() && !SS->isInvalid()))
    Diag(IILoc, diag::err_unknown_typename) << &II;
  else if (DeclContext *DC = computeDeclContext(*SS, false))
    Diag(IILoc, diag::err_typename_nested_not_found) 
      << &II << DC << SS->getRange();
  else if (isDependentScopeSpecifier(*SS)) {
    Diag(SS->getRange().getBegin(), diag::err_typename_missing)
      << (NestedNameSpecifier *)SS->getScopeRep() << II.getName()
      << SourceRange(SS->getRange().getBegin(), IILoc)
      << FixItHint::CreateInsertion(SS->getRange().getBegin(), "typename ");
    SuggestedType = ActOnTypenameType(S, SourceLocation(), *SS, II, IILoc).get();
  } else {
    assert(SS && SS->isInvalid() && 
           "Invalid scope specifier has already been diagnosed");
  }
#endif
  return true;
}

// Determines the context to return to after temporarily entering a
// context.  This depends in an unnecessarily complicated way on the
// exact ordering of callbacks from the parser.
DefnContext *Sema::getContainingDC(DefnContext *DC) {
	if (isa<FunctionDefn> (DC)) {
		DC = DC->getParent();

		// A function not defined within a class will always return to its
		// lexical context.
		if (!isa<UserClassDefn> (DC))
			return DC;

		// A method is parsed *after* the topmost class it was declared in
		//  is fully parsed ("complete");  the topmost class is the
		//  context we need to return to.
		while (UserClassDefn *UC = dyn_cast<UserClassDefn> (
				DC->getParent()))
			DC = UC;

		// Return the declaration context of the topmost class the inline method is
		// defined in.
		return DC;
	}

  return DC->getParent();
}

void Sema::PushDefnContext(Scope *S, DefnContext *DC) {
  assert(getContainingDC(DC) == CurContext &&
      "The next DefnContext should be lexically contained in the current one.");
  CurContext = DC;

  /// all the script defn resides in the current scope
  /// only the entry script will be set to be the entity of
  /// top scope.
  if(ScriptDefn *SD = dyn_cast<ScriptDefn>(DC))
  	  if(!SD->isEntryScript())
  		  return;

  S->setEntity(DC);
}

void Sema::PopDefnContext() {
  assert(CurContext && "DefnContext imbalance!");

  CurContext = getContainingDC(CurContext);
  assert(CurContext && "Popped translation unit!");
}

/// EnterDeclaratorContext - Used when we must lookup names in the context
/// of a declarator's nested name specifier.
///
void Sema::EnterDeclaratorContext(Scope *S, DefnContext *DC) {
  // C++0x [basic.lookup.unqual]p13:
  //   A name used in the definition of a static data member of class
  //   X (after the qualified-id of the static member) is looked up as
  //   if the name was used in a member function of X.
  // C++0x [basic.lookup.unqual]p14:
  //   If a variable member of a namespace is defined outside of the
  //   scope of its namespace then any name used in the definition of
  //   the variable member (after the declarator-id) is looked up as
  //   if the definition of the variable member occurred in its
  //   namespace.
  // Both of these imply that we should push a scope whose context
  // is the semantic context of the declaration.  We can't use
  // PushDefnContext here because that context is not necessarily
  // lexically contained in the current context.  Fortunately,
  // the containing scope should have the appropriate information.

  assert(!S->getEntity() && "scope already has entity");

#ifndef NDEBUG
  Scope *Ancestor = S->getParent();
  while (!Ancestor->getEntity()) Ancestor = Ancestor->getParent();
  assert(Ancestor->getEntity() == CurContext && "ancestor context mismatch");
#endif

  CurContext = DC;
  S->setEntity(DC);
}

Defn *Sema::ActOnStartOfScriptDef(Scope *S, Defn *D) {
	ScriptDefn *SD = ScriptDefn::Create(Context, CurContext);
	SD->setEntryScript(D == NULL);
	PushDefnContext(S, static_cast<DefnContext *>(SD));
	return SD;
}

Defn *Sema::ActOnFinishScriptBody(Defn *D, Stmt *stmt) {
  assert(isa<ScriptDefn>(D) && "not a script definition");
	if(isa<BlockCmd>(stmt)) {
		cast<ScriptDefn>(D)->setBody(cast<BlockCmd>(stmt));
	}
  getASTContext().getTranslationUnitDefn()->addDefn(D);
	return D;
}

Defn *Sema::ActOnStartOfFunctionDef(Scope *FnBodyScope, Defn *D) {
  /*// Clear the last template instantiation error context.
  LastTemplateInstantiationErrorContext = ActiveTemplateInstantiation();

  if (!D)
    return D;
  FunctionDefn *FD = 0;

  if (FunctionTemplateDefn *FunTmpl = dyn_cast<FunctionTemplateDefn>(D))
    FD = FunTmpl->getTemplatedDefn();
  else
    FD = cast<FunctionDefn>(D);

  // Enter a new function scope
  PushFunctionScope();

  // See if this is a redefinition.
  // But don't complain if we're in GNU89 mode and the previous definition
  // was an extern inline function.
  const FunctionDefn *Definition;
  if (FD->hasBody(Definition) &&
      !canRedefineFunction(Definition, getLangOptions())) {
    if (getLangOptions().GNUMode && Definition->isInlineSpecified() &&
        Definition->getStorageClass() == SC_Extern)
      Diag(FD->getLocation(), diag::err_redefinition_extern_inline)
        << FD->getDefnName() << getLangOptions().CPlusPlus;
    else
      Diag(FD->getLocation(), diag::err_redefinition) << FD->getDefnName();
    Diag(Definition->getLocation(), diag::note_previous_definition);
  }

  // Builtin functions cannot be defined.
  if (unsigned BuiltinID = FD->getBuiltinID()) {
    if (!Context.BuiltinInfo.isPredefinedLibFunction(BuiltinID)) {
      Diag(FD->getLocation(), diag::err_builtin_definition) << FD;
      FD->setInvalidDefn();
    }
  }

  // The return type of a function definition must be complete
  // (C99 6.9.1p3, C++ [dcl.fct]p6).
  Type ResultType = FD->getResultType();
  if (!ResultType->isDependentType() && !ResultType->isVoidType() &&
      !FD->isInvalidDefn() &&
      RequireCompleteType(FD->getLocation(), ResultType,
                          diag::err_func_def_incomplete_result))
    FD->setInvalidDefn();

  // GNU warning -Wmissing-prototypes:
  //   Warn if a global function is defined without a previous
  //   prototype declaration. This warning is issued even if the
  //   definition itself provides a prototype. The aim is to detect
  //   global functions that fail to be declared in header files.
  if (ShouldWarnAboutMissingPrototype(FD))
    Diag(FD->getLocation(), diag::warn_missing_prototype) << FD;

  if (FnBodyScope)
    PushDefnContext(FnBodyScope, FD);

  // Check the validity of our function parameters
  CheckParmsForFunctionDef(FD->param_begin(), FD->param_end(),
                           CheckParameterNames=true);

  // Introduce our parameters into the function scope
  for (unsigned p = 0, NumParams = FD->getNumParams(); p < NumParams; ++p) {
    ParmVarDefn *Param = FD->getParamDefn(p);
    Param->setOwningFunction(FD);

    // If this has an identifier, add it to the scope stack.
    if (Param->getIdentifier() && FnBodyScope) {
      CheckShadow(FnBodyScope, Param);

      PushOnScopeChains(Param, FnBodyScope);
    }
  }

  // Checking attributes of current function definition
  // dllimport attribute.
  DLLImportAttr *DA = FD->getAttr<DLLImportAttr>();
  if (DA && (!FD->getAttr<DLLExportAttr>())) {
    // dllimport attribute cannot be directly applied to definition.
    if (!DA->isInherited()) {
      Diag(FD->getLocation(),
           diag::err_attribute_can_be_applied_only_to_symbol_declaration)
        << "dllimport";
      FD->setInvalidDefn();
      return FD;
    }

    // Visual C++ appears to not think this is an issue, so only issue
    // a warning when Microsoft extensions are disabled.
    if (!LangOpts.Microsoft) {
      // If a symbol previously declared dllimport is later defined, the
      // attribute is ignored in subsequent references, and a warning is
      // emitted.
      Diag(FD->getLocation(),
           diag::warn_redeclaration_without_attribute_prev_attribute_ignored)
        << FD->getName() << "dllimport";
    }
  }
  return FD;*/

	return NULL;
}

Defn *Sema::ActOnFinishFunctionBody(Defn *D, Stmt *stmt) {
	return NULL;
}

/// Add this Defn to the scope shadowed Defn chains.
void Sema::PushOnScopeChains(NamedDefn *D, Scope *S, bool AddToContext) {
  // Add scoped declarations into their context, so that they can be
  // found later. Declarations without a context won't be inserted
  // into any context.
  // FIXME yabin
  if (AddToContext)
    CurContext->addDefn(D);

  // If this replaces anything in the current scope, 
  IdentifierResolver::iterator I = IdResolver.begin(D->getDefnName()),
                               IEnd = IdResolver.end();
  for (; I != IEnd; ++I) {
    if (S->isDefnScope(*I) && D->definitionReplaces(*I)) {
      S->RemoveDefn(*I);
      IdResolver.RemoveDefn(*I);

      // Should only need to replace one Defn.
      break;
    }
  }

  S->AddDefn(D);
  IdResolver.AddDefn(D);
}

bool Sema::isDefnInScope(NamedDefn *&D, DefnContext *Ctx, Scope *S) {
	return IdResolver.isDefnInScope(D, Ctx, Context, S);
}

Scope *Sema::getScopeForDefnContext(Scope *S, DefnContext *DC) {
  return 0;
}

/// Filters out lookup results that don't fall within the given scope
/// as determined by isDefnInScope.
//static void FilterLookupForScope(Sema &SemaRef, LookupResult &R,
//                                 DefnContext *Ctx, Scope *S,
//                                 bool ConsiderLinkage) {
//}

static bool ShouldDiagnoseUnusedDefn(const NamedDefn *D) {
  return true;
}

void Sema::DiagnoseUnusedDefn(const NamedDefn *D) {
	if(ShouldDiagnoseUnusedDefn(D))
		return;
}

/// LazilyCreateBuiltin - The specified Builtin-ID was first used at
/// file scope.  lazily create a decl for it. ForRedefinition is true
/// if we're creating this built-in in anticipation of redeclaring the
/// built-in.
NamedDefn *Sema::LazilyCreateBuiltin(IdentifierInfo *II, unsigned bid,
                                     Scope *S, bool ForRedefinition,
                                     SourceLocation Loc) {
	Builtin::ID BID = (Builtin::ID) bid;
	ASTContext::GetBuiltinTypeError Error;
	Type R = Context.GetBuiltinFunctionType(BID, Error);

	if (!ForRedefinition && Context.BuiltinInfo.isPredefinedLibFunction(BID)) {
//		Diag(Loc, diag::ext_implicit_lib_function_decl)
//				<< Context.BuiltinInfo.GetName(BID) << R;
//		if (Context.BuiltinInfo.getHeaderName(BID) && Diags.getDiagnosticLevel(
//				diag::ext_implicit_lib_function_decl, Loc)
//				!= Diagnostic::Ignored)
//			Diag(Loc, diag::note_please_include_header)
//					<< Context.BuiltinInfo.getHeaderName(BID)
//					<< Context.BuiltinInfo.GetName(BID);
	}

	/// All the built-in function defns placed into the
	/// translation unit context.
	DefnContext *TU = static_cast<DefnContext*>(
			Context.getTranslationUnitDefn());
	FunctionDefn *New = FunctionDefn::Create(Context,
			TU, Loc, DefinitionName(II), R,
			SC_Global, SC_None, false,
			/*hasPrototype=*/true);
	New->setImplicit();

	// Create Defn objects for each parameter, adding them to the
	// FunctionDecl.
//	if (FunctionProtoType *FT = dyn_cast<FunctionProtoType>(R)) {
//		llvm::SmallVector<ParamVarDefn*, 16> Params;
//		for (unsigned i = 0, e = FT->getNumArgs(); i != e; ++i)
//			Params.push_back(ParamVarDefn::Create(Context, New,
//					SourceLocation(), 0, FT->getArgType(i),
//					SC_None, SC_None, 0));
//		New->setParams(Params.data(), Params.size());
//	}

	// AddKnownFunctionAttributes(New);

	// TUScope is the translation-unit scope to insert this function into.
	// FIXME: This is hideous. We need to teach PushOnScopeChains to
	// relate Scopes to DeclContexts, and probably eliminate CurContext
	// entirely, but we're not there yet.
	DefnContext *SavedContext = CurContext;
	CurContext = Context.getTranslationUnitDefn();
	PushOnScopeChains(New, getCurScope());
  CurContext = SavedContext;
	return New;
}

#if 0
/// DefnhasAttr - returns true if decl Definition already has the target
/// attribute.
static bool
DefnHasAttr(const Defn *D, const Attr *A) {
  return false;
}

/// MergeDefnAttributes - append attributes from the Old decl to the New one.
static void MergeDefnAttributes(Defn *New, Defn *Old, ASTContext &C) {

}

/// getSpecialMember - get the special member enum for a method.
Sema::ClassSpecialMember Sema::getSpecialMember(const ClassMethodDefn *MD) {
  return Sema::CXXCopyAssignment;
}

/// MergeVarDefn - We just parsed a variable 'New' which has the same name
/// and scope as a previous declaration 'Old'.  Figure out how to resolve this
/// situation, merging decls or emitting diagnostics as appropriate.
///
/// Tentative definition rules (C99 6.9.2p2) are checked by
/// FinalizeDeclaratorGroup. Unfortunately, we can't analyze tentative
/// definitions here, since the initializer hasn't been attached.
///
void Sema::MergeVarDefn(VarDefn *New, LookupResult &Previous) {
  
}

/// \brief Retrieves the declaration name from a parsed unqualified-id.
DefinitionNameInfo
Sema::GetNameFromUnqualifiedId(const UnqualifiedId &Name) {
  DefinitionNameInfo NameInfo;
  NameInfo.setLoc(Name.StartLocation);

  switch (Name.getKind()) {

  case UnqualifiedId::IK_Identifier:
    NameInfo.setName(Name.Identifier);
    NameInfo.setLoc(Name.StartLocation);
    return NameInfo;

  case UnqualifiedId::IK_OperatorFunctionId:
    NameInfo.setName(Context.DefinitionNames.getCXXOperatorName(
                                           Name.OperatorFunctionId.Operator));
    NameInfo.setLoc(Name.StartLocation);
    NameInfo.getInfo().CXXOperatorName.BeginOpNameLoc
      = Name.OperatorFunctionId.SymbolLocations[0];
    NameInfo.getInfo().CXXOperatorName.EndOpNameLoc
      = Name.EndLocation.getRawEncoding();
    return NameInfo;

  case UnqualifiedId::IK_LiteralOperatorId:
    NameInfo.setName(Context.DefinitionNames.getCXXLiteralOperatorName(
                                                           Name.Identifier));
    NameInfo.setLoc(Name.StartLocation);
    NameInfo.setCXXLiteralOperatorNameLoc(Name.EndLocation);
    return NameInfo;

  case UnqualifiedId::IK_ConversionFunctionId: {
    TypeSourceInfo *TInfo;
    Type Ty = GetTypeFromParser(Name.ConversionFunctionId, &TInfo);
    if (Ty.isNull())
      return DefinitionNameInfo();
    NameInfo.setName(Context.DefinitionNames.getCXXConversionFunctionName(
                                               Context.getCanonicalType(Ty)));
    NameInfo.setLoc(Name.StartLocation);
    NameInfo.setNamedTypeInfo(TInfo);
    return NameInfo;
  }

  case UnqualifiedId::IK_ConstructorName: {
    TypeSourceInfo *TInfo;
    Type Ty = GetTypeFromParser(Name.ConstructorName, &TInfo);
    if (Ty.isNull())
      return DefinitionNameInfo();
    NameInfo.setName(Context.DefinitionNames.getCXXConstructorName(
                                              Context.getCanonicalType(Ty)));
    NameInfo.setLoc(Name.StartLocation);
    NameInfo.setNamedTypeInfo(TInfo);
    return NameInfo;
  }

  case UnqualifiedId::IK_ConstructorTemplateId: {
    // In well-formed code, we can only have a constructor
    // template-id that refers to the current context, so go there
    // to find the actual type being constructed.
    CXXRecordDefn *CurClass = dyn_cast<CXXRecordDefn>(CurContext);
    if (!CurClass || CurClass->getIdentifier() != Name.TemplateId->Name)
      return DefinitionNameInfo();

    // Determine the type of the class being constructed.
    Type CurClassType = Context.getTypeDefnType(CurClass);

    // FIXME: Check two things: that the template-id names the same type as
    // CurClassType, and that the template-id does not occur when the name
    // was qualified.

    NameInfo.setName(Context.DefinitionNames.getCXXConstructorName(
                                    Context.getCanonicalType(CurClassType)));
    NameInfo.setLoc(Name.StartLocation);
    // FIXME: should we retrieve TypeSourceInfo?
    NameInfo.setNamedTypeInfo(0);
    return NameInfo;
  }

  case UnqualifiedId::IK_DestructorName: {
    TypeSourceInfo *TInfo;
    Type Ty = GetTypeFromParser(Name.DestructorName, &TInfo);
    if (Ty.isNull())
      return DefinitionNameInfo();
    NameInfo.setName(Context.DefinitionNames.getCXXDestructorName(
                                              Context.getCanonicalType(Ty)));
    NameInfo.setLoc(Name.StartLocation);
    NameInfo.setNamedTypeInfo(TInfo);
    return NameInfo;
  }

  case UnqualifiedId::IK_TemplateId: {
    TemplateName TName = Name.TemplateId->Template.get();
    SourceLocation TNameLoc = Name.TemplateId->TemplateNameLoc;
    return Context.getNameForTemplate(TName, TNameLoc);
  }

  } // switch (Name.getKind())

  assert(false && "Unknown name kind");
  return DefinitionNameInfo();
}

/// isNearlyMatchingFunction - Determine whether the C++ functions
/// Definition and Definition are "nearly" matching. This heuristic
/// is used to improve diagnostics in the case where an out-of-line
/// function definition doesn't match any declaration within
/// the class or namespace.
static bool isNearlyMatchingFunction(ASTContext &Context,
                                     FunctionDefn *Declaration,
                                     FunctionDefn *Definition) {
  return true;
}

static void SetNestedNameSpecifier(DeclaratorDefn *DD, Declarator &D) {
  CXXScopeSpec &SS = D.getCXXScopeSpec();
  if (!SS.isSet()) return;
  DD->setQualifierInfo(static_cast<NestedNameSpecifier*>(SS.getScopeRep()),
                       SS.getRange());
}

/// \brief Diagnose variable or built-in function shadowing.  Implements
/// -Wshadow.
///
/// This method is called whenever a VarDefn is added to a "useful"
/// scope.
///
/// \param S the scope in which the shadowing name is being declared
/// \param R the lookup of the name
///
void Sema::CheckShadow(Scope *S, VarDefn *D, const LookupResult& R) {
  // Return if warning is ignored.
  if (Diags.getDiagnosticLevel(diag::warn_decl_shadow, R.getNameLoc()) ==
        Diagnostic::Ignored)
    return;

  // Don't diagnose declarations at file scope.  The scope might not
  // have a DefnContext if (e.g.) we're parsing a function prototype.
  DefnContext *NewDC = static_cast<DefnContext*>(S->getEntity());
  if (NewDC && NewDC->isFileContext())
    return;
  
  // Only diagnose if we're shadowing an unambiguous field or variable.
  if (R.getResultKind() != LookupResult::Found)
    return;

  NamedDefn* ShadowedDefn = R.getFoundDefn();
  if (!isa<VarDefn>(ShadowedDefn) && !isa<FieldDefn>(ShadowedDefn))
    return;

  DefnContext *OldDC = ShadowedDefn->getDefnContext();

  // Only warn about certain kinds of shadowing for class members.
  if (NewDC && NewDC->isRecord()) {
    // In particular, don't warn about shadowing non-class members.
    if (!OldDC->isRecord())
      return;

    // TODO: should we warn about static data members shadowing
    // static data members from base classes?
    
    // TODO: don't diagnose for inaccessible shadowed members.
    // This is hard to do perfectly because we might friend the
    // shadowing context, but that's just a false negative.
  }

  // Determine what kind of declaration we're shadowing.
  unsigned Kind;
  if (isa<RecordDefn>(OldDC)) {
    if (isa<FieldDefn>(ShadowedDefn))
      Kind = 3; // field
    else
      Kind = 2; // static data member
  } else if (OldDC->isFileContext())
    Kind = 1; // global
  else
    Kind = 0; // local

  DefinitionName Name = R.getLookupName();

  // Emit warning and note.
  Diag(R.getNameLoc(), diag::warn_decl_shadow) << Name << Kind << OldDC;
  Diag(ShadowedDefn->getLocation(), diag::note_previous_declaration);
}

/// \brief Check -Wshadow without the advantage of a previous lookup.
void Sema::CheckShadow(Scope *S, VarDefn *D) {
  if (Diags.getDiagnosticLevel(diag::warn_decl_shadow, D->getLocation()) ==
        Diagnostic::Ignored)
    return;

  LookupResult R(*this, D->getDefnName(), D->getLocation(),
                 Sema::LookupOrdinaryName, Sema::ForRedefinition);
  LookupName(R, S);
  CheckShadow(S, D, R);
}

/// \brief Perform semantic checking on a newly-created variable
/// declaration.
///
/// This routine performs all of the type-checking required for a
/// variable declaration once it has been built. It is used both to
/// check variables after they have been parsed and their declarators
/// have been translated into a declaration, and to check variables
/// that have been instantiated from a template.
///
/// Sets NewVD->isInvalidDefn() if an error was encountered.
void Sema::CheckVariableDefinition(VarDefn *NewVD,
                                    LookupResult &Previous,
                                    bool &Redeclaration) {
  // If the decl is already known invalid, don't check it.
  if (NewVD->isInvalidDefn())
    return;

  Type T = NewVD->getType();

  if (T->isObjCObjectType()) {
    Diag(NewVD->getLocation(), diag::err_statically_allocated_object);
    return NewVD->setInvalidDefn();
  }

  // Emit an error if an address space was applied to decl with local storage.
  // This includes arrays of objects with address space qualifiers, but not
  // automatic variables that point to other address spaces.
  // ISO/IEC TR 18037 S5.1.2
  if (NewVD->hasLocalStorage() && T.getAddressSpace() != 0) {
    Diag(NewVD->getLocation(), diag::err_as_qualified_auto_decl);
    return NewVD->setInvalidDefn();
  }

  if (NewVD->hasLocalStorage() && T.isObjCGCWeak()
      && !NewVD->hasAttr<BlocksAttr>())
    Diag(NewVD->getLocation(), diag::warn_attribute_weak_on_local);
  
  bool isVM = T->isVariablyModifiedType();
  if (isVM || NewVD->hasAttr<CleanupAttr>() ||
      NewVD->hasAttr<BlocksAttr>())
    getCurFunction()->setHasBranchProtectedScope();

  if ((isVM && NewVD->hasLinkage()) ||
      (T->isVariableArrayType() && NewVD->hasGlobalStorage())) {
    bool SizeIsNegative;
    llvm::APSInt Oversized;
    Type FixedTy =
        TryToFixInvalidVariablyModifiedType(T, Context, SizeIsNegative,
                                            Oversized);

    if (FixedTy.isNull() && T->isVariableArrayType()) {
      const VariableArrayType *VAT = Context.getAsVariableArrayType(T);
      // FIXME: This won't give the correct result for
      // int a[10][n];
      SourceRange SizeRange = VAT->getSizeExpr()->getSourceRange();

      if (NewVD->isFileVarDefn())
        Diag(NewVD->getLocation(), diag::err_vla_decl_in_file_scope)
        << SizeRange;
      else if (NewVD->getStorageClass() == SC_Static)
        Diag(NewVD->getLocation(), diag::err_vla_decl_has_static_storage)
        << SizeRange;
      else
        Diag(NewVD->getLocation(), diag::err_vla_decl_has_extern_linkage)
        << SizeRange;
      return NewVD->setInvalidDefn();
    }

    if (FixedTy.isNull()) {
      if (NewVD->isFileVarDefn())
        Diag(NewVD->getLocation(), diag::err_vm_decl_in_file_scope);
      else
        Diag(NewVD->getLocation(), diag::err_vm_decl_has_extern_linkage);
      return NewVD->setInvalidDefn();
    }

    Diag(NewVD->getLocation(), diag::warn_illegal_constant_array_size);
    NewVD->setType(FixedTy);
  }

  if (Previous.empty() && NewVD->isExternC()) {
    // Since we did not find anything by this name and we're declaring
    // an extern "C" variable, look for a non-visible extern "C"
    // declaration with the same name.
    llvm::DenseMap<DefinitionName, NamedDefn *>::iterator Pos
      = LocallyScopedExternalDefns.find(NewVD->getDefnName());
    if (Pos != LocallyScopedExternalDefns.end())
      Previous.addDefn(Pos->second);
  }

  if (T->isVoidType() && !NewVD->hasExternalStorage()) {
    Diag(NewVD->getLocation(), diag::err_typecheck_decl_incomplete_type)
      << T;
    return NewVD->setInvalidDefn();
  }

  if (!NewVD->hasLocalStorage() && NewVD->hasAttr<BlocksAttr>()) {
    Diag(NewVD->getLocation(), diag::err_block_on_nonlocal);
    return NewVD->setInvalidDefn();
  }

  if (isVM && NewVD->hasAttr<BlocksAttr>()) {
    Diag(NewVD->getLocation(), diag::err_block_on_vm);
    return NewVD->setInvalidDefn();
  }

  // Function pointers and references cannot have qualified function type, only
  // function pointer-to-members can do that.
  Type Pointee;
  unsigned PtrOrRef = 0;
  if (const PointerType *Ptr = T->getAs<PointerType>())
    Pointee = Ptr->getPointeeType();
  else if (const ReferenceType *Ref = T->getAs<ReferenceType>()) {
    Pointee = Ref->getPointeeType();
    PtrOrRef = 1;
  }
  if (!Pointee.isNull() && Pointee->isFunctionProtoType() &&
      Pointee->getAs<FunctionProtoType>()->getTypeQuals() != 0) {
    Diag(NewVD->getLocation(), diag::err_invalid_qualified_function_pointer)
        << PtrOrRef;
    return NewVD->setInvalidDefn();
  }

  if (!Previous.empty()) {
    Redeclaration = true;
    MergeVarDefn(NewVD, Previous);
  }
}

/// \brief Member lookup function that determines whether a given C++
/// method overrides a method in a base class, to be used with
/// CXXRecordDefn::lookupInBases().
static bool FindOverriddenMethod(const CXXBaseSpecifier *Specifier,
                                 CXXBasePath &Path,
                                 void *UserData) {
  RecordDefn *BaseRecord = Specifier->getType()->getAs<HeteroContainerType>()->getDefn();

  FindOverriddenMethodData *Data 
    = reinterpret_cast<FindOverriddenMethodData*>(UserData);
  
  DefinitionName Name = Data->Method->getDefnName();
  
  // FIXME: Do we care about other names here too?
  if (Name.getNameKind() == DefinitionName::CXXDestructorName) {
    // We really want to find the base class destructor here.
    Type T = Data->S->Context.getTypeDefnType(BaseRecord);
    Type CT = Data->S->Context.getCanonicalType(T);
    
    Name = Data->S->Context.DefinitionNames.getCXXDestructorName(CT);
  }    
  
  for (Path.Defns = BaseRecord->lookup(Name);
       Path.Defns.first != Path.Defns.second;
       ++Path.Defns.first) {
    NamedDefn *D = *Path.Defns.first;
    if (CXXMethodDefn *MD = dyn_cast<CXXMethodDefn>(D)) {
      if (MD->isVirtual() && !Data->S->IsOverload(Data->Method, MD, false))
        return true;
    }
  }
  
  return false;
}

/// AddOverriddenMethods - See if a method overrides any in the base classes,
/// and if so, check that it's a valid override and remember it.
bool Sema::AddOverriddenMethods(UserClassDefn *DC, ClassMethodDefn *MD) {
  // Look for virtual methods in base classes that this method might override.
  CXXBasePaths Paths;
  FindOverriddenMethodData Data;
  Data.Method = MD;
  Data.S = this;
  bool AddedAny = false;
  if (DC->lookupInBases(&FindOverriddenMethod, &Data, Paths)) {
    for (CXXBasePaths::decl_iterator I = Paths.found_decls_begin(),
         E = Paths.found_decls_end(); I != E; ++I) {
      if (CXXMethodDefn *OldMD = dyn_cast<CXXMethodDefn>(*I)) {
        if (!CheckOverridingFunctionReturnType(MD, OldMD) &&
            !CheckOverridingFunctionExceptionSpec(MD, OldMD) &&
            !CheckOverridingFunctionAttributes(MD, OldMD)) {
          MD->addOverriddenMethod(OldMD->getCanonicalDefn());
          AddedAny = true;
        }
      }
    }
  }
  
  return AddedAny;
}

static void DiagnoseInvalidRedeclaration(Sema &S, FunctionDefn *NewFD) {
  LookupResult Prev(S, NewFD->getDefnName(), NewFD->getLocation(),
                    Sema::LookupOrdinaryName, Sema::ForRedefinition);
  S.LookupQualifiedName(Prev, NewFD->getDefnContext());
  assert(!Prev.isAmbiguous() &&
         "Cannot have an ambiguity in previous-declaration lookup");
  for (LookupResult::iterator Func = Prev.begin(), FuncEnd = Prev.end();
       Func != FuncEnd; ++Func) {
    if (isa<FunctionDefn>(*Func) &&
        isNearlyMatchingFunction(S.Context, cast<FunctionDefn>(*Func), NewFD))
      S.Diag((*Func)->getLocation(), diag::note_member_def_close_match);
  }
}

/// CheckClassMemberNameAttributes - Check for class member name checking
/// attributes according to [dcl.attr.override]
static void 
CheckClassMemberNameAttributes(Sema& SemaRef, const FunctionDefn *FD) {
  const CXXMethodDefn *MD = dyn_cast<CXXMethodDefn>(FD);
  if (!MD || !MD->isVirtual())
    return;

  bool HasOverrideAttr = MD->hasAttr<OverrideAttr>();
  bool HasOverriddenMethods = 
    MD->begin_overridden_methods() != MD->end_overridden_methods();

  /// C++ [dcl.attr.override]p2:
  ///   If a virtual member function f is marked override and does not override
  ///   a member function of a base class the program is ill-formed.
  if (HasOverrideAttr && !HasOverriddenMethods) {
    SemaRef.Diag(MD->getLocation(), diag::err_override_function_not_overriding)
      << MD->getDefnName();
    return;
  }

  if (!MD->getParent()->hasAttr<BaseCheckAttr>())
    return;

  /// C++ [dcl.attr.override]p6:
  ///   In a class definition marked base_check, if a virtual member function
  ///    that is neither implicitly-declared nor a destructor overrides a 
  ///    member function of a base class and it is not marked override, the
  ///    program is ill-formed.
  if (HasOverriddenMethods && !HasOverrideAttr && !MD->isImplicit() &&
      !isa<CXXDestructorDefn>(MD)) {
    llvm::SmallVector<const CXXMethodDefn*, 4>
      OverriddenMethods(MD->begin_overridden_methods(), 
                        MD->end_overridden_methods());

    SemaRef.Diag(MD->getLocation(), 
                 diag::err_function_overriding_without_override)
      << MD->getDefnName() << (unsigned)OverriddenMethods.size();

    for (unsigned I = 0; I != OverriddenMethods.size(); ++I)
      SemaRef.Diag(OverriddenMethods[I]->getLocation(),
                   diag::note_overridden_virtual_function);
  }
}

NamedDefn*
Sema::ActOnFunctionDeclarator(Scope* S, Declarator& D, DefnContext* DC,
                              Type R, TypeSourceInfo *TInfo,
                              LookupResult &Previous,
                              MultiTemplateParamsArg TemplateParamLists,
                              bool IsFunctionDefinition, bool &Redeclaration) {
  assert(R.getRawTypePtr()->isFunctionType());

  // TODO: consider using NameInfo for diagnostic.
  DefinitionNameInfo NameInfo = GetNameForDeclarator(D);
  DefinitionName Name = NameInfo.getName();
  FunctionDefn::StorageClass SC = SC_None;
  switch (D.getDefnSpec().getStorageClassSpec()) {
  default: assert(0 && "Unknown storage class!");
  case DefnSpec::SCS_auto:
  case DefnSpec::SCS_register:
  case DefnSpec::SCS_mutable:
    Diag(D.getDefnSpec().getStorageClassSpecLoc(),
         diag::err_typecheck_sclass_func);
    D.setInvalidType();
    break;
  case DefnSpec::SCS_unspecified: SC = SC_None; break;
  case DefnSpec::SCS_extern:      SC = SC_Extern; break;
  case DefnSpec::SCS_static: {
    if (CurContext->getRedeclContext()->isFunctionOrMethod()) {
      // C99 6.7.1p5:
      //   The declaration of an identifier for a function that has
      //   block scope shall have no explicit storage-class specifier
      //   other than extern
      // See also (C++ [dcl.stc]p4).
      Diag(D.getDefnSpec().getStorageClassSpecLoc(),
           diag::err_static_block_func);
      SC = SC_None;
    } else
      SC = SC_Static;
    break;
  }
  case DefnSpec::SCS_private_extern: SC = SC_PrivateExtern; break;
  }

  if (D.getDefnSpec().isThreadSpecified())
    Diag(D.getDefnSpec().getThreadSpecLoc(), diag::err_invalid_thread);

  // Do not allow returning a objc interface by-value.
  if (R->getAs<FunctionType>()->getResultType()->isObjCObjectType()) {
    Diag(D.getIdentifierLoc(),
         diag::err_object_cannot_be_passed_returned_by_value) << 0
    << R->getAs<FunctionType>()->getResultType();
    D.setInvalidType();
  }
  
  FunctionDefn *NewFD;
  bool isInline = D.getDefnSpec().isInlineSpecified();
  bool isFriend = false;
  DefnSpec::SCS SCSpec = D.getDefnSpec().getStorageClassSpecAsWritten();
  FunctionDefn::StorageClass SCAsWritten
    = StorageClassSpecToFunctionDefnStorageClass(SCSpec);
  FunctionTemplateDefn *FunctionTemplate = 0;
  bool isExplicitSpecialization = false;
  bool isFunctionTemplateSpecialization = false;
  unsigned NumMatchedTemplateParamLists = 0;
  
  if (!getLangOptions().CPlusPlus) {
    // Determine whether the function was written with a
    // prototype. This true when:
    //   - there is a prototype in the declarator, or
    //   - the type R of the function is some kind of typedef or other reference
    //     to a type name (which eventually refers to a function type).
    bool HasPrototype =
    (D.isFunctionDeclarator() && D.getFunctionTypeInfo().hasPrototype) ||
    (!isa<FunctionType>(R.getRawTypePtr()) && R->isFunctionProtoType());
  
    NewFD = FunctionDefn::Create(Context, DC,
                                 NameInfo, R, TInfo, SC, SCAsWritten, isInline,
                                 HasPrototype);
    if (D.isInvalidType())
      NewFD->setInvalidDefn();
    
    // Set the lexical context.
    NewFD->setLexicalDefnContext(CurContext);
    // Filter out previous declarations that don't match the scope.
    FilterLookupForScope(*this, Previous, DC, S, NewFD->hasLinkage());
  } else {
    isFriend = D.getDefnSpec().isFriendSpecified();
    bool isVirtual = D.getDefnSpec().isVirtualSpecified();
    bool isExplicit = D.getDefnSpec().isExplicitSpecified();
    bool isVirtualOkay = false;

    // Check that the return type is not an abstract class type.
    // For record types, this is done by the AbstractClassUsageDiagnoser once
    // the class has been completely parsed.
    if (!DC->isRecord() &&
      RequireNonAbstractType(D.getIdentifierLoc(),
                             R->getAs<FunctionType>()->getResultType(),
                             diag::err_abstract_type_in_decl,
                             AbstractReturnType))
      D.setInvalidType();


    if (isFriend) {
      // C++ [class.friend]p5
      //   A function can be defined in a friend declaration of a
      //   class . . . . Such a function is implicitly inline.
      isInline |= IsFunctionDefinition;
    }

    if (Name.getNameKind() == DefinitionName::CXXConstructorName) {
      // This is a C++ constructor declaration.
      assert(DC->isRecord() &&
             "Constructors can only be declared in a member context");

      R = CheckConstructorDeclarator(D, R, SC);

      // Create the new declaration
      NewFD = CXXConstructorDefn::Create(Context,
                                         cast<CXXRecordDefn>(DC),
                                         NameInfo, R, TInfo,
                                         isExplicit, isInline,
                                         /*isImplicitlyDefnared=*/false);
    } else if (Name.getNameKind() == DefinitionName::CXXDestructorName) {
      // This is a C++ destructor declaration.
      if (DC->isRecord()) {
        R = CheckDestructorDeclarator(D, R, SC);

        NewFD = CXXDestructorDefn::Create(Context,
                                          cast<CXXRecordDefn>(DC),
                                          NameInfo, R, TInfo,
                                          isInline,
                                          /*isImplicitlyDefnared=*/false);
        isVirtualOkay = true;
      } else {
        Diag(D.getIdentifierLoc(), diag::err_destructor_not_member);

        // Create a FunctionDefn to satisfy the function definition parsing
        // code path.
        NewFD = FunctionDefn::Create(Context, DC, D.getIdentifierLoc(),
                                     Name, R, TInfo, SC, SCAsWritten, isInline,
                                     /*hasPrototype=*/true);
        D.setInvalidType();
      }
    } else if (Name.getNameKind() == DefinitionName::CXXConversionFunctionName) {
      if (!DC->isRecord()) {
        Diag(D.getIdentifierLoc(),
             diag::err_conv_function_not_member);
        return 0;
      }

      CheckConversionDeclarator(D, R, SC);
      NewFD = CXXConversionDefn::Create(Context, cast<CXXRecordDefn>(DC),
                                        NameInfo, R, TInfo,
                                        isInline, isExplicit);

      isVirtualOkay = true;
    } else if (DC->isRecord()) {
      // If the of the function is the same as the name of the record, then this
      // must be an invalid constructor that has a return type.
      // (The parser checks for a return type and makes the declarator a
      // constructor if it has no return type).
      // must have an invalid constructor that has a return type
      if (Name.getAsIdentifierInfo() &&
          Name.getAsIdentifierInfo() == cast<CXXRecordDefn>(DC)->getIdentifier()){
        Diag(D.getIdentifierLoc(), diag::err_constructor_return_type)
          << SourceRange(D.getDefnSpec().getTypeSpecTypeLoc())
          << SourceRange(D.getIdentifierLoc());
        return 0;
      }

      bool isStatic = SC == SC_Static;
    
      // [class.free]p1:
      // Any allocation function for a class T is a static member
      // (even if not explicitly declared static).
      if (Name.getCXXOverloadedOperator() == OO_New ||
          Name.getCXXOverloadedOperator() == OO_Array_New)
        isStatic = true;

      // [class.free]p6 Any deallocation function for a class X is a static member
      // (even if not explicitly declared static).
      if (Name.getCXXOverloadedOperator() == OO_Delete ||
          Name.getCXXOverloadedOperator() == OO_Array_Delete)
        isStatic = true;
    
      // This is a C++ method declaration.
      NewFD = CXXMethodDefn::Create(Context, cast<CXXRecordDefn>(DC),
                                    NameInfo, R, TInfo,
                                    isStatic, SCAsWritten, isInline);

      isVirtualOkay = !isStatic;
    } else {
      // Determine whether the function was written with a
      // prototype. This true when:
      //   - we're in C++ (where every function has a prototype),
      NewFD = FunctionDefn::Create(Context, DC,
                                   NameInfo, R, TInfo, SC, SCAsWritten, isInline,
                                   true/*HasPrototype*/);
    }
    SetNestedNameSpecifier(NewFD, D);
    isExplicitSpecialization = false;
    isFunctionTemplateSpecialization = false;
    NumMatchedTemplateParamLists = TemplateParamLists.size();
    if (D.isInvalidType())
      NewFD->setInvalidDefn();
    
    // Set the lexical context. If the declarator has a C++
    // scope specifier, or is the object of a friend declaration, the
    // lexical context will be different from the semantic context.
    NewFD->setLexicalDefnContext(CurContext);
    
    // Match up the template parameter lists with the scope specifier, then
    // determine whether we have a template or a template specialization.
    bool Invalid = false;
    if (TemplateParameterList *TemplateParams
        = MatchTemplateParametersToScopeSpecifier(
                                  D.getDefnSpec().getSourceRange().getBegin(),
                                  D.getCXXScopeSpec(),
                                  TemplateParamLists.get(),
                                  TemplateParamLists.size(),
                                  isFriend,
                                  isExplicitSpecialization,
                                  Invalid)) {
          // All but one template parameter lists have been matching.
          --NumMatchedTemplateParamLists;

          if (TemplateParams->size() > 0) {
            // This is a function template

            // Check that we can declare a template here.
            if (CheckTemplateDefnScope(S, TemplateParams))
              return 0;

            FunctionTemplate = FunctionTemplateDefn::Create(Context, DC,
                                                      NewFD->getLocation(),
                                                      Name, TemplateParams,
                                                      NewFD);
            FunctionTemplate->setLexicalDefnContext(CurContext);
            NewFD->setDescribedFunctionTemplate(FunctionTemplate);
          } else {
            // This is a function template specialization.
            isFunctionTemplateSpecialization = true;

            // C++0x [temp.expl.spec]p20 forbids "template<> friend void foo(int);".
            if (isFriend && isFunctionTemplateSpecialization) {
              // We want to remove the "template<>", found here.
              SourceRange RemoveRange = TemplateParams->getSourceRange();

              // If we remove the template<> and the name is not a
              // template-id, we're actually silently creating a problem:
              // the friend declaration will refer to an untemplated decl,
              // and clearly the user wants a template specialization.  So
              // we need to insert '<>' after the name.
              SourceLocation InsertLoc;
              if (D.getName().getKind() != UnqualifiedId::IK_TemplateId) {
                InsertLoc = D.getName().getSourceRange().getEnd();
                InsertLoc = PP.getLocForEndOfToken(InsertLoc);
              }

              Diag(D.getIdentifierLoc(), diag::err_template_spec_decl_friend)
              << Name << RemoveRange
              << FixItHint::CreateRemoval(RemoveRange)
              << FixItHint::CreateInsertion(InsertLoc, "<>");
            }
          }
        }

    if (NumMatchedTemplateParamLists > 0 && D.getCXXScopeSpec().isSet()) {
      NewFD->setTemplateParameterListsInfo(Context,
                                           NumMatchedTemplateParamLists,
                                           TemplateParamLists.release());
    }

    if (Invalid) {
      NewFD->setInvalidDefn();
      if (FunctionTemplate)
        FunctionTemplate->setInvalidDefn();
    }
  
    // C++ [dcl.fct.spec]p5:
    //   The virtual specifier shall only be used in declarations of
    //   nonstatic class member functions that appear within a
    //   member-specification of a class declaration; see 10.3.
    //
    if (isVirtual && !NewFD->isInvalidDefn()) {
      if (!isVirtualOkay) {
        Diag(D.getDefnSpec().getVirtualSpecLoc(),
             diag::err_virtual_non_function);
      } else if (!CurContext->isRecord()) {
        // 'virtual' was specified outside of the class.
        Diag(D.getDefnSpec().getVirtualSpecLoc(), diag::err_virtual_out_of_class)
          << FixItHint::CreateRemoval(D.getDefnSpec().getVirtualSpecLoc());
      } else {
        // Okay: Add virtual to the method.
        NewFD->setVirtualAsWritten(true);
      }
    }

    // C++ [dcl.fct.spec]p3:
    //  The inline specifier shall not appear on a block scope function declaration.
    if (isInline && !NewFD->isInvalidDefn()) {
      if (CurContext->isFunctionOrMethod()) {
        // 'inline' is not allowed on block scope function declaration.
        Diag(D.getDefnSpec().getInlineSpecLoc(),
             diag::err_inline_declaration_block_scope) << Name
          << FixItHint::CreateRemoval(D.getDefnSpec().getInlineSpecLoc());
      }
    }
 
    // C++ [dcl.fct.spec]p6:
    //  The explicit specifier shall be used only in the declaration of a
    //  constructor or conversion function within its class definition; see 12.3.1
    //  and 12.3.2.
    if (isExplicit && !NewFD->isInvalidDefn()) {
      if (!CurContext->isRecord()) {
        // 'explicit' was specified outside of the class.
        Diag(D.getDefnSpec().getExplicitSpecLoc(),
             diag::err_explicit_out_of_class)
          << FixItHint::CreateRemoval(D.getDefnSpec().getExplicitSpecLoc());
      } else if (!isa<CXXConstructorDefn>(NewFD) &&
                 !isa<CXXConversionDefn>(NewFD)) {
        // 'explicit' was specified on a function that wasn't a constructor
        // or conversion function.
        Diag(D.getDefnSpec().getExplicitSpecLoc(),
             diag::err_explicit_non_ctor_or_conv_function)
          << FixItHint::CreateRemoval(D.getDefnSpec().getExplicitSpecLoc());
      }      
    }

    // Filter out previous declarations that don't match the scope.
    FilterLookupForScope(*this, Previous, DC, S, NewFD->hasLinkage());

    if (isFriend) {
      // For now, claim that the objects have no previous declaration.
      if (FunctionTemplate) {
        FunctionTemplate->setObjectOfFriendDefn(false);
        FunctionTemplate->setAccess(AS_public);
      }
      NewFD->setObjectOfFriendDefn(false);
      NewFD->setAccess(AS_public);
    }

    if (isa<CXXMethodDefn>(NewFD) && DC == CurContext && IsFunctionDefinition) {
      // A method is implicitly inline if it's defined in its class
      // definition.
      NewFD->setImplicitlyInline();
    }

    if (SC == SC_Static && isa<CXXMethodDefn>(NewFD) &&
        !CurContext->isRecord()) {
      // C++ [class.static]p1:
      //   A data or function member of a class may be declared static
      //   in a class definition, in which case it is a static member of
      //   the class.

      // Complain about the 'static' specifier if it's on an out-of-line
      // member function definition.
      Diag(D.getDefnSpec().getStorageClassSpecLoc(),
           diag::err_static_out_of_line)
        << FixItHint::CreateRemoval(D.getDefnSpec().getStorageClassSpecLoc());
    }
  }
  
  // Handle GNU asm-label extension (encoded as an attribute).
  if (Expr *E = (Expr*) D.getAsmLabel()) {
    // The parser guarantees this is a string.
    StringLiteral *SE = cast<StringLiteral>(E);
    NewFD->addAttr(::new (Context) AsmLabelAttr(SE->getStrTokenLoc(0), Context,
                                                SE->getString()));
  }

  // Copy the parameter declarations from the declarator D to the function
  // declaration NewFD, if they are available.  First scavenge them into Params.
  llvm::SmallVector<ParmVarDefn*, 16> Params;
  if (D.isFunctionDeclarator()) {
    DeclaratorChunk::FunctionTypeInfo &FTI = D.getFunctionTypeInfo();

    // Check for C99 6.7.5.3p10 - foo(void) is a non-varargs
    // function that takes no arguments, not a function that takes a
    // single void argument.
    // We let through "const void" here because Sema::GetTypeForDeclarator
    // already checks for that case.
    if (FTI.NumArgs == 1 && !FTI.isVariadic && FTI.ArgInfo[0].Ident == 0 &&
        FTI.ArgInfo[0].Param &&
        cast<ParmVarDefn>(FTI.ArgInfo[0].Param)->getType()->isVoidType()) {
      // Empty arg list, don't push any params.
      ParmVarDefn *Param = cast<ParmVarDefn>(FTI.ArgInfo[0].Param);

      // In C++, the empty parameter-type-list must be spelled "void"; a
      // typedef of void is not permitted.
      if (getLangOptions().CPlusPlus &&
          Param->getType().getUnqualifiedType() != Context.VoidTy)
        Diag(Param->getLocation(), diag::err_param_typedef_of_void);
    } else if (FTI.NumArgs > 0 && FTI.ArgInfo[0].Param != 0) {
      for (unsigned i = 0, e = FTI.NumArgs; i != e; ++i) {
        ParmVarDefn *Param = cast<ParmVarDefn>(FTI.ArgInfo[i].Param);
        assert(Param->getDefnContext() != NewFD && "Was set before ?");
        Param->setDefnContext(NewFD);
        Params.push_back(Param);

        if (Param->isInvalidDefn())
          NewFD->setInvalidDefn();
      }
    }

  } else if (const FunctionProtoType *FT = R->getAs<FunctionProtoType>()) {
    // When we're declaring a function with a typedef, typeof, etc as in the
    // following example, we'll need to synthesize (unnamed)
    // parameters for use in the declaration.
    //
    // @code
    // typedef void fn(int);
    // fn f;
    // @endcode

    // Synthesize a parameter for each argument type.
    for (FunctionProtoType::arg_type_iterator AI = FT->arg_type_begin(),
         AE = FT->arg_type_end(); AI != AE; ++AI) {
      ParmVarDefn *Param =
        BuildParmVarDefnForTypedef(NewFD, D.getIdentifierLoc(), *AI);
      Params.push_back(Param);
    }
  } else {
    assert(R->isFunctionNoProtoType() && NewFD->getNumParams() == 0 &&
           "Should not need args for typedef of non-prototype fn");
  }
  // Finally, we know we have the right number of parameters, install them.
  NewFD->setParams(Params.data(), Params.size());

  bool OverloadableAttrRequired=false; // FIXME: HACK!
  if (!getLangOptions().CPlusPlus) {
    // Perform semantic checking on the function declaration.
    bool isExplctSpecialization=false;
    CheckFunctionDefinition(S, NewFD, Previous, isExplctSpecialization,
                             Redeclaration, 
                             /*FIXME:*/OverloadableAttrRequired);
    assert((NewFD->isInvalidDefn() || !Redeclaration ||
            Previous.getResultKind() != LookupResult::FoundOverloaded) &&
           "previous declaration set still overloaded");
  } else {
    // If the declarator is a template-id, translate the parser's template 
    // argument list into our AST format.
    bool HasExplicitTemplateArgs = false;
    TemplateArgumentListInfo TemplateArgs;
    if (D.getName().getKind() == UnqualifiedId::IK_TemplateId) {
      TemplateIdAnnotation *TemplateId = D.getName().TemplateId;
      TemplateArgs.setLAngleLoc(TemplateId->LAngleLoc);
      TemplateArgs.setRAngleLoc(TemplateId->RAngleLoc);
      ASTTemplateArgsPtr TemplateArgsPtr(*this,
                                         TemplateId->getTemplateArgs(),
                                         TemplateId->NumArgs);
      translateTemplateArguments(TemplateArgsPtr,
                                 TemplateArgs);
      TemplateArgsPtr.release();
    
      HasExplicitTemplateArgs = true;
    
      if (FunctionTemplate) {
        // FIXME: Diagnose function template with explicit template
        // arguments.
        HasExplicitTemplateArgs = false;
      } else if (!isFunctionTemplateSpecialization && 
                 !D.getDefnSpec().isFriendSpecified()) {
        // We have encountered something that the user meant to be a 
        // specialization (because it has explicitly-specified template
        // arguments) but that was not introduced with a "template<>" (or had
        // too few of them).
        Diag(D.getIdentifierLoc(), diag::err_template_spec_needs_header)
          << SourceRange(TemplateId->LAngleLoc, TemplateId->RAngleLoc)
          << FixItHint::CreateInsertion(
                                        D.getDefnSpec().getSourceRange().getBegin(),
                                                  "template<> ");
        isFunctionTemplateSpecialization = true;
      } else {
        // "friend void foo<>(int);" is an implicit specialization decl.
        isFunctionTemplateSpecialization = true;
      }
    } else if (isFriend && isFunctionTemplateSpecialization) {
      // This combination is only possible in a recovery case;  the user
      // wrote something like:
      //   template <> friend void foo(int);
      // which we're recovering from as if the user had written:
      //   friend void foo<>(int);
      // Go ahead and fake up a template id.
      HasExplicitTemplateArgs = true;
        TemplateArgs.setLAngleLoc(D.getIdentifierLoc());
      TemplateArgs.setRAngleLoc(D.getIdentifierLoc());
    }

    // If it's a friend (and only if it's a friend), it's possible
    // that either the specialized function type or the specialized
    // template is dependent, and therefore matching will fail.  In
    // this case, don't check the specialization yet.
    if (isFunctionTemplateSpecialization && isFriend &&
        (NewFD->getType()->isDependentType() || DC->isDependentContext())) {
      assert(HasExplicitTemplateArgs &&
             "friend function specialization without template args");
      if (CheckDependentFunctionTemplateSpecialization(NewFD, TemplateArgs,
                                                       Previous))
        NewFD->setInvalidDefn();
    } else if (isFunctionTemplateSpecialization) {
      if (CheckFunctionTemplateSpecialization(NewFD,
                                              (HasExplicitTemplateArgs ? &TemplateArgs : 0),
                                              Previous))
        NewFD->setInvalidDefn();
    } else if (isExplicitSpecialization && isa<CXXMethodDefn>(NewFD)) {
      if (CheckMemberSpecialization(NewFD, Previous))
          NewFD->setInvalidDefn();
    }

    // Perform semantic checking on the function declaration.
    bool flag_c_overloaded=false; // unused for c++
    CheckFunctionDefinition(S, NewFD, Previous, isExplicitSpecialization,
                             Redeclaration, /*FIXME:*/flag_c_overloaded);

    assert((NewFD->isInvalidDefn() || !Redeclaration ||
            Previous.getResultKind() != LookupResult::FoundOverloaded) &&
           "previous declaration set still overloaded");

    NamedDefn *PrincipalDefn = (FunctionTemplate
                                ? cast<NamedDefn>(FunctionTemplate)
                                : NewFD);

    if (isFriend && Redeclaration) {
      AccessSpecifier Access = AS_public;
      if (!NewFD->isInvalidDefn())
        Access = NewFD->getPreviousDefinition()->getAccess();

      NewFD->setAccess(Access);
      if (FunctionTemplate) FunctionTemplate->setAccess(Access);

      PrincipalDefn->setObjectOfFriendDefn(true);
    }

    if (NewFD->isOverloadedOperator() && !DC->isRecord() &&
        PrincipalDefn->isInIdentifierNamespace(Defn::IDNS_Ordinary))
      PrincipalDefn->setNonMemberOperator();

    // If we have a function template, check the template parameter
    // list. This will check and merge default template arguments.
    if (FunctionTemplate) {
      FunctionTemplateDefn *PrevTemplate = FunctionTemplate->getPreviousDefinition();
      CheckTemplateParameterList(FunctionTemplate->getTemplateParameters(),
                                 PrevTemplate? PrevTemplate->getTemplateParameters() : 0,
                                 D.getDefnSpec().isFriendSpecified()? TPC_FriendFunctionTemplate
                                                  : TPC_FunctionTemplate);
    }

    if (NewFD->isInvalidDefn()) {
      // Ignore all the rest of this.
    } else if (!Redeclaration) {
      // Fake up an access specifier if it's supposed to be a class member.
      if (isa<CXXRecordDefn>(NewFD->getDefnContext()))
        NewFD->setAccess(AS_public);

      // Qualified decls generally require a previous declaration.
      if (D.getCXXScopeSpec().isSet()) {
        // ...with the major exception of templated-scope or
        // dependent-scope friend declarations.

        // TODO: we currently also suppress this check in dependent
        // contexts because (1) the parameter depth will be off when
        // matching friend templates and (2) we might actually be
        // selecting a friend based on a dependent factor.  But there
        // are situations where these conditions don't apply and we
        // can actually do this check immediately.
        if (isFriend &&
            (NumMatchedTemplateParamLists ||
             D.getCXXScopeSpec().getScopeRep()->isDependent() ||
             CurContext->isDependentContext())) {
              // ignore these
            } else {
              // The user tried to provide an out-of-line definition for a
              // function that is a member of a class or namespace, but there
              // was no such member function declared (C++ [class.mfct]p2,
              // C++ [namespace.memdef]p2). For example:
              //
              // class X {
              //   void f() const;
              // };
              //
              // void X::f() { } // ill-formed
              //
              // Complain about this problem, and attempt to suggest close
              // matches (e.g., those that differ only in cv-qualifiers and
              // whether the parameter types are references).
              Diag(D.getIdentifierLoc(), diag::err_member_def_does_not_match)
              << Name << DC << D.getCXXScopeSpec().getRange();
              NewFD->setInvalidDefn();

              DiagnoseInvalidRedeclaration(*this, NewFD);
            }

        // Unqualified local friend declarations are required to resolve
        // to something.
        } else if (isFriend && cast<CXXRecordDefn>(CurContext)->isLocalClass()) {
          Diag(D.getIdentifierLoc(), diag::err_no_matching_local_friend);
          NewFD->setInvalidDefn();
          DiagnoseInvalidRedeclaration(*this, NewFD);
        }

    } else if (!IsFunctionDefinition && D.getCXXScopeSpec().isSet() &&
               !isFriend && !isFunctionTemplateSpecialization &&
               !isExplicitSpecialization) {
      // An out-of-line member function declaration must also be a
      // definition (C++ [dcl.meaning]p1).
      // Note that this is not the case for explicit specializations of
      // function templates or member functions of class templates, per
      // C++ [temp.expl.spec]p2. We also allow these declarations as an extension
      // for compatibility with old SWIG code which likes to generate them.
      Diag(NewFD->getLocation(), diag::ext_out_of_line_declaration)
        << D.getCXXScopeSpec().getRange();
    }
  }
  
  
  // Handle attributes. We need to have merged decls when handling attributes
  // (for example to check for conflicts, etc).
  // FIXME: This needs to happen before we merge declarations. Then,
  // let attribute merging cope with attribute conflicts.
  ProcessDefnAttributes(S, NewFD, D);

  // attributes declared post-definition are currently ignored
  // FIXME: This should happen during attribute merging
  if (Redeclaration && Previous.isSingleResult()) {
    const FunctionDefn *Def;
    FunctionDefn *PrevFD = dyn_cast<FunctionDefn>(Previous.getFoundDefn());
    if (PrevFD && PrevFD->hasBody(Def) && D.hasAttributes()) {
      Diag(NewFD->getLocation(), diag::warn_attribute_precede_definition);
      Diag(Def->getLocation(), diag::note_previous_definition);
    }
  }

  AddKnownFunctionAttributes(NewFD);

  if (OverloadableAttrRequired && !NewFD->hasAttr<OverloadableAttr>()) {
    // If a function name is overloadable in C, then every function
    // with that name must be marked "overloadable".
    Diag(NewFD->getLocation(), diag::err_attribute_overloadable_missing)
      << Redeclaration << NewFD;
    if (!Previous.empty())
      Diag(Previous.getRepresentativeDefn()->getLocation(),
           diag::note_attribute_overloadable_prev_overload);
    NewFD->addAttr(::new (Context) OverloadableAttr(SourceLocation(), Context));
  }

  if (NewFD->hasAttr<OverloadableAttr>() && 
      !NewFD->getType()->getAs<FunctionProtoType>()) {
    Diag(NewFD->getLocation(),
         diag::err_attribute_overloadable_no_prototype)
      << NewFD;

    // Turn this into a variadic function with no parameters.
    const FunctionType *FT = NewFD->getType()->getAs<FunctionType>();
    FunctionProtoType::ExtProtoInfo EPI;
    EPI.Variadic = true;
    EPI.ExtInfo = FT->getExtInfo();

    Type R = Context.getFunctionType(FT->getResultType(), 0, 0, EPI);
    NewFD->setType(R);
  }

  // If there's a #pragma GCC visibility in scope, and this isn't a class
  // member, set the visibility of this function.
  if (NewFD->getLinkage() == ExternalLinkage && !DC->isRecord())
    AddPushedVisibilityAttribute(NewFD);

  // If this is a locally-scoped extern C function, update the
  // map of such names.
  if (CurContext->isFunctionOrMethod() && NewFD->isExternC()
      && !NewFD->isInvalidDefn())
    RegisterLocallyScopedExternCDefn(NewFD, Previous, S);

  // Set this FunctionDefn's range up to the right paren.
  NewFD->setLocEnd(D.getSourceRange().getEnd());

  if (getLangOptions().CPlusPlus) {
    if (FunctionTemplate) {
      if (NewFD->isInvalidDefn())
        FunctionTemplate->setInvalidDefn();
      return FunctionTemplate;
    }
    CheckClassMemberNameAttributes(*this, NewFD);
  }

  MarkUnusedFileScopedDefn(NewFD);
  return NewFD;
}

/// \brief Perform semantic checking of a new function declaration.
///
/// Performs semantic analysis of the new function declaration
/// NewFD. This routine performs all semantic checking that does not
/// require the actual declarator involved in the declaration, and is
/// used both for the declaration of functions as they are parsed
/// (called via ActOnDeclarator) and for the declaration of functions
/// that have been instantiated via C++ template instantiation (called
/// via InstantiateDefn).
///
/// \param IsExplicitSpecialiation whether this new function declaration is
/// an explicit specialization of the previous declaration.
///
/// This sets NewFD->isInvalidDefn() to true if there was an error.
void Sema::CheckFunctionDefinition(Scope *S, FunctionDefn *NewFD,
                                    LookupResult &Previous,
                                    bool IsExplicitSpecialization,
                                    bool &Redeclaration,
                                    bool &OverloadableAttrRequired) {
  // If NewFD is already known erroneous, don't do any of this checking.
  if (NewFD->isInvalidDefn()) {
    // If this is a class member, mark the class invalid immediately.
    // This avoids some consistency errors later.
    if (isa<CXXMethodDefn>(NewFD))
      cast<CXXMethodDefn>(NewFD)->getParent()->setInvalidDefn();

    return;
  }

  if (NewFD->getResultType()->isVariablyModifiedType()) {
    // Functions returning a variably modified type violate C99 6.7.5.2p2
    // because all functions have linkage.
    Diag(NewFD->getLocation(), diag::err_vm_func_decl);
    return NewFD->setInvalidDefn();
  }

  if (NewFD->isMain()) 
    CheckMain(NewFD);

  // Check for a previous declaration of this name.
  if (Previous.empty() && NewFD->isExternC()) {
    // Since we did not find anything by this name and we're declaring
    // an extern "C" function, look for a non-visible extern "C"
    // declaration with the same name.
    llvm::DenseMap<DefinitionName, NamedDefn *>::iterator Pos
      = LocallyScopedExternalDefns.find(NewFD->getDefnName());
    if (Pos != LocallyScopedExternalDefns.end())
      Previous.addDefn(Pos->second);
  }

  // Merge or overload the declaration with an existing declaration of
  // the same name, if appropriate.
  if (!Previous.empty()) {
    // Determine whether NewFD is an overload of PrevDefn or
    // a declaration that requires merging. If it's an overload,
    // there's no more work to do here; we'll just add the new
    // function to the scope.

    NamedDefn *OldDefn = 0;
    if (!AllowOverloadingOfFunction(Previous, Context)) {
      Redeclaration = true;
      OldDefn = Previous.getFoundDefn();
    } else {
      if (!getLangOptions().CPlusPlus)
        OverloadableAttrRequired = true;

      switch (CheckOverload(S, NewFD, Previous, OldDefn,
                            /*NewIsUsingDefn*/ false)) {
      case Ovl_Match:
        Redeclaration = true;
        break;

      case Ovl_NonFunction:
        Redeclaration = true;
        break;

      case Ovl_Overload:
        Redeclaration = false;
        break;
      }
    }

    if (Redeclaration) {
      // NewFD and OldDefn represent declarations that need to be
      // merged.
      if (MergeFunctionDefn(NewFD, OldDefn))
        return NewFD->setInvalidDefn();

      Previous.clear();
      Previous.addDefn(OldDefn);

      if (FunctionTemplateDefn *OldTemplateDefn
                                    = dyn_cast<FunctionTemplateDefn>(OldDefn)) {
        NewFD->setPreviousDefinition(OldTemplateDefn->getTemplatedDefn());
        FunctionTemplateDefn *NewTemplateDefn
          = NewFD->getDescribedFunctionTemplate();
        assert(NewTemplateDefn && "Template/non-template mismatch");
        if (CXXMethodDefn *Method
              = dyn_cast<CXXMethodDefn>(NewTemplateDefn->getTemplatedDefn())) {
          Method->setAccess(OldTemplateDefn->getAccess());
          NewTemplateDefn->setAccess(OldTemplateDefn->getAccess());
        }
        
        // If this is an explicit specialization of a member that is a function
        // template, mark it as a member specialization.
        if (IsExplicitSpecialization && 
            NewTemplateDefn->getInstantiatedFromMemberTemplate()) {
          NewTemplateDefn->setMemberSpecialization();
          assert(OldTemplateDefn->isMemberSpecialization());
        }
      } else {
        if (isa<CXXMethodDefn>(NewFD)) // Set access for out-of-line definitions
          NewFD->setAccess(OldDefn->getAccess());
        NewFD->setPreviousDefinition(cast<FunctionDefn>(OldDefn));
      }
    }
  }

  // Semantic checking for this function declaration (in isolation).
  if (getLangOptions().CPlusPlus) {
    // C++-specific checks.
    if (CXXConstructorDefn *Constructor = dyn_cast<CXXConstructorDefn>(NewFD)) {
      CheckConstructor(Constructor);
    } else if (CXXDestructorDefn *Destructor =
                dyn_cast<CXXDestructorDefn>(NewFD)) {
      CXXRecordDefn *Record = Destructor->getParent();
      Type ClassType = Context.getTypeDefnType(Record);
      
      // FIXME: Shouldn't we be able to perform this check even when the class
      // type is dependent? Both gcc and edg can handle that.
      if (!ClassType->isDependentType()) {
        DefinitionName Name
          = Context.DefinitionNames.getCXXDestructorName(
                                        Context.getCanonicalType(ClassType));
        if (NewFD->getDefnName() != Name) {
          Diag(NewFD->getLocation(), diag::err_destructor_name);
          return NewFD->setInvalidDefn();
        }
      }
    } else if (CXXConversionDefn *Conversion
               = dyn_cast<CXXConversionDefn>(NewFD)) {
      ActOnConversionDeclarator(Conversion);
    }

    // Find any virtual functions that this function overrides.
    if (CXXMethodDefn *Method = dyn_cast<CXXMethodDefn>(NewFD)) {
      if (!Method->isFunctionTemplateSpecialization() && 
          !Method->getDescribedFunctionTemplate()) {
        if (AddOverriddenMethods(Method->getParent(), Method)) {
          // If the function was marked as "static", we have a problem.
          if (NewFD->getStorageClass() == SC_Static) {
            Diag(NewFD->getLocation(), diag::err_static_overrides_virtual)
              << NewFD->getDefnName();
            for (CXXMethodDefn::method_iterator
                      Overridden = Method->begin_overridden_methods(),
                   OverriddenEnd = Method->end_overridden_methods();
                 Overridden != OverriddenEnd;
                 ++Overridden) {
              Diag((*Overridden)->getLocation(), 
                   diag::note_overridden_virtual_function);
            }
          }
        }        
      }
    }

    // Extra checking for C++ overloaded operators (C++ [over.oper]).
    if (NewFD->isOverloadedOperator() &&
        CheckOverloadedOperatorDefinition(NewFD))
      return NewFD->setInvalidDefn();

    // Extra checking for C++0x literal operators (C++0x [over.literal]).
    if (NewFD->getLiteralIdentifier() &&
        CheckLiteralOperatorDefinition(NewFD))
      return NewFD->setInvalidDefn();

    // In C++, check default arguments now that we have merged decls. Unless
    // the lexical context is the class, because in this case this is done
    // during delayed parsing anyway.
    if (!CurContext->isRecord())
      CheckCXXDefaultArguments(NewFD);
    
    // If this function declares a builtin function, check the type of this
    // declaration against the expected type for the builtin. 
    if (unsigned BuiltinID = NewFD->getBuiltinID()) {
      ASTContext::GetBuiltinTypeError Error;
      Type T = Context.GetBuiltinFunctionType(BuiltinID, Error);
      if (!T.isNull() && !Context.hasSameType(T, NewFD->getType())) {
        // The type of this function differs from the type of the builtin,
        // so forget about the builtin entirely.
        Context.BuiltinInfo.ForgetBuiltin(BuiltinID, Context.Idents);
      }
    }
  }
}

void Sema::CheckMain(FunctionDefn* FD) {
  // C++ [basic.start.main]p3:  A program that declares main to be inline
  //   or static is ill-formed.
  // C99 6.7.4p4:  In a hosted environment, the inline function specifier
  //   shall not appear in a declaration of main.
  // static main is not an error under C99, but we should warn about it.
  bool isInline = FD->isInlineSpecified();
  bool isStatic = FD->getStorageClass() == SC_Static;
  if (isInline || isStatic) {
    unsigned diagID = diag::warn_unusual_main_decl;
    if (isInline || getLangOptions().CPlusPlus)
      diagID = diag::err_unusual_main_decl;

    int which = isStatic + (isInline << 1) - 1;
    Diag(FD->getLocation(), diagID) << which;
  }

  Type T = FD->getType();
  assert(T->isFunctionType() && "function decl is not of function type");
  const FunctionType* FT = T->getAs<FunctionType>();

  if (!Context.hasSameUnqualifiedType(FT->getResultType(), Context.IntTy)) {
    TypeSourceInfo *TSI = FD->getTypeSourceInfo();
    TypeLoc TL = TSI->getTypeLoc().IgnoreParens();
    const SemaDiagnosticBuilder& D = Diag(FD->getTypeSpecStartLoc(),
                                          diag::err_main_returns_nonint);
    if (FunctionTypeLoc* PTL = dyn_cast<FunctionTypeLoc>(&TL)) {
      D << FixItHint::CreateReplacement(PTL->getResultLoc().getSourceRange(),
                                        "int");
    }
    FD->setInvalidDefn(true);
  }

  // Treat protoless main() as nullary.
  if (isa<FunctionNoProtoType>(FT)) return;

  const FunctionProtoType* FTP = cast<const FunctionProtoType>(FT);
  unsigned nparams = FTP->getNumArgs();
  assert(FD->getNumParams() == nparams);

  bool HasExtraParameters = (nparams > 3);

  // Darwin passes an undocumented fourth argument of type char**.  If
  // other platforms start sprouting these, the logic below will start
  // getting shifty.
  if (nparams == 4 &&
      Context.Target.getTriple().getOS() == llvm::Triple::Darwin)
    HasExtraParameters = false;

  if (HasExtraParameters) {
    Diag(FD->getLocation(), diag::err_main_surplus_args) << nparams;
    FD->setInvalidDefn(true);
    nparams = 3;
  }

  // FIXME: a lot of the following diagnostics would be improved
  // if we had some location information about types.

  Type CharPP =
    Context.getPointerType(Context.getPointerType(Context.CharTy));
  Type Expected[] = { Context.IntTy, CharPP, CharPP, CharPP };

  for (unsigned i = 0; i < nparams; ++i) {
    Type AT = FTP->getArgType(i);

    bool mismatch = true;

    if (Context.hasSameUnqualifiedType(AT, Expected[i]))
      mismatch = false;
    else if (Expected[i] == CharPP) {
      // As an extension, the following forms are okay:
      //   char const **
      //   char const * const *
      //   char * const *

      QualifierCollector qs;
      const PointerType* PT;
      if ((PT = qs.strip(AT)->getAs<PointerType>()) &&
          (PT = qs.strip(PT->getPointeeType())->getAs<PointerType>()) &&
          (Type(qs.strip(PT->getPointeeType()), 0) == Context.CharTy)) {
        qs.removeConst();
        mismatch = !qs.empty();
      }
    }

    if (mismatch) {
      Diag(FD->getLocation(), diag::err_main_arg_wrong) << i << Expected[i];
      // TODO: suggest replacing given type with expected type
      FD->setInvalidDefn(true);
    }
  }

  if (nparams == 1 && !FD->isInvalidDefn()) {
    Diag(FD->getLocation(), diag::warn_main_one_arg);
  }
  
  if (!FD->isInvalidDefn() && FD->getDescribedFunctionTemplate()) {
    Diag(FD->getLocation(), diag::err_main_template_decl);
    FD->setInvalidDefn();
  }
}

bool Sema::CheckForConstantInitializer(Expr *Init, Type DclT) {
  // FIXME: Need strict checking.  In C89, we need to check for
  // any assignment, increment, decrement, function-calls, or
  // commas outside of a sizeof.  In C99, it's the same list,
  // except that the aforementioned are allowed in unevaluated
  // expressions.  Everything else falls under the
  // "may accept other forms of constant expressions" exception.
  // (We never end up here for C++, so the constant expression
  // rules there don't matter.)
  if (Init->isConstantInitializer(Context, false))
    return false;
  Diag(Init->getExprLoc(), diag::err_init_element_not_constant)
    << Init->getSourceRange();
  return true;
}

void Sema::AddInitializerToDefn(Defn *dcl, Expr *init) {
  AddInitializerToDefn(dcl, init, /*DirectInit=*/false);
}

/// AddInitializerToDefn - Adds the initializer Init to the
/// declaration dcl. If DirectInit is true, this is C++ direct
/// initialization rather than copy initialization.
void Sema::AddInitializerToDefn(Defn *RealDefn, Expr *Init, bool DirectInit) {
  // If there is no declaration, there was an error parsing it.  Just ignore
  // the initializer.
  if (RealDefn == 0)
    return;

  if (CXXMethodDefn *Method = dyn_cast<CXXMethodDefn>(RealDefn)) {
    // With declarators parsed the way they are, the parser cannot
    // distinguish between a normal initializer and a pure-specifier.
    // Thus this grotesque test.
    IntegerLiteral *IL;
    if ((IL = dyn_cast<IntegerLiteral>(Init)) && IL->getValue() == 0 &&
        Context.getCanonicalType(IL->getType()) == Context.IntTy)
      CheckPureMethod(Method, Init->getSourceRange());
    else {
      Diag(Method->getLocation(), diag::err_member_function_initialization)
        << Method->getDefnName() << Init->getSourceRange();
      Method->setInvalidDefn();
    }
    return;
  }

  VarDefn *VDefn = dyn_cast<VarDefn>(RealDefn);
  if (!VDefn) {
    if (getLangOptions().CPlusPlus &&
        RealDefn->getLexicalDefnContext()->isRecord() &&
        isa<NamedDefn>(RealDefn))
      Diag(RealDefn->getLocation(), diag::err_member_initialization);
    else
      Diag(RealDefn->getLocation(), diag::err_illegal_initializer);
    RealDefn->setInvalidDefn();
    return;
  }

  

  // A definition must end up with a complete type, which means it must be
  // complete with the restriction that an array type might be completed by the
  // initializer; note that later code assumes this restriction.
  Type BaseDefnType = VDefn->getType();
  if (const ArrayType *Array = Context.getAsIncompleteArrayType(BaseDefnType))
    BaseDefnType = Array->getElementType();
  if (RequireCompleteType(VDefn->getLocation(), BaseDefnType,
                          diag::err_typecheck_decl_incomplete_type)) {
    RealDefn->setInvalidDefn();
    return;
  }

  // The variable can not have an abstract class type.
  if (RequireNonAbstractType(VDefn->getLocation(), VDefn->getType(),
                             diag::err_abstract_type_in_decl,
                             AbstractVariableType))
    VDefn->setInvalidDefn();

  const VarDefn *Def;
  if ((Def = VDefn->getDefinition()) && Def != VDefn) {
    Diag(VDefn->getLocation(), diag::err_redefinition)
      << VDefn->getDefnName();
    Diag(Def->getLocation(), diag::note_previous_definition);
    VDefn->setInvalidDefn();
    return;
  }
  
  const VarDefn* PrevInit = 0;
  if (getLangOptions().CPlusPlus) {
    // C++ [class.static.data]p4
    //   If a static data member is of const integral or const
    //   enumeration type, its declaration in the class definition can
    //   specify a constant-initializer which shall be an integral
    //   constant expression (5.19). In that case, the member can appear
    //   in integral constant expressions. The member shall still be
    //   defined in a namespace scope if it is used in the program and the
    //   namespace scope definition shall not contain an initializer.
    //
    // We already performed a redefinition check above, but for static
    // data members we also need to check whether there was an in-class
    // declaration with an initializer.
    if (VDefn->isStaticDataMember() && VDefn->getAnyInitializer(PrevInit)) {
      Diag(VDefn->getLocation(), diag::err_redefinition) << VDefn->getDefnName();
      Diag(PrevInit->getLocation(), diag::note_previous_definition);
      return;
    }  

    if (VDefn->hasLocalStorage())
      getCurFunction()->setHasBranchProtectedScope();

    if (DiagnoseUnexpandedParameterPack(Init, UPPC_Initializer)) {
      VDefn->setInvalidDefn();
      return;
    }
  }

  // Capture the variable that is being initialized and the style of
  // initialization.
  InitializedEntity Entity = InitializedEntity::InitializeVariable(VDefn);
  
  // FIXME: Poor source location information.
  InitializationKind Kind
    = DirectInit? InitializationKind::CreateDirect(VDefn->getLocation(),
                                                   Init->getLocStart(),
                                                   Init->getLocEnd())
                : InitializationKind::CreateCopy(VDefn->getLocation(),
                                                 Init->getLocStart());
  
  // Get the decls type and save a reference for later, since
  // CheckInitializerTypes may change it.
  Type DclT = VDefn->getType(), SavT = DclT;
  if (VDefn->isLocalVarDefn()) {
    if (VDefn->hasExternalStorage()) { // C99 6.7.8p5
      Diag(VDefn->getLocation(), diag::err_block_extern_cant_init);
      VDefn->setInvalidDefn();
    } else if (!VDefn->isInvalidDefn()) {
      InitializationSequence InitSeq(*this, Entity, Kind, &Init, 1);
      ExprResult Result = InitSeq.Perform(*this, Entity, Kind,
                                                MultiExprArg(*this, &Init, 1),
                                                &DclT);
      if (Result.isInvalid()) {
        VDefn->setInvalidDefn();
        return;
      }

      Init = Result.takeAs<Expr>();

      // C++ 3.6.2p2, allow dynamic initialization of static initializers.
      // Don't check invalid declarations to avoid emitting useless diagnostics.
      if (!getLangOptions().CPlusPlus && !VDefn->isInvalidDefn()) {
        if (VDefn->getStorageClass() == SC_Static) // C99 6.7.8p4.
          CheckForConstantInitializer(Init, DclT);
      }
    }
  } else if (VDefn->isStaticDataMember() &&
             VDefn->getLexicalDefnContext()->isRecord()) {
    // This is an in-class initialization for a static data member, e.g.,
    //
    // struct S {
    //   static const int value = 17;
    // };

    // Try to perform the initialization regardless.
    if (!VDefn->isInvalidDefn()) {
      InitializationSequence InitSeq(*this, Entity, Kind, &Init, 1);
      ExprResult Result = InitSeq.Perform(*this, Entity, Kind,
                                          MultiExprArg(*this, &Init, 1),
                                          &DclT);
      if (Result.isInvalid()) {
        VDefn->setInvalidDefn();
        return;
      }

      Init = Result.takeAs<Expr>();
    }

    // C++ [class.mem]p4:
    //   A member-declarator can contain a constant-initializer only
    //   if it declares a static member (9.4) of const integral or
    //   const enumeration type, see 9.4.2.
    Type T = VDefn->getType();

    // Do nothing on dependent types.
    if (T->isDependentType()) {

    // Require constness.
    } else if (!T.isConstQualified()) {
      Diag(VDefn->getLocation(), diag::err_in_class_initializer_non_const)
        << Init->getSourceRange();
      VDefn->setInvalidDefn();

    // We allow integer constant expressions in all cases.
    } else if (T->isIntegralOrEnumerationType()) {
      if (!Init->isValueDependent()) {
        // Check whether the expression is a constant expression.
        llvm::APSInt Value;
        SourceLocation Loc;
        if (!Init->isIntegerConstantExpr(Value, Context, &Loc)) {
          Diag(Loc, diag::err_in_class_initializer_non_constant)
            << Init->getSourceRange();
          VDefn->setInvalidDefn();
        }
      }

    // We allow floating-point constants as an extension in C++03, and
    // C++0x has far more complicated rules that we don't really
    // implement fully.
    } else {
      bool Allowed = false;
      if (getLangOptions().CPlusPlus0x) {
        Allowed = T->isLiteralType();
      } else if (T->isFloatingType()) { // also permits complex, which is ok
        Diag(VDefn->getLocation(), diag::ext_in_class_initializer_float_type)
          << T << Init->getSourceRange();
        Allowed = true;
      }

      if (!Allowed) {
        Diag(VDefn->getLocation(), diag::err_in_class_initializer_bad_type)
          << T << Init->getSourceRange();
        VDefn->setInvalidDefn();

      // TODO: there are probably expressions that pass here that shouldn't.
      } else if (!Init->isValueDependent() &&
                 !Init->isConstantInitializer(Context, false)) {
        Diag(Init->getExprLoc(), diag::err_in_class_initializer_non_constant)
          << Init->getSourceRange();
        VDefn->setInvalidDefn();
      }
    }
  } else if (VDefn->isFileVarDefn()) {
    if (VDefn->getStorageClassAsWritten() == SC_Extern &&
        (!getLangOptions().CPlusPlus || 
         !Context.getBaseElementType(VDefn->getType()).isConstQualified()))
      Diag(VDefn->getLocation(), diag::warn_extern_init);
    if (!VDefn->isInvalidDefn()) {
      InitializationSequence InitSeq(*this, Entity, Kind, &Init, 1);
      ExprResult Result = InitSeq.Perform(*this, Entity, Kind,
                                                MultiExprArg(*this, &Init, 1),
                                                &DclT);
      if (Result.isInvalid()) {
        VDefn->setInvalidDefn();
        return;
      }

      Init = Result.takeAs<Expr>();
    }

    // C++ 3.6.2p2, allow dynamic initialization of static initializers.
    // Don't check invalid declarations to avoid emitting useless diagnostics.
    if (!getLangOptions().CPlusPlus && !VDefn->isInvalidDefn()) {
      // C99 6.7.8p4. All file scoped initializers need to be constant.
      CheckForConstantInitializer(Init, DclT);
    }
  }
  // If the type changed, it means we had an incomplete type that was
  // completed by the initializer. For example:
  //   int ary[] = { 1, 3, 5 };
  // "ary" transitions from a VariableArrayType to a ConstantArrayType.
  if (!VDefn->isInvalidDefn() && (DclT != SavT)) {
    VDefn->setType(DclT);
    Init->setType(DclT);
  }

  
  // If this variable is a local declaration with record type, make sure it
  // doesn't have a flexible member initialization.  We only support this as a
  // global/static definition.
  if (VDefn->hasLocalStorage())
    if (const HeteroContainerType *RT = VDefn->getType()->getAs<HeteroContainerType>())
      if (RT->getDefn()->hasFlexibleArrayMember()) {
        // Check whether the initializer tries to initialize the flexible
        // array member itself to anything other than an empty initializer list.
        if (InitListExpr *ILE = dyn_cast<InitListExpr>(Init)) {
          unsigned Index = std::distance(RT->getDefn()->field_begin(),
                                         RT->getDefn()->field_end()) - 1;
          if (Index < ILE->getNumInits() &&
              !(isa<InitListExpr>(ILE->getInit(Index)) &&
                cast<InitListExpr>(ILE->getInit(Index))->getNumInits() == 0)) {
            Diag(VDefn->getLocation(), diag::err_nonstatic_flexible_variable);
            VDefn->setInvalidDefn();
          }
        }
      }
  
  // Check any implicit conversions within the expression.
  CheckImplicitConversions(Init, VDefn->getLocation());

  Init = MaybeCreateExprWithCleanups(Init);
  // Attach the initializer to the decl.
  VDefn->setInit(Init);

  if (getLangOptions().CPlusPlus) {
    if (!VDefn->isInvalidDefn() &&
        !VDefn->getDefnContext()->isDependentContext() &&
        VDefn->hasGlobalStorage() && !VDefn->isStaticLocal() &&
        !Init->isConstantInitializer(Context,
                                     VDefn->getType()->isReferenceType()))
      Diag(VDefn->getLocation(), diag::warn_global_constructor)
        << Init->getSourceRange();

    // Make sure we mark the destructor as used if necessary.
    Type InitType = VDefn->getType();
    while (const ArrayType *Array = Context.getAsArrayType(InitType))
      InitType = Context.getBaseElementType(Array);
    if (const HeteroContainerType *Record = InitType->getAs<HeteroContainerType>())
      FinalizeVarWithDestructor(VDefn, Record);
  }

  return;
}

/// ActOnInitializerError - Given that there was an error parsing an
/// initializer for the given declaration, try to return to some form
/// of sanity.
void Sema::ActOnInitializerError(Defn *D) {
  // Our main concern here is re-establishing invariants like "a
  // variable's type is either dependent or complete".
  if (!D || D->isInvalidDefn()) return;

  VarDefn *VD = dyn_cast<VarDefn>(D);
  if (!VD) return;

  Type Ty = VD->getType();
  if (Ty->isDependentType()) return;

  // Require a complete type.
  if (RequireCompleteType(VD->getLocation(), 
                          Context.getBaseElementType(Ty),
                          diag::err_typecheck_decl_incomplete_type)) {
    VD->setInvalidDefn();
    return;
  }

  // Require an abstract type.
  if (RequireNonAbstractType(VD->getLocation(), Ty,
                             diag::err_abstract_type_in_decl,
                             AbstractVariableType)) {
    VD->setInvalidDefn();
    return;
  }

  // Don't bother complaining about constructors or destructors,
  // though.
}

void Sema::ActOnUninitializedDefn(Defn *RealDefn,
                                  bool TypeContainsUndeducedAuto) {
  // If there is no declaration, there was an error parsing it. Just ignore it.
  if (RealDefn == 0)
    return;

  if (VarDefn *Var = dyn_cast<VarDefn>(RealDefn)) {
    Type Type = Var->getType();

    // C++0x [dcl.spec.auto]p3
    if (TypeContainsUndeducedAuto) {
      Diag(Var->getLocation(), diag::err_auto_var_requires_init)
        << Var->getDefnName() << Type;
      Var->setInvalidDefn();
      return;
    }

    switch (Var->isThisDefinitionADefinition()) {
    case VarDefn::Definition:
      if (!Var->isStaticDataMember() || !Var->getAnyInitializer())
        break;

      // We have an out-of-line definition of a static data member
      // that has an in-class initializer, so we type-check this like
      // a declaration. 
      //
      // Fall through
      
    case VarDefn::DefinitionOnly:
      // It's only a declaration. 

      // Block scope. C99 6.7p7: If an identifier for an object is
      // declared with no linkage (C99 6.2.2p6), the type for the
      // object shall be complete.
      if (!Type->isDependentType() && Var->isLocalVarDefn() &&
          !Var->getLinkage() && !Var->isInvalidDefn() &&
          RequireCompleteType(Var->getLocation(), Type,
                              diag::err_typecheck_decl_incomplete_type))
        Var->setInvalidDefn();

      // Make sure that the type is not abstract.
      if (!Type->isDependentType() && !Var->isInvalidDefn() &&
          RequireNonAbstractType(Var->getLocation(), Type,
                                 diag::err_abstract_type_in_decl,
                                 AbstractVariableType))
        Var->setInvalidDefn();
      return;

    case VarDefn::TentativeDefinition:
      // File scope. C99 6.9.2p2: A declaration of an identifier for an
      // object that has file scope without an initializer, and without a
      // storage-class specifier or with the storage-class specifier "static",
      // constitutes a tentative definition. Note: A tentative definition with
      // external linkage is valid (C99 6.2.2p5).
      if (!Var->isInvalidDefn()) {
        if (const IncompleteArrayType *ArrayT
                                    = Context.getAsIncompleteArrayType(Type)) {
          if (RequireCompleteType(Var->getLocation(),
                                  ArrayT->getElementType(),
                                  diag::err_illegal_decl_array_incomplete_type))
            Var->setInvalidDefn();
        } else if (Var->getStorageClass() == SC_Static) {
          // C99 6.9.2p3: If the declaration of an identifier for an object is
          // a tentative definition and has internal linkage (C99 6.2.2p3), the
          // declared type shall not be an incomplete type.
          // NOTE: code such as the following
          //     static struct s;
          //     struct s { int a; };
          // is accepted by gcc. Hence here we issue a warning instead of
          // an error and we do not invalidate the static declaration.
          // NOTE: to avoid multiple warnings, only check the first declaration.
          if (Var->getPreviousDefinition() == 0)
            RequireCompleteType(Var->getLocation(), Type,
                                diag::ext_typecheck_decl_incomplete_type);
        }
      }

      // Record the tentative definition; we're done.
      if (!Var->isInvalidDefn())
        TentativeDefinitions.push_back(Var);
      return;
    }

    // Provide a specific diagnostic for uninitialized variable
    // definitions with incomplete array type.
    if (Type->isIncompleteArrayType()) {
      Diag(Var->getLocation(),
           diag::err_typecheck_incomplete_array_needs_initializer);
      Var->setInvalidDefn();
      return;
    }

    // Provide a specific diagnostic for uninitialized variable
    // definitions with reference type.
    if (Type->isReferenceType()) {
      Diag(Var->getLocation(), diag::err_reference_var_requires_init)
        << Var->getDefnName()
        << SourceRange(Var->getLocation(), Var->getLocation());
      Var->setInvalidDefn();
      return;
    }

    // Do not attempt to type-check the default initializer for a
    // variable with dependent type.
    if (Type->isDependentType())
      return;

    if (Var->isInvalidDefn())
      return;

    if (RequireCompleteType(Var->getLocation(), 
                            Context.getBaseElementType(Type),
                            diag::err_typecheck_decl_incomplete_type)) {
      Var->setInvalidDefn();
      return;
    }

    // The variable can not have an abstract class type.
    if (RequireNonAbstractType(Var->getLocation(), Type,
                               diag::err_abstract_type_in_decl,
                               AbstractVariableType)) {
      Var->setInvalidDefn();
      return;
    }

    const HeteroContainerType *Record
      = Context.getBaseElementType(Type)->getAs<HeteroContainerType>();
    if (Record && getLangOptions().CPlusPlus && !getLangOptions().CPlusPlus0x &&
        cast<CXXRecordDefn>(Record->getDefn())->isPOD()) {
      // C++03 [dcl.init]p9:
      //   If no initializer is specified for an object, and the
      //   object is of (possibly cv-qualified) non-POD class type (or
      //   array thereof), the object shall be default-initialized; if
      //   the object is of const-qualified type, the underlying class
      //   type shall have a user-declared default
      //   constructor. Otherwise, if no initializer is specified for
      //   a non- static object, the object and its subobjects, if
      //   any, have an indeterminate initial value); if the object
      //   or any of its subobjects are of const-qualified type, the
      //   program is ill-formed.
      // FIXME: DPG thinks it is very fishy that C++0x disables this.
    } else {
      // Check for jumps past the implicit initializer.  C++0x
      // clarifies that this applies to a "variable with automatic
      // storage duration", not a "local variable".
      if (getLangOptions().CPlusPlus && Var->hasLocalStorage())
        getCurFunction()->setHasBranchProtectedScope();

      InitializedEntity Entity = InitializedEntity::InitializeVariable(Var);
      InitializationKind Kind
        = InitializationKind::CreateDefault(Var->getLocation());
    
      InitializationSequence InitSeq(*this, Entity, Kind, 0, 0);
      ExprResult Init = InitSeq.Perform(*this, Entity, Kind,
                                        MultiExprArg(*this, 0, 0));
      if (Init.isInvalid())
        Var->setInvalidDefn();
      else if (Init.get()) {
        Var->setInit(MaybeCreateExprWithCleanups(Init.get()));

        if (getLangOptions().CPlusPlus && !Var->isInvalidDefn() &&
            Var->hasGlobalStorage() && !Var->isStaticLocal() &&
            !Var->getDefnContext()->isDependentContext() &&
            !Var->getInit()->isConstantInitializer(Context, false))
          Diag(Var->getLocation(), diag::warn_global_constructor);
      }
    }

    if (!Var->isInvalidDefn() && getLangOptions().CPlusPlus && Record)
      FinalizeVarWithDestructor(Var, Record);
  }
}

Sema::DefnGroupPtrTy
Sema::FinalizeDeclaratorGroup(Scope *S, const DefnSpec &DS,
                              Defn **Group, unsigned NumDefns) {
  llvm::SmallVector<Defn*, 8> Defns;

  if (DS.isTypeSpecOwned())
    Defns.push_back(DS.getRepAsDefn());

  for (unsigned i = 0; i != NumDefns; ++i)
    if (Defn *D = Group[i])
      Defns.push_back(D);

  return DefnGroupPtrTy::make(DefnGroupRef::Create(Context,
                                                   Defns.data(), Defns.size()));
}


/// ActOnParamDeclarator - Called from Parser::ParseFunctionDeclarator()
/// to introduce parameters into function prototype scope.
Defn *Sema::ActOnParamDeclarator(Scope *S, Declarator &D) {
  const DefnSpec &DS = D.getDefnSpec();

  // Verify C99 6.7.5.3p2: The only SCS allowed is 'register'.
  VarDefn::StorageClass StorageClass = SC_None;
  VarDefn::StorageClass StorageClassAsWritten = SC_None;
  if (DS.getStorageClassSpec() == DefnSpec::SCS_register) {
    StorageClass = SC_Register;
    StorageClassAsWritten = SC_Register;
  } else if (DS.getStorageClassSpec() != DefnSpec::SCS_unspecified) {
    Diag(DS.getStorageClassSpecLoc(),
         diag::err_invalid_storage_class_in_func_decl);
    D.getMutableDefnSpec().ClearStorageClassSpecs();
  }

  if (D.getDefnSpec().isThreadSpecified())
    Diag(D.getDefnSpec().getThreadSpecLoc(), diag::err_invalid_thread);

  DiagnoseFunctionSpecifiers(D);

  TagDefn *OwnedDefn = 0;
  TypeSourceInfo *TInfo = GetTypeForDeclarator(D, S, &OwnedDefn);
  Type parmDefnType = TInfo->getType();

  if (getLangOptions().CPlusPlus) {
    // Check that there are no default arguments inside the type of this
    // parameter.
    CheckExtraCXXDefaultArguments(D);
     
    if (OwnedDefn && OwnedDefn->isDefinition()) {
      // C++ [dcl.fct]p6:
      //   Types shall not be defined in return or parameter types.
      Diag(OwnedDefn->getLocation(), diag::err_type_defined_in_param_type)
        << Context.getTypeDefnType(OwnedDefn);
    }
    
    // Parameter declarators cannot be qualified (C++ [dcl.meaning]p1).
    if (D.getCXXScopeSpec().isSet()) {
      Diag(D.getIdentifierLoc(), diag::err_qualified_param_declarator)
        << D.getCXXScopeSpec().getRange();
      D.getCXXScopeSpec().clear();
    }

    // FIXME: Variadic templates.
    if (D.hasEllipsis()) {
      Diag(D.getEllipsisLoc(), diag::err_function_parameter_pack_unsupported);
      D.setInvalidType();
    }
  }

  // Ensure we have a valid name
  IdentifierInfo *II = 0;
  if (D.hasName()) {
    II = D.getIdentifier();
    if (!II) {
      Diag(D.getIdentifierLoc(), diag::err_bad_parameter_name)
        << GetNameForDeclarator(D).getName().getAsString();
      D.setInvalidType(true);
    }
  }

  // Check for redeclaration of parameters, e.g. int foo(int x, int x);
  if (II) {
    LookupResult R(*this, II, D.getIdentifierLoc(), LookupOrdinaryName,
                   ForRedefinition);
    LookupName(R, S);
    if (R.isSingleResult()) {
      NamedDefn *PrevDefn = R.getFoundDefn();
      if (PrevDefn->isTemplateParameter()) {
        // Maybe we will complain about the shadowed template parameter.
        DiagnoseTemplateParameterShadow(D.getIdentifierLoc(), PrevDefn);
        // Just pretend that we didn't see the previous declaration.
        PrevDefn = 0;
      } else if (S->isDefnScope(PrevDefn)) {
        Diag(D.getIdentifierLoc(), diag::err_param_redefinition) << II;
        Diag(PrevDefn->getLocation(), diag::note_previous_declaration);

        // Recover by removing the name
        II = 0;
        D.SetIdentifier(0, D.getIdentifierLoc());
        D.setInvalidType(true);
      }
    }
  }

  // Temporarily put parameter variables in the translation unit, not
  // the enclosing context.  This prevents them from accidentally
  // looking like class members in C++.
  ParmVarDefn *New = CheckParameter(Context.getTranslationUnitDefn(),
                                    TInfo, parmDefnType, II,
                                    D.getIdentifierLoc(),
                                    StorageClass, StorageClassAsWritten);

  if (D.isInvalidType())
    New->setInvalidDefn();
  
  // Add the parameter declaration into this scope.
  S->AddDefn(New);
  if (II)
    IdResolver.AddDefn(New);

  ProcessDefnAttributes(S, New, D);

  if (New->hasAttr<BlocksAttr>()) {
    Diag(New->getLocation(), diag::err_block_on_nonlocal);
  }
  return New;
}

/// \brief Synthesizes a variable for a parameter arising from a
/// typedef.
ParmVarDefn *Sema::BuildParmVarDefnForTypedef(DefnContext *DC,
                                              SourceLocation Loc,
                                              Type T) {
  ParmVarDefn *Param = ParmVarDefn::Create(Context, DC, Loc, 0,
                                T, Context.getTrivialTypeSourceInfo(T, Loc),
                                           SC_None, SC_None, 0);
  Param->setImplicit();
  return Param;
}

void Sema::DiagnoseUnusedParameters(ParmVarDefn * const *Param,
                                    ParmVarDefn * const *ParamEnd) {
  // Don't diagnose unused-parameter errors in template instantiations; we
  // will already have done so in the template itself.
  if (!ActiveTemplateInstantiations.empty())
    return;

  for (; Param != ParamEnd; ++Param) {
    if (!(*Param)->isUsed() && (*Param)->getDefnName() &&
        !(*Param)->hasAttr<UnusedAttr>()) {
      Diag((*Param)->getLocation(), diag::warn_unused_parameter)
        << (*Param)->getDefnName();
    }
  }
}

void Sema::DiagnoseSizeOfParametersAndReturnValue(ParmVarDefn * const *Param,
                                                  ParmVarDefn * const *ParamEnd,
                                                  Type ReturnTy,
                                                  NamedDefn *D) {
  if (LangOpts.NumLargeByValueCopy == 0) // No check.
    return;

  // Warn if the return value is pass-by-value and larger than the specified
  // threshold.
  if (ReturnTy->isPODType()) {
    unsigned Size = Context.getTypeSizeInChars(ReturnTy).getQuantity();
    if (Size > LangOpts.NumLargeByValueCopy)
      Diag(D->getLocation(), diag::warn_return_value_size)
          << D->getDefnName() << Size;
  }

  // Warn if any parameter is pass-by-value and larger than the specified
  // threshold.
  for (; Param != ParamEnd; ++Param) {
    Type T = (*Param)->getType();
    if (!T->isPODType())
      continue;
    unsigned Size = Context.getTypeSizeInChars(T).getQuantity();
    if (Size > LangOpts.NumLargeByValueCopy)
      Diag((*Param)->getLocation(), diag::warn_parameter_size)
          << (*Param)->getDefnName() << Size;
  }
}

ParamVarDefn *Sema::CheckParameter(DefnContext *DC,
		                               Type T,
                                  IdentifierInfo *Name,
                                  SourceLocation NameLoc,
                                  VarDefn::StorageClass StorageClass,
                                  VarDefn::StorageClass StorageClassAsWritten) {
  return NULL;
}

void Sema::ActOnFinishKNRParamDefinitions(Scope *S, Declarator &D,
                                           SourceLocation LocAfterDefns) {
  DeclaratorChunk::FunctionTypeInfo &FTI = D.getFunctionTypeInfo();

  // Verify 6.9.1p6: 'every identifier in the identifier list shall be declared'
  // for a K&R function.
  if (!FTI.hasPrototype) {
    for (int i = FTI.NumArgs; i != 0; /* decrement in loop */) {
      --i;
      if (FTI.ArgInfo[i].Param == 0) {
        llvm::SmallString<256> Code;
        llvm::raw_svector_ostream(Code) << "  int "
                                        << FTI.ArgInfo[i].Ident->getName()
                                        << ";\n";
        Diag(FTI.ArgInfo[i].IdentLoc, diag::ext_param_not_declared)
          << FTI.ArgInfo[i].Ident
          << FixItHint::CreateInsertion(LocAfterDefns, Code.str());

        // Implicitly declare the argument as type 'int' for lack of a better
        // type.
        DefnSpec DS;
        const char* PrevSpec; // unused
        unsigned DiagID; // unused
        DS.SetTypeSpecType(DefnSpec::TST_int, FTI.ArgInfo[i].IdentLoc,
                           PrevSpec, DiagID);
        Declarator ParamD(DS, Declarator::KNRTypeListContext);
        ParamD.SetIdentifier(FTI.ArgInfo[i].Ident, FTI.ArgInfo[i].IdentLoc);
        FTI.ArgInfo[i].Param = ActOnParamDeclarator(S, ParamD);
      }
    }
  }
}

static bool ShouldWarnAboutMissingPrototype(const FunctionDefn *FD) {
  // Don't warn about invalid declarations.
  if (FD->isInvalidDefn())
    return false;

  // Or declarations that aren't global.
  if (!FD->isGlobal())
    return false;

  // Don't warn about C++ member functions.
  if (isa<CXXMethodDefn>(FD))
    return false;

  // Don't warn about 'main'.
  if (FD->isMain())
    return false;

  // Don't warn about inline functions.
  if (FD->isInlineSpecified())
    return false;

  // Don't warn about function templates.
  if (FD->getDescribedFunctionTemplate())
    return false;

  // Don't warn about function template specializations.
  if (FD->isFunctionTemplateSpecialization())
    return false;

  bool MissingPrototype = true;
  for (const FunctionDefn *Prev = FD->getPreviousDefinition();
       Prev; Prev = Prev->getPreviousDefinition()) {
    // Ignore any declarations that occur in function or method
    // scope, because they aren't visible from the header.
    if (Prev->getDefnContext()->isFunctionOrMethod())
      continue;

    MissingPrototype = !Prev->getType()->isFunctionProtoType();
    break;
  }

  return MissingPrototype;
}

/// \brief Given the set of return statements within a function body,
/// compute the variables that are subject to the named return value 
/// optimization.
///
/// Each of the variables that is subject to the named return value 
/// optimization will be marked as NRVO variables in the AST, and any
/// return statement that has a marked NRVO variable as its NRVO candidate can
/// use the named return value optimization.
///
/// This function applies a very simplistic algorithm for NRVO: if every return
/// statement in the function has the same NRVO candidate, that candidate is
/// the NRVO variable.
///
/// FIXME: Employ a smarter algorithm that accounts for multiple return 
/// statements and the lifetimes of the NRVO candidates. We should be able to
/// find a maximal set of NRVO variables.
static void ComputeNRVO(Stmt *Body, FunctionScopeInfo *Scope) {
  ReturnStmt **Returns = Scope->Returns.data();

  const VarDefn *NRVOCandidate = 0;
  for (unsigned I = 0, E = Scope->Returns.size(); I != E; ++I) {
    if (!Returns[I]->getNRVOCandidate())
      return;
    
    if (!NRVOCandidate)
      NRVOCandidate = Returns[I]->getNRVOCandidate();
    else if (NRVOCandidate != Returns[I]->getNRVOCandidate())
      return;
  }
  
  if (NRVOCandidate)
    const_cast<VarDefn*>(NRVOCandidate)->setNRVOVariable(true);
}

Defn *Sema::ActOnFinishFunctionBody(Defn *D, Stmt *BodyArg) {
  return ActOnFinishFunctionBody(D, move(BodyArg), false);
}

Defn *Sema::ActOnFinishFunctionBody(Defn *dcl, Stmt *Body,
                                    bool IsInstantiation) {
  FunctionDefn *FD = 0;
  FunctionTemplateDefn *FunTmpl = dyn_cast_or_null<FunctionTemplateDefn>(dcl);
  if (FunTmpl)
    FD = FunTmpl->getTemplatedDefn();
  else
    FD = dyn_cast_or_null<FunctionDefn>(dcl);

  sema::AnalysisBasedWarnings::Policy WP = AnalysisWarnings.getDefaultPolicy();

  if (FD) {
    FD->setBody(Body);
    if (FD->isMain()) {
      // C and C++ allow for main to automagically return 0.
      // Implements C++ [basic.start.main]p5 and C99 5.1.2.2.3.
      FD->setHasImplicitReturnZero(true);
      WP.disableCheckFallThrough();
    }

    if (!FD->isInvalidDefn()) {
      DiagnoseUnusedParameters(FD->param_begin(), FD->param_end());
      DiagnoseSizeOfParametersAndReturnValue(FD->param_begin(), FD->param_end(),
                                             FD->getResultType(), FD);
      
      // If this is a constructor, we need a vtable.
      if (CXXConstructorDefn *Constructor = dyn_cast<CXXConstructorDefn>(FD))
        MarkVTableUsed(FD->getLocation(), Constructor->getParent());
      
      ComputeNRVO(Body, getCurFunction());
    }
    
    assert(FD == getCurFunctionDefn() && "Function parsing confused");
  } else if (ObjCMethodDefn *MD = dyn_cast_or_null<ObjCMethodDefn>(dcl)) {
    assert(MD == getCurMethodDefn() && "Method parsing confused");
    MD->setBody(Body);
    if (Body)
      MD->setEndLoc(Body->getLocEnd());
    if (!MD->isInvalidDefn()) {
      DiagnoseUnusedParameters(MD->param_begin(), MD->param_end());
      DiagnoseSizeOfParametersAndReturnValue(MD->param_begin(), MD->param_end(),
                                             MD->getResultType(), MD);
    }
  } else {
    return 0;
  }

  // Verify and clean out per-function state.

  // Check goto/label use.
  FunctionScopeInfo *CurFn = getCurFunction();
  for (llvm::DenseMap<IdentifierInfo*, LabelStmt*>::iterator
         I = CurFn->LabelMap.begin(), E = CurFn->LabelMap.end(); I != E; ++I) {
    LabelStmt *L = I->second;

    // Verify that we have no forward references left.  If so, there was a goto
    // or address of a label taken, but no definition of it.  Label fwd
    // definitions are indicated with a null substmt.
    if (L->getSubStmt() != 0) {
      if (!L->isUsed())
        Diag(L->getIdentLoc(), diag::warn_unused_label) << L->getName();
      continue;
    }

    // Emit error.
    Diag(L->getIdentLoc(), diag::err_undeclared_label_use) << L->getName();

    // At this point, we have gotos that use the bogus label.  Stitch it into
    // the function body so that they aren't leaked and that the AST is well
    // formed.
    if (Body == 0) {
      // The whole function wasn't parsed correctly.
      continue;
    }

    // Otherwise, the body is valid: we want to stitch the label decl into the
    // function somewhere so that it is properly owned and so that the goto
    // has a valid target.  Do this by creating a new compound stmt with the
    // label in it.

    // Give the label a sub-statement.
    L->setSubStmt(new (Context) NullStmt(L->getIdentLoc()));

    CompoundStmt *Compound = isa<CXXTryStmt>(Body) ?
                               cast<CXXTryStmt>(Body)->getTryBlock() :
                               cast<CompoundStmt>(Body);
    llvm::SmallVector<Stmt*, 64> Elements(Compound->body_begin(),
                                          Compound->body_end());
    Elements.push_back(L);
    Compound->setStmts(Context, Elements.data(), Elements.size());
  }

  if (Body) {
    // C++ constructors that have function-try-blocks can't have return
    // statements in the handlers of that block. (C++ [except.handle]p14)
    // Verify this.
    if (FD && isa<CXXConstructorDefn>(FD) && isa<CXXTryStmt>(Body))
      DiagnoseReturnInConstructorExceptionHandler(cast<CXXTryStmt>(Body));
    
    // Verify that that gotos and switch cases don't jump into scopes illegally.
    // Verify that that gotos and switch cases don't jump into scopes illegally.
    if (getCurFunction()->NeedsScopeChecking() &&
        !dcl->isInvalidDefn() &&
        !hasAnyErrorsInThisFunction())
      DiagnoseInvalidJumps(Body);

    if (CXXDestructorDefn *Destructor = dyn_cast<CXXDestructorDefn>(dcl)) {
      if (!Destructor->getParent()->isDependentType())
        CheckDestructor(Destructor);

      MarkBaseAndMemberDestructorsReferenced(Destructor->getLocation(),
                                             Destructor->getParent());
    }
    
    // If any errors have occurred, clear out any temporaries that may have
    // been leftover. This ensures that these temporaries won't be picked up for
    // deletion in some later function.
    if (PP.getDiagnostics().hasErrorOccurred())
      ExprTemporaries.clear();
    else if (!isa<FunctionTemplateDefn>(dcl)) {
      // Since the body is valid, issue any analysis-based warnings that are
      // enabled.
      Type ResultType;
      if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(dcl)) {
        AnalysisWarnings.IssueWarnings(WP, FD);
      } else {
        ObjCMethodDefn *MD = cast<ObjCMethodDefn>(dcl);
        AnalysisWarnings.IssueWarnings(WP, MD);
      }
    }

    assert(ExprTemporaries.empty() && "Leftover temporaries in function");
  }
  
  if (!IsInstantiation)
    PopDefnContext();

  PopFunctionOrBlockScope();
  
  // If any errors have occurred, clear out any temporaries that may have
  // been leftover. This ensures that these temporaries won't be picked up for
  // deletion in some later function.
  if (getDiagnostics().hasErrorOccurred())
    ExprTemporaries.clear();

  return dcl;
}

/// ImplicitlyDefineFunction - An undeclared identifier was used in a function
/// call, forming a call to an implicitly defined function (per C99 6.5.1p2).
NamedDefn *Sema::ImplicitlyDefineFunction(SourceLocation Loc,
                                          IdentifierInfo &II, Scope *S) {
  // Before we produce a declaration for an implicitly defined
  // function, see whether there was a locally-scoped declaration of
  // this name as a function or variable. If so, use that
  // (non-visible) declaration, and complain about it.
  llvm::DenseMap<DefinitionName, NamedDefn *>::iterator Pos
    = LocallyScopedExternalDefns.find(&II);
  if (Pos != LocallyScopedExternalDefns.end()) {
    Diag(Loc, diag::warn_use_out_of_scope_declaration) << Pos->second;
    Diag(Pos->second->getLocation(), diag::note_previous_declaration);
    return Pos->second;
  }

  // Extension in C99.  Legal in C90, but warn about it.
  if (II.getName().startswith("__builtin_"))
    Diag(Loc, diag::warn_builtin_unknown) << &II;
  else if (getLangOptions().C99)
    Diag(Loc, diag::ext_implicit_function_decl) << &II;
  else
    Diag(Loc, diag::warn_implicit_function_decl) << &II;

  // Set a Declarator for the implicit definition: int foo();
  const char *Dummy;
  DefnSpec DS;
  unsigned DiagID;
  bool Error = DS.SetTypeSpecType(DefnSpec::TST_int, Loc, Dummy, DiagID);
  (void)Error; // Silence warning.
  assert(!Error && "Error setting up implicit decl!");
  Declarator D(DS, Declarator::BlockContext);
  D.AddTypeInfo(DeclaratorChunk::getFunction(ParsedAttributes(),
                                             false, false, SourceLocation(), 0,
                                             0, 0, false, SourceLocation(),
                                             false, 0,0,0, Loc, Loc, D),
                SourceLocation());
  D.SetIdentifier(&II, Loc);

  // Insert this function into translation-unit scope.

  DefnContext *PrevDC = CurContext;
  CurContext = Context.getTranslationUnitDefn();

  FunctionDefn *FD = dyn_cast<FunctionDefn>(ActOnDeclarator(BaseWorkspace, D));
  FD->setImplicit();

  CurContext = PrevDC;

  AddKnownFunctionAttributes(FD);

  return FD;
}

/// \brief Adds any function attributes that we know a priori based on
/// the declaration of this function.
///
/// These attributes can apply both to implicitly-declared builtins
/// (like __builtin___printf_chk) or to library-declared functions
/// like NSLog or printf.
void Sema::AddKnownFunctionAttributes(FunctionDefn *FD) {
  if (FD->isInvalidDefn())
    return;

  // If this is a built-in function, map its builtin attributes to
  // actual attributes.
  if (unsigned BuiltinID = FD->getBuiltinID()) {
    // Handle printf-formatting attributes.
    unsigned FormatIdx;
    bool HasVAListArg;
    if (Context.BuiltinInfo.isPrintfLike(BuiltinID, FormatIdx, HasVAListArg)) {
      if (!FD->getAttr<FormatAttr>())
        FD->addAttr(::new (Context) FormatAttr(FD->getLocation(), Context,
                                                "printf", FormatIdx+1,
                                               HasVAListArg ? 0 : FormatIdx+2));
    }
    if (Context.BuiltinInfo.isScanfLike(BuiltinID, FormatIdx,
                                             HasVAListArg)) {
     if (!FD->getAttr<FormatAttr>())
       FD->addAttr(::new (Context) FormatAttr(FD->getLocation(), Context,
                                              "scanf", FormatIdx+1,
                                              HasVAListArg ? 0 : FormatIdx+2));
    }

    // Mark const if we don't care about errno and that is the only
    // thing preventing the function from being const. This allows
    // IRgen to use LLVM intrinsics for such functions.
    if (!getLangOptions().MathErrno &&
        Context.BuiltinInfo.isConstWithoutErrno(BuiltinID)) {
      if (!FD->getAttr<ConstAttr>())
        FD->addAttr(::new (Context) ConstAttr(FD->getLocation(), Context));
    }

    if (Context.BuiltinInfo.isNoThrow(BuiltinID))
      FD->addAttr(::new (Context) NoThrowAttr(FD->getLocation(), Context));
    if (Context.BuiltinInfo.isConst(BuiltinID))
      FD->addAttr(::new (Context) ConstAttr(FD->getLocation(), Context));
  }

  IdentifierInfo *Name = FD->getIdentifier();
  if (!Name)
    return;
  if ((!getLangOptions().CPlusPlus &&
       FD->getDefnContext()->isTranslationUnit()) ||
      (isa<LinkageSpecDefn>(FD->getDefnContext()) &&
       cast<LinkageSpecDefn>(FD->getDefnContext())->getLanguage() ==
       LinkageSpecDefn::lang_c)) {
    // Okay: this could be a libc/libm/Objective-C function we know
    // about.
  } else
    return;

  if (Name->isStr("NSLog") || Name->isStr("NSLogv")) {
    // FIXME: NSLog and NSLogv should be target specific
    if (const FormatAttr *Format = FD->getAttr<FormatAttr>()) {
      // FIXME: We known better than our headers.
      const_cast<FormatAttr *>(Format)->setType(Context, "printf");
    } else
      FD->addAttr(::new (Context) FormatAttr(FD->getLocation(), Context,
                                             "printf", 1,
                                             Name->isStr("NSLogv") ? 0 : 2));
  } else if (Name->isStr("asprintf") || Name->isStr("vasprintf")) {
    // FIXME: asprintf and vasprintf aren't C99 functions. Should they be
    // target-specific builtins, perhaps?
    if (!FD->getAttr<FormatAttr>())
      FD->addAttr(::new (Context) FormatAttr(FD->getLocation(), Context,
                                             "printf", 2,
                                             Name->isStr("vasprintf") ? 0 : 3));
  }
}

/// \brief Determine whether a tag with a given kind is acceptable
/// as a redeclaration of the given tag declaration.
///
/// \returns true if the new tag kind is acceptable, false otherwise.
bool Sema::isAcceptableTagRedefinition(const UserClassDefn *Previous,
                                        SourceLocation NewTagLoc,
                                        const IdentifierInfo &Name) {
  // C++ [dcl.type.elab]p3:
  //   The class-key or enum keyword present in the
  //   elaborated-type-specifier shall agree in kind with the
  //   declaration to which the name in the elaborated-type-specifier
  //   refers. This rule also applies to the form of
  //   elaborated-type-specifier that declares a class-name or
  //   friend class since it can be construed as referring to the
  //   definition of the class. Thus, in any
  //   elaborated-type-specifier, the enum keyword shall be used to
  //   refer to an enumeration (7.2), the union class-key shall be
  //   used to refer to a union (clause 9), and either the class or
  //   struct class-key shall be used to refer to a class (clause 9)
  //   declared using the class or struct class-key.
  TagTypeKind OldTag = Previous->getTagKind();
  if (OldTag == NewTag)
    return true;

  if ((OldTag == TTK_Struct || OldTag == TTK_Class) &&
      (NewTag == TTK_Struct || NewTag == TTK_Class)) {
    // Warn about the struct/class tag mismatch.
    bool isTemplate = false;
    if (const CXXRecordDefn *Record = dyn_cast<CXXRecordDefn>(Previous))
      isTemplate = Record->getDescribedClassTemplate();

    Diag(NewTagLoc, diag::warn_struct_class_tag_mismatch)
      << (NewTag == TTK_Class)
      << isTemplate << &Name
      << FixItHint::CreateReplacement(SourceRange(NewTagLoc),
                              OldTag == TTK_Class? "class" : "struct");
    Diag(Previous->getLocation(), diag::note_previous_use);
    return true;
  }
  return false;
}

/// ActOnTag - This is invoked when we see 'struct foo' or 'struct {'.  In the
/// former case, Name will be non-null.  In the later case, Name will be null.
/// TagSpec indicates what kind of tag this is. TUK indicates whether this is a
/// reference/declaration/definition of a tag.
Defn *Sema::ActOnTag(Scope *S, unsigned TagSpec, TagUseKind TUK,
                     SourceLocation KWLoc, CXXScopeSpec &SS,
                     IdentifierInfo *Name, SourceLocation NameLoc,
                     AttributeList *Attr, AccessSpecifier AS,
                     MultiTemplateParamsArg TemplateParameterLists,
                     bool &OwnedDefn, bool &IsDependent,
                     bool ScopedEnum, bool ScopedEnumUsesClassTag,
                     TypeResult UnderlyingType) {
  // If this is not a definition, it must have a name.
  assert((Name != 0 || TUK == TUK_Definition) &&
         "Nameless record must be a definition!");
  assert(TemplateParameterLists.size() == 0 || TUK != TUK_Reference);

  OwnedDefn = false;
  TagTypeKind Kind = TypeWithKeyword::getTagTypeKindForTypeSpec(TagSpec);

  // FIXME: Check explicit specializations more carefully.
  bool isExplicitSpecialization = false;
  unsigned NumMatchedTemplateParamLists = TemplateParameterLists.size();
  bool Invalid = false;

  // We only need to do this matching if we have template parameters
  // or a scope specifier, which also conveniently avoids this work
  // for non-C++ cases.
  if (NumMatchedTemplateParamLists ||
      (SS.isNotEmpty() && TUK != TUK_Reference)) {
    if (TemplateParameterList *TemplateParams
          = MatchTemplateParametersToScopeSpecifier(KWLoc, SS,
                                                TemplateParameterLists.get(),
                                               TemplateParameterLists.size(),
                                                    TUK == TUK_Friend,
                                                    isExplicitSpecialization,
                                                    Invalid)) {
      // All but one template parameter lists have been matching.
      --NumMatchedTemplateParamLists;

      if (TemplateParams->size() > 0) {
        // This is a declaration or definition of a class template (which may
        // be a member of another template).
        if (Invalid)
          return 0;
        
        OwnedDefn = false;
        DefnResult Result = CheckClassTemplate(S, TagSpec, TUK, KWLoc,
                                               SS, Name, NameLoc, Attr,
                                               TemplateParams,
                                               AS);
        TemplateParameterLists.release();
        return Result.get();
      } else {
        // The "template<>" header is extraneous.
        Diag(TemplateParams->getTemplateLoc(), diag::err_template_tag_noparams)
          << TypeWithKeyword::getTagTypeKindName(Kind) << Name;
        isExplicitSpecialization = true;
      }
    }
  }

  // Figure out the underlying type if this a enum declaration. We need to do
  // this early, because it's needed to detect if this is an incompatible
  // redeclaration.
  llvm::PointerUnion<const Type*, TypeSourceInfo*> EnumUnderlying;

  if (Kind == TTK_Enum) {
    if (UnderlyingType.isInvalid() || (!UnderlyingType.get() && ScopedEnum))
      // No underlying type explicitly specified, or we failed to parse the
      // type, default to int.
      EnumUnderlying = Context.IntTy.getRawTypePtr();
    else if (UnderlyingType.get()) {
      // C++0x 7.2p2: The type-specifier-seq of an enum-base shall name an
      // integral type; any cv-qualification is ignored.
      TypeSourceInfo *TI = 0;
      Type T = GetTypeFromParser(UnderlyingType.get(), &TI);
      EnumUnderlying = TI;

      SourceLocation UnderlyingLoc = TI->getTypeLoc().getBeginLoc();

      if (!T->isDependentType() && !T->isIntegralType(Context)) {
        Diag(UnderlyingLoc, diag::err_enum_invalid_underlying)
          << T;
        // Recover by falling back to int.
        EnumUnderlying = Context.IntTy.getRawTypePtr();
      }

      if (DiagnoseUnexpandedParameterPack(UnderlyingLoc, TI, 
                                          UPPC_FixedUnderlyingType))
        EnumUnderlying = Context.IntTy.getRawTypePtr();

    } else if (getLangOptions().Microsoft)
      // Microsoft enums are always of int type.
      EnumUnderlying = Context.IntTy.getRawTypePtr();
  }

  DefnContext *SearchDC = CurContext;
  DefnContext *DC = CurContext;
  bool isStdBadAlloc = false;

  RedefinitionKind Redecl = ForRedefinition;
  if (TUK == TUK_Friend || TUK == TUK_Reference)
    Redecl = NotForRedefinition;

  LookupResult Previous(*this, Name, NameLoc, LookupTypeName, Redecl);

  if (Name && SS.isNotEmpty()) {
    // We have a nested-name tag ('struct foo::bar').

    // Check for invalid 'foo::'.
    if (SS.isInvalid()) {
      Name = 0;
      goto CreateNewDefn;
    }

    // If this is a friend or a reference to a class in a dependent
    // context, don't try to make a decl for it.
    if (TUK == TUK_Friend || TUK == TUK_Reference) {
      DC = computeDefnContext(SS, false);
      if (!DC) {
        IsDependent = true;
        return 0;
      }
    } else {
      DC = computeDefnContext(SS, true);
      if (!DC) {
        Diag(SS.getRange().getBegin(), diag::err_dependent_nested_name_spec)
          << SS.getRange();
        return 0;
      }
    }

    if (RequireCompleteDefnContext(SS, DC))
      return 0;

    SearchDC = DC;
    // Look-up name inside 'foo::'.
    LookupQualifiedName(Previous, DC);

    if (Previous.isAmbiguous())
      return 0;

    if (Previous.empty()) {
      // Name lookup did not find anything. However, if the
      // nested-name-specifier refers to the current instantiation,
      // and that current instantiation has any dependent base
      // classes, we might find something at instantiation time: treat
      // this as a dependent elaborated-type-specifier.
      // But this only makes any sense for reference-like lookups.
      if (Previous.wasNotFoundInCurrentInstantiation() &&
          (TUK == TUK_Reference || TUK == TUK_Friend)) {
        IsDependent = true;
        return 0;
      }

      // A tag 'foo::bar' must already exist.
      Diag(NameLoc, diag::err_not_tag_in_scope) 
        << Kind << Name << DC << SS.getRange();
      Name = 0;
      Invalid = true;
      goto CreateNewDefn;
    }
  } else if (Name) {
    // If this is a named struct, check to see if there was a previous forward
    // declaration or definition.
    // FIXME: We're looking into outer scopes here, even when we
    // shouldn't be. Doing so can result in ambiguities that we
    // shouldn't be diagnosing.
    LookupName(Previous, S);

    // Note:  there used to be some attempt at recovery here.
    if (Previous.isAmbiguous())
      return 0;

    if (!getLangOptions().CPlusPlus && TUK != TUK_Reference) {
      // FIXME: This makes sure that we ignore the contexts associated
      // with C structs, unions, and enums when looking for a matching
      // tag declaration or definition. See the similar lookup tweak
      // in Sema::LookupName; is there a better way to deal with this?
      while (isa<RecordDefn>(SearchDC) || isa<EnumDefn>(SearchDC))
        SearchDC = SearchDC->getParent();
    }
  } else if (S->isFunctionPrototypeScope()) {
    // If this is an enum declaration in function prototype scope, set its
    // initial context to the translation unit.
    SearchDC = Context.getTranslationUnitDefn();
  }

  if (Previous.isSingleResult() &&
      Previous.getFoundDefn()->isTemplateParameter()) {
    // Maybe we will complain about the shadowed template parameter.
    DiagnoseTemplateParameterShadow(NameLoc, Previous.getFoundDefn());
    // Just pretend that we didn't see the previous declaration.
    Previous.clear();
  }

  if (getLangOptions().CPlusPlus && Name && DC && StdNamespace &&
      DC->Equals(getStdNamespace()) && Name->isStr("bad_alloc")) {
    // This is a declaration of or a reference to "std::bad_alloc".
    isStdBadAlloc = true;
    
    if (Previous.empty() && StdBadAlloc) {
      // std::bad_alloc has been implicitly declared (but made invisible to
      // name lookup). Fill in this implicit declaration as the previous 
      // declaration, so that the declarations get chained appropriately.
      Previous.addDefn(getStdBadAlloc());
    }
  }

  // If we didn't find a previous declaration, and this is a reference
  // (or friend reference), move to the correct scope.  In C++, we
  // also need to do a redeclaration lookup there, just in case
  // there's a shadow friend decl.
  if (Name && Previous.empty() &&
      (TUK == TUK_Reference || TUK == TUK_Friend)) {
    if (Invalid) goto CreateNewDefn;
    assert(SS.isEmpty());

    if (TUK == TUK_Reference) {
      // C++ [basic.scope.pdecl]p5:
      //   -- for an elaborated-type-specifier of the form
      //
      //          class-key identifier
      //
      //      if the elaborated-type-specifier is used in the
      //      decl-specifier-seq or parameter-declaration-clause of a
      //      function defined in namespace scope, the identifier is
      //      declared as a class-name in the namespace that contains
      //      the declaration; otherwise, except as a friend
      //      declaration, the identifier is declared in the smallest
      //      non-class, non-function-prototype scope that contains the
      //      declaration.
      //
      // C99 6.7.2.3p8 has a similar (but not identical!) provision for
      // C structs and unions.
      //
      // It is an error in C++ to declare (rather than define) an enum
      // type, including via an elaborated type specifier.  We'll
      // diagnose that later; for now, declare the enum in the same
      // scope as we would have picked for any other tag type.
      //
      // GNU C also supports this behavior as part of its incomplete
      // enum types extension, while GNU C++ does not.
      //
      // Find the context where we'll be declaring the tag.
      // FIXME: We would like to maintain the current DefnContext as the
      // lexical context,
      while (SearchDC->isRecord())
        SearchDC = SearchDC->getParent();

      // Find the scope where we'll be declaring the tag.
      while (S->isClassScope() ||
             (getLangOptions().CPlusPlus &&
              S->isFunctionPrototypeScope()) ||
             ((S->getFlags() & Scope::DefnScope) == 0) ||
             (S->getEntity() &&
              ((DefnContext *)S->getEntity())->isTransparentContext()))
        S = S->getParent();
    } else {
      assert(TUK == TUK_Friend);
      // C++ [namespace.memdef]p3:
      //   If a friend declaration in a non-local class first declares a
      //   class or function, the friend class or function is a member of
      //   the innermost enclosing namespace.
      SearchDC = SearchDC->getEnclosingNamespaceContext();
    }

    // In C++, we need to do a redeclaration lookup to properly
    // diagnose some problems.
    if (getLangOptions().CPlusPlus) {
      Previous.setRedeclarationKind(ForRedefinition);
      LookupQualifiedName(Previous, SearchDC);
    }
  }

  if (!Previous.empty()) {
    NamedDefn *PrevDefn = (*Previous.begin())->getUnderlyingDefn();

    // It's okay to have a tag decl in the same scope as a typedef
    // which hides a tag decl in the same scope.  Finding this
    // insanity with a redeclaration lookup can only actually happen
    // in C++.
    //
    // This is also okay for elaborated-type-specifiers, which is
    // technically forbidden by the current standard but which is
    // okay according to the likely resolution of an open issue;
    // see http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html#407
    if (getLangOptions().CPlusPlus) {
      if (TypedefDefn *TD = dyn_cast<TypedefDefn>(PrevDefn)) {
        if (const TagType *TT = TD->getUnderlyingType()->getAs<TagType>()) {
          TagDefn *Tag = TT->getDefn();
          if (Tag->getDefnName() == Name &&
              Tag->getDefnContext()->getRedeclContext()
                          ->Equals(TD->getDefnContext()->getRedeclContext())) {
            PrevDefn = Tag;
            Previous.clear();
            Previous.addDefn(Tag);
            Previous.resolveKind();
          }
        }
      }
    }

    if (TagDefn *PrevTagDefn = dyn_cast<TagDefn>(PrevDefn)) {
      // If this is a use of a previous tag, or if the tag is already declared
      // in the same scope (so that the definition/declaration completes or
      // rementions the tag), reuse the decl.
      if (TUK == TUK_Reference || TUK == TUK_Friend ||
          isDefnInScope(PrevDefn, SearchDC, S)) {
        // Make sure that this wasn't declared as an enum and now used as a
        // struct or something similar.
        if (!isAcceptableTagRedefinition(PrevTagDefn, Kind, KWLoc, *Name)) {
          bool SafeToContinue
            = (PrevTagDefn->getTagKind() != TTK_Enum &&
               Kind != TTK_Enum);
          if (SafeToContinue)
            Diag(KWLoc, diag::err_use_with_wrong_tag)
              << Name
              << FixItHint::CreateReplacement(SourceRange(KWLoc),
                                              PrevTagDefn->getKindName());
          else
            Diag(KWLoc, diag::err_use_with_wrong_tag) << Name;
          Diag(PrevTagDefn->getLocation(), diag::note_previous_use);

          if (SafeToContinue)
            Kind = PrevTagDefn->getTagKind();
          else {
            // Recover by making this an anonymous redefinition.
            Name = 0;
            Previous.clear();
            Invalid = true;
          }
        }

        if (Kind == TTK_Enum && PrevTagDefn->getTagKind() == TTK_Enum) {
          const EnumDefn *PrevEnum = cast<EnumDefn>(PrevTagDefn);

          // All conflicts with previous declarations are recovered by
          // returning the previous declaration.
          if (ScopedEnum != PrevEnum->isScoped()) {
            Diag(KWLoc, diag::err_enum_redeclare_scoped_mismatch)
              << PrevEnum->isScoped();
            Diag(PrevTagDefn->getLocation(), diag::note_previous_use);
            return PrevTagDefn;
          }
          else if (EnumUnderlying && PrevEnum->isFixed()) {
            Type T;
            if (TypeSourceInfo *TI = EnumUnderlying.dyn_cast<TypeSourceInfo*>())
                T = TI->getType();
            else
                T = Type(EnumUnderlying.get<const Type*>(), 0);

            if (!Context.hasSameUnqualifiedType(T, PrevEnum->getIntegerType())) {
              Diag(NameLoc.isValid() ? NameLoc : KWLoc, 
                   diag::err_enum_redeclare_type_mismatch)
                << T
                << PrevEnum->getIntegerType();
              Diag(PrevTagDefn->getLocation(), diag::note_previous_use);
              return PrevTagDefn;
            }
          }
          else if (!EnumUnderlying.isNull() != PrevEnum->isFixed()) {
            Diag(KWLoc, diag::err_enum_redeclare_fixed_mismatch)
              << PrevEnum->isFixed();
            Diag(PrevTagDefn->getLocation(), diag::note_previous_use);
            return PrevTagDefn;
          }
        }

        if (!Invalid) {
          // If this is a use, just return the declaration we found.

          // FIXME: In the future, return a variant or some other clue
          // for the consumer of this Defn to know it doesn't own it.
          // For our current ASTs this shouldn't be a problem, but will
          // need to be changed with DefnGroups.
          if ((TUK == TUK_Reference && !PrevTagDefn->getFriendObjectKind()) ||
              TUK == TUK_Friend)
            return PrevTagDefn;

          // Diagnose attempts to redefine a tag.
          if (TUK == TUK_Definition) {
            if (TagDefn *Def = PrevTagDefn->getDefinition()) {
              // If we're defining a specialization and the previous definition
              // is from an implicit instantiation, don't emit an error
              // here; we'll catch this in the general case below.
              if (!isExplicitSpecialization ||
                  !isa<CXXRecordDefn>(Def) ||
                  cast<CXXRecordDefn>(Def)->getTemplateSpecializationKind()
                                               == TSK_ExplicitSpecialization) {
                Diag(NameLoc, diag::err_redefinition) << Name;
                Diag(Def->getLocation(), diag::note_previous_definition);
                // If this is a redefinition, recover by making this
                // struct be anonymous, which will make any later
                // references get the previous definition.
                Name = 0;
                Previous.clear();
                Invalid = true;
              }
            } else {
              // If the type is currently being defined, complain
              // about a nested redefinition.
              TagType *Tag = cast<TagType>(Context.getTagDefnType(PrevTagDefn));
              if (Tag->isBeingDefined()) {
                Diag(NameLoc, diag::err_nested_redefinition) << Name;
                Diag(PrevTagDefn->getLocation(),
                     diag::note_previous_definition);
                Name = 0;
                Previous.clear();
                Invalid = true;
              }
            }

            // Okay, this is definition of a previously declared or referenced
            // tag PrevDefn. We're going to create a new Defn for it.
          }
        }
        // If we get here we have (another) forward declaration or we
        // have a definition.  Just create a new decl.

      } else {
        // If we get here, this is a definition of a new tag type in a nested
        // scope, e.g. "struct foo; void bar() { struct foo; }", just create a
        // new decl/type.  We set PrevDefn to NULL so that the entities
        // have distinct types.
        Previous.clear();
      }
      // If we get here, we're going to create a new Defn. If PrevDefn
      // is non-NULL, it's a definition of the tag declared by
      // PrevDefn. If it's NULL, we have a new definition.


    // Otherwise, PrevDefn is not a tag, but was found with tag
    // lookup.  This is only actually possible in C++, where a few
    // things like templates still live in the tag namespace.
    } else {
      assert(getLangOptions().CPlusPlus);

      // Use a better diagnostic if an elaborated-type-specifier
      // found the wrong kind of type on the first
      // (non-redeclaration) lookup.
      if ((TUK == TUK_Reference || TUK == TUK_Friend) &&
          !Previous.isForRedeclaration()) {
        unsigned Kind = 0;
        if (isa<TypedefDefn>(PrevDefn)) Kind = 1;
        else if (isa<ClassTemplateDefn>(PrevDefn)) Kind = 2;
        Diag(NameLoc, diag::err_tag_reference_non_tag) << Kind;
        Diag(PrevDefn->getLocation(), diag::note_declared_at);
        Invalid = true;

      // Otherwise, only diagnose if the declaration is in scope.
      } else if (!isDefnInScope(PrevDefn, SearchDC, S)) {
        // do nothing

      // Diagnose implicit declarations introduced by elaborated types.
      } else if (TUK == TUK_Reference || TUK == TUK_Friend) {
        unsigned Kind = 0;
        if (isa<TypedefDefn>(PrevDefn)) Kind = 1;
        else if (isa<ClassTemplateDefn>(PrevDefn)) Kind = 2;
        Diag(NameLoc, diag::err_tag_reference_conflict) << Kind;
        Diag(PrevDefn->getLocation(), diag::note_previous_decl) << PrevDefn;
        Invalid = true;

      // Otherwise it's a declaration.  Call out a particularly common
      // case here.
      } else if (isa<TypedefDefn>(PrevDefn)) {
        Diag(NameLoc, diag::err_tag_definition_of_typedef)
          << Name
          << cast<TypedefDefn>(PrevDefn)->getUnderlyingType();
        Diag(PrevDefn->getLocation(), diag::note_previous_decl) << PrevDefn;
        Invalid = true;

      // Otherwise, diagnose.
      } else {
        // The tag name clashes with something else in the target scope,
        // issue an error and recover by making this tag be anonymous.
        Diag(NameLoc, diag::err_redefinition_different_kind) << Name;
        Diag(PrevDefn->getLocation(), diag::note_previous_definition);
        Name = 0;
        Invalid = true;
      }

      // The existing declaration isn't relevant to us; we're in a
      // new scope, so clear out the previous declaration.
      Previous.clear();
    }
  }

CreateNewDefn:

  TagDefn *PrevDefn = 0;
  if (Previous.isSingleResult())
    PrevDefn = cast<TagDefn>(Previous.getFoundDefn());

  // If there is an identifier, use the location of the identifier as the
  // location of the decl, otherwise use the location of the struct/union
  // keyword.
  SourceLocation Loc = NameLoc.isValid() ? NameLoc : KWLoc;

  // Otherwise, create a new declaration. If there is a previous
  // declaration of the same entity, the two will be linked via
  // PrevDefn.
  TagDefn *New;

  bool IsForwardReference = false;
  if (Kind == TTK_Enum) {
    // FIXME: Tag decls should be chained to any simultaneous vardecls, e.g.:
    // enum X { A, B, C } D;    D should chain to X.
    New = EnumDefn::Create(Context, SearchDC, Loc, Name, KWLoc,
                           cast_or_null<EnumDefn>(PrevDefn), ScopedEnum,
                           ScopedEnumUsesClassTag, !EnumUnderlying.isNull());
    // If this is an undefined enum, warn.
    if (TUK != TUK_Definition && !Invalid) {
      TagDefn *Def;
      if (getLangOptions().CPlusPlus0x && cast<EnumDefn>(New)->isFixed()) {
        // C++0x: 7.2p2: opaque-enum-declaration.
        // Conflicts are diagnosed above. Do nothing.
      }
      else if (PrevDefn && (Def = cast<EnumDefn>(PrevDefn)->getDefinition())) {
        Diag(Loc, diag::ext_forward_ref_enum_def)
          << New;
        Diag(Def->getLocation(), diag::note_previous_definition);
      } else {
        unsigned DiagID = diag::ext_forward_ref_enum;
        if (getLangOptions().Microsoft)
          DiagID = diag::ext_ms_forward_ref_enum;
        else if (getLangOptions().CPlusPlus)
          DiagID = diag::err_forward_ref_enum;
        Diag(Loc, DiagID);
        
        // If this is a forward-declared reference to an enumeration, make a 
        // note of it; we won't actually be introducing the declaration into
        // the declaration context.
        if (TUK == TUK_Reference)
          IsForwardReference = true;
      }
    }

    if (EnumUnderlying) {
      EnumDefn *ED = cast<EnumDefn>(New);
      if (TypeSourceInfo *TI = EnumUnderlying.dyn_cast<TypeSourceInfo*>())
        ED->setIntegerTypeSourceInfo(TI);
      else
        ED->setIntegerType(Type(EnumUnderlying.get<const Type*>(), 0));
      ED->setPromotionType(ED->getIntegerType());
    }

  } else {
    // struct/union/class

    // FIXME: Tag decls should be chained to any simultaneous vardecls, e.g.:
    // struct X { int A; } D;    D should chain to X.
    if (getLangOptions().CPlusPlus) {
      // FIXME: Look for a way to use RecordDefn for simple structs.
      New = CXXRecordDefn::Create(Context, Kind, SearchDC, Loc, Name, KWLoc,
                                  cast_or_null<CXXRecordDefn>(PrevDefn));
      
      if (isStdBadAlloc && (!StdBadAlloc || getStdBadAlloc()->isImplicit()))
        StdBadAlloc = cast<CXXRecordDefn>(New);
    } else
      New = RecordDefn::Create(Context, Kind, SearchDC, Loc, Name, KWLoc,
                               cast_or_null<RecordDefn>(PrevDefn));
  }

  // Maybe add qualifier info.
  if (SS.isNotEmpty()) {
    if (SS.isSet()) {
      NestedNameSpecifier *NNS
        = static_cast<NestedNameSpecifier*>(SS.getScopeRep());
      New->setQualifierInfo(NNS, SS.getRange());
      if (NumMatchedTemplateParamLists > 0) {
        New->setTemplateParameterListsInfo(Context,
                                           NumMatchedTemplateParamLists,
                    (TemplateParameterList**) TemplateParameterLists.release());
      }
    }
    else
      Invalid = true;
  }

  if (RecordDefn *RD = dyn_cast<RecordDefn>(New)) {
    // Add alignment attributes if necessary; these attributes are checked when
    // the ASTContext lays out the structure.
    //
    // It is important for implementing the correct semantics that this
    // happen here (in act on tag decl). The #pragma pack stack is
    // maintained as a result of parser callbacks which can occur at
    // many points during the parsing of a struct declaration (because
    // the #pragma tokens are effectively skipped over during the
    // parsing of the struct).
    AddAlignmentAttributesForRecord(RD);
  }

  // If this is a specialization of a member class (of a class template),
  // check the specialization.
  if (isExplicitSpecialization && CheckMemberSpecialization(New, Previous))
    Invalid = true;

  if (Invalid)
    New->setInvalidDefn();

  if (Attr)
    ProcessDefnAttributeList(S, New, Attr);

  // If we're declaring or defining a tag in function prototype scope
  // in C, note that this type can only be used within the function.
  if (Name && S->isFunctionPrototypeScope() && !getLangOptions().CPlusPlus)
    Diag(Loc, diag::warn_decl_in_param_list) << Context.getTagDefnType(New);

  // Set the lexical context. If the tag has a C++ scope specifier, the
  // lexical context will be different from the semantic context.
  New->setLexicalDefnContext(CurContext);

  // Mark this as a friend decl if applicable.
  if (TUK == TUK_Friend)
    New->setObjectOfFriendDefn(/* PreviouslyDefnared = */ !Previous.empty());

  // Set the access specifier.
  if (!Invalid && SearchDC->isRecord())
    SetMemberAccessSpecifier(New, PrevDefn, AS);

  if (TUK == TUK_Definition)
    New->startDefinition();

  // If this has an identifier, add it to the scope stack.
  if (TUK == TUK_Friend) {
    // We might be replacing an existing declaration in the lookup tables;
    // if so, borrow its access specifier.
    if (PrevDefn)
      New->setAccess(PrevDefn->getAccess());

    DefnContext *DC = New->getDefnContext()->getRedeclContext();
    DC->makeDefnVisibleInContext(New, /* Recoverable = */ false);
    if (Name) // can be null along some error paths
      if (Scope *EnclosingScope = getScopeForDefnContext(S, DC))
        PushOnScopeChains(New, EnclosingScope, /* AddToContext = */ false);
  } else if (Name) {
    S = getNonFieldDefnScope(S);
    PushOnScopeChains(New, S, !IsForwardReference);
    if (IsForwardReference)
      SearchDC->makeDefnVisibleInContext(New, /* Recoverable = */ false);

  } else {
    CurContext->addDefn(New);
  }

  // If this is the C FILE type, notify the AST context.
  if (IdentifierInfo *II = New->getIdentifier())
    if (!New->isInvalidDefn() &&
        New->getDefnContext()->getRedeclContext()->isTranslationUnit() &&
        II->isStr("FILE"))
      Context.setFILEDefn(New);

  OwnedDefn = true;
  return New;
}

void Sema::ActOnTagStartDefinition(Scope *S, Defn *TagD) {
  AdjustDefnIfTemplate(TagD);
  TagDefn *Tag = cast<TagDefn>(TagD);
  
  // Enter the tag context.
  PushDefnContext(S, Tag);
}

void Sema::ActOnStartCXXMemberDefinitions(Scope *S, Defn *TagD,
                                           SourceLocation LBraceLoc) {
  AdjustDefnIfTemplate(TagD);
  CXXRecordDefn *Record = cast<CXXRecordDefn>(TagD);

  FieldCollector->StartClass();

  if (!Record->getIdentifier())
    return;

  // C++ [class]p2:
  //   [...] The class-name is also inserted into the scope of the
  //   class itself; this is known as the injected-class-name. For
  //   purposes of access checking, the injected-class-name is treated
  //   as if it were a public member name.
  CXXRecordDefn *InjectedClassName
    = CXXRecordDefn::Create(Context, Record->getTagKind(),
                            CurContext, Record->getLocation(),
                            Record->getIdentifier(),
                            Record->getTagKeywordLoc(),
                            /*PrevDefn=*/0,
                            /*DelayTypeCreation=*/true);
  Context.getTypeDefnType(InjectedClassName, Record);
  InjectedClassName->setImplicit();
  InjectedClassName->setAccess(AS_public);
  if (ClassTemplateDefn *Template = Record->getDescribedClassTemplate())
      InjectedClassName->setDescribedClassTemplate(Template);
  PushOnScopeChains(InjectedClassName, S);
  assert(InjectedClassName->isInjectedClassName() &&
         "Broken injected-class-name");
}

void Sema::ActOnTagFinishDefinition(Scope *S, Defn *TagD,
                                    SourceLocation RBraceLoc) {
  AdjustDefnIfTemplate(TagD);
  TagDefn *Tag = cast<TagDefn>(TagD);
  Tag->setRBraceLoc(RBraceLoc);

  if (isa<CXXRecordDefn>(Tag))
    FieldCollector->FinishClass();

  // Exit this scope of this tag's definition.
  PopDefnContext();
                                          
  // Notify the consumer that we've defined a tag.
  Consumer.HandleTagDefnDefinition(Tag);
}

void Sema::ActOnTagDefinitionError(Scope *S, Defn *TagD) {
  AdjustDefnIfTemplate(TagD);
  TagDefn *Tag = cast<TagDefn>(TagD);
  Tag->setInvalidDefn();

  // We're undoing ActOnTagStartDefinition here, not
  // ActOnStartCXXMemberDefinitions, so we don't have to mess with
  // the FieldCollector.

  PopDefnContext();
}

/// DiagnoseNontrivial - Given that a class has a non-trivial
/// special member, figure out why.
void Sema::DiagnoseNontrivial(const UserClassType* T, ClassSpecialMember member) {
  Type QT(T, 0U);
  CXXRecordDefn* RD = cast<CXXRecordDefn>(T->getDefn());

  // Check whether the member was user-declared.
  switch (member) {
  case CXXInvalid:
    break;

  case CXXConstructor:
    if (RD->hasUserDefnaredConstructor()) {
      typedef CXXRecordDefn::ctor_iterator ctor_iter;
      for (ctor_iter ci = RD->ctor_begin(), ce = RD->ctor_end(); ci != ce;++ci){
        const FunctionDefn *body = 0;
        ci->hasBody(body);
        if (!body || !cast<CXXConstructorDefn>(body)->isImplicitlyDefined()) {
          SourceLocation CtorLoc = ci->getLocation();
          Diag(CtorLoc, diag::note_nontrivial_user_defined) << QT << member;
          return;
        }
      }

      assert(0 && "found no user-declared constructors");
      return;
    }
    break;

  case CXXCopyConstructor:
    if (RD->hasUserDefnaredCopyConstructor()) {
      SourceLocation CtorLoc =
        RD->getCopyConstructor(Context, 0)->getLocation();
      Diag(CtorLoc, diag::note_nontrivial_user_defined) << QT << member;
      return;
    }
    break;

  case CXXCopyAssignment:
    if (RD->hasUserDefnaredCopyAssignment()) {
      // FIXME: this should use the location of the copy
      // assignment, not the type.
      SourceLocation TyLoc = RD->getSourceRange().getBegin();
      Diag(TyLoc, diag::note_nontrivial_user_defined) << QT << member;
      return;
    }
    break;

  case CXXDestructor:
    if (RD->hasUserDefinedDestructor()) {
      SourceLocation DtorLoc = LookupDestructor(RD)->getLocation();
      Diag(DtorLoc, diag::note_nontrivial_user_defined) << QT << member;
      return;
    }
    break;
  }

  typedef CXXRecordDefn::base_class_iterator base_iter;

  // Virtual bases and members inhibit trivial copying/construction,
  // but not trivial destruction.
  if (member != CXXDestructor) {
    // Check for virtual bases.  vbases includes indirect virtual bases,
    // so we just iterate through the direct bases.
    for (base_iter bi = RD->bases_begin(), be = RD->bases_end(); bi != be; ++bi)
      if (bi->isVirtual()) {
        SourceLocation BaseLoc = bi->getSourceRange().getBegin();
        Diag(BaseLoc, diag::note_nontrivial_has_virtual) << QT << 1;
        return;
      }

    // Check for virtual methods.
    typedef CXXRecordDefn::method_iterator meth_iter;
    for (meth_iter mi = RD->method_begin(), me = RD->method_end(); mi != me;
         ++mi) {
      if (mi->isVirtual()) {
        SourceLocation MLoc = mi->getSourceRange().getBegin();
        Diag(MLoc, diag::note_nontrivial_has_virtual) << QT << 0;
        return;
      }
    }
  }

  bool (CXXRecordDefn::*hasTrivial)() const;
  switch (member) {
  case CXXConstructor:
    hasTrivial = &CXXRecordDefn::hasTrivialConstructor; break;
  case CXXCopyConstructor:
    hasTrivial = &CXXRecordDefn::hasTrivialCopyConstructor; break;
  case CXXCopyAssignment:
    hasTrivial = &CXXRecordDefn::hasTrivialCopyAssignment; break;
  case CXXDestructor:
    hasTrivial = &CXXRecordDefn::hasTrivialDestructor; break;
  default:
    assert(0 && "unexpected special member"); return;
  }

  // Check for nontrivial bases (and recurse).
  for (base_iter bi = RD->bases_begin(), be = RD->bases_end(); bi != be; ++bi) {
    const HeteroContainerType *BaseRT = bi->getType()->getAs<HeteroContainerType>();
    assert(BaseRT && "Don't know how to handle dependent bases");
    CXXRecordDefn *BaseRecTy = cast<CXXRecordDefn>(BaseRT->getDefn());
    if (!(BaseRecTy->*hasTrivial)()) {
      SourceLocation BaseLoc = bi->getSourceRange().getBegin();
      Diag(BaseLoc, diag::note_nontrivial_has_nontrivial) << QT << 1 << member;
      DiagnoseNontrivial(BaseRT, member);
      return;
    }
  }

  // Check for nontrivial members (and recurse).
  typedef RecordDefn::field_iterator field_iter;
  for (field_iter fi = RD->field_begin(), fe = RD->field_end(); fi != fe;
       ++fi) {
    Type EltTy = Context.getBaseElementType((*fi)->getType());
    if (const HeteroContainerType *EltRT = EltTy->getAs<HeteroContainerType>()) {
      CXXRecordDefn* EltRD = cast<CXXRecordDefn>(EltRT->getDefn());

      if (!(EltRD->*hasTrivial)()) {
        SourceLocation FLoc = (*fi)->getLocation();
        Diag(FLoc, diag::note_nontrivial_has_nontrivial) << QT << 0 << member;
        DiagnoseNontrivial(EltRT, member);
        return;
      }
    }
  }

  assert(0 && "found no explanation for non-trivial member");
}

/// \brief Determine whether the given integral value is representable within
/// the given type T.
static bool isRepresentableIntegerValue(ASTContext &Context,
                                        llvm::APSInt &Value,
                                        Type T) {
  assert(T->isIntegralType(Context) && "Integral type required!");
  unsigned BitWidth = Context.getIntWidth(T);
  
  if (Value.isUnsigned() || Value.isNonNegative()) {
    if (T->isSignedIntegerType()) 
      --BitWidth;
    return Value.getActiveBits() <= BitWidth;
  }  
  return Value.getMinSignedBits() <= BitWidth;
}
#endif

VarDefn *Sema::BuildExceptionDeclaration(Scope *S, IdentifierInfo *Name,
			SourceLocation Loc) {
  return NULL;
}

Defn *Sema::ActOnExceptionDeclarator(Scope *S) {
	return NULL;
}

void Sema::DiagnoseReturnInConstructorExceptionHandler(TryCmd *TryBlock){

}
