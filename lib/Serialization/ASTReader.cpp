//===--- ASTReader.cpp - AST File Reader ------------------------*- C++ -*-===//
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

#include "mlang/Serialization/ASTReader.h"
#include "mlang/Serialization/ASTDeserializationListener.h"
#include "ASTCommon.h"
#include "mlang/Frontend/FrontendDiagnostic.h"
#include "mlang/Frontend/Utils.h"
#include "mlang/Sema/Sema.h"
#include "mlang/Sema/Scope.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/NestedNameSpecifier.h"
#include "mlang/AST/Type.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Lex/ImportSearch.h"
#include "mlang/Basic/OnDiskHashTable.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/SourceManagerInternals.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Basic/FileSystemStatCache.h"
#include "mlang/Basic/TargetInfo.h"
//#include "mlang/Basic/Version.h"
//#include "mlang/Basic/VersionTuple.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Optional.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/system_error.h"
#include <algorithm>
#include <iterator>
#include <cstdio>
#include <sys/stat.h>

using namespace mlang;
using namespace mlang::serialization;

//===----------------------------------------------------------------------===//
// ASTReaderListener implementation
//===----------------------------------------------------------------------===//
ASTReaderListener::~ASTReaderListener() {}

//===----------------------------------------------------------------------===//
// AST reader implementation
//===----------------------------------------------------------------------===//
void
ASTReader::setDeserializationListener(ASTDeserializationListener *Listener) {
  DeserializationListener = Listener;
}


namespace mlang {
class ASTIdentifierLookupTrait {
  ASTReader &Reader;
  ASTReader::PerFileData &F;

  // If we know the IdentifierInfo in advance, it is here and we will
  // not build a new one. Used when deserializing information about an
  // identifier that was constructed before the AST file was read.
  IdentifierInfo *KnownII;

public:
  typedef IdentifierInfo * data_type;

  typedef const std::pair<const char*, unsigned> external_key_type;

  typedef external_key_type internal_key_type;

  ASTIdentifierLookupTrait(ASTReader &Reader, ASTReader::PerFileData &F,
                           IdentifierInfo *II = 0)
    : Reader(Reader), F(F), KnownII(II) { }

  static bool EqualKey(const internal_key_type& a,
                       const internal_key_type& b) {
    return (a.second == b.second) ? memcmp(a.first, b.first, a.second) == 0
                                  : false;
  }

  static unsigned ComputeHash(const internal_key_type& a) {
    return llvm::HashString(llvm::StringRef(a.first, a.second));
  }

  // This hopefully will just get inlined and removed by the optimizer.
  static const internal_key_type&
  GetInternalKey(const external_key_type& x) { return x; }

  // This hopefully will just get inlined and removed by the optimizer.
  static const external_key_type&
  GetExternalKey(const internal_key_type& x) { return x; }

  static std::pair<unsigned, unsigned>
  ReadKeyDataLength(const unsigned char*& d) {
    using namespace mlang::io;
    unsigned DataLen = ReadUnalignedLE16(d);
    unsigned KeyLen = ReadUnalignedLE16(d);
    return std::make_pair(KeyLen, DataLen);
  }

  static std::pair<const char*, unsigned>
  ReadKey(const unsigned char* d, unsigned n) {
    assert(n >= 2 && d[n-1] == '\0');
    return std::make_pair((const char*) d, n-1);
  }

  IdentifierInfo *ReadData(const internal_key_type& k,
                           const unsigned char* d,
                           unsigned DataLen) {
    using namespace mlang::io;
    IdentID ID = ReadUnalignedLE32(d);
    bool IsInteresting = ID & 0x01;

    // Wipe out the "is interesting" bit.
    ID = ID >> 1;

    if (!IsInteresting) {
      // For uninteresting identifiers, just build the IdentifierInfo
      // and associate it with the persistent ID.
      IdentifierInfo *II = KnownII;
      if (!II)
        II = &Reader.getIdentifierTable().getOwn(
        		llvm::StringRef(k.first, k.second));
      Reader.SetIdentifierInfo(ID, II);
      II->setIsFromAST();
      return II;
    }

    unsigned Bits = ReadUnalignedLE16(d);
//    bool CPlusPlusOperatorKeyword = Bits & 0x01;
    Bits >>= 1;
    bool HasRevertedTokenIDToIdentifier = Bits & 0x01;
    Bits >>= 1;
 //   bool Poisoned = Bits & 0x01;
    Bits >>= 1;
//    bool ExtensionToken = Bits & 0x01;
    Bits >>= 1;
//    bool hasMacroDefinition = Bits & 0x01;
    Bits >>= 1;
    unsigned BuiltinID = Bits & 0x3FF;
    Bits >>= 10;

    assert(Bits == 0 && "Extra bits in the identifier?");
    DataLen -= 6;

    // Build the IdentifierInfo itself and link the identifier ID with
    // the new IdentifierInfo.
    IdentifierInfo *II = KnownII;
    if (!II)
      II = &Reader.getIdentifierTable().getOwn(
    		  llvm::StringRef(k.first, k.second));
    Reader.SetIdentifierInfo(ID, II);

    // Set or check the various bits in the IdentifierInfo structure.
    // Token IDs are read-only.
    if (HasRevertedTokenIDToIdentifier)
      II->RevertTokenIDToIdentifier();
    II->setBuiltinID(BuiltinID);
//    assert(II->isExtensionToken() == ExtensionToken &&
//           "Incorrect extension token flag");
//    (void)ExtensionToken;
//    II->setIsPoisoned(Poisoned);
//    assert(II->isCPlusPlusOperatorKeyword() == CPlusPlusOperatorKeyword &&
//           "Incorrect C++ operator keyword flag");
//    (void)CPlusPlusOperatorKeyword;

    // Read all of the declarations visible at global scope with this
    // name.
    if (Reader.getContext() == 0) return II;
    if (DataLen > 0) {
      llvm::SmallVector<uint32_t, 4> DefnIDs;
      for (; DataLen > 0; DataLen -= 4)
        DefnIDs.push_back(ReadUnalignedLE32(d));
      Reader.SetGloballyVisibleDefns(II, DefnIDs);
    }

    II->setIsFromAST();
    return II;
  }
};

} // end anonymous namespace

/// \brief The on-disk hash table used to contain information about
/// all of the identifiers in the program.
typedef OnDiskChainedHashTable<ASTIdentifierLookupTrait>
  ASTIdentifierLookupTable;

namespace {
class ASTDefnContextNameLookupTrait {
  ASTReader &Reader;

public:
  /// \brief Pair of begin/end iterators for DefnIDs.
  typedef std::pair<DefnID *, DefnID *> data_type;

  /// \brief Special internal key for declaration names.
  /// The hash table creates keys for comparison; we do not create
  /// a DefinitionName for the internal key to avoid deserializing types.
  struct DefnNameKey {
    DefinitionName::NameKind Kind;
    uint64_t Data;
    DefnNameKey() : Kind((DefinitionName::NameKind)0), Data(0) { }
  };

  typedef DefinitionName external_key_type;
  typedef DefnNameKey internal_key_type;

  explicit ASTDefnContextNameLookupTrait(ASTReader &Reader) : Reader(Reader) { }

  static bool EqualKey(const internal_key_type& a,
                       const internal_key_type& b) {
    return a.Kind == b.Kind && a.Data == b.Data;
  }

  unsigned ComputeHash(const DefnNameKey &Key) const {
    llvm::FoldingSetNodeID ID;
    ID.AddInteger(Key.Kind);

    switch (Key.Kind) {
    case DefinitionName::NK_Identifier:
    case DefinitionName::NK_LiteralOperatorName:
      ID.AddString(((IdentifierInfo*)Key.Data)->getName());
      break;
    case DefinitionName::NK_ConstructorName:
    case DefinitionName::NK_DestructorName:
      ID.AddInteger((TypeID)Key.Data);
      break;
    case DefinitionName::NK_OperatorName:
      ID.AddInteger((OverloadedOperatorKind)Key.Data);
      break;
    }

    return ID.ComputeHash();
  }

  internal_key_type GetInternalKey(const external_key_type& Name) const {
    DefnNameKey Key;
    Key.Kind = Name.getNameKind();
    switch (Name.getNameKind()) {
    case DefinitionName::NK_Identifier:
      Key.Data = (uint64_t)Name.getAsIdentifierInfo();
      break;
    case DefinitionName::NK_ConstructorName:
    case DefinitionName::NK_DestructorName:
      Key.Data = Reader.GetTypeID(Name.getClassNameType());
      break;
    case DefinitionName::NK_OperatorName:
//      Key.Data = Name.getClassOverloadedOperator();
      break;
    case DefinitionName::NK_LiteralOperatorName:
//      Key.Data = (uint64_t)Name.getClassLiteralIdentifier();
      break;
    }

    return Key;
  }

  external_key_type GetExternalKey(const internal_key_type& Key) const {
    ASTContext *Context = Reader.getContext();
    switch (Key.Kind) {
    case DefinitionName::NK_Identifier:
      return DefinitionName((IdentifierInfo*)Key.Data);

    case DefinitionName::NK_ConstructorName:
      return Context->DefinitionNames.getClassConstructorName(
                           Reader.GetType(Key.Data));

    case DefinitionName::NK_DestructorName:
      return Context->DefinitionNames.getClassDestructorName(
                           Reader.GetType(Key.Data));

    case DefinitionName::NK_OperatorName:
      return Context->DefinitionNames.getOperatorName(
                                         (OverloadedOperatorKind)Key.Data);

    case DefinitionName::NK_LiteralOperatorName:
      return Context->DefinitionNames.getLiteralOperatorName(
                                                     (IdentifierInfo*)Key.Data);
    }

    llvm_unreachable("Invalid Name Kind ?");
  }

  static std::pair<unsigned, unsigned>
  ReadKeyDataLength(const unsigned char*& d) {
    using namespace mlang::io;
    unsigned KeyLen = ReadUnalignedLE16(d);
    unsigned DataLen = ReadUnalignedLE16(d);
    return std::make_pair(KeyLen, DataLen);
  }

  internal_key_type ReadKey(const unsigned char* d, unsigned) {
    using namespace mlang::io;

    DefnNameKey Key;
    Key.Kind = (DefinitionName::NameKind)*d++;
    switch (Key.Kind) {
    case DefinitionName::NK_Identifier:
      Key.Data = (uint64_t)Reader.DecodeIdentifierInfo(ReadUnalignedLE32(d));
      break;
    case DefinitionName::NK_ConstructorName:
    case DefinitionName::NK_DestructorName:
      Key.Data = ReadUnalignedLE32(d); // TypeID
      break;
    case DefinitionName::NK_OperatorName:
      Key.Data = *d++; // OverloadedOperatorKind
      break;
    case DefinitionName::NK_LiteralOperatorName:
      Key.Data = (uint64_t)Reader.DecodeIdentifierInfo(ReadUnalignedLE32(d));
      break;
    }

    return Key;
  }

  data_type ReadData(internal_key_type, const unsigned char* d,
                     unsigned DataLen) {
    using namespace mlang::io;
    unsigned NumDefns = ReadUnalignedLE16(d);
    DefnID *Start = (DefnID *)d;
    return std::make_pair(Start, Start + NumDefns);
  }
};

} // end anonymous namespace

/// \brief The on-disk hash table used for the DefnContext's Name lookup table.
typedef OnDiskChainedHashTable<ASTDefnContextNameLookupTrait>
  ASTDefnContextNameLookupTable;

bool ASTReader::ReadDefnContextStorage(llvm::BitstreamCursor &Cursor,
                                   const std::pair<uint64_t, uint64_t> &Offsets,
                                       DefnContextInfo &Info) {
  SavedStreamPosition SavedPosition(Cursor);
  // First the lexical decls.
  if (Offsets.first != 0) {
    Cursor.JumpToBit(Offsets.first);

    RecordData Record;
    const char *Blob;
    unsigned BlobLen;
    unsigned Code = Cursor.ReadCode();
    unsigned RecCode = Cursor.ReadRecord(Code, Record, &Blob, &BlobLen);
    if (RecCode != DECL_CONTEXT_LEXICAL) {
      Error("Expected lexical block");
      return true;
    }

    Info.LexicalDefns = reinterpret_cast<const KindDefnIDPair*>(Blob);
    Info.NumLexicalDefns = BlobLen / sizeof(KindDefnIDPair);
  } else {
    Info.LexicalDefns = 0;
    Info.NumLexicalDefns = 0;
  }

  // Now the lookup table.
  if (Offsets.second != 0) {
    Cursor.JumpToBit(Offsets.second);

    RecordData Record;
    const char *Blob;
    unsigned BlobLen;
    unsigned Code = Cursor.ReadCode();
    unsigned RecCode = Cursor.ReadRecord(Code, Record, &Blob, &BlobLen);
    if (RecCode != DECL_CONTEXT_VISIBLE) {
      Error("Expected visible lookup table block");
      return true;
    }
    Info.NameLookupTableData
      = ASTDefnContextNameLookupTable::Create(
                    (const unsigned char *)Blob + Record[0],
                    (const unsigned char *)Blob,
                    ASTDefnContextNameLookupTrait(*this));
  } else {
    Info.NameLookupTableData = 0;
  }

  return false;
}

