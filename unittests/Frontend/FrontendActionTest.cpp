//===- unittests/Frontend/FrontendActionTest.cpp - FrontendAction tests ---===//
//
//                     The Gmat Compiler Framework
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/RecursiveASTVisitor.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/Frontend/CompilerInstance.h"
#include "mlang/Frontend/CompilerInvocation.h"
#include "mlang/Frontend/FrontendAction.h"

#include "llvm/ADT/Triple.h"
#include "llvm/Support/MemoryBuffer.h"

#include "gtest/gtest.h"

using namespace llvm;
using namespace mlang;

namespace {

class TestASTFrontendAction : public ASTFrontendAction {
public:
  std::vector<std::string> defn_names;

  virtual ASTConsumer *CreateASTConsumer(CompilerInstance &CI,
                                         StringRef InFile) {
    return new Visitor(defn_names);
  }

private:
  class Visitor : public ASTConsumer, public RecursiveASTVisitor<Visitor> {
  public:
    Visitor(std::vector<std::string> &defn_names) : defn_names_(defn_names) {}

    virtual void HandleTranslationUnit(ASTContext &context) {
      TraverseDefn(context.getTranslationUnitDefn());
    }

    virtual bool VisitNamedDefn(NamedDefn *Defn) {
      defn_names_.push_back(Defn->getQualifiedNameAsString());
      return true;
    }

  private:
    std::vector<std::string> &defn_names_;
  };
};

TEST(ASTFrontendAction, Sanity) {
  CompilerInvocation *invocation = new CompilerInvocation;
  ASSERT_TRUE(invocation != 0);
  invocation->getPreprocessorOpts().addRemappedFile(
    "test.m", MemoryBuffer::getMemBuffer("b = a * [12 23 45]"));
  invocation->getFrontendOpts().Inputs.push_back(
    std::make_pair(IK_CXX, "test.m"));
  invocation->getFrontendOpts().ProgramAction = frontend::ASTPrint;
  invocation->getTargetOpts().Triple = "i386-unknown-linux-gnu";
  CompilerInstance compiler;
  compiler.setInvocation(invocation);
  compiler.createDiagnostics(0, NULL);

  TestASTFrontendAction test_action;
  ASSERT_TRUE(compiler.ExecuteAction(test_action));
  ASSERT_EQ(3U, test_action.defn_names.size());
  EXPECT_EQ("", test_action.defn_names[0]);
  EXPECT_EQ("b", test_action.defn_names[1]);
  EXPECT_EQ("a", test_action.defn_names[2]);
}

} // anonymous namespace
