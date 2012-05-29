//===--- ASTConsumer.cpp - Abstract interface for reading ASTs --*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTConsumer class.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/ASTConsumer.h"
#include "mlang/AST/DefnGroup.h"

using namespace mlang;

void ASTConsumer::HandleTopLevelDefn(DefnGroupRef D) {}

void ASTConsumer::HandleInterestingDefn(DefnGroupRef D) {
  HandleTopLevelDefn(D);
}