void ASTReader::Error(llvm::StringRef Msg) {
  Error(diag::err_fe_pch_malformed, Msg);
}

void ASTReader::Error(unsigned DiagID,
                      llvm::StringRef Arg1, llvm::StringRef Arg2) {
  if (Diags.isDiagnosticInFlight())
    Diags.SetDelayedDiagnostic(DiagID, Arg1, Arg2);
  else
    Diag(DiagID) << Arg1 << Arg2;
}

//===----------------------------------------------------------------------===//
// Source Manager Deserialization
//===----------------------------------------------------------------------===//

/// \brief Read the line table in the source manager block.
/// \returns true if there was an error.
bool ASTReader::ParseLineTable(PerFileData &F,
                               llvm::SmallVectorImpl<uint64_t> &Record) {
  unsigned Idx = 0;
  LineTableInfo &LineTable = SourceMgr.getLineTable();

  // Parse the file names
  std::map<int, int> FileIDs;
  for (int I = 0, N = Record[Idx++]; I != N; ++I) {
    // Extract the file name
    unsigned FilenameLen = Record[Idx++];
    std::string Filename(&Record[Idx], &Record[Idx] + FilenameLen);
    Idx += FilenameLen;
    MaybeAddSystemRootToFilename(Filename);
    FileIDs[I] = LineTable.getLineTableFilenameID(Filename);
  }

  // Parse the line entries
  std::vector<LineEntry> Entries;
  while (Idx < Record.size()) {
    int FID = Record[Idx++];

    // Extract the line entries
    unsigned NumEntries = Record[Idx++];
    assert(NumEntries && "Numentries is 00000");
    Entries.clear();
    Entries.reserve(NumEntries);
    for (unsigned I = 0; I != NumEntries; ++I) {
      unsigned FileOffset = Record[Idx++];
      unsigned LineNo = Record[Idx++];
      int FilenameID = FileIDs[Record[Idx++]];
      SrcMgr::CharacteristicKind FileKind
        = (SrcMgr::CharacteristicKind)Record[Idx++];
      unsigned IncludeOffset = Record[Idx++];
      Entries.push_back(LineEntry::get(FileOffset, LineNo, FilenameID,
                                       FileKind, IncludeOffset));
    }
    LineTable.AddEntry(FID, Entries);
  }

  return false;
}

namespace {

class ASTStatData {
public:
  const ino_t ino;
  const dev_t dev;
  const mode_t mode;
  const time_t mtime;
  const off_t size;

  ASTStatData(ino_t i, dev_t d, mode_t mo, time_t m, off_t s)
    : ino(i), dev(d), mode(mo), mtime(m), size(s) {}
};

class ASTStatLookupTrait {
 public:
  typedef const char *external_key_type;
  typedef const char *internal_key_type;

  typedef ASTStatData data_type;

  static unsigned ComputeHash(const char *path) {
    return llvm::HashString(path);
  }

  static internal_key_type GetInternalKey(const char *path) { return path; }

  static bool EqualKey(internal_key_type a, internal_key_type b) {
    return strcmp(a, b) == 0;
  }

  static std::pair<unsigned, unsigned>
  ReadKeyDataLength(const unsigned char*& d) {
    unsigned KeyLen = (unsigned) mlang::io::ReadUnalignedLE16(d);
    unsigned DataLen = (unsigned) *d++;
    return std::make_pair(KeyLen + 1, DataLen);
  }

  static internal_key_type ReadKey(const unsigned char *d, unsigned) {
    return (const char *)d;
  }

  static data_type ReadData(const internal_key_type, const unsigned char *d,
                            unsigned /*DataLen*/) {
    using namespace mlang::io;

    ino_t ino = (ino_t) ReadUnalignedLE32(d);
    dev_t dev = (dev_t) ReadUnalignedLE32(d);
    mode_t mode = (mode_t) ReadUnalignedLE16(d);
    time_t mtime = (time_t) ReadUnalignedLE64(d);
    off_t size = (off_t) ReadUnalignedLE64(d);
    return data_type(ino, dev, mode, mtime, size);
  }
};

/// \brief stat() cache for precompiled headers.
///
/// This cache is very similar to the stat cache used by pretokenized
/// headers.
class ASTStatCache : public FileSystemStatCache {
  typedef OnDiskChainedHashTable<ASTStatLookupTrait> CacheTy;
  CacheTy *Cache;

  unsigned &NumStatHits, &NumStatMisses;
public:
  ASTStatCache(const unsigned char *Buckets, const unsigned char *Base,
               unsigned &NumStatHits, unsigned &NumStatMisses)
    : Cache(0), NumStatHits(NumStatHits), NumStatMisses(NumStatMisses) {
    Cache = CacheTy::Create(Buckets, Base);
  }

  ~ASTStatCache() { delete Cache; }

  LookupResult getStat(const char *Path, struct stat &StatBuf,
                       int *FileDescriptor) {
    // Do the lookup for the file's data in the AST file.
    CacheTy::iterator I = Cache->find(Path);

    // If we don't get a hit in the AST file just forward to 'stat'.
    if (I == Cache->end()) {
      ++NumStatMisses;
      return statChained(Path, StatBuf, FileDescriptor);
    }

    ++NumStatHits;
    ASTStatData Data = *I;

    StatBuf.st_ino = Data.ino;
    StatBuf.st_dev = Data.dev;
    StatBuf.st_mtime = Data.mtime;
    StatBuf.st_mode = Data.mode;
    StatBuf.st_size = Data.size;
    return CacheExists;
  }
};
} // end anonymous namespace


/// \brief Read a source manager block
ASTReader::ASTReadResult ASTReader::ReadSourceManagerBlock(PerFileData &F) {
  using namespace SrcMgr;

  llvm::BitstreamCursor &SLocEntryCursor = F.SLocEntryCursor;

  // Set the source-location entry cursor to the current position in
  // the stream. This cursor will be used to read the contents of the
  // source manager block initially, and then lazily read
  // source-location entries as needed.
  SLocEntryCursor = F.Stream;

  // The stream itself is going to skip over the source manager block.
  if (F.Stream.SkipBlock()) {
    Error("malformed block record in AST file");
    return Failure;
  }

  // Enter the source manager block.
  if (SLocEntryCursor.EnterSubBlock(SOURCE_MANAGER_BLOCK_ID)) {
    Error("malformed source manager block record in AST file");
    return Failure;
  }

  RecordData Record;
  while (true) {
    unsigned Code = SLocEntryCursor.ReadCode();
    if (Code == llvm::bitc::END_BLOCK) {
      if (SLocEntryCursor.ReadBlockEnd()) {
        Error("error at end of Source Manager block in AST file");
        return Failure;
      }
      return Success;
    }

    if (Code == llvm::bitc::ENTER_SUBBLOCK) {
      // No known subblocks, always skip them.
      SLocEntryCursor.ReadSubBlockID();
      if (SLocEntryCursor.SkipBlock()) {
        Error("malformed block record in AST file");
        return Failure;
      }
      continue;
    }

    if (Code == llvm::bitc::DEFINE_ABBREV) {
      SLocEntryCursor.ReadAbbrevRecord();
      continue;
    }

    // Read a record.
    const char *BlobStart;
    unsigned BlobLen;
    Record.clear();
    switch (SLocEntryCursor.ReadRecord(Code, Record, &BlobStart, &BlobLen)) {
    default:  // Default behavior: ignore.
      break;

    case SM_LINE_TABLE:
      if (ParseLineTable(F, Record))
        return Failure;
      break;

    case SM_SLOC_FILE_ENTRY:
    case SM_SLOC_BUFFER_ENTRY:
    case SM_SLOC_INSTANTIATION_ENTRY:
      // Once we hit one of the source location entries, we're done.
      return Success;
    }
  }
}

/// \brief If a header file is not found at the path that we expect it to be
/// and the PCH file was moved from its original location, try to resolve the
/// file by assuming that header+PCH were moved together and the header is in
/// the same place relative to the PCH.
static std::string
resolveFileRelativeToOriginalDir(const std::string &Filename,
                                 const std::string &OriginalDir,
                                 const std::string &CurrDir) {
  assert(OriginalDir != CurrDir &&
         "No point trying to resolve the file if the PCH dir didn't change");
  using namespace llvm::sys;
  llvm::SmallString<128> filePath(Filename);
  fs::make_absolute(filePath);
  assert(path::is_absolute(OriginalDir));
  llvm::SmallString<128> currPCHPath(CurrDir);

  path::const_iterator fileDirI = path::begin(path::parent_path(filePath)),
                       fileDirE = path::end(path::parent_path(filePath));
  path::const_iterator origDirI = path::begin(OriginalDir),
                       origDirE = path::end(OriginalDir);
  // Skip the common path components from filePath and OriginalDir.
  while (fileDirI != fileDirE && origDirI != origDirE &&
         *fileDirI == *origDirI) {
    ++fileDirI;
    ++origDirI;
  }
  for (; origDirI != origDirE; ++origDirI)
    path::append(currPCHPath, "..");
  path::append(currPCHPath, fileDirI, fileDirE);
  path::append(currPCHPath, path::filename(Filename));
  return currPCHPath.str();
}

/// \brief Get a cursor that's correctly positioned for reading the source
/// location entry with the given ID.
ASTReader::PerFileData *ASTReader::SLocCursorForID(unsigned ID) {
  assert(ID != 0 && ID <= TotalNumSLocEntries &&
         "SLocCursorForID should only be called for real IDs.");

  ID -= 1;
  PerFileData *F = 0;
  for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
    F = Chain[N - I - 1];
    if (ID < F->LocalNumSLocEntries)
      break;
    ID -= F->LocalNumSLocEntries;
  }
  assert(F && F->LocalNumSLocEntries > ID && "Chain corrupted");

  F->SLocEntryCursor.JumpToBit(F->SLocOffsets[ID]);
  return F;
}

