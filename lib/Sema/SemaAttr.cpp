//===--- SemaAttr.cpp - Semantic Analysis for Attributes --------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for non-trivial attributes.
//
//===----------------------------------------------------------------------===//

#include "mlang/Sema/Sema.h"
using namespace mlang;

/// FreePackedContext - Deallocate and null out PackContext.
void Sema::FreePackedContext() {

}

/// PushVisibilityAttr - Note that we've entered a context with a
/// visibility attribute.
//void Sema::PushVisibilityAttr(const VisibilityAttr *Attr) {
//
//}

/// AddPushedVisibilityAttribute - If '#pragma GCC visibility' was used,
/// add an appropriate visibility attribute.
//void Sema::AddPushedVisibilityAttribute(Defn *RD) {
//
//}

/// FreeVisContext - Deallocate and null out VisContext.
void Sema::FreeVisContext() {

}
