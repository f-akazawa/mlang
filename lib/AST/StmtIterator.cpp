//===--- StmtIterator.cpp - Iterators for Statements ------------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file defines internal methods for StmtIterator.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/StmtIterator.h"
#include "mlang/AST/DefnSub.h"

using namespace mlang;

//
void StmtIteratorBase::NextDefn(bool ImmediateAdvance) {
  assert (getRawPtr() == NULL);

  if (inDefn()) {
    assert(defn);

    // FIXME: SIMPLIFY AWAY.
    if (ImmediateAdvance)
      defn = 0;
    else if (HandleDefn(defn))
      return;
  }
  else {
    assert(inDefnGroup());

    if (ImmediateAdvance)
      ++DGI;

    for ( ; DGI != DGE; ++DGI)
      if (HandleDefn(*DGI))
        return;
  }

  RawPtr = 0;
}

bool StmtIteratorBase::HandleDefn(Defn* D) {

  if (VarDefn* VD = dyn_cast<VarDefn>(D)) {
    if (VD->getInit())
      return true;
  }

  return false;
}

StmtIteratorBase::StmtIteratorBase(Defn *d, Stmt **s)
  : stmt(s), defn(d), RawPtr(d ? DefnMode : 0) {
  if (defn)
    NextDefn(false);
}

StmtIteratorBase::StmtIteratorBase(Defn** dgi, Defn** dge)
  : stmt(0), DGI(dgi), RawPtr(DefnGroupMode), DGE(dge) {
  NextDefn(false);
}

Stmt*& StmtIteratorBase::GetDefnExpr() const {

  assert (inDefn() || inDefnGroup());

  if (inDefnGroup()) {
    VarDefn* VD = cast<VarDefn>(*DGI);
    return *VD->getInitAddress();
  }

  assert (inDefn());

  if (VarDefn* VD = dyn_cast<VarDefn>(defn)) {
    assert (VD->Init);
    return *VD->getInitAddress();
  }
}
