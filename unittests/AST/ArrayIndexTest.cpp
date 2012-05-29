//===--- ArrayIndexTest.cpp - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/ExprArrayIndex.h"
#include "gtest/gtest.h"

namespace {
// 测试Foo类的测试固件
class ArrayIndexTest : public testing::Test {
protected :
  // You can remove any or all of the following functions if its body
  // is empty.
  ArrayIndexTest() {
    // You can do set-up work for each test here.

  }

  virtual ~ArrayIndexTest() {
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
TEST_F(ArrayIndexTest, MethodBarDoesAbc) {

}

// Tests that Foo does Xyz.
TEST_F(ArrayIndexTest, DoesXyz) {
  // Exercises the Xyz feature of Foo.
}
}   // namespace
