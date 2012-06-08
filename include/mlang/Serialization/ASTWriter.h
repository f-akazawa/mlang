//===--- ASTWriter.h - AST File Writer --------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTWriter class, which writes an AST file
//  containing a serialized representation of a translation unit.
//
//===----------------------------------------------------------------------===//
#ifndef MLANG_SERIALIZATION_AST_WRITER_H_
#define MLANG_SERIALIZATION_AST_WRITER_H_

#include "mlang/AST/Defn.h"
#include "mlang/AST/DefinitionName.h"
#include "mlang/AST/ASTMutationListener.h"
#include "mlang/Serialization/ASTBitCodes.h"
#include "mlang/Serialization/ASTDeserializationListener.h"
#include "mlang/Sema/SemaConsumer.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include <map>
#include <queue>
#include <vector>

namespace llvm {
  class APFloat;
  class APInt;
  class BitstreamWriter;
}

namespace mlang {

class ASTContext;
class NestedNameSpecifier;
class ClassBaseSpecifier;
class FPOptions;
class ImportSearch;
class MemorizeStatCalls;
class ASTReader;
class Preprocessor;
class Sema;
class SourceManager;
class CaseCmd;
class TargetInfo;
class VersionTuple;
class UnresolvedSetImpl;

/// \brief Writes an AST file containing the contents of a translation unit.
///
/// The ASTWriter class produces a bitstream containing the serialized
/// representation of a given abstract syntax tree and its supporting
/// data structures. This bitstream can be de-serialized via an
/// instance of the ASTReader class.
class ASTWriter : public ASTDeserializationListener,
                  public ASTMutationListener {
public:
  typedef llvm::SmallVector<uint64_t, 64> RecordData;
  typedef llvm::SmallVectorImpl<uint64_t> RecordDataImpl;

  friend class ASTDefnWriter;

private:
  /// \brief The bitstream writer used to emit this precompiled header.
  llvm::BitstreamWriter &Stream;

  /// \brief The reader of existing AST files, if we're chaining.
  ASTReader *Chain;

  /// \brief Stores a declaration or a type to be written to the AST file.
  class DefnOrType {
  public:
    DefnOrType(Defn *D) : Stored(D), IsType(false) { }
    DefnOrType(Type T) : Stored(T.getAsOpaquePtr()), IsType(true) { }
    
    bool isType() const { return IsType; }
    bool isDefn() const { return !IsType; }
    
    Type getType() const {
      assert(isType() && "Not a type!");
      return Type::getFromOpaquePtr(Stored);
    }
    
    Defn *getDefn() const {
      assert(isDefn() && "Not a defn!");
      return static_cast<Defn *>(Stored);
    }
    
  private:
    void *Stored;
    bool IsType;
  };

  /// \brief The declarations and types to emit.
  std::queue<DefnOrType> DeclTypesToEmit;

  /// \brief The first ID number we can use for our own declarations.
  serialization::DefnID FirstDefnID;

  /// \brief The decl ID that will be assigned to the next new decl.
  serialization::DefnID NextDefnID;

  /// \brief Map that provides the ID numbers of each declaration within
  /// the output stream.
  ///
  /// The ID numbers of declarations are consecutive (in order of
  /// discovery) and start at 2. 1 is reserved for the translation
  /// unit, while 0 is reserved for NULL.
  llvm::DenseMap<const Defn *, serialization::DefnID> DefnIDs;

  /// \brief Offset of each declaration in the bitstream, indexed by
  /// the declaration's ID.
  std::vector<uint32_t> DefnOffsets;

  /// \brief The first ID number we can use for our own types.
  serialization::TypeID FirstTypeID;

  /// \brief The type ID that will be assigned to the next new type.
  serialization::TypeID NextTypeID;

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

  /// \brief Offset of each type in the bitstream, indexed by
  /// the type's ID.
  std::vector<uint32_t> TypeOffsets;

  /// \brief The first ID number we can use for our own identifiers.
  serialization::IdentID FirstIdentID;

  /// \brief The identifier ID that will be assigned to the next new identifier.
  serialization::IdentID NextIdentID;

  /// \brief Map that provides the ID numbers of each identifier in
  /// the output stream.
  ///
  /// The ID numbers for identifiers are consecutive (in order of
  /// discovery), starting at 1. An ID of zero refers to a NULL
  /// IdentifierInfo.
  llvm::DenseMap<const IdentifierInfo *, serialization::IdentID> IdentifierIDs;

  /// \brief Offsets of each of the identifier IDs into the identifier
  /// table.
  std::vector<uint32_t> IdentifierOffsets;

  typedef llvm::SmallVector<uint64_t, 2> UpdateRecord;
  typedef llvm::DenseMap<const Defn *, UpdateRecord> DeclUpdateMap;
  /// \brief Mapping from declarations that came from a chained PCH to the
  /// record containing modifications to them.
  DeclUpdateMap DeclUpdates;

  typedef llvm::DenseMap<Defn *, Defn *> FirstLatestDeclMap;
  /// \brief Map of first declarations from a chained PCH that point to the
  /// most recent declarations in another PCH.
  FirstLatestDeclMap FirstLatestDecls;
  
  /// \brief Declarations encountered that might be external
  /// definitions.
  ///
  /// We keep track of external definitions (as well as tentative
  /// definitions) as we are emitting declarations to the AST
  /// file. The AST file contains a separate record for these external
  /// definitions, which are provided to the AST consumer by the AST
  /// reader. This is behavior is required to properly cope with,
  /// e.g., tentative variable definitions that occur within
  /// headers. The declarations themselves are stored as declaration
  /// IDs, since they will be written out to an EXTERNAL_DEFINITIONS
  /// record.
  llvm::SmallVector<uint64_t, 16> ExternalDefinitions;

  /// \brief DeclContexts that have received extensions since their serialized
  /// form.
  ///
  /// For namespaces, when we're chaining and encountering a namespace, we check if
  /// its primary namespace comes from the chain. If it does, we add the primary
  /// to this set, so that we can write out lexical content updates for it.
  llvm::SmallPtrSet<const DefnContext *, 16> UpdatedDefnContexts;

  typedef llvm::SmallPtrSet<const Defn *, 16> DefnsToRewriteTy;
  /// \brief Decls that will be replaced in the current dependent AST file.
  DefnsToRewriteTy DefnsToRewrite;

  /// \brief Decls that have been replaced in the current dependent AST file.
  ///
  /// When a decl changes fundamentally after being deserialized (this shouldn't
  /// happen, but the ObjC AST nodes are designed this way), it will be
  /// serialized again. In this case, it is registered here, so that the reader
  /// knows to read the updated version.
  llvm::SmallVector<std::pair<serialization::DefnID, uint64_t>, 16>
      ReplacedDefns;

  /// \brief Statements that we've encountered while serializing a
  /// declaration or type.
  llvm::SmallVector<Stmt *, 16> StmtsToEmit;

  /// \brief Statements collection to use for ASTWriter::AddStmt().
  /// It will point to StmtsToEmit unless it is overriden. 
  llvm::SmallVector<Stmt *, 16> *CollectedStmts;

  /// \brief Mapping from SwitchCase statements to IDs.
  std::map<CaseCmd *, unsigned> SwitchCaseIDs;

  /// \brief The number of statements written to the AST file.
  unsigned NumStatements;

  /// \brief The number of macros written to the AST file.
  unsigned NumMacros;

  /// \brief The number of lexical declcontexts written to the AST
  /// file.
  unsigned NumLexicalDeclContexts;

  /// \brief The number of visible declcontexts written to the AST
  /// file.
  unsigned NumVisibleDeclContexts;

  /// \brief The offset of each ClassBaseSpecifier set within the AST.
  llvm::SmallVector<uint32_t, 4> ClassBaseSpecifiersOffsets;
                    
  /// \brief The first ID number we can use for our own base specifiers.
  serialization::ClassBaseSpecifiersID FirstClassBaseSpecifiersID;
  
  /// \brief The base specifiers ID that will be assigned to the next new 
  /// set of C++ base specifiers.
  serialization::ClassBaseSpecifiersID NextClassBaseSpecifiersID;

  /// \brief A set of C++ base specifiers that is queued to be written into the 
  /// AST file.                    
  struct QueuedClassBaseSpecifiers {
    QueuedClassBaseSpecifiers() : ID(), Bases(), BasesEnd() { }
    
    QueuedClassBaseSpecifiers(serialization::ClassBaseSpecifiersID ID,
                            ClassBaseSpecifier const *Bases,
                            ClassBaseSpecifier const *BasesEnd)
      : ID(ID), Bases(Bases), BasesEnd(BasesEnd) { }
                            
    serialization::ClassBaseSpecifiersID ID;
    ClassBaseSpecifier const * Bases;
    ClassBaseSpecifier const * BasesEnd;
  };
                    
  /// \brief Queue of C++ base specifiers to be written to the AST file,
  /// in the order they should be written.
  llvm::SmallVector<QueuedClassBaseSpecifiers, 2> ClassBaseSpecifiersToWrite;
                    
  /// \brief Write the given subexpression to the bitstream.
  void WriteSubStmt(Stmt *S);

  void WriteBlockInfoBlock();
  void WriteMetadata(ASTContext &Context, const char *isysroot,
                     const std::string &OutputFile);
  void WriteLanguageOptions(const LangOptions &LangOpts);
  void WriteStatCache(MemorizeStatCalls &StatCalls);
  void WriteSourceManagerBlock(SourceManager &SourceMgr,
                               const Preprocessor &PP,
                               const char* isysroot);
  void WritePreprocessor(const Preprocessor &PP);
  void WriteImportSearch(ImportSearch &HS, const char* isysroot);
  void WritePragmaDiagnosticMappings(const DiagnosticsEngine &Diag);
  void WriteClassBaseSpecifiersOffsets();
  void WriteType(Type T);
  uint64_t WriteDeclContextLexicalBlock(ASTContext &Context, DefnContext *DC);
  uint64_t WriteDeclContextVisibleBlock(ASTContext &Context, DefnContext *DC);
  void WriteTypeDeclOffsets();
  void WriteSelectors(Sema &SemaRef);
  void WriteReferencedSelectorsPool(Sema &SemaRef);
  void WriteIdentifierTable(Preprocessor &PP);
//  void WriteAttributes(const AttrVec &Attrs, RecordDataImpl &Record);
  void WriteDeclUpdatesBlocks();
  void WriteDeclReplacementsBlock();
  void WriteDeclContextVisibleUpdate(const DefnContext *DC);
  void WriteFPPragmaOptions(const FPOptions &Opts);
  void WriteOpenCLExtensions(Sema &SemaRef);

  unsigned ParmVarDeclAbbrev;
  unsigned DeclContextLexicalAbbrev;
  unsigned DeclContextVisibleLookupAbbrev;
  unsigned UpdateVisibleAbbrev;
  void WriteDeclsBlockAbbrevs();
  void WriteDecl(ASTContext &Context, Defn *D);

  void WriteASTCore(Sema &SemaRef, MemorizeStatCalls *StatCalls,
                    const char* isysroot, const std::string &OutputFile);
  void WriteASTChain(Sema &SemaRef, MemorizeStatCalls *StatCalls,
                     const char* isysroot);
  
public:
  /// \brief Create a new precompiled header writer that outputs to
  /// the given bitstream.
  ASTWriter(llvm::BitstreamWriter &Stream);

  /// \brief Write a precompiled header for the given semantic analysis.
  ///
  /// \param SemaRef a reference to the semantic analysis object that processed
  /// the AST to be written into the precompiled header.
  ///
  /// \param StatCalls the object that cached all of the stat() calls made while
  /// searching for source files and headers.
  ///
  /// \param isysroot if non-NULL, write a relocatable PCH file whose headers
  /// are relative to the given system root.
  ///
  /// \param PPRec Record of the preprocessing actions that occurred while
  /// preprocessing this file, e.g., macro instantiations
  void WriteAST(Sema &SemaRef, MemorizeStatCalls *StatCalls,
                const std::string &OutputFile,
                const char* isysroot);

  /// \brief Emit a source location.
  void AddSourceLocation(SourceLocation Loc, RecordDataImpl &Record);

  /// \brief Emit a source range.
  void AddSourceRange(SourceRange Range, RecordDataImpl &Record);
  
  /// \brief Emit an integral value.
  void AddAPInt(const llvm::APInt &Value, RecordDataImpl &Record);

  /// \brief Emit a signed integral value.
  void AddAPSInt(const llvm::APSInt &Value, RecordDataImpl &Record);

  /// \brief Emit a floating-point value.
  void AddAPFloat(const llvm::APFloat &Value, RecordDataImpl &Record);

  /// \brief Emit a reference to an identifier.
  void AddIdentifierRef(const IdentifierInfo *II, RecordDataImpl &Record);

  /// \brief Emit a set of C++ base specifiers to the record.
  void AddClassBaseSpecifiersRef(ClassBaseSpecifier const *Bases,
                               ClassBaseSpecifier const *BasesEnd,
                               RecordDataImpl &Record);
                    

  /// \brief Get the unique number used to refer to the given identifier.
  serialization::IdentID getIdentifierRef(const IdentifierInfo *II);

  /// \brief Emit a reference to a type.
  void AddTypeRef(Type T, RecordDataImpl &Record);

  /// \brief Force a type to be emitted and get its ID.
  serialization::TypeID GetOrCreateTypeID(Type T);

  /// \brief Determine the type ID of an already-emitted type.
  serialization::TypeID getTypeID(Type T) const;

  /// \brief Force a type to be emitted and get its index.
  serialization::TypeIdx GetOrCreateTypeIdx(Type T);

  /// \brief Determine the type index of an already-emitted type.
  serialization::TypeIdx getTypeIdx(Type T) const;

  /// \brief Emit a reference to a declaration.
  void AddDefnRef(const Defn *D, RecordDataImpl &Record);

                    
  /// \brief Force a declaration to be emitted and get its ID.
  serialization::DefnID GetDefnRef(const Defn *D);

  /// \brief Determine the declaration ID of an already-emitted
  /// declaration.
  serialization::DefnID getDefnID(const Defn *D);

  /// \brief Emit a declaration name.
  void AddDeclarationName(DefinitionName Name, RecordDataImpl &Record);

  void AddDeclarationNameInfo(const DefinitionNameInfo &NameInfo,
                              RecordDataImpl &Record);

  /// \brief Emit a nested name specifier.
  void AddNestedNameSpecifier(NestedNameSpecifier *NNS, RecordDataImpl &Record);

  /// \brief Emit a UnresolvedSet structure.
  void AddUnresolvedSet(const UnresolvedSetImpl &Set, RecordDataImpl &Record);

  /// \brief Emit a C++ base specifier.
  void AddClassBaseSpecifier(const ClassBaseSpecifier &Base, RecordDataImpl &Record);

  /// \brief Add a string to the given record.
  void AddString(llvm::StringRef Str, RecordDataImpl &Record);

  /// \brief Add a version tuple to the given record
  //void AddVersionTuple(const VersionTuple &Version, RecordDataImpl &Record);

  /// \brief Mark a declaration context as needing an update.
  void AddUpdatedDefnContext(const DefnContext *DC) {
    UpdatedDefnContexts.insert(DC);
  }

  void RewriteDefn(const Defn *D) {
    DefnsToRewrite.insert(D);
    // Reset the flag, so that we don't add this decl multiple times.
//    const_cast<Defn *>(D)->setChangedSinceDeserialization(false);
  }

  /// \brief Note that the identifier II occurs at the given offset
  /// within the identifier table.
  void SetIdentifierOffset(const IdentifierInfo *II, uint32_t Offset);

  /// \brief Add the given statement or expression to the queue of
  /// statements to emit.
  ///
  /// This routine should be used when emitting types and declarations
  /// that have expressions as part of their formulation. Once the
  /// type or declaration has been written, call FlushStmts() to write
  /// the corresponding statements just after the type or
  /// declaration.
  void AddStmt(Stmt *S) {
      CollectedStmts->push_back(S);
  }

  /// \brief Flush all of the statements and expressions that have
  /// been added to the queue via AddStmt().
  void FlushStmts();

  /// \brief Flush all of the C++ base specifier sets that have been added 
  /// via \c AddClassBaseSpecifiersRef().
  void FlushClassBaseSpecifiers();
                    
  /// \brief Record an ID for the given switch-case statement.
  unsigned RecordSwitchCaseID(CaseCmd *S);

  /// \brief Retrieve the ID for the given switch-case statement.
  unsigned getSwitchCaseID(CaseCmd *S);

  void ClearSwitchCaseIDs();

  unsigned getParmVarDeclAbbrev() const { return ParmVarDeclAbbrev; }

  bool hasChain() const { return Chain; }

  // ASTDeserializationListener implementation
  //virtual void ReaderInitialized(ASTReader *Reader);
  //virtual void IdentifierRead(serialization::IdentID ID, IdentifierInfo *II);
  //virtual void TypeRead(serialization::TypeIdx Idx, Type T);
  //virtual void DefnRead(serialization::DefnID ID, const Defn *D);

  // ASTMutationListener implementation.
 // virtual void CompletedTypeDefinition(const TypeDefn *D);
 // virtual void AddedVisibleDefn(const DefnContext *DC, const Defn *D);
 // virtual void AddedClassImplicitMember(const UserClassDefn *RD, const Defn *D);
};

} // end namespace mlang

#endif /* MLANG_SERIALIZATION_AST_WRITER_H_ */
