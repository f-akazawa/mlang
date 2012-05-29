//===--- ASTWriter.cpp - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#include "mlang/Serialization/ASTWriter.h"
#include "ASTCommon.h"
#include "mlang/Sema/Sema.h"
#include "mlang/Sema/IdentifierResolver.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/Defn.h"
#include "mlang/AST/DefnContextInternals.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/Type.h"
#include "mlang/Serialization/ASTReader.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Lex/ImportSearch.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Basic/FileSystemStatCache.h"
#include "mlang/Basic/OnDiskHashTable.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/SourceManagerInternals.h"
#include "mlang/Basic/TargetInfo.h"
//#include "mlang/Basic/Version.h"
//#include "mlang/Basic/VersionTuple.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include <cstdio>
#include <string.h>
using namespace mlang;
using namespace mlang::serialization;

template <typename T, typename Allocator>
static llvm::StringRef data(const std::vector<T, Allocator> &v) {
  if (v.empty()) return llvm::StringRef();
  return llvm::StringRef(reinterpret_cast<const char*>(&v[0]),
                         sizeof(T) * v.size());
}

template <typename T>
static llvm::StringRef data(const llvm::SmallVectorImpl<T> &v) {
  return llvm::StringRef(reinterpret_cast<const char*>(v.data()),
                         sizeof(T) * v.size());
}

/****************************************************************/
ASTWriter::ASTWriter(llvm::BitstreamWriter &Stream)
  : Stream(Stream), Chain(0),
    FirstDefnID(1), NextDefnID(FirstDefnID),
    FirstTypeID(NUM_PREDEF_TYPE_IDS), NextTypeID(FirstTypeID),
    FirstIdentID(1), NextIdentID(FirstIdentID),
    CollectedStmts(&StmtsToEmit),
    NumStatements(0), NumMacros(0), NumLexicalDeclContexts(0),
    NumVisibleDeclContexts(0), FirstClassBaseSpecifiersID(1),
    NextClassBaseSpecifiersID(1)
{
}

void ASTWriter::WriteAST(Sema &SemaRef, MemorizeStatCalls *StatCalls,
                         const std::string &OutputFile,
                         const char *isysroot) {
  // Emit the file header.
  Stream.Emit((unsigned)'C', 8);
  Stream.Emit((unsigned)'P', 8);
  Stream.Emit((unsigned)'C', 8);
  Stream.Emit((unsigned)'H', 8);

//  WriteBlockInfoBlock();
//
//  if (Chain)
//    WriteASTChain(SemaRef, StatCalls, isysroot);
//  else
//    WriteASTCore(SemaRef, StatCalls, isysroot, OutputFile);
}


//void ASTWriter::IdentifierRead(IdentID ID, IdentifierInfo *II) {
//  IdentifierIDs[II] = ID;
//}

//void ASTWriter::CompletedTypeDefinition(const TypeDefn *D) {
//
//}
