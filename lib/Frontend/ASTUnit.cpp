//===--- ASTUnit.cpp - ASTUnit utility ------------------------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// ASTUnit Implementation.
//
//===----------------------------------------------------------------------===//

#include "mlang/Frontend/ASTUnit.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/AST/DefnVisitor.h"
#include "mlang/AST/TypeOrdering.h"
#include "mlang/AST/StmtVisitor.h"
//#include "mlang/Driver/Compilation.h"
//#include "mlang/Driver/Driver.h"
//#include "mlang/Driver/Job.h"
//#include "mlang/Driver/ArgList.h"
//#include "mlang/Driver/Options.h"
//#include "mlang/Driver/Tool.h"
#include "mlang/Frontend/CompilerInstance.h"
#include "mlang/Frontend/FrontendActions.h"
#include "mlang/Frontend/FrontendDiagnostic.h"
#include "mlang/Frontend/FrontendOptions.h"
#include "mlang/Frontend/Utils.h"
#include "mlang/Serialization/ASTReader.h"
#include "mlang/Serialization/ASTWriter.h"
#include "mlang/Lex/ImportSearch.h"
#include "mlang/Lex/Preprocessor.h"
//#include "mlang/Basic/TargetOptions.h"
#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/Atomic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/CrashRecoveryContext.h"
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
using namespace mlang;

using llvm::TimeRecord;

namespace {
  class SimpleTimer {
    bool WantTiming;
    TimeRecord Start;
    std::string Output;

  public:
    explicit SimpleTimer(bool WantTiming) : WantTiming(WantTiming) {
      if (WantTiming)
        Start = TimeRecord::getCurrentTime();
    }

    void setOutput(const llvm::Twine &Output) {
      if (WantTiming)
        this->Output = Output.str();
    }

    ~SimpleTimer() {
      if (WantTiming) {
        TimeRecord Elapsed = TimeRecord::getCurrentTime();
        Elapsed -= Start;
        llvm::errs() << Output << ':';
        Elapsed.print(Elapsed, llvm::errs());
        llvm::errs() << '\n';
      }
    }
  };
}

/// \brief After failing to build a precompiled preamble (due to
/// errors in the source that occurs in the preamble), the number of
/// reparses during which we'll skip even trying to precompile the
/// preamble.
const unsigned DefaultPreambleRebuildInterval = 5;

/// \brief Tracks the number of ASTUnit objects that are currently active.
///
/// Used for debugging purposes only.
static llvm::sys::cas_flag ActiveASTUnitObjects;

ASTUnit::ASTUnit(bool _MainFileIsAST)
  : OnlyLocalDecls(false), CaptureDiagnostics(false),
    MainFileIsAST(_MainFileIsAST), 
    CompleteTranslationUnit(true), WantTiming(getenv("LIBCLANG_TIMING")),
    OwnsRemappedFileBuffers(true),
    NumStoredDiagnosticsFromDriver(0),
    ConcurrencyCheckValue(CheckUnlocked), 
    SavedMainFileBuffer(0),
    CompletionCacheTopLevelHashValue(0),
    PreambleTopLevelHashValue(0),
    CurrentTopLevelHashValue(0),
    UnsafeToFree(false) { 
  if (getenv("LIBCLANG_OBJTRACKING")) {
    llvm::sys::AtomicIncrement(&ActiveASTUnitObjects);
    fprintf(stderr, "+++ %d translation units\n", ActiveASTUnitObjects);
  }    
}

ASTUnit::~ASTUnit() {
  ConcurrencyCheckValue = CheckLocked;
  CleanTemporaryFiles();
  
  // Free the buffers associated with remapped files. We are required to
  // perform this operation here because we explicitly request that the
  // compiler instance *not* free these buffers for each invocation of the
  // parser.
  if (Invocation.getPtr() && OwnsRemappedFileBuffers) {
    PreprocessorOptions &PPOpts = Invocation->getPreprocessorOpts();
    for (PreprocessorOptions::remapped_file_buffer_iterator
           FB = PPOpts.remapped_file_buffer_begin(),
           FBEnd = PPOpts.remapped_file_buffer_end();
         FB != FBEnd;
         ++FB)
      delete FB->second;
  }
  
  delete SavedMainFileBuffer;

  if (getenv("LIBCLANG_OBJTRACKING")) {
    llvm::sys::AtomicDecrement(&ActiveASTUnitObjects);
    fprintf(stderr, "--- %d translation units\n", ActiveASTUnitObjects);
  }    
}