/// \brief Read in the source location entry with the given ID.
ASTReader::ASTReadResult ASTReader::ReadSLocEntryRecord(unsigned ID) {
  if (ID == 0)
    return Success;

  if (ID > TotalNumSLocEntries) {
    Error("source location entry ID out-of-range for AST file");
    return Failure;
  }

  PerFileData *F = SLocCursorForID(ID);
  llvm::BitstreamCursor &SLocEntryCursor = F->SLocEntryCursor;

  ++NumSLocEntriesRead;
  unsigned Code = SLocEntryCursor.ReadCode();
  if (Code == llvm::bitc::END_BLOCK ||
      Code == llvm::bitc::ENTER_SUBBLOCK ||
      Code == llvm::bitc::DEFINE_ABBREV) {
    Error("incorrectly-formatted source location entry in AST file");
    return Failure;
  }

  RecordData Record;
  const char *BlobStart;
  unsigned BlobLen;
  switch (SLocEntryCursor.ReadRecord(Code, Record, &BlobStart, &BlobLen)) {
  default:
    Error("incorrectly-formatted source location entry in AST file");
    return Failure;

  case SM_SLOC_FILE_ENTRY: {
    std::string Filename(BlobStart, BlobStart + BlobLen);
    MaybeAddSystemRootToFilename(Filename);
    const FileEntry *File = FileMgr.getFile(Filename);
    if (File == 0 && !OriginalDir.empty() && !CurrentDir.empty() &&
        OriginalDir != CurrentDir) {
      std::string resolved = resolveFileRelativeToOriginalDir(Filename,
                                                              OriginalDir,
                                                              CurrentDir);
      if (!resolved.empty())
        File = FileMgr.getFile(resolved);
    }
    if (File == 0)
      File = FileMgr.getVirtualFile(Filename, (off_t)Record[4],
                                    (time_t)Record[5]);
    if (File == 0) {
      std::string ErrorStr = "could not find file '";
      ErrorStr += Filename;
      ErrorStr += "' referenced by AST file";
      Error(ErrorStr.c_str());
      return Failure;
    }

    if (Record.size() < 6) {
      Error("source location entry is incorrect");
      return Failure;
    }

    FileID FID = SourceMgr.createFileID(File, ReadSourceLocation(*F, Record[1]),
                                        (SrcMgr::CharacteristicKind)Record[2],
                                        ID, Record[0]);
    if (Record[3])
      const_cast<SrcMgr::FileInfo&>(SourceMgr.getSLocEntry(FID).getFile())
        .setHasLineDirectives();
    
    break;
  }

  case SM_SLOC_BUFFER_ENTRY: {
    const char *Name = BlobStart;
    unsigned Offset = Record[0];
    unsigned Code = SLocEntryCursor.ReadCode();
    Record.clear();
    unsigned RecCode
      = SLocEntryCursor.ReadRecord(Code, Record, &BlobStart, &BlobLen);

    if (RecCode != SM_SLOC_BUFFER_BLOB) {
      Error("AST record has invalid code");
      return Failure;
    }

    llvm::MemoryBuffer *Buffer
    = llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(BlobStart, BlobLen - 1),
                                       Name);
    FileID BufferID = SourceMgr.createFileIDForMemBuffer(Buffer, ID, Offset);

    break;
  }

  case SM_SLOC_INSTANTIATION_ENTRY: {
    SourceLocation SpellingLoc = ReadSourceLocation(*F, Record[1]);
    SourceMgr.createInstantiationLoc(SpellingLoc,
                                     ReadSourceLocation(*F, Record[2]),
                                     ReadSourceLocation(*F, Record[3]),
                                     Record[4],
                                     ID,
                                     Record[0]);
    break;
  }
  }

  return Success;
}

/// ReadBlockAbbrevs - Enter a subblock of the specified BlockID with the
/// specified cursor.  Read the abbreviations that are at the top of the block
/// and then leave the cursor pointing into the block.
bool ASTReader::ReadBlockAbbrevs(llvm::BitstreamCursor &Cursor,
                                 unsigned BlockID) {
  if (Cursor.EnterSubBlock(BlockID)) {
    Error("malformed block record in AST file");
    return Failure;
  }

  while (true) {
    uint64_t Offset = Cursor.GetCurrentBitNo();
    unsigned Code = Cursor.ReadCode();

    // We expect all abbrevs to be at the start of the block.
    if (Code != llvm::bitc::DEFINE_ABBREV) {
      Cursor.JumpToBit(Offset);
      return false;
    }
    Cursor.ReadAbbrevRecord();
  }
}

namespace {
  /// \brief Trait class used to search the on-disk hash table containing all of
  /// the header search information.
  ///
  /// The on-disk hash table contains a mapping from each header path to 
  /// information about that header (how many times it has been included, its
  /// controlling macro, etc.). Note that we actually hash based on the 
  /// filename, and support "deep" comparisons of file names based on current
  /// inode numbers, so that the search can cope with non-normalized path names
  /// and symlinks.
  class HeaderFileInfoTrait {
    const char *SearchPath;
    struct stat SearchPathStatBuf;
    llvm::Optional<int> SearchPathStatResult;
    
    int StatSimpleCache(const char *Path, struct stat *StatBuf) {
      if (Path == SearchPath) {
        if (!SearchPathStatResult)
          SearchPathStatResult = stat(Path, &SearchPathStatBuf);
        
        *StatBuf = SearchPathStatBuf;
        return *SearchPathStatResult;
      }
      
      return stat(Path, StatBuf);
    }
    
  public:
    typedef const char *external_key_type;
    typedef const char *internal_key_type;
    
    typedef ImportedFileInfo data_type;
    
    HeaderFileInfoTrait(const char *SearchPath = 0) : SearchPath(SearchPath) { }
    
    static unsigned ComputeHash(const char *path) {
      return llvm::HashString(llvm::sys::path::filename(path));
    }
    
    static internal_key_type GetInternalKey(const char *path) { return path; }
    
    bool EqualKey(internal_key_type a, internal_key_type b) {
      if (strcmp(a, b) == 0)
        return true;
      
      if (llvm::sys::path::filename(a) != llvm::sys::path::filename(b))
        return false;
      
      // The file names match, but the path names don't. stat() the files to
      // see if they are the same.      
      struct stat StatBufA, StatBufB;
      if (StatSimpleCache(a, &StatBufA) || StatSimpleCache(b, &StatBufB))
        return false;
      
      return StatBufA.st_ino == StatBufB.st_ino;
    }
    
    static std::pair<unsigned, unsigned>
    ReadKeyDataLength(const unsigned char*& d) {
      unsigned KeyLen = (unsigned) mlang::io::ReadUnalignedLE16(d);
      unsigned DataLen = (unsigned) *d++;
      return std::make_pair(KeyLen + 1, DataLen);
    }
    
    static internal_key_type ReadKey(const unsigned char *d, unsigned) {
      return (const char *)d;
    }
    
    static data_type ReadData(const internal_key_type, const unsigned char *d,
                              unsigned DataLen) {
      const unsigned char *End = d + DataLen;
      using namespace mlang::io;
      ImportedFileInfo HFI;
      unsigned Flags = *d++;
      HFI.isImport = (Flags >> 4) & 0x01;
      HFI.isPragmaOnce = (Flags >> 3) & 0x01;
      HFI.DirInfo = (Flags >> 1) & 0x03;
      HFI.Resolved = Flags & 0x01;
      HFI.NumIncludes = ReadUnalignedLE16(d);
      HFI.ControllingMacroID = ReadUnalignedLE32(d);
      assert(End == d && "Wrong data length in HeaderFileInfo deserialization");
      (void)End;
      
      // This HeaderFileInfo was externally loaded.
      HFI.External = true;
      return HFI;
    }
  };
}

/// \brief The on-disk hash table used for the global method pool.
typedef OnDiskChainedHashTable<HeaderFileInfoTrait>
  HeaderFileInfoLookupTable;

/// \brief If we are loading a relocatable PCH file, and the filename is
/// not an absolute path, add the system root to the beginning of the file
/// name.
void ASTReader::MaybeAddSystemRootToFilename(std::string &Filename) {
  if (Filename.empty() || llvm::sys::path::is_absolute(Filename))
    return;

  if (isysroot == 0) {
    // If no system root was given, default to '/'
    Filename.insert(Filename.begin(), '/');
    return;
  }

  unsigned Length = strlen(isysroot);
  if (isysroot[Length - 1] != '/')
    Filename.insert(Filename.begin(), '/');

  Filename.insert(Filename.begin(), isysroot, isysroot + Length);
}

