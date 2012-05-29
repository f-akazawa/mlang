//===--- ParsingExprTest.cpp - unittest for Parsing Expression --*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements the ParsingExprTest.
//
//===----------------------------------------------------------------------===//

#include "mlang/Parse/Parser.h"
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
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Lex/ImportSearch.h"
#include "mlang/AST/Stmt.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/CmdAll.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/Sema/Scope.h"
#include "mlang/Sema/Sema.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cassert>

using namespace mlang;
using llvm::dyn_cast;

namespace {

// 测试Foo类的测试固件
class ParsingCmdTest: public testing::Test {
protected:
	// You can remove any or all of the following functions if its body
	// is empty.
	ParsingCmdTest() {
		mparser = 0;
		ctx = 0;
		sema = 0;
		consumer = 0;
		FileMgr = 0;
		SrcMgr = 0;
		ImportInfo = 0;
		PP = 0;
	}

	virtual ~ParsingCmdTest() {
		// You can do clean-up work that doesn't throw exceptions here.
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
		assert(PP != NULL);

		SourceManager & SourceMgr = PP->getSourceManager();
		llvm::StringRef InputFile = "my_Cmd.m";
		FileManager & FM =  SourceMgr.getFileManager();
		const FileEntry *File = FM.getFile(InputFile);
		if (!File) {
			llvm::errs() << "error\n";
			//Diags.Report(diag::err_fe_error_reading) << InputFile;
		}
		SourceMgr.createMainFileID(File);

		ctx = new ASTContext(PP->getLangOptions(), SourceMgr,
				PP->getTargetInfo(), PP->getIdentifierTable(), PP->getBuiltinInfo(),
				/*size_reserve=*/ 0);

		consumer = new ASTConsumer();
		sema = new Sema(*PP, *ctx, *consumer);
		mparser = new Parser(*PP, *sema);
		PP->getBuiltinInfo().InitializeBuiltins(
				PP->getIdentifierTable(), false);
	}

	virtual void TearDown() {
		// Code here will be called immediately after each test (right
		// before the destructor).
		delete mparser;
		delete ctx;
		delete sema;
		delete consumer;
		delete SrcMgr;
		delete FileMgr;
		delete ImportInfo;
		delete PP;
	}

	// Objects declared here can be used by all tests in the test case for Foo.
	Preprocessor *PP;
	FileManager *FileMgr;
	SourceManager *SrcMgr;
	ASTContext *ctx;
	ASTConsumer *consumer;
	Sema *sema;
	Parser *mparser;
	ImportSearch *ImportInfo;
};

TEST_F(ParsingCmdTest, ParseForKeyword) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
//	mparser->getPreprocessor().DumpToken(mparser->getCurToken());
	EXPECT_TRUE(mparser->getCurToken().is(tok::kw_for));
}
//
TEST_F(ParsingCmdTest, ParseAssignmentExpression) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
	//mparser->getPreprocessor().DumpToken(mparser->getCurToken());
	mparser->ConsumeToken();
	//mparser->getPreprocessor().DumpToken(mparser->getCurToken());
	ExprResult Var;
	ExprResult expr = mparser->ParseAssignmentExpression(Var);
	EXPECT_FALSE(expr.isInvalid());
//	expr.get()->dump();
	StmtResult body = mparser->ParseBlockStatementBody(false);
	EXPECT_FALSE(body.isInvalid());
//	body.get()->dump();
//	EXPECT_TRUE(isa<BinaryOperator>(*expr.get()));
//	BinaryOperator *assign = dyn_cast<BinaryOperator>(expr.get());
//	EXPECT_TRUE(assign->isAssignmentOp());
}
//
TEST_F(ParsingCmdTest, ParseForCmd) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
	StmtResult expr = mparser->ParseForCmd();
	EXPECT_FALSE(expr.isInvalid());
//  Stmt * E = expr.take();
//  E->dump();
  StmtResult expr1 = mparser->ParseForCmd();
  EXPECT_FALSE(expr1.isInvalid());
//  Stmt * E1 = expr1.take();
//  E1->dump();
  StmtResult expr2 = mparser->ParseForCmd();
  EXPECT_FALSE(expr2.isInvalid());
//  Stmt * E2 = expr2.take();
//  E2->dump();
//  StmtResult expr3 = mparser->ParseForCmd();
//  EXPECT_FALSE(expr3.isInvalid());
//  Stmt * E3 = expr3.take();
//  E3->dump();
}
//
TEST_F(ParsingCmdTest, ParseWhileCmd) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
	mparser->ParseForCmd();
	mparser->ParseForCmd();
	mparser->ParseForCmd();
//	mparser->ParseForCmd();
	StmtResult stmt = mparser->ParseWhileCmd();
	EXPECT_FALSE(stmt.isInvalid());
}
//
//TEST_F(ParsingCmdTest, ParseIfCmd) {
//	mparser->getPreprocessor().EnterMainSourceFile();
//	mparser->Initialize();
//	mparser->ParseForCmd();
//	mparser->ParseForCmd();
//	mparser->ParseForCmd();
//	mparser->ParseForCmd();
//	mparser->ParseWhileCmd();
//	StmtResult stmt1 = mparser->ParseIfCmd();
//	EXPECT_FALSE(stmt1.isInvalid());
//	StmtResult stmt2 = mparser->ParseIfCmd();
//	EXPECT_FALSE(stmt2.isInvalid());
//	StmtResult stmt3 = mparser->ParseIfCmd();
//	EXPECT_FALSE(stmt3.isInvalid());
//	StmtResult stmt4 = mparser->ParseIfCmd();
//	EXPECT_FALSE(stmt4.isInvalid());
//}
//
//TEST_F(ParsingCmdTest, ParseSwitchCmd) {
//	mparser->getPreprocessor().EnterMainSourceFile();
//	mparser->Initialize();
//	mparser->ParseForCmd();
//	mparser->ParseForCmd();
//	mparser->ParseForCmd();
//	mparser->ParseForCmd();
//	mparser->ParseWhileCmd();
//  mparser->ParseIfCmd();
//  mparser->ParseIfCmd();
//  mparser->ParseIfCmd();
//  mparser->ParseIfCmd();
//  StmtResult stmt = mparser->ParseSwitchCmd();
////  EXPECT_FALSE(stmt.isInvalid());
////  stmt.get()->dump();
//}
} // namespace