void ASTUnit::CleanTemporaryFiles() {
  for (unsigned I = 0, N = TemporaryFiles.size(); I != N; ++I)
    TemporaryFiles[I].eraseFromDisk();
  TemporaryFiles.clear();
}

namespace {

/// \brief Gathers information from ASTReader that will be used to initialize
/// a Preprocessor.
class ASTInfoCollector : public ASTReaderListener {
  LangOptions &LangOpt;
  ImportSearch &HSI;
  std::string &TargetTriple;
  std::string &Predefines;
  unsigned &Counter;

  unsigned NumHeaderInfos;

public:
  ASTInfoCollector(LangOptions &LangOpt, ImportSearch &HSI,
                   std::string &TargetTriple, std::string &Predefines,
                   unsigned &Counter)
    : LangOpt(LangOpt), HSI(HSI), TargetTriple(TargetTriple),
      Predefines(Predefines), Counter(Counter), NumHeaderInfos(0) {}

  virtual bool ReadLanguageOptions(const LangOptions &LangOpts) {
    LangOpt = LangOpts;
    return false;
  }

  virtual bool ReadTargetTriple(llvm::StringRef Triple) {
    TargetTriple = Triple;
    return false;
  }

  virtual void ReadHeaderFileInfo(const ImportedFileInfo &HFI, unsigned ID) {
    HSI.setImportedFileInfoForUID(HFI, NumHeaderInfos++);
  }

  virtual void ReadCounter(unsigned Value) {
    Counter = Value;
  }
};

class StoredDiagnosticClient : public DiagnosticClient {
  llvm::SmallVectorImpl<StoredDiagnostic> &StoredDiags;
  
public:
  explicit StoredDiagnosticClient(
                          llvm::SmallVectorImpl<StoredDiagnostic> &StoredDiags)
    : StoredDiags(StoredDiags) { }
  
  virtual void HandleDiagnostic(Diagnostic::Level Level,
                                const DiagnosticInfo &Info);
};

/// \brief RAII object that optionally captures diagnostics, if
/// there is no diagnostic client to capture them already.
class CaptureDroppedDiagnostics {
  Diagnostic &Diags;
  StoredDiagnosticClient Client;
  DiagnosticClient *PreviousClient;

public:
  CaptureDroppedDiagnostics(bool RequestCapture, Diagnostic &Diags, 
                          llvm::SmallVectorImpl<StoredDiagnostic> &StoredDiags)
    : Diags(Diags), Client(StoredDiags), PreviousClient(0)
  {
    if (RequestCapture || Diags.getClient() == 0) {
      PreviousClient = Diags.takeClient();
      Diags.setClient(&Client);
    }
  }

  ~CaptureDroppedDiagnostics() {
    if (Diags.getClient() == &Client) {
      Diags.takeClient();
      Diags.setClient(PreviousClient);
    }
  }
};

} // anonymous namespace

void StoredDiagnosticClient::HandleDiagnostic(Diagnostic::Level Level,
                                              const DiagnosticInfo &Info) {
  // Default implementation (Warnings/errors count).
  DiagnosticClient::HandleDiagnostic(Level, Info);

  StoredDiags.push_back(StoredDiagnostic(Level, Info));
}

const std::string &ASTUnit::getOriginalSourceFileName() {
  return OriginalSourceFile;
}

const std::string &ASTUnit::getASTFileName() {
  assert(isMainFileAST() && "Not an ASTUnit from an AST file!");
  return static_cast<ASTReader *>(Ctx->getExternalSource())->getFileName();
}

llvm::MemoryBuffer *ASTUnit::getBufferForFile(llvm::StringRef Filename,
                                              std::string *ErrorStr) {
  assert(FileMgr);
  return FileMgr->getBufferForFile(Filename, ErrorStr);
}

/// \brief Configure the diagnostics object for use with ASTUnit.
void ASTUnit::ConfigureDiags(llvm::IntrusiveRefCntPtr<Diagnostic> &Diags,
                             const char **ArgBegin, const char **ArgEnd,
                             ASTUnit &AST, bool CaptureDiagnostics) {
  if (!Diags.getPtr()) {
    // No diagnostics engine was provided, so create our own diagnostics object
    // with the default options.
    DiagnosticOptions DiagOpts;
    DiagnosticClient *Client = 0;
    if (CaptureDiagnostics)
      Client = new StoredDiagnosticClient(AST.StoredDiagnostics);
    Diags = CompilerInstance::createDiagnostics(DiagOpts, ArgEnd- ArgBegin, 
                                                ArgBegin, Client);
  } else if (CaptureDiagnostics) {
    Diags->setClient(new StoredDiagnosticClient(AST.StoredDiagnostics));
  }
}

