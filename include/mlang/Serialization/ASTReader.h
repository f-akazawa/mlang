//===--- ASTReader.h - AST File Reader --------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTReader class, which reads AST files.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SERIALIZATION_AST_READER_H_
#define MLANG_SERIALIZATION_AST_READER_H_

#include "mlang/Serialization/ASTBitCodes.h"
#include "mlang/Sema/Sema.h"
#include "mlang/AST/DefinitionName.h"
#include "mlang/AST/DefnContext.h"
#include "mlang/Lex/ImportSearch.h"
#include "mlang/Basic/IdentifierTable.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "llvm/Support/DataTypes.h"
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace llvm {
  class MemoryBuffer;
}

namespace mlang {
class ASTConsumer;
class ASTContext;
class ASTIdentifierIterator;
class NestedNameSpecifier;
class ClassBaseSpecifier;
class ClassConstructorDefn;
class NamedDefn;
class Preprocessor;
class Sema;
class CaseCmd;
class ASTDeserializationListener;
class ASTReader;
class ASTDefnReader;
class ASTStmtReader;
class ASTIdentifierLookupTrait;
struct ImportedFileInfo;
class UnresolvedSetImpl;
class VersionTuple;

/// \brief Abstract interface for callback invocations by the ASTReader.
///
/// While reading an AST file, the ASTReader will call the methods of the
/// listener to pass on specific information. Some of the listener methods can
/// return true to indicate to the ASTReader that the information (and
/// consequently the AST file) is invalid.
class ASTReaderListener {
public:
  virtual ~ASTReaderListener();

  /// \brief Receives the language options.
  ///
  /// \returns true to indicate the options are invalid or false otherwise.
  virtual bool ReadLanguageOptions(const LangOptions &LangOpts) {
    return false;
  }

  /// \brief Receives the target triple.
  ///
  /// \returns true to indicate the target triple is invalid or false otherwise.
  virtual bool ReadTargetTriple(llvm::StringRef Triple) {
    return false;
  }

  /// \brief Receives a ImportedFileInfo entry.
  virtual void ReadImportedFileInfo(const ImportedFileInfo &HFI, unsigned ID) {}

