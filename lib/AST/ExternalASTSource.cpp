//===--- ExternalASTSource.cpp - Abstract External AST Interface-*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file provides the default implementation of the ExternalASTSource
//  interface, which enables construction of AST nodes from some external
//  source.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/ExternalASTSource.h"
#include "mlang/AST/DefinitionName.h"
#include "mlang/AST/DefnContext.h"

using namespace mlang;

ExternalASTSource::~ExternalASTSource() { }

void ExternalASTSource::PrintStats() { }

Defn *
ExternalASTSource::GetExternalDefn(uint32_t ID) {
  return 0;
}

Stmt *
ExternalASTSource::GetExternalDefnStmt(uint64_t Offset) {
  return 0;
}

ClassBaseSpecifier *
ExternalASTSource::GetExternalClassBaseSpecifiers(uint64_t Offset) {
  return 0;
}

DefnContextLookupResult
ExternalASTSource::FindExternalVisibleDefnsByName(const DefnContext *DC,
                                                  DefinitionName Name) {
  return DefnContext::lookup_result();
}

void ExternalASTSource::MaterializeVisibleDefns(const DefnContext *DC) { }

void ExternalASTSource::getMemoryBufferSizes(MemoryBufferSizes &sizes) const { }