ASTUnit *ASTUnit::LoadFromASTFile(const std::string &Filename,
                                  llvm::IntrusiveRefCntPtr<Diagnostic> Diags,
                                  const FileSystemOptions &FileSystemOpts,
                                  bool OnlyLocalDecls,
                                  RemappedFile *RemappedFiles,
                                  unsigned NumRemappedFiles,
                                  bool CaptureDiagnostics) {
  llvm::OwningPtr<ASTUnit> AST(new ASTUnit(true));

  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<ASTUnit>
    ASTUnitCleanup(AST.get());
  llvm::CrashRecoveryContextCleanupRegistrar<Diagnostic,
    llvm::CrashRecoveryContextReleaseRefCleanup<Diagnostic> >
    DiagCleanup(Diags.getPtr());

  ConfigureDiags(Diags, 0, 0, *AST, CaptureDiagnostics);

  AST->OnlyLocalDecls = OnlyLocalDecls;
  AST->CaptureDiagnostics = CaptureDiagnostics;
  AST->Diagnostics = Diags;
  AST->FileMgr = new FileManager(FileSystemOpts);
  AST->SourceMgr = new SourceManager(AST->getDiagnostics(),
                                     AST->getFileManager());
  AST->ImportInfo.reset(new ImportSearch(AST->getFileManager()));
  
  for (unsigned I = 0; I != NumRemappedFiles; ++I) {
    FilenameOrMemBuf fileOrBuf = RemappedFiles[I].second;
    if (const llvm::MemoryBuffer *
          memBuf = fileOrBuf.dyn_cast<const llvm::MemoryBuffer *>()) {
      // Create the file entry for the file that we're mapping from.
      const FileEntry *FromFile
        = AST->getFileManager().getVirtualFile(RemappedFiles[I].first,
                                               memBuf->getBufferSize(),
                                               0);
      if (!FromFile) {
        AST->getDiagnostics().Report(diag::err_fe_remap_missing_from_file)
          << RemappedFiles[I].first;
        delete memBuf;
        continue;
      }
      
      // Override the contents of the "from" file with the contents of
      // the "to" file.
      AST->getSourceManager().overrideFileContents(FromFile, memBuf);

    } else {
      const char *fname = fileOrBuf.get<const char *>();
      const FileEntry *ToFile = AST->FileMgr->getFile(fname);
      if (!ToFile) {
        AST->getDiagnostics().Report(diag::err_fe_remap_missing_to_file)
        << RemappedFiles[I].first << fname;
        continue;
      }

      // Create the file entry for the file that we're mapping from.
      const FileEntry *FromFile
        = AST->getFileManager().getVirtualFile(RemappedFiles[I].first,
                                               ToFile->getSize(),
                                               0);
      if (!FromFile) {
        AST->getDiagnostics().Report(diag::err_fe_remap_missing_from_file)
          << RemappedFiles[I].first;
        delete memBuf;
        continue;
      }
      
      // Override the contents of the "from" file with the contents of
      // the "to" file.
      AST->getSourceManager().overrideFileContents(FromFile, ToFile);
    }
  }
  
  // Gather Info for preprocessor construction later on.

  LangOptions LangInfo;
  ImportSearch &HeaderInfo = *AST->ImportInfo.get();
  std::string TargetTriple;
  std::string Predefines;
  unsigned Counter;

  llvm::OwningPtr<ASTReader> Reader;

  Reader.reset(new ASTReader(AST->getSourceManager(), AST->getFileManager(),
                             AST->getDiagnostics()));
  
  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<ASTReader>
    ReaderCleanup(Reader.get());

  Reader->setListener(new ASTInfoCollector(LangInfo, HeaderInfo, TargetTriple,
                                           Predefines, Counter));

  switch (Reader->ReadAST(Filename, ASTReader::MainFile)) {
  case ASTReader::Success:
    break;

  case ASTReader::Failure:
  case ASTReader::IgnorePCH:
    AST->getDiagnostics().Report(diag::err_fe_unable_to_load_pch);
    return NULL;
  }

  AST->OriginalSourceFile = Reader->getOriginalSourceFile();

  // AST file loaded successfully. Now create the preprocessor.

  // Get information about the target being compiled for.
  //
  // FIXME: This is broken, we should store the TargetOptions in the AST file.
  TargetOptions TargetOpts;
  TargetOpts.ABI = "";
  TargetOpts.GmatABI = "";
  TargetOpts.CPU = "";
  TargetOpts.Features.clear();
  TargetOpts.Triple = TargetTriple;
  AST->Target = TargetInfo::CreateTargetInfo(AST->getDiagnostics(),
                                             TargetOpts);
  AST->PP = new Preprocessor(AST->getDiagnostics(), LangInfo, *AST->Target,
                             AST->getSourceManager(), HeaderInfo);
  Preprocessor &PP = *AST->PP;

  PP.setCounterValue(Counter);
  Reader->setPreprocessor(PP);

  // Create and initialize the ASTContext.

  AST->Ctx = new ASTContext(LangInfo,
                            AST->getSourceManager(),
                            *AST->Target,
                            PP.getIdentifierTable(),
                            PP.getBuiltinInfo(),
                            /* size_reserve = */0);
  ASTContext &Context = *AST->Ctx;

  Reader->InitializeContext(Context);

  // Attach the AST reader to the AST context as an external AST
  // source, so that declarations will be deserialized from the
  // AST file as needed.
  ASTReader *ReaderPtr = Reader.get();
  llvm::OwningPtr<ExternalASTSource> Source(Reader.take());

  // Unregister the cleanup for ASTReader.  It will get cleaned up
  // by the ASTUnit cleanup.
  ReaderCleanup.unregister();

  Context.setExternalSource(Source);

  // Create an AST consumer, even though it isn't used.
  AST->Consumer.reset(new ASTConsumer);
  
  // Create a semantic analysis object and tell the AST reader about it.
  AST->TheSema.reset(new Sema(PP, Context, *AST->Consumer));
  AST->TheSema->Initialize();
  ReaderPtr->InitializeSema(*AST->TheSema);

  return AST.take();
}

