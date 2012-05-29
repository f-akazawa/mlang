//===--- Sema.cpp - AST Builder and Semantic Analysis Implementation ------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements the actions class which performs semantic analysis and
// builds an AST out of a parse stream.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
#include "mlang/Sema/PrettyDeclStackTrace.h"
#include "mlang/Sema/Scope.h"
#include "mlang/Sema/ScopeInfo.h"
#include "mlang/Sema/SemaConsumer.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/ASTDiagnostic.h"
#include "mlang/AST/DefnSub.h"
#include "mlang/AST/Expr.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Basic/SourceLocation.h"
#include "mlang/Diag/PartialDiagnostic.h"
#include "mlang/Lex/Preprocessor.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/APFloat.h"
using namespace mlang;
using namespace sema;

// copy from SemaInternal.h
inline PartialDiagnostic Sema::PDiag(unsigned DiagID) {
  return PartialDiagnostic(DiagID, Context.getDiagAllocator());
}

FunctionScopeInfo::~FunctionScopeInfo() { }

void FunctionScopeInfo::Clear() {
  HasBranchProtectedScope = false;
  HasBranchIntoScope = false;
  HasIndirectGoto = false;
  
  SwitchStack.clear();
  Returns.clear();
  ErrorTrap.reset();
}

BlockScopeInfo::~BlockScopeInfo() { }

void Sema::ActOnPopScope(SourceLocation Loc, Scope *S) {

}

void Sema::ActOnTranslationUnitScope(Scope *S) {
  BaseWorkspace = S;
  PushDefnContext(S, Context.getTranslationUnitDefn());
}

Sema::Sema(Preprocessor &pp, ASTContext &ctxt, ASTConsumer &consumer,
           bool CompleteTranslationUnit)
  : LangOpts(pp.getLangOptions()), PP(pp), Context(ctxt), Consumer(consumer),
    Diags(PP.getDiagnostics()), SourceMgr(PP.getSourceManager()),
    ExternalSource(0), CurContext(0), ParsingDefnDepth(0),
    IdResolver(pp.getLangOptions()),
    CompleteTranslationUnit(CompleteTranslationUnit),
    NumSFINAEErrors(0),
    /*AnalysisWarnings(*this),*/
    /*CurrentInstantiationScope(0),*/
    TyposCorrected(0)
{
  BaseWorkspace = 0;
  //TODO yabin if (getLangOptions().CPlusPlus)
  //  FieldCollector.reset(new CXXFieldCollector());

  // Tell diagnostics how to render things from the AST library.
  PP.getDiagnostics().SetArgToStringFn(&FormatASTNodeDiagnosticArgument, 
                                       &Context);

  ExprEvalContexts.push_back(
                  ExpressionEvaluationContextRecord(PotentiallyEvaluated, 0));  

  FunctionScopes.push_back(new FunctionScopeInfo(Diags));
}

void Sema::Initialize() {
  // Tell the AST consumer about this Sema object.
  Consumer.Initialize(Context);
  
  // FIXME: Isn't this redundant with the initialization above?
  if (SemaConsumer *SC = dyn_cast<SemaConsumer>(&Consumer))
    SC->InitializeSema(*this);
  
  // Tell the external Sema source about this Sema object.
  if (ExternalSemaSource *ExternalSema
      = dyn_cast_or_null<ExternalSemaSource>(Context.getExternalSource()))
    ExternalSema->InitializeSema(*this);
}

Sema::~Sema() {
  // Kill all the active scopes.
  for (unsigned I = 1, E = FunctionScopes.size(); I != E; ++I)
    delete FunctionScopes[I];
  if (FunctionScopes.size() == 1)
    delete FunctionScopes[0];
  
  // Tell the SemaConsumer to forget about us; we're going out of scope.
  if (SemaConsumer *SC = dyn_cast<SemaConsumer>(&Consumer))
    SC->ForgetSema();

  // Detach from the external Sema source.
  if (ExternalSemaSource *ExternalSema
        = dyn_cast_or_null<ExternalSemaSource>(Context.getExternalSource()))
    ExternalSema->ForgetSema();
}

/// ImpCastExprToType - If Expr is not of type 'Type', insert an implicit cast.
/// If there is already an implicit cast, merge into the existing one.
/// The result is of the given category.
void Sema::ImpCastExprToType(Expr *&Expr, Type Ty,
                             CastKind Kind, ExprValueKind VK) {
  Type ExprTy = Expr->getType();
  Type TypeTy = Ty;

  if (ExprTy == TypeTy)
    return;
}

ExprValueKind Sema::CastCategory(Expr *E) {

  return VK_RValue ;
}

