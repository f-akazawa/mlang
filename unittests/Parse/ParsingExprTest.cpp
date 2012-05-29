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
//#include "mlang/AST/CmdAll.h"
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
class ParsingExprTest: public testing::Test {
protected:
	// You can remove any or all of the following functions if its body
	// is empty.
	ParsingExprTest() {
		mparser = 0;
		ctx = 0;
		sema = 0;
		consumer = 0;
		FileMgr = 0;
		SrcMgr = 0;
		PP = 0;
		ImportInfo = 0;
	}

	virtual ~ParsingExprTest() {
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
		assert(PP!=NULL);

		SourceManager & SourceMgr = PP->getSourceManager();
		llvm::StringRef InputFile = "my.m";
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
	}

	virtual void TearDown() {
		// Code here will be called immediately after each test (right
		// before the destructor).
		delete mparser;
		delete sema;
//		delete ctx;
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

TEST_F(ParsingExprTest, ParseIdentifier) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
	Token lTok;
	do {
		lTok = mparser->getCurToken();
		if(lTok.is(tok::Identifier)) {
			ExprResult expr = mparser->ParseIdentifier();
			EXPECT_FALSE(expr.isInvalid());
			//Expr * E = expr.take();
			//E->dump();
		}
		mparser->ConsumeAnyToken();
	}while (lTok.isNot(tok::EoF));
}

TEST_F(ParsingExprTest, ParsePrimaryExpression) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
	Token lTok;
	do {
		lTok = mparser->getCurToken();
		if(lTok.is(tok::Identifier) || lTok.is(tok::NumericConstant)) {
			ExprResult expr = mparser->ParsePrimaryExpression();
			EXPECT_FALSE(expr.isInvalid());
//			Expr * E = expr.take();
//			E->dump();
		}
		mparser->ConsumeAnyToken();
	}while (lTok.isNot(tok::EoF));
}

TEST_F(ParsingExprTest, ParseUnaryExpression) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
//	ExprResult LHS = mparser->ParseIdentifier();
//	Token lTok;
//	do {
//		lTok = mparser->getCurToken();
//		if(lTok.is(tok::Assignment)) {
//				break;
//		}
//		mparser->ConsumeAnyToken();
//	}while (lTok.isNot(tok::EoF));
//
//	// Consume the operator, saving the operator token for error reporting.
//	Token OpToken = mparser->getCurToken();
//	mparser->ConsumeToken();
//
//	ExprResult RHS;
//	RHS = mparser->ParseUnaryExpression();
	// RHS = mparser->ParsePostfixExpression();
//	EXPECT_FALSE(RHS.isInvalid());
//	Expr *E = RHS.get();
//	E->dump();
}

TEST_F(ParsingExprTest, ParseRHSOfBinaryExpression) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
	//ExprResult LHS = mparser->ParseIdentifier();
	ExprResult LHS = mparser->ParseUnaryExpression();
	EXPECT_FALSE(LHS.isInvalid());
	LHS.get()->dump();
	mparser->ConsumeToken();
	ExprResult RHS = mparser->ParseUnaryExpression();
	EXPECT_FALSE(RHS.isInvalid());
	RHS.get()->dump();
//	ExprResult RHS = mparser->ParseRHSOfBinaryExpression(LHS, prec::Assignment);
//	EXPECT_FALSE(RHS.isInvalid());
//	Expr *E = RHS.get();
//	E->dump();
// mparser->getPreprocessor().DumpToken(mparser->getCurToken());
// mparser->ConsumeToken();
//
//	ExprResult LHS1 = mparser->ParseIdentifier();
//	EXPECT_FALSE(LHS1.isInvalid());
//	//Expr *E = LHS1.get();
//	//E->dump();
//
//	ExprResult RHS1 = mparser->ParseRHSOfBinaryExpression(LHS1, prec::Assignment);
//	EXPECT_FALSE(RHS1.isInvalid());
	//Expr *E1 = RHS1.get();
	//E1->dump();
}

TEST_F(ParsingExprTest, ParseAssignmentExpression) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
	ExprResult LHS;
	ExprResult expr = mparser->ParseAssignmentExpression(LHS);
	EXPECT_FALSE(LHS.isInvalid());
//	LHS.get()->dump();
	EXPECT_FALSE(expr.isInvalid());
//	expr.get()->dump();
//	EXPECT_TRUE(isa<BinaryOperator>(*expr.get()));
//	BinaryOperator *assign = dyn_cast<BinaryOperator>(expr.get());
//	EXPECT_TRUE(assign->isAssignmentOp());
}

TEST_F(ParsingExprTest, ParseExpression) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
//	ExprResult expr = mparser->ParseExpression();
//	EXPECT_FALSE(expr.isInvalid());
//  Expr * E = expr.take();
//  E->dump();
//  EXPECT_TRUE(isa<BinaryOperator>(*expr.get()));
//	BinaryOperator *assign = dyn_cast<BinaryOperator>(expr.get());
//	EXPECT_TRUE(assign->isAssignmentOp());
//	EXPECT_TRUE(mparser->getCurToken().getKind() == tok::Semicolon);
//	mparser->ConsumeToken();
//	ExprResult expr1 = mparser->ParseExpression();
//	EXPECT_FALSE(expr1.isInvalid());
//	Expr * E1 = expr1.take();
//	E1->dump();
}
} // namespace
