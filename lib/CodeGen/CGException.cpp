//===--- CGException.cpp - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/CmdAll.h"

#include "llvm/Intrinsics.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Support/CallSite.h"

#include "CodeGenFunction.h"
#include "CGException.h"
#include "CGCleanup.h"
#include "TargetInfo.h"

using namespace mlang;
using namespace CodeGen;

llvm::BasicBlock *CodeGenFunction::getInvokeDestImpl() {
  assert(EHStack.requiresLandingPad());
  assert(!EHStack.empty());

  if (!CGM.getLangOptions().Exceptions)
    return 0;

  // Check the innermost scope for a cached landing pad.  If this is
  // a non-EH cleanup, we'll check enclosing scopes in EmitLandingPad.
  llvm::BasicBlock *LP = EHStack.begin()->getCachedLandingPad();
  if (LP) return LP;

  // Build the landing pad for this scope.
//  LP = EmitLandingPad();
//  assert(LP);
//
//  // Cache the landing pad on the innermost scope.  If this is a
//  // non-EH scope, cache the landing pad on the enclosing scope, too.
//  for (EHScopeStack::iterator ir = EHStack.begin(); true; ++ir) {
//    ir->setCachedLandingPad(LP);
//    if (!isNonEHScope(*ir)) break;
//  }
//
//  return LP;
  //FIXME huyabin
  return NULL;
}
