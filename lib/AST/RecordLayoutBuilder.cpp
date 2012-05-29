//===--- RecordLayoutBuilder.cpp - Record Layout Builder---------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements record layout building related interfaces.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/RecordLayout.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlang;

//===----------------------------------------------------------------------===//
// Local data types used for record layout building
//===----------------------------------------------------------------------===//
namespace {

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// ASTContext interfaces
//===----------------------------------------------------------------------===//

const ASTRecordLayout & ASTContext::getASTRecordLayout(const Defn *D) const{

}

const ClassMethodDefn *ASTContext::getKeyFunction(const UserClassDefn *RD) {
  const ClassMethodDefn *&Entry = KeyFunctions[RD];
  if (!Entry) {
	  // Entry = RecordLayoutBuilder::ComputeKeyFunction(RD);
  }


  return Entry;
}

static void PrintOffset(llvm::raw_ostream &OS,
                        CharUnits Offset, unsigned IndentLevel) {
  OS << llvm::format("%4d | ", Offset.getQuantity());
  OS.indent(IndentLevel * 2);
}

static void DumpClassRecordLayout(llvm::raw_ostream &OS,
                                const UserClassDefn *RD, ASTContext &C,
                                CharUnits Offset,
                                unsigned IndentLevel,
                                const char* Description,
                                bool IncludeVirtualBases) {
  const ASTRecordLayout &Layout = C.getASTRecordLayout(RD);

  PrintOffset(OS, Offset, IndentLevel);
  OS << C.getTypeDefnType(const_cast<UserClassDefn *>(RD)).getAsString();
  if (Description)
    OS << ' ' << Description;
  OS << '\n';

  IndentLevel++;

  const UserClassDefn *PrimaryBase = Layout.getPrimaryBase();

  // Dump bases


  // Dump fields.

 }

void ASTContext::DumpRecordLayout(const Defn *D, llvm::raw_ostream &OS) {

}