namespace {

/// \brief Add the given declaration to the hash of all top-level entities.
void AddTopLevelDefinitionToHash(Defn *D, unsigned &Hash) {
  if (!D)
    return;
  
  DefnContext *DC = D->getDefnContext();
  if (!DC)
    return;
  
  if (!(DC->isTranslationUnit() || DC->getParent()->isTranslationUnit()))
    return;

  if (NamedDefn *ND = dyn_cast<NamedDefn>(D)) {
    if (ND->getIdentifier())
      Hash = llvm::HashString(ND->getIdentifier()->getName(), Hash);
    else if (DefinitionName Name = ND->getDefnName()) {
      std::string NameStr = Name.getAsString();
      Hash = llvm::HashString(NameStr, Hash);
    }
    return;
  }
  
}

class TopLevelDeclTrackerConsumer : public ASTConsumer {
  ASTUnit &Unit;
  unsigned &Hash;
  
public:
  TopLevelDeclTrackerConsumer(ASTUnit &_Unit, unsigned &Hash)
    : Unit(_Unit), Hash(Hash) {
    Hash = 0;
  }
  
  void HandleTopLevelDecl(DefnGroupRef D) {
    for (DefnGroupRef::iterator it = D.begin(), ie = D.end(); it != ie; ++it) {
      Defn *D = *it;

      AddTopLevelDefinitionToHash(D, Hash);
      Unit.addTopLevelDecl(D);
    }
  }

  // We're not interested in "interesting" decls.
  void HandleInterestingDecl(DefnGroupRef) {}
};

class TopLevelDeclTrackerAction : public ASTFrontendAction {
public:
  ASTUnit &Unit;

  virtual ASTConsumer *CreateASTConsumer(CompilerInstance &CI,
                                         llvm::StringRef InFile) {
    return new TopLevelDeclTrackerConsumer(Unit, 
                                           Unit.getCurrentTopLevelHashValue());
  }

public:
  TopLevelDeclTrackerAction(ASTUnit &_Unit) : Unit(_Unit) {}

  virtual bool hasCodeCompletionSupport() const { return false; }
  virtual bool usesCompleteTranslationUnit()  { 
    return Unit.isCompleteTranslationUnit(); 
  }
};

}