ASTReader::ASTReadResult
ASTReader::ReadASTBlock(PerFileData &F) {
  llvm::BitstreamCursor &Stream = F.Stream;

  if (Stream.EnterSubBlock(AST_BLOCK_ID)) {
    Error("malformed block record in AST file");
    return Failure;
  }

  // Read all of the records and blocks for the ASt file.
  RecordData Record;
  bool First = true;
  while (!Stream.AtEndOfStream()) {
    unsigned Code = Stream.ReadCode();
    if (Code == llvm::bitc::END_BLOCK) {
      if (Stream.ReadBlockEnd()) {
        Error("error at end of module block in AST file");
        return Failure;
      }

      return Success;
    }

    if (Code == llvm::bitc::ENTER_SUBBLOCK) {
      switch (Stream.ReadSubBlockID()) {
      case DEFNTYPES_BLOCK_ID:
        // We lazily load the decls block, but we want to set up the
        // DefnsCursor cursor to point into it.  Clone our current bitcode
        // cursor to it, enter the block and read the abbrevs in that block.
        // With the main cursor, we just skip over it.
        F.DefnsCursor = Stream;
        if (Stream.SkipBlock() ||  // Skip with the main cursor.
            // Read the abbrevs.
            ReadBlockAbbrevs(F.DefnsCursor, DEFNTYPES_BLOCK_ID)) {
          Error("malformed block record in AST file");
          return Failure;
        }
        break;

      case DECL_UPDATES_BLOCK_ID:
        if (Stream.SkipBlock()) {
          Error("malformed block record in AST file");
          return Failure;
        }
        break;

     case SOURCE_MANAGER_BLOCK_ID:
        switch (ReadSourceManagerBlock(F)) {
        case Success:
          break;

        case Failure:
          Error("malformed source manager block in AST file");
          return Failure;

        case IgnorePCH:
          return IgnorePCH;
        }
        break;
      }
      First = false;
      continue;
    }

    if (Code == llvm::bitc::DEFINE_ABBREV) {
      Stream.ReadAbbrevRecord();
      continue;
    }

    // Read and process a record.
    Record.clear();
    const char *BlobStart = 0;
    unsigned BlobLen = 0;
    switch ((ASTRecordTypes)Stream.ReadRecord(Code, Record,
                                              &BlobStart, &BlobLen)) {
    default:  // Default behavior: ignore.
      break;

    case METADATA:
      break;

    case CHAINED_METADATA:
      break;

    case TYPE_OFFSET:
      if (F.LocalNumTypes != 0) {
        Error("duplicate TYPE_OFFSET record in AST file");
        return Failure;
      }
      F.TypeOffsets = (const uint32_t *)BlobStart;
      F.LocalNumTypes = Record[0];
      break;

    case DECL_OFFSET:
      if (F.LocalNumDefns != 0) {
        Error("duplicate DECL_OFFSET record in AST file");
        return Failure;
      }
      F.DefnOffsets = (const uint32_t *)BlobStart;
      F.LocalNumDefns = Record[0];
      break;

    case TU_UPDATE_LEXICAL: {
      DefnContextInfo Info = {
        /* No visible information */ 0,
        reinterpret_cast<const KindDefnIDPair *>(BlobStart),
        BlobLen / sizeof(KindDefnIDPair)
      };
      DefnContextOffsets[Context ? Context->getTranslationUnitDefn() : 0]
        .push_back(Info);
      break;
    }

    case UPDATE_VISIBLE: {
      serialization::DefnID ID = Record[0];
      void *Table = ASTDefnContextNameLookupTable::Create(
                        (const unsigned char *)BlobStart + Record[1],
                        (const unsigned char *)BlobStart,
                        ASTDefnContextNameLookupTrait(*this));
      if (ID == 1 && Context) { // Is it the TU?
        DefnContextInfo Info = {
          Table, /* No lexical inforamtion */ 0, 0
        };
        DefnContextOffsets[Context->getTranslationUnitDefn()].push_back(Info);
      } else
        PendingVisibleUpdates[ID].push_back(Table);
      break;
    }

    case REDECLS_UPDATE_LATEST: {
      assert(Record.size() % 2 == 0 && "Expected pairs of DefnIDs");
      for (unsigned i = 0, e = Record.size(); i < e; i += 2) {
        DefnID First = Record[i], Latest = Record[i+1];
        assert((FirstLatestDefnIDs.find(First) == FirstLatestDefnIDs.end() ||
                Latest > FirstLatestDefnIDs[First]) &&
               "The new latest is supposed to come after the previous latest");
        FirstLatestDefnIDs[First] = Latest;
      }
      break;
    }

    case LANGUAGE_OPTIONS:
      if (ParseLanguageOptions(Record))
        return IgnorePCH;
      break;

    case IDENTIFIER_TABLE:
      F.IdentifierTableData = BlobStart;
      if (Record[0]) {
        F.IdentifierLookupTable
          = ASTIdentifierLookupTable::Create(
                       (const unsigned char *)F.IdentifierTableData + Record[0],
                       (const unsigned char *)F.IdentifierTableData,
                       ASTIdentifierLookupTrait(*this, F));
        if (PP)
          PP->getIdentifierTable().setExternalIdentifierLookup(this);
      }
      break;

    case IDENTIFIER_OFFSET:
      if (F.LocalNumIdentifiers != 0) {
        Error("duplicate IDENTIFIER_OFFSET record in AST file");
        return Failure;
      }
      F.IdentifierOffsets = (const uint32_t *)BlobStart;
      F.LocalNumIdentifiers = Record[0];
      break;

    case EXTERNAL_DEFINITIONS:
      // Optimization for the first block.
      if (ExternalDefinitions.empty())
        ExternalDefinitions.swap(Record);
      else
        ExternalDefinitions.insert(ExternalDefinitions.end(),
                                   Record.begin(), Record.end());
      break;

    case SPECIAL_TYPES:
      // Optimization for the first block
      if (SpecialTypes.empty())
        SpecialTypes.swap(Record);
      else
        SpecialTypes.insert(SpecialTypes.end(), Record.begin(), Record.end());
      break;

    case STATISTICS:
      TotalNumStatements += Record[0];
      TotalLexicalDefnContexts += Record[2];
      TotalVisibleDefnContexts += Record[3];
      break;

    case UNUSED_FILESCOPED_DECLS:
      // Optimization for the first block.
      if (UnusedFileScopedDefns.empty())
        UnusedFileScopedDefns.swap(Record);
      else
        UnusedFileScopedDefns.insert(UnusedFileScopedDefns.end(),
                                     Record.begin(), Record.end());
      break;

    case DELEGATING_CTORS:
      // Optimization for the first block.
      if (DelegatingCtorDefns.empty())
        DelegatingCtorDefns.swap(Record);
      else
        DelegatingCtorDefns.insert(DelegatingCtorDefns.end(),
                                   Record.begin(), Record.end());
      break;

    case WEAK_UNDECLARED_IDENTIFIERS:
      // Later blocks overwrite earlier ones.
      WeakUndeclaredIdentifiers.swap(Record);
      break;

    case LOCALLY_SCOPED_EXTERNAL_DECLS:
      // Optimization for the first block.
      if (LocallyScopedExternalDefns.empty())
        LocallyScopedExternalDefns.swap(Record);
      else
        LocallyScopedExternalDefns.insert(LocallyScopedExternalDefns.end(),
                                          Record.begin(), Record.end());
      break;

    case PP_COUNTER_VALUE:
//      if (!Record.empty() && Listener)
//        Listener->ReadCounter(Record[0]);
      break;

    case SOURCE_LOCATION_OFFSETS:
      F.SLocOffsets = (const uint32_t *)BlobStart;
      F.LocalNumSLocEntries = Record[0];
      F.LocalSLocSize = Record[1];
      break;

    case SOURCE_LOCATION_PRELOADS:
      if (PreloadSLocEntries.empty())
        PreloadSLocEntries.swap(Record);
      else
        PreloadSLocEntries.insert(PreloadSLocEntries.end(),
            Record.begin(), Record.end());
      break;

    case STAT_CACHE: {
      if (!DisableStatCache) {
        ASTStatCache *MyStatCache =
          new ASTStatCache((const unsigned char *)BlobStart + Record[0],
                           (const unsigned char *)BlobStart,
                           NumStatHits, NumStatMisses);
        FileMgr.addStatCache(MyStatCache);
        F.StatCache = MyStatCache;
      }
      break;
    }

    case EXT_VECTOR_DECLS:
      // Optimization for the first block.
      if (ExtVectorDefns.empty())
        ExtVectorDefns.swap(Record);
      else
        ExtVectorDefns.insert(ExtVectorDefns.end(),
                              Record.begin(), Record.end());
      break;

    case VTABLE_USES:
      // Later tables overwrite earlier ones.
      VTableUses.swap(Record);
      break;

    case DYNAMIC_CLASSES:
      // Optimization for the first block.
      if (DynamicClasses.empty())
        DynamicClasses.swap(Record);
      else
        DynamicClasses.insert(DynamicClasses.end(),
                              Record.begin(), Record.end());
      break;

    case PENDING_IMPLICIT_INSTANTIATIONS:
      F.PendingInstantiations.swap(Record);
      break;

    case SEMA_DECL_REFS:
      // Later tables overwrite earlier ones.
      SemaDefnRefs.swap(Record);
      break;

    case ORIGINAL_FILE_NAME:
      // The primary AST will be the last to get here, so it will be the one
      // that's used.
      ActualOriginalFileName.assign(BlobStart, BlobLen);
      OriginalFileName = ActualOriginalFileName;
      MaybeAddSystemRootToFilename(OriginalFileName);
      break;

    case ORIGINAL_FILE_ID:
      OriginalFileID = FileID::get(Record[0]);
      break;
        
    case ORIGINAL_PCH_DIR:
      // The primary AST will be the last to get here, so it will be the one
      // that's used.
      OriginalDir.assign(BlobStart, BlobLen);
      break;

    case VERSION_CONTROL_BRANCH_REVISION: {
//      const std::string &CurBranch = getClangFullRepositoryVersion();
//      llvm::StringRef ASTBranch(BlobStart, BlobLen);
//      if (llvm::StringRef(CurBranch) != ASTBranch && !DisableValidation) {
//        Diag(diag::warn_pch_different_branch) << ASTBranch << CurBranch;
//        return IgnorePCH;
//      }
      break;
    }

    case DECL_UPDATE_OFFSETS: {
      if (Record.size() % 2 != 0) {
        Error("invalid DECL_UPDATE_OFFSETS block in AST file");
        return Failure;
      }
      for (unsigned I = 0, N = Record.size(); I != N; I += 2)
        DefnUpdateOffsets[static_cast<DefnID>(Record[I])]
            .push_back(std::make_pair(&F, Record[I+1]));
      break;
    }

    case DECL_REPLACEMENTS: {
      if (Record.size() % 2 != 0) {
        Error("invalid DECL_REPLACEMENTS block in AST file");
        return Failure;
      }
      for (unsigned I = 0, N = Record.size(); I != N; I += 2)
        ReplacedDefns[static_cast<DefnID>(Record[I])] =
            std::make_pair(&F, Record[I+1]);
      break;
    }
        
    case CXX_BASE_SPECIFIER_OFFSETS: {
      if (F.LocalNumClassBaseSpecifiers != 0) {
        Error("duplicate Class_BASE_SPECIFIER_OFFSETS record in AST file");
        return Failure;
      }
      
      F.LocalNumClassBaseSpecifiers = Record[0];
      F.ClassBaseSpecifiersOffsets = (const uint32_t *)BlobStart;
      break;
    }

    case DIAG_PRAGMA_MAPPINGS:
      if (Record.size() % 2 != 0) {
        Error("invalid DIAG_USER_MAPPINGS block in AST file");
        return Failure;
      }
      if (PragmaDiagMappings.empty())
        PragmaDiagMappings.swap(Record);
      else
        PragmaDiagMappings.insert(PragmaDiagMappings.end(),
                                Record.begin(), Record.end());
      break;
        
    case CUDA_SPECIAL_DECL_REFS:
      // Later tables overwrite earlier ones.
      CUDASpecialDefnRefs.swap(Record);
      break;

    case HEADER_SEARCH_TABLE:
      F.HeaderFileInfoTableData = BlobStart;
      F.LocalNumHeaderFileInfos = Record[1];
      if (Record[0]) {
        F.HeaderFileInfoTable
          = HeaderFileInfoLookupTable::Create(
                   (const unsigned char *)F.HeaderFileInfoTableData + Record[0],
                   (const unsigned char *)F.HeaderFileInfoTableData);
        if (PP)
          PP->getImportSearchInfo().SetExternalSource(this);
      }
      break;

    case FP_PRAGMA_OPTIONS:
      // Later tables overwrite earlier ones.
      FPPragmaOptions.swap(Record);
      break;

    case OPENCL_EXTENSIONS:
      // Later tables overwrite earlier ones.
      OpenCLExtensions.swap(Record);
      break;

    case TENTATIVE_DEFINITIONS:
      // Optimization for the first block.
      if (TentativeDefinitions.empty())
        TentativeDefinitions.swap(Record);
      else
        TentativeDefinitions.insert(TentativeDefinitions.end(),
                                    Record.begin(), Record.end());
      break;
    }
    First = false;
  }
  Error("premature end of bitstream in AST file");
  return Failure;
}

ASTReader::ASTReadResult ASTReader::ReadAST(const std::string &FileName,
                                            ASTFileType Type) {
  switch(ReadASTCore(FileName, Type)) {
  case Failure: return Failure;
  case IgnorePCH: return IgnorePCH;
  case Success: break;
  }

  // Here comes stuff that we only do once the entire chain is loaded.

  // Allocate space for loaded slocentries, identifiers, decls and types.
  unsigned TotalNumIdentifiers = 0, TotalNumTypes = 0, TotalNumDefns = 0;
  for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
    TotalNumSLocEntries += Chain[I]->LocalNumSLocEntries;
    NextSLocOffset += Chain[I]->LocalSLocSize;
    TotalNumIdentifiers += Chain[I]->LocalNumIdentifiers;
    TotalNumTypes += Chain[I]->LocalNumTypes;
    TotalNumDefns += Chain[I]->LocalNumDefns;
  }
  SourceMgr.PreallocateSLocEntries(this, TotalNumSLocEntries, NextSLocOffset);
  IdentifiersLoaded.resize(TotalNumIdentifiers);
  TypesLoaded.resize(TotalNumTypes);
  DefnsLoaded.resize(TotalNumDefns);
  if (PP) {
    if (TotalNumIdentifiers > 0)
      PP->getImportSearchInfo().SetExternalLookup(this);
  }
  // Preload SLocEntries.
  for (unsigned I = 0, N = PreloadSLocEntries.size(); I != N; ++I) {
    ASTReadResult Result = ReadSLocEntryRecord(PreloadSLocEntries[I]);
    if (Result != Success)
      return Result;
  }

  if (PP) {
    // Initialization of keywords and pragmas occurs before the
    // AST file is read, so there may be some identifiers that were
    // loaded into the IdentifierTable before we intercepted the
    // creation of identifiers. Iterate through the list of known
    // identifiers and determine whether we have to establish
    // preprocessor definitions or top-level identifier declaration
    // chains for those identifiers.
    //
    // We copy the IdentifierInfo pointers to a small vector first,
    // since de-serializing declarations or macro definitions can add
    // new entries into the identifier table, invalidating the
    // iterators.
    llvm::SmallVector<IdentifierInfo *, 128> Identifiers;
    for (IdentifierTable::iterator Id = PP->getIdentifierTable().begin(),
                                IdEnd = PP->getIdentifierTable().end();
         Id != IdEnd; ++Id)
      Identifiers.push_back(Id->second);
    // We need to search the tables in all files.
    for (unsigned J = 0, M = Chain.size(); J != M; ++J) {
      ASTIdentifierLookupTable *IdTable
        = (ASTIdentifierLookupTable *)Chain[J]->IdentifierLookupTable;
      // Not all AST files necessarily have identifier tables, only the useful
      // ones.
      if (!IdTable)
        continue;
      for (unsigned I = 0, N = Identifiers.size(); I != N; ++I) {
        IdentifierInfo *II = Identifiers[I];
        // Look in the on-disk hash tables for an entry for this identifier
        ASTIdentifierLookupTrait Info(*this, *Chain[J], II);
        std::pair<const char*,unsigned> Key(II->getNameStart(),II->getLength());
        ASTIdentifierLookupTable::iterator Pos = IdTable->find(Key, &Info);
        if (Pos == IdTable->end())
          continue;

        // Dereferencing the iterator has the effect of populating the
        // IdentifierInfo node with the various declarations it needs.
        (void)*Pos;
      }
    }
  }

  if (Context)
    InitializeContext(*Context);

  if (DeserializationListener)
    DeserializationListener->ReaderInitialized(this);

  // If this AST file is a precompiled preamble, then set the main file ID of 
  // the source manager to the file source file from which the preamble was
  // built. This is the only valid way to use a precompiled preamble.
  if (Type == Preamble) {
    if (OriginalFileID.isInvalid()) {
      SourceLocation Loc
        = SourceMgr.getLocation(FileMgr.getFile(getOriginalSourceFile()), 1, 1);
      if (Loc.isValid())
        OriginalFileID = SourceMgr.getDecomposedLoc(Loc).first;
    }
    
//    if (!OriginalFileID.isInvalid())
//      SourceMgr.SetPreambleFileID(OriginalFileID);
  }
  
  return Success;
}