  /// \brief Receives __COUNTER__ value.
  virtual void ReadCounter(unsigned Value) {}
};

/// \brief Reads an AST files chain containing the contents of a translation
/// unit.
///
/// The ASTReader class reads bitstreams (produced by the ASTWriter
/// class) containing the serialized representation of a given
/// abstract syntax tree and its supporting data structures. An
/// instance of the ASTReader can be attached to an ASTContext object,
/// which will provide access to the contents of the AST files.
///
/// The AST reader provides lazy de-serialization of declarations, as
/// required when traversing the AST. Only those AST nodes that are
/// actually required will be de-serialized.
class ASTReader
  : public ExternalImportedFileInfoSource,
    public ExternalSemaSource,
    public IdentifierInfoLookup,
    public ExternalIdentifierLookup,
    public ExternalSLocEntrySource 
{
public:
  enum ASTReadResult { Success, Failure, IgnorePCH };
  /// \brief Types of AST files.
  enum ASTFileType {
    Module,   ///< File is a module proper.
    PCH,      ///< File is a PCH file treated as such.
    Preamble, ///< File is a PCH file treated as the preamble.
    MainFile  ///< File is a PCH file treated as the actual main file.
  };

  friend class ASTDefnReader;
  friend class ASTStmtReader;
  friend class ASTIdentifierIterator;
  friend class ASTIdentifierLookupTrait;
  friend class TypeLocReader;

private:
  /// \brief The receiver of some callbacks invoked by ASTReader.
  llvm::OwningPtr<ASTReaderListener> Listener;

  /// \brief The receiver of deserialization events.
  ASTDeserializationListener *DeserializationListener;

  SourceManager &SourceMgr;
  FileManager &FileMgr;
  Diagnostic &Diags;

  /// \brief The semantic analysis object that will be processing the
  /// AST files and the translation unit that uses it.
  Sema *SemaObj;

  /// \brief The preprocessor that will be loading the source file.
  Preprocessor *PP;

  /// \brief The AST context into which we'll read the AST files.
  ASTContext *Context;
      
  /// \brief The AST consumer.
  ASTConsumer *Consumer;

  /// \brief AST buffers for chained PCHs created and stored in memory.
  /// First (not depending on another) PCH in chain is in front.
  std::vector<llvm::MemoryBuffer *> ASTBuffers;

  /// \brief Information that is needed for every module.
  struct PerFileData {
    PerFileData(ASTFileType Ty);
    ~PerFileData();

    // === General information ===

    /// \brief The type of this AST file.
    ASTFileType Type;

    /// \brief The file name of the AST file.
    std::string FileName;

    /// \brief The memory buffer that stores the data associated with
    /// this AST file.
    llvm::OwningPtr<llvm::MemoryBuffer> Buffer;

    /// \brief The size of this file, in bits.
    uint64_t SizeInBits;

    /// \brief The bitstream reader from which we'll read the AST file.
    llvm::BitstreamReader StreamFile;

    /// \brief The main bitstream cursor for the main block.
    llvm::BitstreamCursor Stream;

    // === Source Locations ===

    /// \brief Cursor used to read source location entries.
    llvm::BitstreamCursor SLocEntryCursor;

    /// \brief The number of source location entries in this AST file.
    unsigned LocalNumSLocEntries;

    /// \brief Offsets for all of the source location entries in the
    /// AST file.
    const uint32_t *SLocOffsets;

    /// \brief The entire size of this module's source location offset range.
    unsigned LocalSLocSize;

    // === Identifiers ===

    /// \brief The number of identifiers in this AST file.
    unsigned LocalNumIdentifiers;

    /// \brief Offsets into the identifier table data.
    ///
    /// This array is indexed by the identifier ID (-1), and provides
    /// the offset into IdentifierTableData where the string data is
    /// stored.
    const uint32_t *IdentifierOffsets;

    /// \brief Actual data for the on-disk hash table of identifiers.
    ///
    /// This pointer points into a memory buffer, where the on-disk hash
    /// table for identifiers actually lives.
    const char *IdentifierTableData;

    /// \brief A pointer to an on-disk hash table of opaque type
    /// IdentifierHashTable.
    void *IdentifierLookupTable;

    // === Header search information ===
    
    /// \brief The number of local ImportedFileInfo structures.
    unsigned LocalNumHeaderFileInfos;
    
    /// \brief Actual data for the on-disk hash table of header file 
    /// information.
    ///
    /// This pointer points into a memory buffer, where the on-disk hash
    /// table for header file information actually lives.
    const char *HeaderFileInfoTableData;

    /// \brief The on-disk hash table that contains information about each of
    /// the header files.
    void *HeaderFileInfoTable;

    // === Declarations ===
      
    /// DefnsCursor - This is a cursor to the start of the DECLS_BLOCK block. It
    /// has read all the abbreviations at the start of the block and is ready to
    /// jump around with these in context.
    llvm::BitstreamCursor DefnsCursor;

    /// \brief The number of declarations in this AST file.
    unsigned LocalNumDefns;

    /// \brief Offset of each declaration within the bitstream, indexed
    /// by the declaration ID (-1).
    const uint32_t *DefnOffsets;

    /// \brief A snapshot of the pending instantiations in the chain.
    ///
    /// This record tracks the instantiations that Sema has to perform at the
    /// end of the TU. It consists of a pair of values for every pending
    /// instantiation where the first value is the ID of the decl and the second
    /// is the instantiation location.
    llvm::SmallVector<uint64_t, 64> PendingInstantiations;

    /// \brief The number of C++ base specifier sets in this AST file.
    unsigned LocalNumClassBaseSpecifiers;
    
    /// \brief Offset of each C++ base specifier set within the bitstream,
    /// indexed by the C++ base specifier set ID (-1).
    const uint32_t *ClassBaseSpecifiersOffsets;
    
    // === Types ===

    /// \brief The number of types in this AST file.
    unsigned LocalNumTypes;

    /// \brief Offset of each type within the bitstream, indexed by the
    /// type ID, or the representation of a Type*.
    const uint32_t *TypeOffsets;

    // === Miscellaneous ===

    /// \brief The AST stat cache installed for this file, if any.
    ///
    /// The dynamic type of this stat cache is always ASTStatCache
    void *StatCache;

    /// \brief The next module in source order.
    PerFileData *NextInSource;

    /// \brief All the modules that loaded this one. Can contain NULL for
    /// directly loaded modules.
    llvm::SmallVector<PerFileData *, 1> Loaders;
  };

  /// \brief All loaded modules, indexed by name.
  llvm::StringMap<PerFileData*> Modules;

  /// \brief The first module in source order.
  PerFileData *FirstInSource;

  /// \brief The chain of AST files. The first entry is the one named by the
  /// user, the last one is the one that doesn't depend on anything further.
  /// That is, the entry I was created with -include-pch I+1.
  llvm::SmallVector<PerFileData*, 2> Chain;

  /// \brief SLocEntries that we're going to preload.
  llvm::SmallVector<uint64_t, 64> PreloadSLocEntries;

  /// \brief Types that have already been loaded from the chain.
  ///
  /// When the pointer at index I is non-NULL, the type with
  /// ID = (I + 1) << FastQual::Width has already been loaded
  std::vector<Type> TypesLoaded;

  /// \brief Map that provides the ID numbers of each type within the
  /// output stream.
  ///
  /// The ID numbers of types are consecutive (in order of discovery)
  /// and start at 1. 0 is reserved for NULL. When types are actually
  /// stored in the stream, the ID number is shifted by 2 bits to
  /// allow for the const/volatile qualifiers.
  ///
  /// Keys in the map never have const/volatile qualifiers.
  serialization::TypeIdxMap TypeIdxs;

  /// \brief Declarations that have already been loaded from the chain.
  ///
  /// When the pointer at index I is non-NULL, the declaration with ID
  /// = I + 1 has already been loaded.
  std::vector<Defn *> DefnsLoaded;

  typedef std::pair<PerFileData *, uint64_t> FileOffset;
  typedef llvm::SmallVector<FileOffset, 2> FileOffsetsTy;
  typedef llvm::DenseMap<serialization::DefnID, FileOffsetsTy>
      DefnUpdateOffsetsMap;
  /// \brief Declarations that have modifications residing in a later file
  /// in the chain.
  DefnUpdateOffsetsMap DefnUpdateOffsets;

  typedef llvm::DenseMap<serialization::DefnID,
                         std::pair<PerFileData *, uint64_t> >
      DefnReplacementMap;
  /// \brief Declarations that have been replaced in a later file in the chain.
  DefnReplacementMap ReplacedDefns;

  /// \brief Information about the contents of a DeclContext.
  struct DefnContextInfo {
    void *NameLookupTableData; // a ASTDeclContextNameLookupTable.
    const serialization::KindDefnIDPair *LexicalDefns;
    unsigned NumLexicalDefns;
  };
  // In a full chain, there could be multiple updates to every decl context,
  // so this is a vector. However, typically a chain is only two elements long,
  // with only one file containing updates, so there will be only one update
  // per decl context.
  typedef llvm::SmallVector<DefnContextInfo, 1> DefnContextInfos;
  typedef llvm::DenseMap<const DefnContext *, DefnContextInfos>
      DefnContextOffsetsMap;
  // Updates for visible decls can occur for other contexts than just the
  // TU, and when we read those update records, the actual context will not
  // be available yet (unless it's the TU), so have this pending map using the
  // ID as a key. It will be realized when the context is actually loaded.
  typedef llvm::SmallVector<void *, 1> DefnContextVisibleUpdates;
  typedef llvm::DenseMap<serialization::DefnID, DefnContextVisibleUpdates>
      DefnContextVisibleUpdatesPending;

  /// \brief Offsets of the lexical and visible declarations for each
  /// DeclContext.
  DefnContextOffsetsMap DefnContextOffsets;

  /// \brief Updates to the visible declarations of declaration contexts that
  /// haven't been loaded yet.
  DefnContextVisibleUpdatesPending PendingVisibleUpdates;

  typedef llvm::SmallVector<UserClassDefn *, 4> ForwardRefs;
  typedef llvm::DenseMap<const UserClassDefn *, ForwardRefs>
      PendingForwardRefsMap;
  /// \brief Forward references that have a definition but the definition decl
  /// is still initializing. When the definition gets read it will update
  /// the DefinitionData pointer of all pending references.
  PendingForwardRefsMap PendingForwardRefs;

  typedef llvm::DenseMap<serialization::DefnID, serialization::DefnID>
      FirstLatestDefnIDMap;
  /// \brief Map of first declarations from a chained PCH that point to the
  /// most recent declarations in another AST file.
  FirstLatestDefnIDMap FirstLatestDefnIDs;

  /// \brief Read the records that describe the contents of declcontexts.
  bool ReadDefnContextStorage(llvm::BitstreamCursor &Cursor,
                              const std::pair<uint64_t, uint64_t> &Offsets,
                              DefnContextInfo &Info);

  /// \brief A vector containing identifiers that have already been
  /// loaded.
  ///
  /// If the pointer at index I is non-NULL, then it refers to the
  /// IdentifierInfo for the identifier with ID=I+1 that has already
  /// been loaded.
  std::vector<IdentifierInfo *> IdentifiersLoaded;

  /// \brief Mapping from identifiers that represent macros whose definitions
  /// have not yet been deserialized to the global offset where the macro
  /// record resides.
  llvm::DenseMap<IdentifierInfo *, uint64_t> UnreadMacroRecordOffsets;
      
  /// \name CodeGen-relevant special data
  /// \brief Fields containing data that is relevant to CodeGen.
  //@{

  /// \brief The IDs of all declarations that fulfill the criteria of
  /// "interesting" defns.
  ///
  /// This contains the data loaded from all EXTERNAL_DEFINITIONS blocks in the
  /// chain. The referenced declarations are deserialized and passed to the
  /// consumer eagerly.
  llvm::SmallVector<uint64_t, 16> ExternalDefinitions;

  /// \brief The IDs of all tentative definitions stored in the the chain.
  ///
  /// Sema keeps track of all tentative definitions in a TU because it has to
  /// complete them and pass them on to CodeGen. Thus, tentative definitions in
  /// the PCH chain must be eagerly deserialized.
  llvm::SmallVector<uint64_t, 16> TentativeDefinitions;

  /// \brief The IDs of all CXXRecordDecls stored in the chain whose VTables are
  /// used.
  ///
  /// CodeGen has to emit VTables for these records, so they have to be eagerly
  /// deserialized.
  llvm::SmallVector<uint64_t, 64> VTableUses;

  //@}

  /// \name Diagnostic-relevant special data
  /// \brief Fields containing data that is used for generating diagnostics
  //@{

  /// \brief A snapshot of Sema's unused file-scoped variable tracking, for
  /// generating warnings.
  llvm::SmallVector<uint64_t, 16> UnusedFileScopedDefns;

  /// \brief A list of all the delegating constructors we've seen, to diagnose
  /// cycles.
  llvm::SmallVector<uint64_t, 4> DelegatingCtorDefns;

  /// \brief A snapshot of Sema's weak undeclared identifier tracking, for
  /// generating warnings.
  llvm::SmallVector<uint64_t, 64> WeakUndeclaredIdentifiers;

  /// \brief The IDs of type aliases for ext_vectors that exist in the chain.
  ///
  /// Used by Sema for finding sugared names for ext_vectors in diagnostics.
  llvm::SmallVector<uint64_t, 4> ExtVectorDefns;

  //@}

  /// \name Sema-relevant special data
  /// \brief Fields containing data that is used for semantic analysis
  //@{

  /// \brief The IDs of all locally scoped external decls in the chain.
  ///
  /// Sema tracks these to validate that the types are consistent across all
  /// local external declarations.
  llvm::SmallVector<uint64_t, 16> LocallyScopedExternalDefns;

  /// \brief The IDs of all dynamic class declarations in the chain.
  ///
  /// Sema tracks these because it checks for the key functions being defined
  /// at the end of the TU, in which case it directs CodeGen to emit the VTable.
  llvm::SmallVector<uint64_t, 16> DynamicClasses;

  /// \brief The IDs of the declarations Sema stores directly.
  ///
  /// Sema tracks a few important decls, such as namespace std, directly.
  llvm::SmallVector<uint64_t, 4> SemaDefnRefs;

  /// \brief The IDs of the types ASTContext stores directly.
  ///
  /// The AST context tracks a few important types, such as va_list, directly.
  llvm::SmallVector<uint64_t, 16> SpecialTypes;

  /// \brief The IDs of CUDA-specific declarations ASTContext stores directly.
  ///
  /// The AST context tracks a few important decls, currently cudaConfigureCall,
  /// directly.
  llvm::SmallVector<uint64_t, 2> CUDASpecialDefnRefs;

  /// \brief The floating point pragma option settings.
  llvm::SmallVector<uint64_t, 1> FPPragmaOptions;

  /// \brief The OpenCL extension settings.
  llvm::SmallVector<uint64_t, 1> OpenCLExtensions;

  //@}

  /// \brief Diagnostic IDs and their mappings that the user changed.
  llvm::SmallVector<uint64_t, 8> PragmaDiagMappings;

  /// \brief The original file name that was used to build the primary AST file,
  /// which may have been modified for relocatable-pch support.
  std::string OriginalFileName;

  /// \brief The actual original file name that was used to build the primary
  /// AST file.
  std::string ActualOriginalFileName;

  /// \brief The file ID for the original file that was used to build the
  /// primary AST file.
  FileID OriginalFileID;
  
  /// \brief The directory that the PCH was originally created in. Used to
  /// allow resolving headers even after headers+PCH was moved to a new path.
  std::string OriginalDir;

  /// \brief The directory that the PCH we are reading is stored in.
  std::string CurrentDir;

  /// \brief The system include root to be used when loading the
  /// precompiled header.
  const char *isysroot;

  /// \brief Whether to disable the use of stat caches in AST files.
  bool DisableStatCache;

  /// \brief Mapping from switch-case IDs in the chain to switch-case statements
  ///
  /// Statements usually don't have IDs, but switch cases need them, so that the
  /// switch statement can refer to them.
  std::map<unsigned, CaseCmd *> SwitchCaseStmts;

  /// \brief The number of stat() calls that hit/missed the stat
  /// cache.
  unsigned NumStatHits, NumStatMisses;

  /// \brief The number of source location entries de-serialized from
  /// the PCH file.
  unsigned NumSLocEntriesRead;

  /// \brief The number of source location entries in the chain.
  unsigned TotalNumSLocEntries;

  /// \brief The next offset for a SLocEntry after everything in this reader.
  unsigned NextSLocOffset;

  /// \brief The number of statements (and expressions) de-serialized
  /// from the chain.
  unsigned NumStatementsRead;

  /// \brief The total number of statements (and expressions) stored
  /// in the chain.
  unsigned TotalNumStatements;

  /// Number of lexical decl contexts read/total.
  unsigned NumLexicalDefnContextsRead, TotalLexicalDefnContexts;

  /// Number of visible decl contexts read/total.
  unsigned NumVisibleDefnContextsRead, TotalVisibleDefnContexts;
  
  /// \brief Number of Decl/types that are currently deserializing.
  unsigned NumCurrentElementsDeserializing;

  /// \brief An IdentifierInfo that has been loaded but whose top-level
  /// declarations of the same name have not (yet) been loaded.
  struct PendingIdentifierInfo {
    IdentifierInfo *II;
    llvm::SmallVector<uint32_t, 4> DefnIDs;
  };

  /// \brief The set of identifiers that were read while the AST reader was
  /// (recursively) loading declarations.
  ///
  /// The declarations on the identifier chain for these identifiers will be
  /// loaded once the recursive loading has completed.
  std::deque<PendingIdentifierInfo> PendingIdentifierInfos;

  /// \brief Contains declarations and definitions that will be
  /// "interesting" to the ASTConsumer, when we get that AST consumer.
  ///
  /// "Interesting" declarations are those that have data that may
  /// need to be emitted, such as inline function definitions or
  /// Objective-C protocols.
  std::deque<Defn *> InterestingDefns;

  /// \brief We delay loading of the previous declaration chain to avoid
  /// deeply nested calls when there are many redeclarations.
  std::deque<std::pair<Defn *, serialization::DefnID> > PendingPreviousDefns;

  /// \brief Ready to load the previous declaration of the given Decl.
  void loadAndAttachPreviousDefn(Defn *D, serialization::DefnID ID);

  /// \brief When reading a Stmt tree, Stmt operands are placed in this stack.
  llvm::SmallVector<Stmt *, 16> StmtStack;

  /// \brief What kind of records we are reading.
  enum ReadingKind {
    Read_Defn, Read_Type, Read_Stmt
  };

  /// \brief What kind of records we are reading. 
  ReadingKind ReadingKind;

  /// \brief RAII object to change the reading kind.
  class ReadingKindTracker {
    ASTReader &Reader;
    enum ReadingKind PrevKind;

    ReadingKindTracker(const ReadingKindTracker&); // do not implement
    ReadingKindTracker &operator=(const ReadingKindTracker&);// do not implement

  public:
    ReadingKindTracker(enum ReadingKind newKind, ASTReader &reader)
      : Reader(reader), PrevKind(Reader.ReadingKind) {
      Reader.ReadingKind = newKind;
    }

    ~ReadingKindTracker() { Reader.ReadingKind = PrevKind; }
  };

  /// \brief Reads a statement from the specified cursor.
  Stmt *ReadStmtFromStream(PerFileData &F);

  void MaybeAddSystemRootToFilename(std::string &Filename);

  ASTReadResult ReadASTCore(llvm::StringRef FileName, ASTFileType Type);
  ASTReadResult ReadASTBlock(PerFileData &F);
  bool ParseLineTable(PerFileData &F, llvm::SmallVectorImpl<uint64_t> &Record);
  ASTReadResult ReadSourceManagerBlock(PerFileData &F);
  ASTReadResult ReadSLocEntryRecord(unsigned ID);
  PerFileData *SLocCursorForID(unsigned ID);
  SourceLocation getImportLocation(PerFileData *F);
  bool ParseLanguageOptions(const llvm::SmallVectorImpl<uint64_t> &Record);

  struct RecordLocation {
    RecordLocation(PerFileData *M, uint64_t O)
      : F(M), Offset(O) {}
    PerFileData *F;
    uint64_t Offset;
  };

  Type ReadTypeRecord(unsigned Index);
  RecordLocation TypeCursorForIndex(unsigned Index);
  void LoadedDefn(unsigned Index, Defn *D);
  Defn *ReadDefnRecord(unsigned Index, serialization::DefnID ID);
  RecordLocation DefnCursorForIndex(unsigned Index, serialization::DefnID ID);

  void PassInterestingDefnsToConsumer();

  /// \brief Produce an error diagnostic and return true.
  ///
  /// This routine should only be used for fatal errors that have to
  /// do with non-routine failures (e.g., corrupted AST file).
  void Error(llvm::StringRef Msg);
  void Error(unsigned DiagID, llvm::StringRef Arg1 = llvm::StringRef(),
             llvm::StringRef Arg2 = llvm::StringRef());

  ASTReader(const ASTReader&); // do not implement
  ASTReader &operator=(const ASTReader &); // do not implement
public:
  typedef llvm::SmallVector<uint64_t, 64> RecordData;

  /// \brief Load the AST file and validate its contents against the given
  /// Preprocessor.
  ///
  /// \param PP the preprocessor associated with the context in which this
  /// precompiled header will be loaded.
  ///
  /// \param Context the AST context that this precompiled header will be
  /// loaded into.
  ///
  /// \param isysroot If non-NULL, the system include path specified by the
  /// user. This is only used with relocatable PCH files. If non-NULL,
  /// a relocatable PCH file will use the default path "/".
  ///
  /// \param DisableValidation If true, the AST reader will suppress most
  /// of its regular consistency checking, allowing the use of precompiled
  /// headers that cannot be determined to be compatible.
  ///
  /// \param DisableStatCache If true, the AST reader will ignore the
  /// stat cache in the AST files. This performance pessimization can
  /// help when an AST file is being used in cases where the
  /// underlying files in the file system may have changed, but
  /// parsing should still continue.
  ASTReader(Preprocessor &PP, ASTContext *Context, const char *isysroot = 0,
            bool DisableValidation = false, bool DisableStatCache = false);

  /// \brief Load the AST file without using any pre-initialized Preprocessor.
  ///
  /// The necessary information to initialize a Preprocessor later can be
  /// obtained by setting a ASTReaderListener.
  ///
  /// \param SourceMgr the source manager into which the AST file will be loaded
  ///
  /// \param FileMgr the file manager into which the AST file will be loaded.
  ///
  /// \param Diags the diagnostics system to use for reporting errors and
  /// warnings relevant to loading the AST file.
  ///
  /// \param isysroot If non-NULL, the system include path specified by the
  /// user. This is only used with relocatable PCH files. If non-NULL,
  /// a relocatable PCH file will use the default path "/".
  ///
  /// \param DisableValidation If true, the AST reader will suppress most
  /// of its regular consistency checking, allowing the use of precompiled
  /// headers that cannot be determined to be compatible.
  ///
  /// \param DisableStatCache If true, the AST reader will ignore the
  /// stat cache in the AST files. This performance pessimization can
  /// help when an AST file is being used in cases where the
  /// underlying files in the file system may have changed, but
  /// parsing should still continue.
  ASTReader(SourceManager &SourceMgr, FileManager &FileMgr,
            Diagnostic &Diags, const char *isysroot = 0,
            bool DisableValidation = false, bool DisableStatCache = false);
  ~ASTReader();

  /// \brief Load the precompiled header designated by the given file
  /// name.
  ASTReadResult ReadAST(const std::string &FileName, ASTFileType Type);

  /// \brief Set the AST callbacks listener.
  void setListener(ASTReaderListener *listener) {
    Listener.reset(listener);
  }

  /// \brief Set the AST deserialization listener.
  void setDeserializationListener(ASTDeserializationListener *Listener);

  /// \brief Set the Preprocessor to use.
  void setPreprocessor(Preprocessor &pp);

  /// \brief Sets and initializes the given Context.
  void InitializeContext(ASTContext &Context);

  /// \brief Set AST buffers for chained PCHs created and stored in memory.
  /// First (not depending on another) PCH in chain is first in array.
  void setASTMemoryBuffers(llvm::MemoryBuffer **bufs, unsigned numBufs) {
    ASTBuffers.clear();
    ASTBuffers.insert(ASTBuffers.begin(), bufs, bufs + numBufs);
  }

  /// \brief Retrieve the name of the named (primary) AST file
  const std::string &getFileName() const { return Chain[0]->FileName; }

  /// \brief Retrieve the name of the original source file name
  const std::string &getOriginalSourceFile() { return OriginalFileName; }

  /// \brief Retrieve the name of the original source file name directly from
  /// the AST file, without actually loading the AST file.
  static std::string getOriginalSourceFile(const std::string &ASTFileName,
                                           FileManager &FileMgr,
                                           Diagnostic &Diags);

  /// \brief Read the header file information for the given file entry.
  virtual ImportedFileInfo GetImportedFileInfo(const FileEntry *FE);

  void ReadPragmaDiagnosticMappings(Diagnostic &Diag);

  /// \brief Returns the number of source locations found in the chain.
  unsigned getTotalNumSLocs() const {
    return TotalNumSLocEntries;
  }

  /// \brief Returns the next SLocEntry offset after the chain.
  unsigned getNextSLocOffset() const {
    return NextSLocOffset;
  }

  /// \brief Returns the number of identifiers found in the chain.
  unsigned getTotalNumIdentifiers() const {
    return static_cast<unsigned>(IdentifiersLoaded.size());
  }

  /// \brief Returns the number of types found in the chain.
  unsigned getTotalNumTypes() const {
    return static_cast<unsigned>(TypesLoaded.size());
  }

  /// \brief Returns the number of declarations found in the chain.
  unsigned getTotalNumDefns() const {
    return static_cast<unsigned>(DefnsLoaded.size());
  }

  /// \brief Returns the number of C++ base specifiers found in the chain.
  unsigned getTotalNumClassBaseSpecifiers() const;

  /// \brief Resolve and return the translation unit declaration.
  TranslationUnitDefn *GetTranslationUnitDefn();

  /// \brief Resolve a type ID into a type, potentially building a new
  /// type.
  Type GetType(serialization::TypeID ID);

  /// \brief Returns the type ID associated with the given type.
  /// If the type didn't come from the AST file the ID that is returned is
  /// marked as "doesn't exist in AST".
  serialization::TypeID GetTypeID(Type T) const;

  /// \brief Returns the type index associated with the given type.
  /// If the type didn't come from the AST file the index that is returned is
  /// marked as "doesn't exist in AST".
  serialization::TypeIdx GetTypeIdx(Type T) const;

  /// \brief Resolve a declaration ID into a declaration, potentially
  /// building a new declaration.
  Defn *GetDefn(serialization::DefnID ID);
  virtual Defn *GetExternalDefn(uint32_t ID);

  /// \brief Resolve a CXXBaseSpecifiers ID into an offset into the chain
  /// of loaded AST files.
  uint64_t GetClassBaseSpecifiersOffset(serialization::ClassBaseSpecifiersID ID);
      
  virtual ClassBaseSpecifier *GetExternalClassBaseSpecifiers(uint64_t Offset);
      
  /// \brief Resolve the offset of a statement into a statement.
  ///
  /// This operation will read a new statement from the external
  /// source each time it is called, and is meant to be used via a
  /// LazyOffsetPtr (which is used by Decls for the body of functions, etc).
  virtual Stmt *GetExternalDefnStmt(uint64_t Offset);

  /// ReadBlockAbbrevs - Enter a subblock of the specified BlockID with the
  /// specified cursor.  Read the abbreviations that are at the top of the block
  /// and then leave the cursor pointing into the block.
  bool ReadBlockAbbrevs(llvm::BitstreamCursor &Cursor, unsigned BlockID);

  /// \brief Finds all the visible declarations with a given name.
  /// The current implementation of this method just loads the entire
  /// lookup table as unmaterialized references.
  virtual DefnContext::lookup_result
  FindExternalVisibleDefnsByName(const DefnContext *DC,
                                 DefinitionName Name);

  virtual void MaterializeVisibleDefns(const DefnContext *DC);

  /// \brief Read all of the declarations lexically stored in a
  /// declaration context.
  ///
  /// \param DC The declaration context whose declarations will be
  /// read.
  ///
  /// \param Decls Vector that will contain the declarations loaded
  /// from the external source. The caller is responsible for merging
  /// these declarations with any declarations already stored in the
  /// declaration context.
  ///
  /// \returns true if there was an error while reading the
  /// declarations for this declaration context.
  virtual bool FindExternalLexicalDefns(const DefnContext *DC,
                                        bool (*isKindWeWant)(Defn::Kind),
                                        llvm::SmallVectorImpl<Defn*> &Defns);

  /// \brief Notify ASTReader that we started deserialization of
  /// a decl or type so until FinishedDeserializing is called there may be
  /// decls that are initializing. Must be paired with FinishedDeserializing.
  virtual void StartedDeserializing() { ++NumCurrentElementsDeserializing; }

  /// \brief Notify ASTReader that we finished the deserialization of
  /// a decl or type. Must be paired with StartedDeserializing.
  virtual void FinishedDeserializing();

  /// \brief Function that will be invoked when we begin parsing a new
  /// translation unit involving this external AST source.
  ///
  /// This function will provide all of the external definitions to
  /// the ASTConsumer.
  virtual void StartTranslationUnit(ASTConsumer *Consumer);

  /// \brief Print some statistics about AST usage.
  virtual void PrintStats();

  /// Return the amount of memory used by memory buffers, breaking down
  /// by heap-backed versus mmap'ed memory.
  virtual void getMemoryBufferSizes(MemoryBufferSizes &sizes) const;

  /// \brief Initialize the semantic source with the Sema instance
  /// being used to perform semantic analysis on the abstract syntax
  /// tree.
  virtual void InitializeSema(Sema &S);

  /// \brief Inform the semantic consumer that Sema is no longer available.
  virtual void ForgetSema() { SemaObj = 0; }

  /// \brief Retrieve the IdentifierInfo for the named identifier.
  ///
  /// This routine builds a new IdentifierInfo for the given identifier. If any
  /// declarations with this name are visible from translation unit scope, their
  /// declarations will be deserialized and introduced into the declaration
  /// chain of the identifier.
  virtual IdentifierInfo *get(const char *NameStart, const char *NameEnd);
  IdentifierInfo *get(llvm::StringRef Name) {
    return get(Name.begin(), Name.end());
  }

  /// \brief Retrieve an iterator into the set of all identifiers
  /// in all loaded AST files.
  virtual IdentifierIterator *getIdentifiers() const;

  void SetIdentifierInfo(unsigned ID, IdentifierInfo *II);
  void SetGloballyVisibleDefns(IdentifierInfo *II,
                               const llvm::SmallVectorImpl<uint32_t> &DefnIDs,
                               bool Nonrecursive = false);

  /// \brief Report a diagnostic.
  DiagnosticBuilder Diag(unsigned DiagID);

  /// \brief Report a diagnostic.
  DiagnosticBuilder Diag(SourceLocation Loc, unsigned DiagID);

  IdentifierInfo *DecodeIdentifierInfo(unsigned Idx);

  IdentifierInfo *GetIdentifierInfo(const RecordData &Record, unsigned &Idx) {
    return DecodeIdentifierInfo(Record[Idx++]);
  }

  virtual IdentifierInfo *GetIdentifier(unsigned ID) {
    return DecodeIdentifierInfo(ID);
  }

  /// \brief Read the source location entry with index ID.
  virtual bool ReadSLocEntry(unsigned ID);

  /// \brief Read a declaration name.
  DefinitionName ReadDefinitionName(const RecordData &Record, unsigned &Idx);
  void ReadDefinitionNameInfo(PerFileData &F, DefinitionNameInfo &NameInfo,
                               const RecordData &Record, unsigned &Idx);

//  void ReadTypeInfoInfo(PerFileData &F, QualifierInfo &Info,
//                         const RecordData &Record, unsigned &Idx);

  NestedNameSpecifier *ReadNestedNameSpecifier(const RecordData &Record,
                                               unsigned &Idx);

  /// \brief Read a UnresolvedSet structure.
  void ReadUnresolvedSet(UnresolvedSetImpl &Set,
                         const RecordData &Record, unsigned &Idx);

  /// \brief Read a C++ base specifier.
  ClassBaseSpecifier ReadClassBaseSpecifier(PerFileData &F,
                                        const RecordData &Record,unsigned &Idx);

  /// \brief Read a source location from raw form.
  SourceLocation ReadSourceLocation(PerFileData &Module, unsigned Raw) {
    (void)Module; // No remapping yet
    return SourceLocation::getFromRawEncoding(Raw);
  }

  /// \brief Read a source location.
  SourceLocation ReadSourceLocation(PerFileData &Module,
                                    const RecordData &Record, unsigned& Idx) {
    return ReadSourceLocation(Module, Record[Idx++]);
  }

  /// \brief Read a source range.
  SourceRange ReadSourceRange(PerFileData &F,
                              const RecordData &Record, unsigned& Idx);

  /// \brief Read an integral value
  llvm::APInt ReadAPInt(const RecordData &Record, unsigned &Idx);

  /// \brief Read a signed integral value
  llvm::APSInt ReadAPSInt(const RecordData &Record, unsigned &Idx);

  /// \brief Read a floating-point value
  llvm::APFloat ReadAPFloat(const RecordData &Record, unsigned &Idx);

  // \brief Read a string
  std::string ReadString(const RecordData &Record, unsigned &Idx);

  /// \brief Read a version tuple.
  VersionTuple ReadVersionTuple(const RecordData &Record, unsigned &Idx);

  /// \brief Reads a statement.
  Stmt *ReadStmt(PerFileData &F);

  /// \brief Reads an expression.
  Expr *ReadExpr(PerFileData &F);

  /// \brief Reads a sub-statement operand during statement reading.
  Stmt *ReadSubStmt() {
    assert(ReadingKind == Read_Stmt &&
           "Should be called only during statement reading!");
    // Subexpressions are stored from last to first, so the next Stmt we need
    // is at the back of the stack.
    assert(!StmtStack.empty() && "Read too many sub statements!");
    return StmtStack.pop_back_val();
  }

  /// \brief Reads a sub-expression operand during statement reading.
  Expr *ReadSubExpr();

  /// \brief Retrieve the AST context that this AST reader supplements.
  ASTContext *getContext() { return Context; }

  // \brief Contains declarations that were loaded before we have
  // access to a Sema object.
  llvm::SmallVector<NamedDefn *, 16> PreloadedDefns;

  /// \brief Retrieve the semantic analysis object used to analyze the
  /// translation unit in which the precompiled header is being
  /// imported.
  Sema *getSema() { return SemaObj; }

  /// \brief Retrieve the identifier table associated with the
  /// preprocessor.
  IdentifierTable &getIdentifierTable();

  /// \brief Record that the given ID maps to the given switch-case
  /// statement.
  void RecordSwitchCaseID(CaseCmd *SC, unsigned ID);

  /// \brief Retrieve the switch-case statement with the given ID.
  CaseCmd *getSwitchCaseWithID(unsigned ID);

  void ClearSwitchCaseIDs();
};

/// \brief Helper class that saves the current stream position and
/// then restores it when destroyed.
struct SavedStreamPosition {
  explicit SavedStreamPosition(llvm::BitstreamCursor &Cursor)
  : Cursor(Cursor), Offset(Cursor.GetCurrentBitNo()) { }

  ~SavedStreamPosition() {
    Cursor.JumpToBit(Offset);
  }

private:
  llvm::BitstreamCursor &Cursor;
  uint64_t Offset;
};

} // end namespace mlang

#endif /* MLANG_SERIALIZATION_AST_READER_H_ */