/// Parse the source file into a translation unit using the given compiler
/// invocation, replacing the current translation unit.
///
/// \returns True if a failure occurred that causes the ASTUnit not to
/// contain any translation-unit information, false otherwise.
bool ASTUnit::Parse(llvm::MemoryBuffer *OverrideMainBuffer) {
  delete SavedMainFileBuffer;
  SavedMainFileBuffer = 0;
  
  if (!Invocation) {
    delete OverrideMainBuffer;
    return true;
  }
  
  // Create the compiler instance to use for building the AST.
  llvm::OwningPtr<CompilerInstance> Mlang(new CompilerInstance());

  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<CompilerInstance>
    CICleanup(Mlang.get());

  Mlang->setInvocation(&*Invocation);
  OriginalSourceFile = Mlang->getFrontendOpts().Inputs[0].second;
    
  // Set up diagnostics, capturing any diagnostics that would
  // otherwise be dropped.
  Mlang->setDiagnostics(&getDiagnostics());
  
  // Create the target instance.
  Mlang->getTargetOpts().Features = TargetFeatures;
  Mlang->setTarget(TargetInfo::CreateTargetInfo(Mlang->getDiagnostics(),
                   Mlang->getTargetOpts()));
  if (!Mlang->hasTarget()) {
    delete OverrideMainBuffer;
    return true;
  }

  // Inform the target of the language options.
  //
  // FIXME: We shouldn't need to do this, the target should be immutable once
  // created. This complexity should be lifted elsewhere.
  Mlang->getTarget().setForcedLangOptions(Mlang->getLangOpts());
  
  assert(Mlang->getFrontendOpts().Inputs.size() == 1 &&
         "Invocation must have exactly one source file!");
  assert(Mlang->getFrontendOpts().Inputs[0].first != IK_AST &&
         "FIXME: AST inputs not yet supported here!");
  assert(Mlang->getFrontendOpts().Inputs[0].first != IK_LLVM_IR &&
         "IR inputs not support here!");

  // Configure the various subsystems.
  // FIXME: Should we retain the previous file manager?
  FileSystemOpts = Mlang->getFileSystemOpts();
  FileMgr = new FileManager(FileSystemOpts);
  SourceMgr = new SourceManager(getDiagnostics(), *FileMgr);
  TheSema.reset();
  Ctx = 0;
  PP = 0;
  
  // Clear out old caches and data.
  TopLevelDefns.clear();
  CleanTemporaryFiles();

  if (!OverrideMainBuffer) {
    StoredDiagnostics.erase(
                    StoredDiagnostics.begin() + NumStoredDiagnosticsFromDriver,
                            StoredDiagnostics.end());
  }

  // Create a file manager object to provide access to and cache the filesystem.
  Mlang->setFileManager(&getFileManager());
  
  // Create the source manager.
  Mlang->setSourceManager(&getSourceManager());
  
  // If the main file has been overridden due to the use of a preamble,
  // make that override happen and introduce the preamble.
  PreprocessorOptions &PreprocessorOpts = Mlang->getPreprocessorOpts();
  if (OverrideMainBuffer) {
    PreprocessorOpts.addRemappedFile(OriginalSourceFile, OverrideMainBuffer);

    // The stored diagnostic has the old source manager in it; update
    // the locations to refer into the new source manager. Since we've
    // been careful to make sure that the source manager's state
    // before and after are identical, so that we can reuse the source
    // location itself.
    for (unsigned I = NumStoredDiagnosticsFromDriver, 
                  N = StoredDiagnostics.size(); 
         I < N; ++I) {
      FullSourceLoc Loc(StoredDiagnostics[I].getLocation(),
                        getSourceManager());
      StoredDiagnostics[I].setLocation(Loc);
    }

    // Keep track of the override buffer;
    SavedMainFileBuffer = OverrideMainBuffer;
  }
  
  llvm::OwningPtr<TopLevelDeclTrackerAction> Act(
    new TopLevelDeclTrackerAction(*this));
    
  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<TopLevelDeclTrackerAction>
    ActCleanup(Act.get());

  if (!Act->BeginSourceFile(*Mlang.get(), Mlang->getFrontendOpts().Inputs[0].second,
                            Mlang->getFrontendOpts().Inputs[0].first))
    goto error;
  
  Act->Execute();
  
  // Steal the created target, context, and preprocessor.
  TheSema.reset(Mlang->takeSema());
  Consumer.reset(Mlang->takeASTConsumer());
  Ctx = &Mlang->getASTContext();
  PP = &Mlang->getPreprocessor();
  Mlang->setSourceManager(0);
  Mlang->setFileManager(0);
  Target = &Mlang->getTarget();
  
  Act->EndSourceFile();

  // Remove the overridden buffer we used for the preamble.
  if (OverrideMainBuffer) {
    PreprocessorOpts.eraseRemappedFile(
                               PreprocessorOpts.remapped_file_buffer_end() - 1);
  }

  return false;

error:
  // Remove the overridden buffer we used for the preamble.
  if (OverrideMainBuffer) {
    PreprocessorOpts.eraseRemappedFile(
                               PreprocessorOpts.remapped_file_buffer_end() - 1);
    delete OverrideMainBuffer;
    SavedMainFileBuffer = 0;
  }
  
  StoredDiagnostics.clear();
  return true;
}