ASTReader::ASTReadResult ASTReader::ReadASTCore(llvm::StringRef FileName,
                                                ASTFileType Type) {
  PerFileData *Prev = Chain.empty() ? 0 : Chain.back();
  Chain.push_back(new PerFileData(Type));
  PerFileData &F = *Chain.back();
  if (Prev)
    Prev->NextInSource = &F;
  else
    FirstInSource = &F;
  F.Loaders.push_back(Prev);

  // Set the AST file name.
  F.FileName = FileName;

  if (FileName != "-") {
    CurrentDir = llvm::sys::path::parent_path(FileName);
    if (CurrentDir.empty()) CurrentDir = ".";
  }

  if (!ASTBuffers.empty()) {
    F.Buffer.reset(ASTBuffers.back());
    ASTBuffers.pop_back();
    assert(F.Buffer && "Passed null buffer");
  } else {
    // Open the AST file.
    //
    // FIXME: This shouldn't be here, we should just take a raw_ostream.
    std::string ErrStr;
    llvm::error_code ec;
    if (FileName == "-") {
      ec = llvm::MemoryBuffer::getSTDIN(F.Buffer);
      if (ec)
        ErrStr = ec.message();
    } else
      F.Buffer.reset(FileMgr.getBufferForFile(FileName, &ErrStr));
    if (!F.Buffer) {
      Error(ErrStr.c_str());
      return IgnorePCH;
    }
  }

  // Initialize the stream
  F.StreamFile.init((const unsigned char *)F.Buffer->getBufferStart(),
                    (const unsigned char *)F.Buffer->getBufferEnd());
  llvm::BitstreamCursor &Stream = F.Stream;
  Stream.init(F.StreamFile);
  F.SizeInBits = F.Buffer->getBufferSize() * 8;

  // Sniff for the signature.
  if (Stream.Read(8) != 'C' ||
      Stream.Read(8) != 'P' ||
      Stream.Read(8) != 'C' ||
      Stream.Read(8) != 'H') {
    Diag(diag::err_not_a_pch_file) << FileName;
    return Failure;
  }

  while (!Stream.AtEndOfStream()) {
    unsigned Code = Stream.ReadCode();

    if (Code != llvm::bitc::ENTER_SUBBLOCK) {
      Error("invalid record at top-level of AST file");
      return Failure;
    }

    unsigned BlockID = Stream.ReadSubBlockID();

    // We only know the AST subblock ID.
    switch (BlockID) {
    case llvm::bitc::BLOCKINFO_BLOCK_ID:
      if (Stream.ReadBlockInfoBlock()) {
        Error("malformed BlockInfoBlock in AST file");
        return Failure;
      }
      break;
    case AST_BLOCK_ID:
      switch (ReadASTBlock(F)) {
      case Success:
        break;

      case Failure:
        return Failure;

      case IgnorePCH:
        // FIXME: We could consider reading through to the end of this
        // AST block, skipping subblocks, to see if there are other
        // AST blocks elsewhere.

        // Clear out any preallocated source location entries, so that
        // the source manager does not try to resolve them later.
        SourceMgr.ClearPreallocatedSLocEntries();

        // Remove the stat cache.
        if (F.StatCache)
          FileMgr.removeStatCache((ASTStatCache*)F.StatCache);

        return IgnorePCH;
      }
      break;
    default:
      if (Stream.SkipBlock()) {
        Error("malformed block record in AST file");
        return Failure;
      }
      break;
    }
  }

  return Success;
}

void ASTReader::setPreprocessor(Preprocessor &pp) {
  PP = &pp;
}

void ASTReader::InitializeContext(ASTContext &Ctx) {
  Context = &Ctx;
  assert(Context && "Passed null context!");

  assert(PP && "Forgot to set Preprocessor ?");
  PP->getIdentifierTable().setExternalIdentifierLookup(this);
  PP->getImportSearchInfo().SetExternalLookup(this);
  PP->getImportSearchInfo().SetExternalSource(this);
  
  // If we have an update block for the TU waiting, we have to add it before
  // deserializing the decl.
  DefnContextOffsetsMap::iterator DCU = DefnContextOffsets.find(0);
  if (DCU != DefnContextOffsets.end()) {
    // Insertion could invalidate map, so grab vector.
    DefnContextInfos T;
    T.swap(DCU->second);
    DefnContextOffsets.erase(DCU);
    DefnContextOffsets[Ctx.getTranslationUnitDefn()].swap(T);
  }

  // Load the translation unit declaration
  GetTranslationUnitDefn();

  ReadPragmaDiagnosticMappings(Context->getDiagnostics());
}

/// \brief Retrieve the name of the original source file name
/// directly from the AST file, without actually loading the AST
/// file.
std::string ASTReader::getOriginalSourceFile(const std::string &ASTFileName,
                                             FileManager &FileMgr,
                                             Diagnostic &Diags) {
  // Open the AST file.
  std::string ErrStr;
  llvm::OwningPtr<llvm::MemoryBuffer> Buffer;
  Buffer.reset(FileMgr.getBufferForFile(ASTFileName, &ErrStr));
  if (!Buffer) {
    Diags.Report(diag::err_fe_unable_to_read_pch_file) << ErrStr;
    return std::string();
  }

  // Initialize the stream
  llvm::BitstreamReader StreamFile;
  llvm::BitstreamCursor Stream;
  StreamFile.init((const unsigned char *)Buffer->getBufferStart(),
                  (const unsigned char *)Buffer->getBufferEnd());
  Stream.init(StreamFile);

  // Sniff for the signature.
  if (Stream.Read(8) != 'C' ||
      Stream.Read(8) != 'P' ||
      Stream.Read(8) != 'C' ||
      Stream.Read(8) != 'H') {
    Diags.Report(diag::err_fe_not_a_pch_file) << ASTFileName;
    return std::string();
  }

  RecordData Record;
  while (!Stream.AtEndOfStream()) {
    unsigned Code = Stream.ReadCode();

    if (Code == llvm::bitc::ENTER_SUBBLOCK) {
      unsigned BlockID = Stream.ReadSubBlockID();

      // We only know the AST subblock ID.
      switch (BlockID) {
      case AST_BLOCK_ID:
        if (Stream.EnterSubBlock(AST_BLOCK_ID)) {
          Diags.Report(diag::err_fe_pch_malformed_block) << ASTFileName;
          return std::string();
        }
        break;

      default:
        if (Stream.SkipBlock()) {
          Diags.Report(diag::err_fe_pch_malformed_block) << ASTFileName;
          return std::string();
        }
        break;
      }
      continue;
    }

    if (Code == llvm::bitc::END_BLOCK) {
      if (Stream.ReadBlockEnd()) {
        Diags.Report(diag::err_fe_pch_error_at_end_block) << ASTFileName;
        return std::string();
      }
      continue;
    }

    if (Code == llvm::bitc::DEFINE_ABBREV) {
      Stream.ReadAbbrevRecord();
      continue;
    }

    Record.clear();
    const char *BlobStart = 0;
    unsigned BlobLen = 0;
    if (Stream.ReadRecord(Code, Record, &BlobStart, &BlobLen)
          == ORIGINAL_FILE_NAME)
      return std::string(BlobStart, BlobLen);
  }

  return std::string();
}

/// \brief Parse the record that corresponds to a LangOptions data
/// structure.
///
/// This routine parses the language options from the AST file and then gives
/// them to the AST listener if one is set.
///
/// \returns true if the listener deems the file unacceptable, false otherwise.
bool ASTReader::ParseLanguageOptions(
                             const llvm::SmallVectorImpl<uint64_t> &Record) {
  if (Listener) {
    LangOptions LangOpts;

  #define PARSE_LANGOPT(Option)                  \
      LangOpts.Option = Record[Idx];             \
      ++Idx

    unsigned Idx = 0;

    PARSE_LANGOPT(BCPLComment);
    PARSE_LANGOPT(NoBuiltin);
    PARSE_LANGOPT(ThreadsafeStatics);
    PARSE_LANGOPT(POSIXThreads);
    PARSE_LANGOPT(MathErrno);
    LangOpts.setSignedOverflowBehavior((LangOptions::SignedOverflowBehaviorTy)
                                       Record[Idx++]);
    PARSE_LANGOPT(Optimize);
    PARSE_LANGOPT(OptimizeSize);
    PARSE_LANGOPT(Static);
    PARSE_LANGOPT(AccessControl);
    PARSE_LANGOPT(CharIsSigned);
    PARSE_LANGOPT(ShortWChar);
    PARSE_LANGOPT(ShortEnums);
    LangOpts.setGCMode((LangOptions::GCMode)Record[Idx++]);
//FIXME    LangOpts.setVisibilityMode((Visibility)Record[Idx++]);
    LangOpts.setStackProtectorMode((LangOptions::StackProtectorMode)
                                   Record[Idx++]);
    PARSE_LANGOPT(InstantiationDepth);
    PARSE_LANGOPT(OpenCL);
  #undef PARSE_LANGOPT

    return Listener->ReadLanguageOptions(LangOpts);
  }

  return false;
}

ImportedFileInfo ASTReader::GetImportedFileInfo(const FileEntry *FE) {
  HeaderFileInfoTrait Trait(FE->getName());
  for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
    PerFileData &F = *Chain[I];
    HeaderFileInfoLookupTable *Table
      = static_cast<HeaderFileInfoLookupTable *>(F.HeaderFileInfoTable);
    if (!Table)
      continue;
    
    // Look in the on-disk hash table for an entry for this file name.
    HeaderFileInfoLookupTable::iterator Pos = Table->find(FE->getName(), 
                                                          &Trait);
    if (Pos == Table->end())
      continue;

    ImportedFileInfo HFI = *Pos;
//    if (Listener)
//      Listener->ReadImportedFileInfo(HFI, FE->getUID());

    return HFI;
  }
  
  return ImportedFileInfo();
}

void ASTReader::ReadPragmaDiagnosticMappings(Diagnostic &Diag) {
  unsigned Idx = 0;
  while (Idx < PragmaDiagMappings.size()) {
    SourceLocation
      Loc = SourceLocation::getFromRawEncoding(PragmaDiagMappings[Idx++]);
    while (1) {
      assert(Idx < PragmaDiagMappings.size() &&
             "Invalid data, didn't find '-1' marking end of diag/map pairs");
      if (Idx >= PragmaDiagMappings.size())
        break; // Something is messed up but at least avoid infinite loop in
               // release build.
      unsigned DiagID = PragmaDiagMappings[Idx++];
      if (DiagID == (unsigned)-1)
        break; // no more diag/map pairs for this location.
      diag::Mapping Map = (diag::Mapping)PragmaDiagMappings[Idx++];
      Diag.setDiagnosticMapping(DiagID, Map, Loc);
    }
  }
}

/// \brief Get the correct cursor and offset for loading a type.
ASTReader::RecordLocation ASTReader::TypeCursorForIndex(unsigned Index) {
  PerFileData *F = 0;
  for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
    F = Chain[N - I - 1];
    if (Index < F->LocalNumTypes)
      break;
    Index -= F->LocalNumTypes;
  }
  assert(F && F->LocalNumTypes > Index && "Broken chain");
  return RecordLocation(F, F->TypeOffsets[Index]);
}

/// \brief Read and return the type with the given index..
///
/// The index is the type ID, shifted and minus the number of predefs. This
/// routine actually reads the record corresponding to the type at the given
/// location. It is a helper routine for GetType, which deals with reading type
/// IDs.
Type ASTReader::ReadTypeRecord(unsigned Index) {
  RecordLocation Loc = TypeCursorForIndex(Index);
  llvm::BitstreamCursor &DefnsCursor = Loc.F->DefnsCursor;

  // Keep track of where we are in the stream, then jump back there
  // after reading this type.
  SavedStreamPosition SavedPosition(DefnsCursor);

  ReadingKindTracker ReadingKind(Read_Type, *this);

  // Note that we are loading a type record.
  Deserializing AType(this);

  DefnsCursor.JumpToBit(Loc.Offset);
//  RecordData Record;
//  unsigned Code = DefnsCursor.ReadCode();
//  switch ((TypeCode)DefnsCursor.ReadRecord(Code, Record)) {
//
//  }
  // Suppress a GCC warning
  return Type();
}

