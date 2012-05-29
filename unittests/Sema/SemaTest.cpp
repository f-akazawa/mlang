//===--- SemaTest.cpp - unit test for Mlang Parser --------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the SemaTest.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
#include "gtest/gtest.h"

#include "mlang/Diag/Diagnostic.h"
#include "DiagnosticOptions.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Basic/FileSystemOptions.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/Specifiers.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Basic/TokenKinds.h"
#include "mlang/Frontend/TextDiagnosticPrinter.h"
#include "mlang/Lex/Lexer.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Lex/ImportSearch.h"
#include "mlang/AST/Stmt.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/AST/Type.h"
//#include "mlang/AST/CmdAll.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/Sema/Lookup.h"
#include "mlang/Sema/Scope.h"
#include "mlang/Sema/SemaConsumer.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cstdio>
#include <iostream>
#include <cassert>

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

// 测试Sema类的测试固件
class SemaTest : public testing::Test {
protected :
  // You can remove any or all of the following functions if its body
  // is empty.
  SemaTest() {
    // You can do set-up work for each test here.
//	  FileMgr = 0;
//	  SrcMgr = 0;
//	  PP = 0;
//	  ctx = 0;
//	  Consumer = 0;
//	  sema = 0;
  }

  virtual ~SemaTest() {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
    // before each test).
	  // Code here will be called immediately after the constructor (right
	  // before each test).
	  llvm::IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
	  Diags = new Diagnostic(DiagID);
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
		FileManager & FM = SourceMgr.getFileManager();
		const FileEntry *File = FM.getFile(InputFile);
		if (!File) {
			std::cout << "error" << std::endl;
			//Diags.Report(diag::err_fe_error_reading) << InputFile;
		}
		SourceMgr.createMainFileID(File);

		ctx = new ASTContext(PP->getLangOptions(), SourceMgr,
				PP->getTargetInfo(), PP->getIdentifierTable(),
				PP->getBuiltinInfo(),
				/*size_reserve=*/0);

		Consumer = new SemaConsumer();

		sema = new Sema(*PP, *ctx, *Consumer);
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right
    // before the destructor).
	  // delete sema;
//	  delete Consumer;
//	  delete PP;
//	  delete ctx;
//	  delete FileMgr;
//	  delete SrcMgr;
   }

// Objects declared here can be used by all tests in the test case for Foo.
  llvm::IntrusiveRefCntPtr<Diagnostic> Diags;
  FileManager *FileMgr;
  SourceManager *SrcMgr;
  Preprocessor *PP;
  ASTContext *ctx;
  SemaConsumer *Consumer;
  Sema *sema;
  ImportSearch *ImportInfo;
};

// Tests that the Foo::Bar() method does Abc.
TEST_F(SemaTest, SemaCTORs) {
	EXPECT_TRUE(sema->CompleteTranslationUnit);
}

TEST_F(SemaTest, PPLex) {
	sema->PP.EnterMainSourceFile();

	Token lTok;
	int i = 0;
	do {
		sema->PP.Lex(lTok);
		EXPECT_EQ((unsigned)lTok.getKind(), out_tokens[i++]);
	}while (lTok.isNot(tok::EoF));

	EXPECT_FALSE(lTok.isNot(tok::EoF));
}

TEST_F(SemaTest, Initialize) {
	sema->Initialize();
}

TEST_F(SemaTest, getLangOptions) {
	EXPECT_EQ(sema->getLangOptions().MATLABKeywords, (unsigned)1);
	EXPECT_EQ(sema->getLangOptions().OCTAVEKeywords, (unsigned)1);
}


TEST_F(SemaTest, getCurBlock) {
	EXPECT_TRUE(sema->getCurBlock()==0);
}

TEST_F(SemaTest, ActOnTranslationUnitScope) {
	DefnContext *tu  = dyn_cast<DefnContext>(ctx->getTranslationUnitDefn());
	EXPECT_TRUE(tu != 0);
	Scope *sco = new Scope(NULL, Scope::TopScope, *Diags);
	EXPECT_TRUE(sco != 0);
	sema->ActOnTranslationUnitScope(sco);
	EXPECT_TRUE(tu == sema->CurContext);
}

// Should be tested at last
//TEST_F(SemaTest, ActOnEndOfTranslationUnit) {
//	sema->ActOnEndOfTranslationUnit();
//	EXPECT_TRUE(sema->BaseWorkspace == 0);
//}

//===----------------------------------------------------------------------===//
// SemaExpr.cpp
//===----------------------------------------------------------------------===//
TEST_F(SemaTest, ActOnIdExpression) {
	sema->PP.EnterMainSourceFile();

	// Get the input params for this method
	Token lTok;
	sema->PP.Lex(lTok);
	ASSERT_EQ((unsigned)(lTok.getKind()), (unsigned)tok::Identifier);
	IdentifierInfo *ID = lTok.getIdentifierInfo();
	ASSERT_TRUE(ID != NULL);

	// Preparation
	SourceLocation loc = lTok.getLocation();
	DefinitionName Name(ID);
	DefinitionNameInfo NameInfo(ID,loc);

	EXPECT_STREQ(NameInfo.getAsString().c_str(), "ya");

	Scope *sco = new Scope(NULL, Scope::TopScope, *Diags);
	ExprResult result = sema->ActOnIdExpression(sco, NameInfo, false);
	ASSERT_FALSE(result.isInvalid());
	Expr *E = result.get();
	ASSERT_TRUE(isa<DefnRefExpr>(E));
	ASSERT_FALSE(isa<FunctionCall>(E));
}

}   // namespace