llvm::StringRef ASTUnit::getMainFileName() const {
  return Invocation->getFrontendOpts().Inputs[0].second;
}

ASTUnit *ASTUnit::create(CompilerInvocation *CI,
                         llvm::IntrusiveRefCntPtr<Diagnostic> Diags) {
  llvm::OwningPtr<ASTUnit> AST;
  AST.reset(new ASTUnit(false));
  ConfigureDiags(Diags, 0, 0, *AST, /*CaptureDiagnostics=*/false);
  AST->Diagnostics = Diags;
  AST->Invocation = CI;
  AST->FileSystemOpts = CI->getFileSystemOpts();
  AST->FileMgr = new FileManager(AST->FileSystemOpts);
  AST->SourceMgr = new SourceManager(*Diags, *AST->FileMgr);

  return AST.take();
}

ASTUnit *ASTUnit::LoadFromCompilerInvocationAction(CompilerInvocation *CI,
                                   llvm::IntrusiveRefCntPtr<Diagnostic> Diags,
                                             ASTFrontendAction *Action) {
  assert(CI && "A CompilerInvocation is required");

  // Create the AST unit.
  llvm::OwningPtr<ASTUnit> AST;
  AST.reset(new ASTUnit(false));
  ConfigureDiags(Diags, 0, 0, *AST, /*CaptureDiagnostics*/false);
  AST->Diagnostics = Diags;
  AST->OnlyLocalDecls = false;
  AST->CaptureDiagnostics = false;
  AST->CompleteTranslationUnit = Action ? Action->usesCompleteTranslationUnit()
                                        : true;
  AST->Invocation = CI;

  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<ASTUnit>
    ASTUnitCleanup(AST.get());
  llvm::CrashRecoveryContextCleanupRegistrar<Diagnostic,
    llvm::CrashRecoveryContextReleaseRefCleanup<Diagnostic> >
    DiagCleanup(Diags.getPtr());

  // We'll manage file buffers ourselves.
  CI->getPreprocessorOpts().RetainRemappedFileBuffers = true;
  CI->getFrontendOpts().DisableFree = false;
  ProcessWarningOptions(AST->getDiagnostics(), CI->getDiagnosticOpts());

  // Save the target features.
  AST->TargetFeatures = CI->getTargetOpts().Features;

  // Create the compiler instance to use for building the AST.
  llvm::OwningPtr<CompilerInstance> Clang(new CompilerInstance());

  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<CompilerInstance>
    CICleanup(Clang.get());

  Clang->setInvocation(CI);
  AST->OriginalSourceFile = Clang->getFrontendOpts().Inputs[0].second;
    
  // Set up diagnostics, capturing any diagnostics that would
  // otherwise be dropped.
  Clang->setDiagnostics(&AST->getDiagnostics());
  
  // Create the target instance.
  Clang->getTargetOpts().Features = AST->TargetFeatures;
  Clang->setTarget(TargetInfo::CreateTargetInfo(Clang->getDiagnostics(),
                   Clang->getTargetOpts()));
  if (!Clang->hasTarget())
    return 0;

  // Inform the target of the language options.
  //
  // FIXME: We shouldn't need to do this, the target should be immutable once
  // created. This complexity should be lifted elsewhere.
  Clang->getTarget().setForcedLangOptions(Clang->getLangOpts());
  
  assert(Clang->getFrontendOpts().Inputs.size() == 1 &&
         "Invocation must have exactly one source file!");
  assert(Clang->getFrontendOpts().Inputs[0].first != IK_AST &&
         "FIXME: AST inputs not yet supported here!");
  assert(Clang->getFrontendOpts().Inputs[0].first != IK_LLVM_IR &&
         "IR inputs not supported here!");

  // Configure the various subsystems.
  AST->FileSystemOpts = Clang->getFileSystemOpts();
  AST->FileMgr = new FileManager(AST->FileSystemOpts);
  AST->SourceMgr = new SourceManager(AST->getDiagnostics(), *AST->FileMgr);
  AST->TheSema.reset();
  AST->Ctx = 0;
  AST->PP = 0;
  
  // Create a file manager object to provide access to and cache the filesystem.
  Clang->setFileManager(&AST->getFileManager());
  
  // Create the source manager.
  Clang->setSourceManager(&AST->getSourceManager());

  ASTFrontendAction *Act = Action;

  llvm::OwningPtr<TopLevelDeclTrackerAction> TrackerAct;
  if (!Act) {
    TrackerAct.reset(new TopLevelDeclTrackerAction(*AST));
    Act = TrackerAct.get();
  }

  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<TopLevelDeclTrackerAction>
    ActCleanup(TrackerAct.get());

  if (!Act->BeginSourceFile(*Clang.get(),
                            Clang->getFrontendOpts().Inputs[0].second,
                            Clang->getFrontendOpts().Inputs[0].first))
    return 0;
  
  Act->Execute();
  
  // Steal the created target, context, and preprocessor.
  AST->TheSema.reset(Clang->takeSema());
  AST->Consumer.reset(Clang->takeASTConsumer());
  AST->Ctx = &Clang->getASTContext();
  AST->PP = &Clang->getPreprocessor();
  Clang->setSourceManager(0);
  Clang->setFileManager(0);
  AST->Target = &Clang->getTarget();
  
  Act->EndSourceFile();

  return AST.take();
}

