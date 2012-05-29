//===--- SourceManagerTest.cpp - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/SourceLocation.h"
#include "gtest/gtest.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Basic/FileSystemOptions.h"
#include "mlang/Diag/Diagnostic.h"
#include "DiagnosticOptions.h"
#include "mlang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace mlang;

namespace {
// 测试Foo类的测试固件
class SourceManagerTest : public testing::Test {
protected :
  // You can remove any or all of the following functions if its body
  // is empty.
  SourceManagerTest() {
    // You can do set-up work for each test here.
  }

  virtual ~SourceManagerTest() {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:
  virtual void SetUp() {
	  FileSystemOptions FileSystemOpts;
	  FileSystemOpts.WorkingDir = "/home/yabin/MatlabSRC/test";
	  FileMgr.reset(new FileManager(FileSystemOpts));

	  llvm::IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
	  llvm::IntrusiveRefCntPtr<Diagnostic> Diags(new Diagnostic(DiagID));
	  DiagnosticOptions Opts;
	  Diags->setClient(new TextDiagnosticPrinter(llvm::errs(), Opts));

	  SrcMgr.reset(new SourceManager(*Diags, *FileMgr.get()));

	  llvm::StringRef InputFile = "my_Expr.m";
	  const FileEntry *File = FileMgr.get()->getFile(InputFile);
	  if (!File) {
		  std::cout << "error" << std::endl;
		  //Diags.Report(diag::err_fe_error_reading) << InputFile;
	  	}

	  SrcMgr.get()->createMainFileID(File);
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right
    // before the destructor).
   }

// Objects declared here can be used by all tests in the test case for Foo.
  /// The file manager.
  llvm::OwningPtr<FileManager> FileMgr;
  /// The source manager.
  llvm::OwningPtr<SourceManager> SrcMgr;
};

// Tests that getFileEntryForID.
TEST_F(SourceManagerTest, getFileEntryForID) {
	SourceManager & SourceMgr = *SrcMgr.get();

	FileID id = SourceMgr.getMainFileID();
	EXPECT_EQ(id.getHashValue(), (unsigned)1);

	const FileEntry *file1 = SourceMgr.getFileEntryForID(id);
	EXPECT_STREQ(file1->getName(),"my_Expr.m");
}

// Tests that getFileEntryForID.
TEST_F(SourceManagerTest, getBuffer) {
	SourceManager & SourceMgr = *SrcMgr.get();

	const llvm::MemoryBuffer *FromFile = SourceMgr.getBuffer(
			SourceMgr.getMainFileID());
	EXPECT_TRUE(FromFile!=NULL);

	EXPECT_EQ(FromFile->getBufferSize(),(unsigned)164);
}

TEST_F(SourceManagerTest, getFileID) {
	SourceManager & SourceMgr = *SrcMgr.get();

	SourceLocation filestart = SourceMgr.getLocForStartOfFile(
			SourceMgr.getMainFileID());

	PresumedLoc PLoc = SourceMgr.getPresumedLoc(filestart);

	EXPECT_STREQ(PLoc.getFilename(), "my_Expr.m");
	EXPECT_EQ(PLoc.getLine(), (unsigned int)1);
	EXPECT_EQ(PLoc.getColumn(), (unsigned int)1);

	FileID id = SourceMgr.getFileID(filestart);
	EXPECT_EQ(id.getHashValue(), (unsigned)1);
}

TEST_F(SourceManagerTest, PrintStats) {
	SourceManager & SourceMgr = *SrcMgr.get();
	SourceMgr.PrintStats();
}
}   // namespace
