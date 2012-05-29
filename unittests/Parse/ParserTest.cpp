//===--- ParserTest.cpp - unit test for Mlang Parser ------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the ParserTest.
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

// 测试ParserTest类的测试固件
class ParserTest: public testing::Test {
protected:
	// You can remove any or all of the following functions if its body
	// is empty.
	ParserTest() {
		mparser = 0;
		ctx = 0;
		sema = 0;
		consumer = 0;
		FileMgr = 0;
		SrcMgr = 0;
		ImportInfo = 0;
		PP = 0;
	}

	virtual ~ParserTest() {
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

TEST_F(ParserTest, Initialize) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
//	const Token &tok = mparser->getCurToken();
//	EXPECT_EQ((unsigned)tok.getKind(),(unsigned)4); //tok::Identifier
//	unsigned curscp = mparser->getCurScope()->getFlags();
//	EXPECT_EQ(curscp, (unsigned)Scope::TopScope);
}

// Tests that Preprocessor and Lexer.
TEST_F(ParserTest, PPLex)
{
	mparser->getPreprocessor().EnterMainSourceFile();
	ASSERT_FALSE(mparser->getPreprocessor().getCurrentLexer()->isLexingRawMode());
	Token lTok;
	do {
		mparser->getPreprocessor().Lex(lTok);
//		mparser->getPreprocessor().DumpToken(lTok, true);
//		llvm::errs() << "\n";
	}while (lTok.isNot(tok::EoF));
}

TEST_F(ParserTest, ParseBlockStatementBody) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
	StmtResult res = mparser->ParseBlockStatementBody(true);
	EXPECT_FALSE(res.isInvalid());
//	res.get()->dump();
}

TEST_F(ParserTest, ParseScriptDefinition) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->Initialize();
  Parser::DefnGroupPtrTy Ds = mparser->ParseScriptDefinition();
	DefnGroupRef defns = Ds.get();
	EXPECT_TRUE(defns.isSingleDefn());
	Defn * Script = defns.getSingleDefn();
	EXPECT_FALSE(Script == NULL);
	EXPECT_TRUE(isa<ScriptDefn>(*Script));
}

TEST_F(ParserTest, ParseTranslationUnit) {
	mparser->getPreprocessor().EnterMainSourceFile();
	mparser->ParseTranslationUnit();
	Scope *S = mparser->getCurScope();
	EXPECT_TRUE(S==NULL);
//	mparser->getPreprocessor().PrintStats();
}

} // namespace