bool ASTUnit::LoadFromCompilerInvocation() {
  if (!Invocation)
    return true;
  
  // We'll manage file buffers ourselves.
  Invocation->getPreprocessorOpts().RetainRemappedFileBuffers = true;
  Invocation->getFrontendOpts().DisableFree = false;
  ProcessWarningOptions(getDiagnostics(), Invocation->getDiagnosticOpts());

  // Save the target features.
  TargetFeatures = Invocation->getTargetOpts().Features;
  
  llvm::MemoryBuffer *OverrideMainBuffer = 0;
  
  SimpleTimer ParsingTimer(WantTiming);
  ParsingTimer.setOutput("Parsing " + getMainFileName());
  
  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<llvm::MemoryBuffer>
    MemBufferCleanup(OverrideMainBuffer);
  
  return Parse(OverrideMainBuffer);
}

ASTUnit *ASTUnit::LoadFromCompilerInvocation(CompilerInvocation *CI,
                                   llvm::IntrusiveRefCntPtr<Diagnostic> Diags,
                                             bool OnlyLocalDecls,
                                             bool CaptureDiagnostics,
                                             bool CompleteTranslationUnit) {
  // Create the AST unit.
  llvm::OwningPtr<ASTUnit> AST;
  AST.reset(new ASTUnit(false));
  ConfigureDiags(Diags, 0, 0, *AST, CaptureDiagnostics);
  AST->Diagnostics = Diags;
  AST->OnlyLocalDecls = OnlyLocalDecls;
  AST->CaptureDiagnostics = CaptureDiagnostics;
  AST->CompleteTranslationUnit = CompleteTranslationUnit;
  AST->Invocation = CI;
  
  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<ASTUnit>
    ASTUnitCleanup(AST.get());
  llvm::CrashRecoveryContextCleanupRegistrar<Diagnostic,
    llvm::CrashRecoveryContextReleaseRefCleanup<Diagnostic> >
    DiagCleanup(Diags.getPtr());

  return AST->LoadFromCompilerInvocation()? 0 : AST.take();
}