Type ASTReader::GetType(TypeID ID) {
  unsigned FastQuals = ID & TypeInfo::FastMask;
  unsigned Index = ID >> TypeInfo::FastWidth;

  if (Index < NUM_PREDEF_TYPE_IDS) {
    Type T;
    switch ((PredefinedTypeIDs)Index) {
    case PREDEF_TYPE_NULL_ID: return Type();
    case PREDEF_TYPE_VOID_ID: /*T = Context->VoidTy;*/ break;
    case PREDEF_TYPE_LOGICAL_ID: T = Context->LogicalTy; break;

    case PREDEF_TYPE_CHAR_ID:
      // FIXME: Check that the signedness of CharTy is correct!
      T = Context->CharTy;
      break;
    case PREDEF_TYPE_INT8_ID:       T = Context->UInt8Ty;            break;
    case PREDEF_TYPE_INT16_ID:      T = Context->UInt8Ty;            break;
    case PREDEF_TYPE_INT32_ID:      T = Context->UInt8Ty;            break;
    case PREDEF_TYPE_INT64_ID:      T = Context->UInt8Ty;            break;
    case PREDEF_TYPE_UINT8_ID:      T = Context->UInt8Ty;            break;
    case PREDEF_TYPE_UINT16_ID:     T = Context->UInt8Ty;            break;
    case PREDEF_TYPE_UINT32_ID:     T = Context->UInt8Ty;            break;
    case PREDEF_TYPE_UINT64_ID:     T = Context->UInt8Ty;            break;
    case PREDEF_TYPE_SINGLE_ID:     T = Context->SingleTy;           break;
    case PREDEF_TYPE_DOUBLE_ID:     T = Context->DoubleTy;           break;
    //case PREDEF_TYPE_CHAR16_ID:     T = Context->Char16Ty;           break;
    //case PREDEF_TYPE_CHAR32_ID:     T = Context->Char32Ty;           break;
    }

    assert(!T.isNull() && "Unknown predefined type");
    return T.withFastTypeInfo(FastQuals);
  }

  Index -= NUM_PREDEF_TYPE_IDS;
  assert(Index < TypesLoaded.size() && "Type index out-of-range");
  if (TypesLoaded[Index].isNull()) {
    TypesLoaded[Index] = ReadTypeRecord(Index);
    if (TypesLoaded[Index].isNull())
      return Type();

    TypesLoaded[Index]->setFromAST();
    TypeIdxs[TypesLoaded[Index]] = TypeIdx::fromTypeID(ID);
    if (DeserializationListener)
      DeserializationListener->TypeRead(TypeIdx::fromTypeID(ID),
                                        TypesLoaded[Index]);
  }

  return TypesLoaded[Index].withFastTypeInfo(FastQuals);
}

TypeID ASTReader::GetTypeID(Type T) const {
  return MakeTypeID(T,
              std::bind1st(std::mem_fun(&ASTReader::GetTypeIdx), this));
}

TypeIdx ASTReader::GetTypeIdx(Type T) const {
  if (T.isNull())
    return TypeIdx();
  assert(!T.getFastTypeInfo());

  TypeIdxMap::const_iterator I = TypeIdxs.find(T);
  // GetTypeIdx is mostly used for computing the hash of DefinitionNames and
  // comparing keys of ASTDefnContextNameLookupTable.
  // If the type didn't come from the AST file use a specially marked index
  // so that any hash/key comparison fail since no such index is stored
  // in a AST file.
  if (I == TypeIdxs.end())
    return TypeIdx(-1);
  return I->second;
}

unsigned ASTReader::getTotalNumClassBaseSpecifiers() const {
  unsigned Result = 0;
  for (unsigned I = 0, N = Chain.size(); I != N; ++I)
    Result += Chain[I]->LocalNumClassBaseSpecifiers;
  
  return Result;
}

Defn *ASTReader::GetExternalDefn(uint32_t ID) {
  return GetDefn(ID);
}

uint64_t 
ASTReader::GetClassBaseSpecifiersOffset(serialization::ClassBaseSpecifiersID ID) {
  if (ID == 0)
    return 0;
  
  --ID;
  uint64_t Offset = 0;
  for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
    PerFileData &F = *Chain[N - I - 1];

    if (ID < F.LocalNumClassBaseSpecifiers)
      return Offset + F.ClassBaseSpecifiersOffsets[ID];
    
    ID -= F.LocalNumClassBaseSpecifiers;
    Offset += F.SizeInBits;
  }
  
  assert(false && "ClassBaseSpecifiers not found");
  return 0;
}

ClassBaseSpecifier *ASTReader::GetExternalClassBaseSpecifiers(uint64_t Offset) {
  // Figure out which AST file contains this offset.
  PerFileData *F = 0;
  for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
    if (Offset < Chain[N - I - 1]->SizeInBits) {
      F = Chain[N - I - 1];
      break;
    }
    
    Offset -= Chain[N - I - 1]->SizeInBits;
  }

  if (!F) {
    Error("Malformed AST file: Class base specifiers at impossible offset");
    return 0;
  }
  
  llvm::BitstreamCursor &Cursor = F->DefnsCursor;
  SavedStreamPosition SavedPosition(Cursor);
  Cursor.JumpToBit(Offset);
  ReadingKindTracker ReadingKind(Read_Defn, *this);
  RecordData Record;
  unsigned Code = Cursor.ReadCode();
  unsigned RecCode = Cursor.ReadRecord(Code, Record);
  if (RecCode != DECL_CXX_BASE_SPECIFIERS) {
    Error("Malformed AST file: missing C++ base specifiers");
    return 0;
  }

  unsigned Idx = 0;
  unsigned NumBases = Record[Idx++];
  void *Mem = Context->Allocate(sizeof(ClassBaseSpecifier) * NumBases);
  ClassBaseSpecifier *Bases = new (Mem) ClassBaseSpecifier [NumBases];
  for (unsigned I = 0; I != NumBases; ++I)
    Bases[I] = ReadClassBaseSpecifier(*F, Record, Idx);
  return Bases;
}

TranslationUnitDefn *ASTReader::GetTranslationUnitDefn() {
  if (!DefnsLoaded[0]) {
    ReadDefnRecord(0, 1);
    if (DeserializationListener)
      DeserializationListener->DefnRead(1, DefnsLoaded[0]);
  }

  return cast<TranslationUnitDefn>(DefnsLoaded[0]);
}

Defn *ASTReader::GetDefn(DefnID ID) {
  if (ID == 0)
    return 0;

  if (ID > DefnsLoaded.size()) {
    Error("declaration ID out-of-range for AST file");
    return 0;
  }

  unsigned Index = ID - 1;
  if (!DefnsLoaded[Index]) {
    ReadDefnRecord(Index, ID);
    if (DeserializationListener)
      DeserializationListener->DefnRead(ID, DefnsLoaded[Index]);
  }

  return DefnsLoaded[Index];
}

/// \brief Resolve the offset of a statement into a statement.
///
/// This operation will read a new statement from the external
/// source each time it is called, and is meant to be used via a
/// LazyOffsetPtr (which is used by Defns for the body of functions, etc).
Stmt *ASTReader::GetExternalDefnStmt(uint64_t Offset) {
  // Switch case IDs are per Defn.
  ClearSwitchCaseIDs();

  // Offset here is a global offset across the entire chain.
  for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
    PerFileData &F = *Chain[N - I - 1];
    if (Offset < F.SizeInBits) {
      // Since we know that this statement is part of a decl, make sure to use
      // the decl cursor to read it.
      F.DefnsCursor.JumpToBit(Offset);
      return ReadStmtFromStream(F);
    }
    Offset -= F.SizeInBits;
  }
  llvm_unreachable("Broken chain");
}

bool ASTReader::FindExternalLexicalDefns(const DefnContext *DC,
                                         bool (*isKindWeWant)(Defn::Kind),
                                         llvm::SmallVectorImpl<Defn*> &Defns) {
  assert(DC->hasExternalLexicalStorage() &&
         "DefnContext has no lexical decls in storage");

  // There might be lexical decls in multiple parts of the chain, for the TU
  // at least.
  // DefnContextOffsets might reallocate as we load additional decls below,
  // so make a copy of the vector.
  DefnContextInfos Infos = DefnContextOffsets[DC];
  for (DefnContextInfos::iterator I = Infos.begin(), E = Infos.end();
       I != E; ++I) {
    // IDs can be 0 if this context doesn't contain declarations.
    if (!I->LexicalDefns)
      continue;

    // Load all of the declaration IDs
    for (const KindDefnIDPair *ID = I->LexicalDefns,
                              *IDE = ID + I->NumLexicalDefns; ID != IDE; ++ID) {
      if (isKindWeWant && !isKindWeWant((Defn::Kind)ID->first))
        continue;

      Defn *D = GetDefn(ID->second);
      assert(D && "Null decl in lexical decls");
      Defns.push_back(D);
    }
  }

  ++NumLexicalDefnContextsRead;
  return false;
}

DefnContext::lookup_result
ASTReader::FindExternalVisibleDefnsByName(const DefnContext *DC,
                                          DefinitionName Name) {
  assert(DC->hasExternalVisibleStorage() &&
         "DefnContext has no visible decls in storage");
  if (!Name)
    return DefnContext::lookup_result(DefnContext::lookup_iterator(0),
                                      DefnContext::lookup_iterator(0));

  llvm::SmallVector<NamedDefn *, 64> Defns;
  // There might be visible decls in multiple parts of the chain, for the TU
  // and namespaces. For any given name, the last available results replace
  // all earlier ones. For this reason, we walk in reverse.
  DefnContextInfos &Infos = DefnContextOffsets[DC];
  for (DefnContextInfos::reverse_iterator I = Infos.rbegin(), E = Infos.rend();
       I != E; ++I) {
    if (!I->NameLookupTableData)
      continue;

    ASTDefnContextNameLookupTable *LookupTable =
        (ASTDefnContextNameLookupTable*)I->NameLookupTableData;
    ASTDefnContextNameLookupTable::iterator Pos = LookupTable->find(Name);
    if (Pos == LookupTable->end())
      continue;

    ASTDefnContextNameLookupTrait::data_type Data = *Pos;
    for (; Data.first != Data.second; ++Data.first)
      Defns.push_back(cast<NamedDefn>(GetDefn(*Data.first)));
    break;
  }

  ++NumVisibleDefnContextsRead;

  SetExternalVisibleDefnsForName(DC, Name, Defns);
  return const_cast<DefnContext*>(DC)->lookup(Name);
}

void ASTReader::MaterializeVisibleDefns(const DefnContext *DC) {
  assert(DC->hasExternalVisibleStorage() &&
         "DefnContext has no visible decls in storage");

  llvm::SmallVector<NamedDefn *, 64> Defns;
  // There might be visible decls in multiple parts of the chain, for the TU
  // and namespaces.
  DefnContextInfos &Infos = DefnContextOffsets[DC];
  for (DefnContextInfos::iterator I = Infos.begin(), E = Infos.end();
       I != E; ++I) {
    if (!I->NameLookupTableData)
      continue;

    ASTDefnContextNameLookupTable *LookupTable =
        (ASTDefnContextNameLookupTable*)I->NameLookupTableData;
    for (ASTDefnContextNameLookupTable::item_iterator
           ItemI = LookupTable->item_begin(),
           ItemEnd = LookupTable->item_end() ; ItemI != ItemEnd; ++ItemI) {
      ASTDefnContextNameLookupTable::item_iterator::value_type Val
          = *ItemI;
      ASTDefnContextNameLookupTrait::data_type Data = Val.second;
      Defns.clear();
      for (; Data.first != Data.second; ++Data.first)
        Defns.push_back(cast<NamedDefn>(GetDefn(*Data.first)));
      MaterializeVisibleDefnsForName(DC, Val.first, Defns);
    }
  }
}

void ASTReader::PassInterestingDefnsToConsumer() {
  assert(Consumer);
  while (!InterestingDefns.empty()) {
    DefnGroupRef DG(InterestingDefns.front());
    InterestingDefns.pop_front();
    Consumer->HandleInterestingDefn(DG);
  }
}

void ASTReader::StartTranslationUnit(ASTConsumer *Consumer) {
  this->Consumer = Consumer;

  if (!Consumer)
    return;

  for (unsigned I = 0, N = ExternalDefinitions.size(); I != N; ++I) {
    // Force deserialization of this decl, which will cause it to be queued for
    // passing to the consumer.
    GetDefn(ExternalDefinitions[I]);
  }

  PassInterestingDefnsToConsumer();
}

