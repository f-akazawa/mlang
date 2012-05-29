//===--- DefnGroup.cpp -Classes for representing groups of Defns-*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the DefnGroup and DefnGroupRef classes.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/DefnGroup.h"
#include "mlang/AST/Defn.h"
#include "mlang/AST/ASTContext.h"
#include "llvm/Support/Allocator.h"
using namespace mlang;

DefnGroup* DefnGroup::Create(ASTContext &C, Defn **Defns, unsigned NumDefns) {
  assert(NumDefns > 1 && "Invalid DefnGroup");
  unsigned Size = sizeof(DefnGroup) + sizeof(Defn*) * NumDefns;
  void* Mem = C.Allocate(Size, llvm::AlignOf<DefnGroup>::Alignment);
  new (Mem) DefnGroup(NumDefns, Defns);
  return static_cast<DefnGroup*>(Mem);
}

DefnGroup::DefnGroup(unsigned numdefns, Defn** defns) : NumDefns(numdefns) {
  assert(numdefns > 0);
  assert(defns);
  memcpy(this+1, defns, numdefns * sizeof(*defns));
}