/// ActOnEndOfTranslationUnit - This is called at the very end of the
/// translation unit when EOF is reached and all but the top-level scope is
/// popped.
void Sema::ActOnEndOfTranslationUnit() {
  // At PCH writing, implicit instantiations and VTable handling info are
  // stored and performed when the PCH is included.
  if (CompleteTranslationUnit) {



  }

  BaseWorkspace = 0;
}

//===----------------------------------------------------------------------===//
// Helper functions.
//===----------------------------------------------------------------------===//

DefnContext *Sema::getFunctionLevelDefnContext() {
  DefnContext *DC = CurContext;

//  while (isa<ScriptDefn>(DC))
//      DC = DC->getParent();

  return DC;
}

/// getCurFunctionDefn - If inside of a function body, this returns a pointer
/// to the function decl for the function being parsed.  If we're currently
/// in a 'block', this returns the containing context.
FunctionDefn *Sema::getCurFunctionDefn() {
  DefnContext *DC = getFunctionLevelDefnContext();
  return dyn_cast<FunctionDefn>(DC);
}

NamedDefn *Sema::getCurFunctionOrMethodDefn() {
  DefnContext *DC = getFunctionLevelDefnContext();
  if (isa<FunctionDefn>(DC))
    return cast<NamedDefn>(DC);
  return 0;
}

Sema::SemaDiagnosticBuilder::~SemaDiagnosticBuilder() {
  if (!isActive())
    return;
}

Sema::SemaDiagnosticBuilder Sema::Diag(SourceLocation Loc, unsigned DiagID) {
  DiagnosticBuilder DB = Diags.Report(Loc, DiagID);
  return SemaDiagnosticBuilder(DB, *this, DiagID);
}

Sema::SemaDiagnosticBuilder
Sema::Diag(SourceLocation Loc, const PartialDiagnostic& PD) {
  SemaDiagnosticBuilder Builder(Diag(Loc, PD.getDiagID()));
  PD.Emit(Builder);

  return Builder;
}

/// \brief Determines the active Scope associated with the given declaration
/// context.
///
/// This routine maps a declaration context to the active Scope object that
/// represents that declaration context in the parser. It is typically used
/// from "scope-less" code (e.g., template instantiation, lazy creation of
/// declarations) that injects a name for name-lookup purposes and, therefore,
/// must update the Scope.
///
/// \returns The scope corresponding to the given declaraion context, or NULL
/// if no such scope is open.
Scope *Sema::getScopeForContext(DefnContext *Ctx) {
  
  if (!Ctx)
    return 0;
  
  return 0;
}

/// \brief Enter a new function scope
void Sema::PushFunctionScope() {
  if (FunctionScopes.size() == 1) {
    // Use the "top" function scope rather than having to allocate
    // memory for a new scope.
    FunctionScopes.back()->Clear();
    FunctionScopes.push_back(FunctionScopes.back());
    return;
  }
  
  FunctionScopes.push_back(new FunctionScopeInfo(getDiagnostics()));
}

void Sema::PushBlockScope(Scope *BlockScope, ScriptDefn *Block) {
  FunctionScopes.push_back(new BlockScopeInfo(getDiagnostics(),
                                              BlockScope, Block));
}

void Sema::PopFunctionOrBlockScope() {
  FunctionScopeInfo *Scope = FunctionScopes.pop_back_val();
  assert(!FunctionScopes.empty() && "mismatched push/pop!");
  if (FunctionScopes.back() != Scope)
    delete Scope;
}

/// \brief Determine whether any errors occurred within this function/method/
/// block.
bool Sema::hasAnyErrorsInThisFunction() const {
  return getCurFunction()->ErrorTrap.hasErrorOccurred();
}

BlockScopeInfo *Sema::getCurBlock() {
  if (FunctionScopes.empty())
    return 0;
  
  return dyn_cast<BlockScopeInfo>(FunctionScopes.back());  
}

// Pin this vtable to this file.
ExternalSemaSource::~ExternalSemaSource() {}

void PrettyDefnStackTraceEntry::print(llvm::raw_ostream &OS) const {
  SourceLocation Loc = this->Loc;
  if (!Loc.isValid() && TheDefn) Loc = TheDefn->getLocation();
  if (Loc.isValid()) {
    Loc.print(OS, S.getSourceManager());
    OS << ": ";
  }
  OS << Message;

  if (TheDefn && isa<NamedDefn>(TheDefn)) {
    std::string Name = cast<NamedDefn>(TheDefn)->getNameAsString();
    if (!Name.empty())
      OS << " '" << Name << '\'';
  }

  OS << '\n';
}

