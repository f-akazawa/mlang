//===-- ASTMerge.cpp - AST Merging Frontent Action --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "mlang/Frontend/ASTUnit.h"
#include "mlang/Frontend/CompilerInstance.h"
#include "mlang/Frontend/FrontendActions.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/ASTDiagnostic.h"
#include "mlang/AST/ASTImporter.h"
#include "mlang/AST/DefnSub.h"
#include "mlang/Diag/Diagnostic.h"

using namespace mlang;

ASTConsumer *ASTMergeAction::CreateASTConsumer(CompilerInstance &CI,
                                               llvm::StringRef InFile) {
  return AdaptedAction->CreateASTConsumer(CI, InFile);
}

bool ASTMergeAction::BeginSourceFileAction(CompilerInstance &CI,
                                           llvm::StringRef Filename) {
  // FIXME: This is a hack. We need a better way to communicate the
  // AST file, compiler instance, and file name than member variables
  // of FrontendAction.
  AdaptedAction->setCurrentFile(getCurrentFile(), getCurrentFileKind(),
                                takeCurrentASTUnit());
  AdaptedAction->setCompilerInstance(&CI);
  return AdaptedAction->BeginSourceFileAction(CI, Filename);
}

void ASTMergeAction::ExecuteAction() {
  CompilerInstance &CI = getCompilerInstance();
  CI.getDiagnostics().getClient()->BeginSourceFile(
                                         CI.getASTContext().getLangOptions());
  CI.getDiagnostics().SetArgToStringFn(&FormatASTNodeDiagnosticArgument,
                                       &CI.getASTContext());
  llvm::IntrusiveRefCntPtr<DiagnosticIDs>
      DiagIDs(CI.getDiagnostics().getDiagnosticIDs());
  for (unsigned I = 0, N = ASTFiles.size(); I != N; ++I) {
    llvm::IntrusiveRefCntPtr<DiagnosticsEngine>
        Diags(new DiagnosticsEngine(DiagIDs, CI.getDiagnostics().getClient(),
                             /*ShouldOwnClient=*/false));
    ASTUnit *Unit = ASTUnit::LoadFromASTFile(ASTFiles[I], Diags,
                                             CI.getFileSystemOpts(), false);
    if (!Unit)
      continue;

    ASTImporter Importer(CI.getASTContext(),
                         CI.getFileManager(),
                         Unit->getASTContext(),
                         Unit->getFileManager());

    TranslationUnitDefn *TU = Unit->getASTContext().getTranslationUnitDefn();
    for (DefnContext::defn_iterator D = TU->defns_begin(),
                                 DEnd = TU->defns_end();
         D != DEnd; ++D) {
      // Don't re-import __va_list_tag, __builtin_va_list.
      if (NamedDefn *ND = dyn_cast<NamedDefn>(*D))
        if (IdentifierInfo *II = ND->getIdentifier())
          if (II->isStr("__va_list_tag") || II->isStr("__builtin_va_list"))
            continue;

      Importer.Import(*D);
    }

    delete Unit;
  }

  AdaptedAction->ExecuteAction();
  CI.getDiagnostics().getClient()->EndSourceFile();
}

void ASTMergeAction::EndSourceFileAction() {
  return AdaptedAction->EndSourceFileAction();
}

ASTMergeAction::ASTMergeAction(FrontendAction *AdaptedAction,
                               std::string *ASTFiles, unsigned NumASTFiles)
  : AdaptedAction(AdaptedAction), ASTFiles(ASTFiles, ASTFiles + NumASTFiles) {
  assert(AdaptedAction && "ASTMergeAction needs an action to adapt");
}

ASTMergeAction::~ASTMergeAction() {
  delete AdaptedAction;
}

bool ASTMergeAction::usesPreprocessorOnly() const {
  return AdaptedAction->usesPreprocessorOnly();
}

bool ASTMergeAction::usesCompleteTranslationUnit() {
  return AdaptedAction->usesCompleteTranslationUnit();
}

bool ASTMergeAction::hasASTFileSupport() const {
  return AdaptedAction->hasASTFileSupport();
}
