//===--- IdentifierTableTest.cpp - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#include "mlang/Basic/IdentifierTable.h"
#include "gtest/gtest.h"
#include "llvm/ADT/APInt.h"

namespace {
// 测试Foo类的测试固件
class IdentifierTableTest : public testing::Test {
protected :
  // You can remove any or all of the following functions if its body
  // is empty.
  IdentifierTableTest() {
    // You can do set-up work for each test here.
  }

  virtual ~IdentifierTableTest() {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right
    // before the destructor).
   }

// Objects declared here can be used by all tests in the test case for Foo.
};

// Tests that the Foo::Bar() method does Abc.
TEST(IdentifierTableTest, MethodBarDoesAbc) {
  const std::string input_filepath = "this/package/testdata/myinputfile.dat" ;
  const std::string output_filepath = "this/package/testdata/myoutputfile.dat" ;
  // IdentifierTable f;
  // EXPECT_EQ(0, f.Bar(input_filepath, output_filepath));
}

// Tests that Foo does Xyz.
TEST(IdentifierTableTest, DoesXyz) {
  // Exercises the Xyz feature of Foo.
}
}   // namespace