void ASTReader::PrintStats() {
  std::fprintf(stderr, "*** AST File Statistics:\n");

  unsigned NumTypesLoaded
    = TypesLoaded.size() - std::count(TypesLoaded.begin(), TypesLoaded.end(),
                                      Type());
  unsigned NumDefnsLoaded
    = DefnsLoaded.size() - std::count(DefnsLoaded.begin(), DefnsLoaded.end(),
                                      (Defn *)0);
  unsigned NumIdentifiersLoaded
    = IdentifiersLoaded.size() - std::count(IdentifiersLoaded.begin(),
                                            IdentifiersLoaded.end(),
                                            (IdentifierInfo *)0);

  std::fprintf(stderr, "  %u stat cache hits\n", NumStatHits);
  std::fprintf(stderr, "  %u stat cache misses\n", NumStatMisses);
  if (TotalNumSLocEntries)
    std::fprintf(stderr, "  %u/%u source location entries read (%f%%)\n",
                 NumSLocEntriesRead, TotalNumSLocEntries,
                 ((float)NumSLocEntriesRead/TotalNumSLocEntries * 100));
  if (!TypesLoaded.empty())
    std::fprintf(stderr, "  %u/%u types read (%f%%)\n",
                 NumTypesLoaded, (unsigned)TypesLoaded.size(),
                 ((float)NumTypesLoaded/TypesLoaded.size() * 100));
  if (!DefnsLoaded.empty())
    std::fprintf(stderr, "  %u/%u declarations read (%f%%)\n",
                 NumDefnsLoaded, (unsigned)DefnsLoaded.size(),
                 ((float)NumDefnsLoaded/DefnsLoaded.size() * 100));
  if (!IdentifiersLoaded.empty())
    std::fprintf(stderr, "  %u/%u identifiers read (%f%%)\n",
                 NumIdentifiersLoaded, (unsigned)IdentifiersLoaded.size(),
                 ((float)NumIdentifiersLoaded/IdentifiersLoaded.size() * 100));
  if (TotalNumStatements)
    std::fprintf(stderr, "  %u/%u statements read (%f%%)\n",
                 NumStatementsRead, TotalNumStatements,
                 ((float)NumStatementsRead/TotalNumStatements * 100));
  if (TotalLexicalDefnContexts)
    std::fprintf(stderr, "  %u/%u lexical declcontexts read (%f%%)\n",
                 NumLexicalDefnContextsRead, TotalLexicalDefnContexts,
                 ((float)NumLexicalDefnContextsRead/TotalLexicalDefnContexts
                  * 100));
  if (TotalVisibleDefnContexts)
    std::fprintf(stderr, "  %u/%u visible declcontexts read (%f%%)\n",
                 NumVisibleDefnContextsRead, TotalVisibleDefnContexts,
                 ((float)NumVisibleDefnContextsRead/TotalVisibleDefnContexts
                  * 100));
  std::fprintf(stderr, "\n");
}

/// Return the amount of memory used by memory buffers, breaking down
/// by heap-backed versus mmap'ed memory.
void ASTReader::getMemoryBufferSizes(MemoryBufferSizes &sizes) const {
  for (unsigned i = 0, e = Chain.size(); i != e; ++i)
    if (llvm::MemoryBuffer *buf = Chain[i]->Buffer.get()) {
      size_t bytes = buf->getBufferSize();
      switch (buf->getBufferKind()) {
        case llvm::MemoryBuffer::MemoryBuffer_Malloc:
          sizes.malloc_bytes += bytes;
          break;
        case llvm::MemoryBuffer::MemoryBuffer_MMap:
          sizes.mmap_bytes += bytes;
          break;
      }
    }
}

void ASTReader::InitializeSema(Sema &S) {
  SemaObj = &S;
  S.ExternalSource = this;

  // Makes sure any declarations that were deserialized "too early"
  // still get added to the identifier's declaration chains.
  for (unsigned I = 0, N = PreloadedDefns.size(); I != N; ++I) {
    if (SemaObj->BaseWorkspace)
      SemaObj->BaseWorkspace->AddDefn(PreloadedDefns[I]);

    SemaObj->IdResolver.AddDefn(PreloadedDefns[I]);
  }
  PreloadedDefns.clear();

  // If there were any locally-scoped external declarations,
  // deserialize them and add them to Sema's table of locally-scoped
  // external declarations.
  for (unsigned I = 0, N = LocallyScopedExternalDefns.size(); I != N; ++I) {
    NamedDefn *D = cast<NamedDefn>(GetDefn(LocallyScopedExternalDefns[I]));
    SemaObj->LocallyScopedExternalDefns[D->getDefnName()] = D;
  }

  // FIXME: Do VTable uses and dynamic classes deserialize too much ?
  // Can we cut them down before writing them ?

  // If there were any dynamic classes declarations, deserialize them
  // and add them to Sema's vector of such declarations.
  for (unsigned I = 0, N = DynamicClasses.size(); I != N; ++I)
    SemaObj->DynamicClasses.push_back(
                               cast<UserClassDefn>(GetDefn(DynamicClasses[I])));

  // Load the offsets of the declarations that Sema references.
  // They will be lazily deserialized when needed.
  if (!SemaDefnRefs.empty()) {
    assert(SemaDefnRefs.size() == 2 && "More decl refs than expected!");
    SemaObj->StdNamespace = SemaDefnRefs[0];
    SemaObj->StdBadAlloc = SemaDefnRefs[1];
  }

  // The special data sets below always come from the most recent PCH,
  // which is at the front of the chain.
  PerFileData &F = *Chain.front();

  // If there were any pending implicit instantiations, deserialize them
  // and add them to Sema's queue of such instantiations.
  assert(F.PendingInstantiations.size() % 2 == 0 &&
         "Expected pairs of entries");
  for (unsigned Idx = 0, N = F.PendingInstantiations.size(); Idx < N;) {
    ValueDefn *D=cast<ValueDefn>(GetDefn(F.PendingInstantiations[Idx++]));
    SourceLocation Loc = ReadSourceLocation(F, F.PendingInstantiations,Idx);
    SemaObj->PendingInstantiations.push_back(std::make_pair(D, Loc));
  }

  // If there were any VTable uses, deserialize the information and add it
  // to Sema's vector and map of VTable uses.
  if (!VTableUses.empty()) {
    unsigned Idx = 0;
    for (unsigned I = 0, N = VTableUses[Idx++]; I != N; ++I) {
      UserClassDefn *Class = cast<UserClassDefn>(GetDefn(VTableUses[Idx++]));
      SourceLocation Loc = ReadSourceLocation(F, VTableUses, Idx);
      bool DefinitionRequired = VTableUses[Idx++];
      SemaObj->VTableUses.push_back(std::make_pair(Class, Loc));
      SemaObj->VTablesUsed[Class] = DefinitionRequired;
    }
  }
}

IdentifierInfo* ASTReader::get(const char *NameStart, const char *NameEnd) {
  // Try to find this name within our on-disk hash tables. We start with the
  // most recent one, since that one contains the most up-to-date info.
  for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
    ASTIdentifierLookupTable *IdTable
        = (ASTIdentifierLookupTable *)Chain[I]->IdentifierLookupTable;
    if (!IdTable)
      continue;
    std::pair<const char*, unsigned> Key(NameStart, NameEnd - NameStart);
    ASTIdentifierLookupTable::iterator Pos = IdTable->find(Key);
    if (Pos == IdTable->end())
      continue;

    // Dereferencing the iterator has the effect of building the
    // IdentifierInfo node and populating it with the various
    // declarations it needs.
    return *Pos;
  }
  return 0;
}

namespace mlang {
  /// \brief An identifier-lookup iterator that enumerates all of the
  /// identifiers stored within a set of AST files.
  class ASTIdentifierIterator : public IdentifierIterator {
    /// \brief The AST reader whose identifiers are being enumerated.
    const ASTReader &Reader;

    /// \brief The current index into the chain of AST files stored in
    /// the AST reader.
    unsigned Index;

    /// \brief The current position within the identifier lookup table
    /// of the current AST file.
    ASTIdentifierLookupTable::key_iterator Current;

    /// \brief The end position within the identifier lookup table of
    /// the current AST file.
    ASTIdentifierLookupTable::key_iterator End;

  public:
    explicit ASTIdentifierIterator(const ASTReader &Reader);

    virtual llvm::StringRef Next();
  };
}

ASTIdentifierIterator::ASTIdentifierIterator(const ASTReader &Reader)
  : Reader(Reader), Index(Reader.Chain.size() - 1) {
  ASTIdentifierLookupTable *IdTable
    = (ASTIdentifierLookupTable *)Reader.Chain[Index]->IdentifierLookupTable;
  Current = IdTable->key_begin();
  End = IdTable->key_end();
}

llvm::StringRef ASTIdentifierIterator::Next() {
  while (Current == End) {
    // If we have exhausted all of our AST files, we're done.
    if (Index == 0)
      return llvm::StringRef();

    --Index;
    ASTIdentifierLookupTable *IdTable
      = (ASTIdentifierLookupTable *)Reader.Chain[Index]->IdentifierLookupTable;
    Current = IdTable->key_begin();
    End = IdTable->key_end();
  }

  // We have any identifiers remaining in the current AST file; return
  // the next one.
  std::pair<const char*, unsigned> Key = *Current;
  ++Current;
  return llvm::StringRef(Key.first, Key.second);
}

IdentifierIterator *ASTReader::getIdentifiers() const {
  return new ASTIdentifierIterator(*this);
}

void ASTReader::SetIdentifierInfo(unsigned ID, IdentifierInfo *II) {
  assert(ID && "Non-zero identifier ID required");
  assert(ID <= IdentifiersLoaded.size() && "identifier ID out of range");
  IdentifiersLoaded[ID - 1] = II;
  if (DeserializationListener)
    DeserializationListener->IdentifierRead(ID, II);
}

/// \brief Set the globally-visible declarations associated with the given
/// identifier.
///
/// If the AST reader is currently in a state where the given declaration IDs
/// cannot safely be resolved, they are queued until it is safe to resolve
/// them.
///
/// \param II an IdentifierInfo that refers to one or more globally-visible
/// declarations.
///
/// \param DefnIDs the set of declaration IDs with the name @p II that are
/// visible at global scope.
///
/// \param Nonrecursive should be true to indicate that the caller knows that
/// this call is non-recursive, and therefore the globally-visible declarations
/// will not be placed onto the pending queue.
void
ASTReader::SetGloballyVisibleDefns(IdentifierInfo *II,
                              const llvm::SmallVectorImpl<uint32_t> &DefnIDs,
                                   bool Nonrecursive) {
  if (NumCurrentElementsDeserializing && !Nonrecursive) {
    PendingIdentifierInfos.push_back(PendingIdentifierInfo());
    PendingIdentifierInfo &PII = PendingIdentifierInfos.back();
    PII.II = II;
    PII.DefnIDs.append(DefnIDs.begin(), DefnIDs.end());
    return;
  }

  for (unsigned I = 0, N = DefnIDs.size(); I != N; ++I) {
    NamedDefn *D = cast<NamedDefn>(GetDefn(DefnIDs[I]));
    if (SemaObj) {
      if (SemaObj->BaseWorkspace) {
        // Introduce this declaration into the translation-unit scope
        // and add it to the declaration chain for this identifier, so
        // that (unqualified) name lookup will find it.
        SemaObj->BaseWorkspace->AddDefn(D);
      }
      SemaObj->IdResolver.AddDefnToIdentifierChain(II, D);
    } else {
      // Queue this declaration so that it will be added to the
      // translation unit scope and identifier's declaration chain
      // once a Sema object is known.
      PreloadedDefns.push_back(D);
    }
  }
}

IdentifierInfo *ASTReader::DecodeIdentifierInfo(unsigned ID) {
  if (ID == 0)
    return 0;

  if (IdentifiersLoaded.empty()) {
    Error("no identifier table in AST file");
    return 0;
  }

  assert(PP && "Forgot to set Preprocessor ?");
  ID -= 1;
  if (!IdentifiersLoaded[ID]) {
    unsigned Index = ID;
    const char *Str = 0;
    for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
      PerFileData *F = Chain[N - I - 1];
      if (Index < F->LocalNumIdentifiers) {
         uint32_t Offset = F->IdentifierOffsets[Index];
         Str = F->IdentifierTableData + Offset;
         break;
      }
      Index -= F->LocalNumIdentifiers;
    }
    assert(Str && "Broken Chain");

    // All of the strings in the AST file are preceded by a 16-bit length.
    // Extract that 16-bit length to avoid having to execute strlen().
    // NOTE: 'StrLenPtr' is an 'unsigned char*' so that we load bytes as
    //  unsigned integers.  This is important to avoid integer overflow when
    //  we cast them to 'unsigned'.
    const unsigned char *StrLenPtr = (const unsigned char*) Str - 2;
    unsigned StrLen = (((unsigned) StrLenPtr[0])
                       | (((unsigned) StrLenPtr[1]) << 8)) - 1;
    IdentifiersLoaded[ID]
      = &PP->getIdentifierTable().get(Str, StrLen);
    if (DeserializationListener)
      DeserializationListener->IdentifierRead(ID + 1, IdentifiersLoaded[ID]);
  }

  return IdentifiersLoaded[ID];
}

