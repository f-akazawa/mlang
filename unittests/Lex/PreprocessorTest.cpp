//===--- PreprocessorTest.cpp - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#include "mlang/Lex/Preprocessor.h"
#include "gtest/gtest.h"
#include "mlang/Diag/Diagnostic.h"
#include "DiagnosticOptions.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Basic/FileSystemOptions.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Basic/TokenKinds.h"
#include "mlang/Frontend/TextDiagnosticPrinter.h"
#include "mlang/Lex/Lexer.h"
#include "mlang/Lex/ImportSearch.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"
#include "llvm/Support/MemoryBuffer.h"
#include <iostream>

using namespace mlang;

namespace {

static const unsigned out_tokens[9] = {
		  tok::Identifier,
		  tok::Assignment,
		  tok::Identifier,
		  tok::LParentheses,
		  tok::Identifier,
		  tok::RParentheses,
		  tok::Semicolon,
		  tok::EoF, 0
};

// 测试Foo类的测试固件
class PreprocessorTest : public testing::Test {
protected :
  // You can remove any or all of the following functions if its body
  // is empty.
  PreprocessorTest() {
    // You can do set-up work for each test here.
	  FileMgr = 0;
	  SrcMgr = 0;
	  PP = 0;
	  ImportInfo = 0;
  }

  virtual ~PreprocessorTest() {
    // You can do clean-up work that doesn't throw exceptions here.
	  delete FileMgr;
	  delete SrcMgr;
	  delete ImportInfo;
	  delete PP;
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
		// before each test).
		llvm::IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
		llvm::IntrusiveRefCntPtr<Diagnostic> Diags(new Diagnostic(DiagID));
		DiagnosticOptions Opts;
		Diags->setClient(new TextDiagnosticPrinter(llvm::errs(), Opts));

		LangOptions lang_opt;
		lang_opt.MATLABKeywords = 1;
		lang_opt.OCTAVEKeywords = 1;

		TargetInfo* target;
		TargetOptions target_opt;
		target_opt.Triple = "i686-PC-Linux";
		//target_opt.ABI = "elf";
		//target_opt.CPU = "x86";
		target = TargetInfo::CreateTargetInfo(*Diags, target_opt);
		assert(target != NULL);

		FileSystemOptions FileSystemOpts;
		FileSystemOpts.WorkingDir = "/home/yabin/MatlabSRC/test";
		FileMgr = new FileManager(FileSystemOpts);
		SrcMgr = new SourceManager(*Diags, *FileMgr);
		ImportInfo = new ImportSearch(*FileMgr);
		/// The preprocessor.
		PP = new Preprocessor(*Diags, lang_opt, *target, *SrcMgr, *ImportInfo);
		assert(PP!=NULL);

		SourceManager & SourceMgr = PP->getSourceManager();

		llvm::StringRef InputFile = "my_Expr.m";
		FileManager & FM =  SourceMgr.getFileManager();
		const FileEntry *File = FM.getFile(InputFile);
		if (!File) {
			std::cout << "error" << std::endl;
			//Diags.Report(diag::err_fe_error_reading) << InputFile;
		}
		SourceMgr.createMainFileID(File);
	}

  virtual void TearDown() {
    // Code here will be called immediately after each test (right
    // before the destructor).
   }

// Objects declared here can be used by all tests in the test case for Foo.
  FileManager *FileMgr;
  SourceManager *SrcMgr;
  Preprocessor *PP;
	ImportSearch *ImportInfo;
};

// Tests that the Foo::Bar() method does Abc.
TEST_F(PreprocessorTest, getSourceManager) {
	int i = 0;
	SourceManager & SourceMgr = PP->getSourceManager();
	SourceManager::fileinfo_iterator it;

	for(it=SourceMgr.fileinfo_begin(); it != SourceMgr.fileinfo_end(); it++) {
		i++;
	}
	EXPECT_EQ(i,1);
}

// Tests that Foo does PPLex.
TEST_F(PreprocessorTest, PPLex) {
	PP->EnterMainSourceFile();
	ASSERT_FALSE(PP->getCurrentLexer()->isLexingRawMode());
	Token lTok;
  // int i = 0;
  do {
		PP->Lex(lTok);
		PP->DumpToken(lTok,true);
		std::cerr << "\n";
		//EXPECT_EQ((unsigned)lTok.getKind(), out_tokens[i++]);
	} while(lTok.isNot(tok::EoF));
}
}   // namespace