ASTUnit *ASTUnit::LoadFromCommandLine(const char **ArgBegin,
                                      const char **ArgEnd,
                                    llvm::IntrusiveRefCntPtr<Diagnostic> Diags,
                                      llvm::StringRef ResourceFilesPath,
                                      bool OnlyLocalDecls,
                                      bool CaptureDiagnostics,
                                      RemappedFile *RemappedFiles,
                                      unsigned NumRemappedFiles,
                                      bool RemappedFilesKeepOriginalName,
                                      bool CompleteTranslationUnit) {
  if (!Diags.getPtr()) {
    // No diagnostics engine was provided, so create our own diagnostics object
    // with the default options.
    DiagnosticOptions DiagOpts;
    Diags = CompilerInstance::createDiagnostics(DiagOpts, ArgEnd - ArgBegin, 
                                                ArgBegin);
  }

  llvm::SmallVector<StoredDiagnostic, 4> StoredDiagnostics;
  
  llvm::IntrusiveRefCntPtr<CompilerInvocation> CI;

  {
    CaptureDroppedDiagnostics Capture(CaptureDiagnostics, *Diags, 
                                      StoredDiagnostics);

    CI = mlang::createInvocationFromCommandLine(
                        llvm::ArrayRef<const char *>(ArgBegin, ArgEnd-ArgBegin),
                        Diags);
    if (!CI)
      return 0;
  }

  // Override any files that need remapping
  for (unsigned I = 0; I != NumRemappedFiles; ++I) {
    FilenameOrMemBuf fileOrBuf = RemappedFiles[I].second;
    if (const llvm::MemoryBuffer *
            memBuf = fileOrBuf.dyn_cast<const llvm::MemoryBuffer *>()) {
      CI->getPreprocessorOpts().addRemappedFile(RemappedFiles[I].first, memBuf);
    } else {
      const char *fname = fileOrBuf.get<const char *>();
      CI->getPreprocessorOpts().addRemappedFile(RemappedFiles[I].first, fname);
    }
  }
  CI->getPreprocessorOpts().RemappedFilesKeepOriginalName =
                                                  RemappedFilesKeepOriginalName;
  
  // Override the resources path.
  CI->getImportSearchOpts().ResourceDir = ResourceFilesPath;

  // Create the AST unit.
  llvm::OwningPtr<ASTUnit> AST;
  AST.reset(new ASTUnit(false));
  ConfigureDiags(Diags, ArgBegin, ArgEnd, *AST, CaptureDiagnostics);
  AST->Diagnostics = Diags;

  AST->FileSystemOpts = CI->getFileSystemOpts();
  AST->FileMgr = new FileManager(AST->FileSystemOpts);
  AST->OnlyLocalDecls = OnlyLocalDecls;
  AST->CaptureDiagnostics = CaptureDiagnostics;
  AST->CompleteTranslationUnit = CompleteTranslationUnit;
  AST->NumStoredDiagnosticsFromDriver = StoredDiagnostics.size();
  AST->StoredDiagnostics.swap(StoredDiagnostics);
  AST->Invocation = CI;
  
  // Recover resources if we crash before exiting this method.
  llvm::CrashRecoveryContextCleanupRegistrar<ASTUnit>
    ASTUnitCleanup(AST.get());
  llvm::CrashRecoveryContextCleanupRegistrar<CompilerInvocation,
    llvm::CrashRecoveryContextReleaseRefCleanup<CompilerInvocation> >
    CICleanup(CI.getPtr());
  llvm::CrashRecoveryContextCleanupRegistrar<Diagnostic,
    llvm::CrashRecoveryContextReleaseRefCleanup<Diagnostic> >
    DiagCleanup(Diags.getPtr());

  return AST->LoadFromCompilerInvocation() ? 0 : AST.take();
}

bool ASTUnit::Reparse(RemappedFile *RemappedFiles, unsigned NumRemappedFiles) {
  if (!Invocation)
    return true;
  
  SimpleTimer ParsingTimer(WantTiming);
  ParsingTimer.setOutput("Reparsing " + getMainFileName());

  // Remap files.
  PreprocessorOptions &PPOpts = Invocation->getPreprocessorOpts();
  PPOpts.DisableStatCache = true;
  for (PreprocessorOptions::remapped_file_buffer_iterator 
         R = PPOpts.remapped_file_buffer_begin(),
         REnd = PPOpts.remapped_file_buffer_end();
       R != REnd; 
       ++R) {
    delete R->second;
  }
  Invocation->getPreprocessorOpts().clearRemappedFiles();
  for (unsigned I = 0; I != NumRemappedFiles; ++I) {
    FilenameOrMemBuf fileOrBuf = RemappedFiles[I].second;
    if (const llvm::MemoryBuffer *
            memBuf = fileOrBuf.dyn_cast<const llvm::MemoryBuffer *>()) {
      Invocation->getPreprocessorOpts().addRemappedFile(RemappedFiles[I].first,
                                                        memBuf);
    } else {
      const char *fname = fileOrBuf.get<const char *>();
      Invocation->getPreprocessorOpts().addRemappedFile(RemappedFiles[I].first,
                                                        fname);
    }
  }
  
  return Parse(0);
}

bool ASTUnit::Save(llvm::StringRef File) {
  if (getDiagnostics().hasErrorOccurred())
    return true;
  
  // FIXME: Can we somehow regenerate the stat cache here, or do we need to 
  // unconditionally create a stat cache when we parse the file?
  std::string ErrorInfo;
  llvm::raw_fd_ostream Out(File.str().c_str(), ErrorInfo,
                           llvm::raw_fd_ostream::F_Binary);
  if (!ErrorInfo.empty() || Out.has_error())
    return true;

  serialize(Out);
  Out.close();
  return Out.has_error();
}

bool ASTUnit::serialize(llvm::raw_ostream &OS) {
  if (getDiagnostics().hasErrorOccurred())
    return true;

  std::vector<unsigned char> Buffer;
  llvm::BitstreamWriter Stream(Buffer);
  ASTWriter Writer(Stream);
  Writer.WriteAST(getSema(), 0, std::string(), 0);
  
  // Write the generated bitstream to "Out".
  if (!Buffer.empty())
    OS.write((char *)&Buffer.front(), Buffer.size());

  return false;
}