bool ASTReader::ReadSLocEntry(unsigned ID) {
  return ReadSLocEntryRecord(ID) != Success;
}

DefinitionName
ASTReader::ReadDefinitionName(const RecordData &Record, unsigned &Idx) {
  DefinitionName::NameKind Kind = (DefinitionName::NameKind)Record[Idx++];
  switch (Kind) {
  case DefinitionName::NK_Identifier:
    return DefinitionName(GetIdentifierInfo(Record, Idx));

  case DefinitionName::NK_ConstructorName:
    return Context->DefinitionNames.getClassConstructorName(
                          GetType(Record[Idx++]));

  case DefinitionName::NK_DestructorName:
    return Context->DefinitionNames.getClassDestructorName(
                          GetType(Record[Idx++]));

  case DefinitionName::NK_OperatorName:
    return Context->DefinitionNames.getOperatorName(
                                       (OverloadedOperatorKind)Record[Idx++]);

  case DefinitionName::NK_LiteralOperatorName:
    return Context->DefinitionNames.getLiteralOperatorName(
                                       GetIdentifierInfo(Record, Idx));
  }

  // Required to silence GCC warning
  return DefinitionName();
}

void ASTReader::ReadDefinitionNameInfo(PerFileData &F,
                                        DefinitionNameInfo &NameInfo,
                                      const RecordData &Record, unsigned &Idx) {
  NameInfo.setName(ReadDefinitionName(Record, Idx));
  NameInfo.setLoc(ReadSourceLocation(F, Record, Idx));
}

/// \brief Read a UnresolvedSet structure.
void ASTReader::ReadUnresolvedSet(UnresolvedSetImpl &Set,
                                  const RecordData &Record, unsigned &Idx) {
  unsigned NumDefns = Record[Idx++];
  while (NumDefns--) {
    NamedDefn *D = cast<NamedDefn>(GetDefn(Record[Idx++]));
    AccessSpecifier AS = (AccessSpecifier)Record[Idx++];
    Set.addDefn(D, AS);
  }
}

ClassBaseSpecifier
ASTReader::ReadClassBaseSpecifier(PerFileData &F,
                                const RecordData &Record, unsigned &Idx) {
  bool isVirtual = static_cast<bool>(Record[Idx++]);
  bool isBaseOfClass = static_cast<bool>(Record[Idx++]);
  AccessSpecifier AS = static_cast<AccessSpecifier>(Record[Idx++]);
  Type TInfo = GetType(Record[Idx++]);
  SourceRange Range = ReadSourceRange(F, Record, Idx);
  SourceLocation EllipsisLoc = ReadSourceLocation(F, Record, Idx);
  ClassBaseSpecifier Result(Range, isVirtual, isBaseOfClass, AS, TInfo,
                          EllipsisLoc);
  return Result;
}

NestedNameSpecifier *
ASTReader::ReadNestedNameSpecifier(const RecordData &Record, unsigned &Idx) {
  unsigned N = Record[Idx++];
  NestedNameSpecifier *NNS = 0, *Prev = 0;
  for (unsigned I = 0; I != N; ++I) {
    NestedNameSpecifier::SpecifierKind Kind
      = (NestedNameSpecifier::SpecifierKind)Record[Idx++];
    switch (Kind) {
    case NestedNameSpecifier::Identifier: {
      IdentifierInfo *II = GetIdentifierInfo(Record, Idx);
      NNS = NestedNameSpecifier::Create(*Context, Prev, II);
      break;
    }

    case NestedNameSpecifier::Namespace: {
      NamespaceDefn *NS = cast<NamespaceDefn>(GetDefn(Record[Idx++]));
      NNS = NestedNameSpecifier::Create(*Context, Prev, NS);
      break;
    }

    case NestedNameSpecifier::TypeSpec: {
      break;
    }

    case NestedNameSpecifier::Global: {
 //     NNS = NestedNameSpecifier::GlobalSpecifier(*Context);
      // No associated value, and there can't be a prefix.
      break;
    }
    }
    Prev = NNS;
  }
  return NNS;
}

SourceRange
ASTReader::ReadSourceRange(PerFileData &F, const RecordData &Record,
                           unsigned &Idx) {
  SourceLocation beg = ReadSourceLocation(F, Record, Idx);
  SourceLocation end = ReadSourceLocation(F, Record, Idx);
  return SourceRange(beg, end);
}

/// \brief Read an integral value
llvm::APInt ASTReader::ReadAPInt(const RecordData &Record, unsigned &Idx) {
  unsigned BitWidth = Record[Idx++];
  unsigned NumWords = llvm::APInt::getNumWords(BitWidth);
  llvm::APInt Result(BitWidth, NumWords, &Record[Idx]);
  Idx += NumWords;
  return Result;
}

/// \brief Read a signed integral value
llvm::APSInt ASTReader::ReadAPSInt(const RecordData &Record, unsigned &Idx) {
  bool isUnsigned = Record[Idx++];
  return llvm::APSInt(ReadAPInt(Record, Idx), isUnsigned);
}

/// \brief Read a floating-point value
llvm::APFloat ASTReader::ReadAPFloat(const RecordData &Record, unsigned &Idx) {
  return llvm::APFloat(ReadAPInt(Record, Idx));
}

// \brief Read a string
std::string ASTReader::ReadString(const RecordData &Record, unsigned &Idx) {
  unsigned Len = Record[Idx++];
  std::string Result(Record.data() + Idx, Record.data() + Idx + Len);
  Idx += Len;
  return Result;
}

//VersionTuple ASTReader::ReadVersionTuple(const RecordData &Record,
//                                         unsigned &Idx) {
//  unsigned Major = Record[Idx++];
//  unsigned Minor = Record[Idx++];
//  unsigned Subminor = Record[Idx++];
//  if (Minor == 0)
//    return VersionTuple(Major);
//  if (Subminor == 0)
//    return VersionTuple(Major, Minor - 1);
//  return VersionTuple(Major, Minor - 1, Subminor - 1);
//}

DiagnosticBuilder ASTReader::Diag(unsigned DiagID) {
  return Diag(SourceLocation(), DiagID);
}

DiagnosticBuilder ASTReader::Diag(SourceLocation Loc, unsigned DiagID) {
  return Diags.Report(Loc, DiagID);
}

/// \brief Retrieve the identifier table associated with the
/// preprocessor.
IdentifierTable &ASTReader::getIdentifierTable() {
  assert(PP && "Forgot to set Preprocessor ?");
  return PP->getIdentifierTable();
}

/// \brief Record that the given ID maps to the given switch-case
/// statement.
void ASTReader::RecordSwitchCaseID(CaseCmd *SC, unsigned ID) {
  assert(SwitchCaseStmts[ID] == 0 && "Already have a SwitchCase with this ID");
  SwitchCaseStmts[ID] = SC;
}

/// \brief Retrieve the switch-case statement with the given ID.
CaseCmd *ASTReader::getSwitchCaseWithID(unsigned ID) {
  assert(SwitchCaseStmts[ID] != 0 && "No SwitchCase with this ID");
  return SwitchCaseStmts[ID];
}

void ASTReader::ClearSwitchCaseIDs() {
  SwitchCaseStmts.clear();
}

void ASTReader::FinishedDeserializing() {
  assert(NumCurrentElementsDeserializing &&
         "FinishedDeserializing not paired with StartedDeserializing");
  if (NumCurrentElementsDeserializing == 1) {
    // If any identifiers with corresponding top-level declarations have
    // been loaded, load those declarations now.
    while (!PendingIdentifierInfos.empty()) {
      SetGloballyVisibleDefns(PendingIdentifierInfos.front().II,
                              PendingIdentifierInfos.front().DefnIDs, true);
      PendingIdentifierInfos.pop_front();
    }

    // Ready to load previous declarations of Defns that were delayed.
    while (!PendingPreviousDefns.empty()) {
      loadAndAttachPreviousDefn(PendingPreviousDefns.front().first,
                                PendingPreviousDefns.front().second);
      PendingPreviousDefns.pop_front();
    }

    // We are not in recursive loading, so it's safe to pass the "interesting"
    // decls to the consumer.
    if (Consumer)
      PassInterestingDefnsToConsumer();

    assert(PendingForwardRefs.size() == 0 &&
           "Some forward refs did not get linked to the definition!");
  }
  --NumCurrentElementsDeserializing;
}

ASTReader::ASTReader(Preprocessor &PP, ASTContext *Context,
                     const char *isysroot, bool DisableValidation,
                     bool DisableStatCache)
  : Listener(new ASTReaderListener()), DeserializationListener(0),
    SourceMgr(PP.getSourceManager()), FileMgr(PP.getFileManager()),
    Diags(PP.getDiagnostics()), SemaObj(0), PP(&PP), Context(Context),
    Consumer(0), isysroot(isysroot),
    DisableStatCache(DisableStatCache), NumStatHits(0), NumStatMisses(0), 
    NumSLocEntriesRead(0), TotalNumSLocEntries(0), NextSLocOffset(0), 
    NumStatementsRead(0), TotalNumStatements(0),
    NumLexicalDefnContextsRead(0), TotalLexicalDefnContexts(0),
    NumVisibleDefnContextsRead(0), TotalVisibleDefnContexts(0),
    NumCurrentElementsDeserializing(0) 
{
}

ASTReader::ASTReader(SourceManager &SourceMgr, FileManager &FileMgr,
                     Diagnostic &Diags, const char *isysroot,
                     bool DisableValidation, bool DisableStatCache)
  : DeserializationListener(0), SourceMgr(SourceMgr), FileMgr(FileMgr),
    Diags(Diags), SemaObj(0), PP(0), Context(0), Consumer(0),
    isysroot(isysroot),
    DisableStatCache(DisableStatCache), NumStatHits(0), NumStatMisses(0), 
    NumSLocEntriesRead(0), TotalNumSLocEntries(0),
    NextSLocOffset(0), NumStatementsRead(0), TotalNumStatements(0),
    NumLexicalDefnContextsRead(0),
    TotalLexicalDefnContexts(0), NumVisibleDefnContextsRead(0),
    TotalVisibleDefnContexts(0), NumCurrentElementsDeserializing(0)
{
}

ASTReader::~ASTReader() {
  for (unsigned i = 0, e = Chain.size(); i != e; ++i)
    delete Chain[e - i - 1];
  // Delete all visible decl lookup tables
  for (DefnContextOffsetsMap::iterator I = DefnContextOffsets.begin(),
                                       E = DefnContextOffsets.end();
       I != E; ++I) {
    for (DefnContextInfos::iterator J = I->second.begin(), F = I->second.end();
         J != F; ++J) {
      if (J->NameLookupTableData)
        delete static_cast<ASTDefnContextNameLookupTable*>(
            J->NameLookupTableData);
    }
  }
  for (DefnContextVisibleUpdatesPending::iterator
           I = PendingVisibleUpdates.begin(),
           E = PendingVisibleUpdates.end();
       I != E; ++I) {
    for (DefnContextVisibleUpdates::iterator J = I->second.begin(),
                                             F = I->second.end();
         J != F; ++J)
      delete static_cast<ASTDefnContextNameLookupTable*>(*J);
  }
}

ASTReader::PerFileData::PerFileData(ASTFileType Ty)
  : Type(Ty), SizeInBits(0), LocalNumSLocEntries(0), SLocOffsets(0), LocalSLocSize(0),
    LocalNumIdentifiers(0), IdentifierOffsets(0), IdentifierTableData(0),
    IdentifierLookupTable(0),
    LocalNumHeaderFileInfos(0), HeaderFileInfoTableData(0),
    HeaderFileInfoTable(0), LocalNumDefns(0),
    DefnOffsets(0), LocalNumClassBaseSpecifiers(0), ClassBaseSpecifiersOffsets(0),
    LocalNumTypes(0), TypeOffsets(0), StatCache(0),
    NextInSource(0)
{}

ASTReader::PerFileData::~PerFileData() {
  delete static_cast<ASTIdentifierLookupTable *>(IdentifierLookupTable);
  delete static_cast<HeaderFileInfoLookupTable *>(HeaderFileInfoTable);
}
