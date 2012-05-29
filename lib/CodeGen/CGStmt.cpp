//===--- CGStmt.cpp - Emit LLVM Code from Statements ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Stmt nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CGDebugInfo.h"
#include "CodeGenModule.h"
#include "CodeGenFunction.h"
#include "TargetInfo.h"
#include "mlang/AST/StmtVisitor.h"
#include "mlang/Basic/PrettyStackTrace.h"
#include "mlang/Basic/TargetInfo.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/InlineAsm.h"
#include "llvm/Intrinsics.h"
#include "llvm/Target/TargetData.h"
using namespace mlang;
using namespace CodeGen;

//===----------------------------------------------------------------------===//
//                              Statement Emission
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitStopPoint(const Stmt *S) {
  if (CGDebugInfo *DI = getDebugInfo()) {
    if (isa<DefnCmd>(S))
      DI->setLocation(S->getLocEnd());
    else
      DI->setLocation(S->getLocStart());
    DI->UpdateLineDirectiveRegion(Builder);
    DI->EmitStopPoint(Builder);
  }
}

void CodeGenFunction::EmitStmt(const Stmt *S) {
  assert(S && "Null statement?");

  // Check if we can handle this without bothering to generate an
  // insert point or debug info.
  if (EmitSimpleStmt(S))
    return;

  // Check if we are generating unreachable code.
  if (!HaveInsertPoint()) {
    EnsureInsertPoint();
  }

  // Generate a stoppoint if we are emitting debug info.
  EmitStopPoint(S);

  switch (S->getStmtClass()) {
  case Stmt::NoStmtClass:
  case Stmt::CatchCmdClass:
    llvm_unreachable("invalid statement class to emit generically");
  case Stmt::NullCmdClass:
  case Stmt::BlockCmdClass:
  case Stmt::DefnCmdClass:
  case Stmt::BreakCmdClass:
  case Stmt::ContinueCmdClass:
  case Stmt::OtherwiseCmdClass:
  case Stmt::CaseCmdClass:
    llvm_unreachable("should have emitted these statements as simple");

#define STMT(Type, Base)
#define ABSTRACT_STMT(Op)
#define EXPR(Type, Base) \
  case Stmt::Type##Class:
#include "mlang/AST/StmtNodes.inc"
  {
    // Remember the block we came in on.
    llvm::BasicBlock *incoming = Builder.GetInsertBlock();
    assert(incoming && "expression emission must have an insertion point");

    EmitIgnoredExpr(cast<Expr>(S));

    llvm::BasicBlock *outgoing = Builder.GetInsertBlock();
    assert(outgoing && "expression emission cleared block!");

    // The expression emitters assume (reasonably!) that the insertion
    // point is always set.  To maintain that, the call-emission code
    // for noreturn functions has to enter a new block with no
    // predecessors.  We want to kill that block and mark the current
    // insertion point unreachable in the common case of a call like
    // "exit();".  Since expression emission doesn't otherwise create
    // blocks with no predecessors, we can just test for that.
    // However, we must be careful not to do this to our incoming
    // block, because *statement* emission does sometimes create
    // reachable blocks which will have no predecessors until later in
    // the function.  This occurs with, e.g., labels that are not
    // reachable by fallthrough.
    if (incoming != outgoing && outgoing->use_empty()) {
      outgoing->eraseFromParent();
      Builder.ClearInsertionPoint();
    }
    break;
  }

  case Stmt::IfCmdClass:       EmitIfCmd(cast<IfCmd>(*S));             break;
  case Stmt::WhileCmdClass:    EmitWhileCmd(cast<WhileCmd>(*S));       break;
  case Stmt::ForCmdClass:      EmitForCmd(cast<ForCmd>(*S));           break;
  case Stmt::ReturnCmdClass:   EmitReturnCmd(cast<ReturnCmd>(*S));     break;
  case Stmt::SwitchCmdClass:   EmitSwitchCmd(cast<SwitchCmd>(*S));     break;
  case Stmt::TryCmdClass:
    EmitTryCmd(cast<TryCmd>(*S));
    break;
  }
}

bool CodeGenFunction::EmitSimpleStmt(const Stmt *S) {
  switch (S->getStmtClass()) {
  default: return false;
  case Stmt::NullCmdClass: break;
  case Stmt::BlockCmdClass:     EmitBlockCmd(cast<BlockCmd>(*S)); break;
  case Stmt::DefnCmdClass:      EmitDefnCmd(cast<DefnCmd>(*S));         break;
  case Stmt::BreakCmdClass:     EmitBreakCmd(cast<BreakCmd>(*S));       break;
  case Stmt::ContinueCmdClass: EmitContinueCmd(cast<ContinueCmd>(*S)); break;
  case Stmt::OtherwiseCmdClass:  EmitOtherwiseCmd(cast<OtherwiseCmd>(*S));   break;
  case Stmt::CaseCmdClass:     EmitCaseCmd(cast<CaseCmd>(*S));         break;
  }

  return true;
}

/// EmitCompoundStmt - Emit a compound statement {..} node.  If GetLast is true,
/// this captures the expression result of the last sub-statement and returns it
/// (for use by the statement expression extension).
RValue CodeGenFunction::EmitBlockCmd(const BlockCmd &S, bool GetLast,
                                     AggValueSlot AggSlot) {
  PrettyStackTraceLoc CrashInfo(getContext().getSourceManager(),S.getLocStart(),
                             "LLVM IR generation of blocks of commands ('{}')");

//  CGDebugInfo *DI = getDebugInfo();
//  if (DI) {
//    DI->setLocation(S.getLocStart());
//    DI->EmitRegionStart(Builder);
//  }

  // Keep track of the current cleanup stack depth.
  RunCleanupsScope Scope(*this);

  for (BlockCmd::const_body_iterator I = S.body_begin(),
       E = S.body_end()-GetLast; I != E; ++I)
    EmitStmt(*I);

//  if (DI) {
//    DI->setLocation(S.getLocEnd());
//    DI->EmitRegionEnd(Builder);
//  }

  RValue RV;
  if (!GetLast)
    RV = RValue::get(0);
  else {
    // We have to special case labels here.  They are statements, but when put
    // at the end of a statement expression, they yield the value of their
    // subexpression.  Handle this by walking through all labels we encounter,
    // emitting them before we evaluate the subexpr.
    const Stmt *LastStmt = S.body_back();

    EnsureInsertPoint();

    RV = EmitAnyExpr(cast<Expr>(LastStmt), AggSlot);
  }

  return RV;
}

void CodeGenFunction::SimplifyForwardingBlocks(llvm::BasicBlock *BB) {
  llvm::BranchInst *BI = dyn_cast<llvm::BranchInst>(BB->getTerminator());

  // If there is a cleanup stack, then we it isn't worth trying to
  // simplify this block (we would need to remove it from the scope map
  // and cleanup entry).
  if (!EHStack.empty())
    return;

  // Can only simplify direct branches.
  if (!BI || !BI->isUnconditional())
    return;

  BB->replaceAllUsesWith(BI->getSuccessor(0));
  BI->eraseFromParent();
  BB->eraseFromParent();
}

void CodeGenFunction::EmitBlock(llvm::BasicBlock *BB, bool IsFinished) {
  llvm::BasicBlock *CurBB = Builder.GetInsertBlock();

  // Fall out of the current block (if necessary).
  EmitBranch(BB);

  if (IsFinished && BB->use_empty()) {
    delete BB;
    return;
  }

  // Place the block after the current block, if possible, or else at
  // the end of the function.
  if (CurBB && CurBB->getParent())
    CurFn->getBasicBlockList().insertAfter(CurBB, BB);
  else
    CurFn->getBasicBlockList().push_back(BB);
  Builder.SetInsertPoint(BB);
}

void CodeGenFunction::EmitBranch(llvm::BasicBlock *Target) {
  // Emit a branch from the current block to the target one if this
  // was a real block.  If this was just a fall-through block after a
  // terminator, don't emit it.
  llvm::BasicBlock *CurBB = Builder.GetInsertBlock();

  if (!CurBB || CurBB->getTerminator()) {
    // If there is no insert point or the previous block is already
    // terminated, don't touch it.
  } else {
    // Otherwise, create a fall-through branch.
    Builder.CreateBr(Target);
  }

  Builder.ClearInsertionPoint();
}

void CodeGenFunction::EmitIfCmd(const IfCmd &S) {
  // C99 6.8.4.1: The first substatement is executed if the expression compares
  // unequal to 0.  The condition must be a scalar type.
  RunCleanupsScope ConditionScope(*this);

//  if (S.getConditionVariable())
//    EmitAutoVarDefn(*S.getConditionVariable());

  // If the condition constant folds and can be elided, try to avoid emitting
  // the condition and the dead arm of the if/else.
  bool CondConstant;
  if (ConstantFoldsToSimpleInteger(S.getCond(), CondConstant)) {
    // Figure out which block (then or else) is executed.
    const Stmt *Executed = S.getThen();
    const Stmt *Skipped  = S.getElse();
    if (!CondConstant)  // Condition false?
      std::swap(Executed, Skipped);

    // If the skipped block has no labels in it, just emit the executed block.
    // This avoids emitting dead code and simplifies the CFG substantially.
//    if (!ContainsLabel(Skipped)) {
//      if (Executed) {
//        RunCleanupsScope ExecutedScope(*this);
//        EmitStmt(Executed);
//      }
//      return;
//    }
  }

  // Otherwise, the condition did not fold, or we couldn't elide it.  Just emit
  // the conditional branch.
  llvm::BasicBlock *ThenBlock = createBasicBlock("if.then");
  llvm::BasicBlock *ContBlock = createBasicBlock("if.end");
  llvm::BasicBlock *ElseBlock = ContBlock;
  if (S.getElse())
    ElseBlock = createBasicBlock("if.else");
  EmitBranchOnBoolExpr(S.getCond(), ThenBlock, ElseBlock);

  // Emit the 'then' code.
  EmitBlock(ThenBlock); 
  {
    RunCleanupsScope ThenScope(*this);
    EmitStmt(S.getThen());
  }
  EmitBranch(ContBlock);

  // Emit the 'else' code if present.
  if (const Stmt *Else = S.getElse()) {
    // There is no need to emit line number for unconditional branch.
    if (getDebugInfo())
      Builder.SetCurrentDebugLocation(llvm::DebugLoc());
    EmitBlock(ElseBlock);
    {
      RunCleanupsScope ElseScope(*this);
      EmitStmt(Else);
    }
    // There is no need to emit line number for unconditional branch.
    if (getDebugInfo())
      Builder.SetCurrentDebugLocation(llvm::DebugLoc());
    EmitBranch(ContBlock);
  }

  // Emit the continuation block for code after the if.
  EmitBlock(ContBlock, true);
}

void CodeGenFunction::EmitWhileCmd(const WhileCmd &S) {
  // Emit the header for the loop, which will also become
  // the continue target.
  JumpDest LoopHeader = getJumpDestInCurrentScope("while.cond");
  EmitBlock(LoopHeader.getBlock());

  // Create an exit block for when the condition fails, which will
  // also become the break target.
  JumpDest LoopExit = getJumpDestInCurrentScope("while.end");

  // Store the blocks to use for break and continue.
  BreakContinueStack.push_back(BreakContinue(LoopExit, LoopHeader));

  // C++ [stmt.while]p2:
  //   When the condition of a while statement is a declaration, the
  //   scope of the variable that is declared extends from its point
  //   of declaration (3.3.2) to the end of the while statement.
  //   [...]
  //   The object created in a condition is destroyed and created
  //   with each iteration of the loop.
  RunCleanupsScope ConditionScope(*this);

//  if (S.getConditionVariable())
//    EmitAutoVarDecl(*S.getConditionVariable());
  
  // Evaluate the conditional in the while header.  C99 6.8.5.1: The
  // evaluation of the controlling expression takes place before each
  // execution of the loop body.
  llvm::Value *BoolCondVal = EvaluateExprAsBool(S.getCond());
   
  // while(1) is common, avoid extra exit blocks.  Be sure
  // to correctly handle break/continue though.
  bool EmitBoolCondBranch = true;
  if (llvm::ConstantInt *C = dyn_cast<llvm::ConstantInt>(BoolCondVal))
    if (C->isOne())
      EmitBoolCondBranch = false;

  // As long as the condition is true, go to the loop body.
  llvm::BasicBlock *LoopBody = createBasicBlock("while.body");
  if (EmitBoolCondBranch) {
    llvm::BasicBlock *ExitBlock = LoopExit.getBlock();
    if (ConditionScope.requiresCleanups())
      ExitBlock = createBasicBlock("while.exit");

    Builder.CreateCondBr(BoolCondVal, LoopBody, ExitBlock);

    if (ExitBlock != LoopExit.getBlock()) {
      EmitBlock(ExitBlock);
      EmitBranchThroughCleanup(LoopExit);
    }
  }
 
  // Emit the loop body.  We have to emit this in a cleanup scope
  // because it might be a singleton DeclCmd.
  {
    RunCleanupsScope BodyScope(*this);
    EmitBlock(LoopBody);
    EmitStmt(S.getBody());
  }

  BreakContinueStack.pop_back();

  // Immediately force cleanup.
  ConditionScope.ForceCleanup();

  // Branch to the loop header again.
  EmitBranch(LoopHeader.getBlock());

  // Emit the exit block.
  EmitBlock(LoopExit.getBlock(), true);

  // The LoopHeader typically is just a branch if we skipped emitting
  // a branch, try to erase it.
  if (!EmitBoolCondBranch)
    SimplifyForwardingBlocks(LoopHeader.getBlock());
}

void CodeGenFunction::EmitForCmd(const ForCmd &S) {
  JumpDest LoopExit = getJumpDestInCurrentScope("for.end");

  RunCleanupsScope ForScope(*this);

  CGDebugInfo *DI = getDebugInfo();
  if (DI) {
    DI->setLocation(S.getSourceRange().getBegin());
    DI->EmitRegionStart(Builder);
  }

  // Evaluate the first part before the loop.
//  if (S.getInit())
//    EmitStmt(S.getInit());

  // Start the loop with a block that tests the condition.
  // If there's an increment, the continue scope will be overwritten
  // later.
  JumpDest Continue = getJumpDestInCurrentScope("for.cond");
  llvm::BasicBlock *CondBlock = Continue.getBlock();
  EmitBlock(CondBlock);

  // Create a cleanup scope for the condition variable cleanups.
  RunCleanupsScope ConditionScope(*this);
  
  llvm::Value *BoolCondVal = 0;
//  if (S.getCond()) {
//    // If the for statement has a condition scope, emit the local variable
//    // declaration.
//    llvm::BasicBlock *ExitBlock = LoopExit.getBlock();
//    if (S.getConditionVariable()) {
//      EmitAutoVarDecl(*S.getConditionVariable());
//    }
//
//    // If there are any cleanups between here and the loop-exit scope,
//    // create a block to stage a loop exit along.
//    if (ForScope.requiresCleanups())
//      ExitBlock = createBasicBlock("for.cond.cleanup");
//
//    // As long as the condition is true, iterate the loop.
//    llvm::BasicBlock *ForBody = createBasicBlock("for.body");
//
//    // C99 6.8.5p2/p4: The first substatement is executed if the expression
//    // compares unequal to 0.  The condition must be a scalar type.
//    BoolCondVal = EvaluateExprAsBool(S.getCond());
//    Builder.CreateCondBr(BoolCondVal, ForBody, ExitBlock);
//
//    if (ExitBlock != LoopExit.getBlock()) {
//      EmitBlock(ExitBlock);
//      EmitBranchThroughCleanup(LoopExit);
//    }
//
//    EmitBlock(ForBody);
//  } else {
//    // Treat it as a non-zero constant.  Don't even create a new block for the
//    // body, just fall into it.
//  }

  // If the for loop doesn't have an increment we can just use the
  // condition as the continue block.  Otherwise we'll need to create
  // a block for it (in the current scope, i.e. in the scope of the
  // condition), and that we will become our continue block.
//  if (S.getInc())
//    Continue = getJumpDestInCurrentScope("for.inc");

  // Store the blocks to use for break and continue.
  BreakContinueStack.push_back(BreakContinue(LoopExit, Continue));

  {
    // Create a separate cleanup scope for the body, in case it is not
    // a compound statement.
    RunCleanupsScope BodyScope(*this);
    EmitStmt(S.getBody());
  }

  // If there is an increment, emit it next.
//  if (S.getInc()) {
//    EmitBlock(Continue.getBlock());
//    EmitStmt(S.getInc());
//  }

  BreakContinueStack.pop_back();

  ConditionScope.ForceCleanup();
  EmitBranch(CondBlock);

  ForScope.ForceCleanup();

  if (DI) {
    DI->setLocation(S.getSourceRange().getEnd());
    DI->EmitRegionEnd(Builder);
  }

  // Emit the fall-through block.
  EmitBlock(LoopExit.getBlock(), true);
}

void CodeGenFunction::EmitReturnOfRValue(RValue RV, Type Ty) {
  if (RV.isScalar()) {
    Builder.CreateStore(RV.getScalarVal(), ReturnValue);
  } else if (RV.isAggregate()) {
    EmitAggregateCopy(ReturnValue, RV.getAggregateAddr(), Ty);
  } else {
    StoreComplexToAddr(RV.getComplexVal(), ReturnValue, false);
  }
  EmitBranchThroughCleanup(ReturnBlock);
}

/// EmitReturnCmd - Note that due to GCC extensions, this can have an operand
/// if the function returns void, or may be missing one if the function returns
/// non-void.  Fun stuff :).
void CodeGenFunction::EmitReturnCmd(const ReturnCmd &S) {
  // Emit the result value, even if unused, to evalute the side effects.
  //const Expr *RV = S.getRetValue();

  // FIXME: Clean this up by using an LValue for ReturnTemp,
  // EmitStoreThroughLValue, and EmitAnyExpr.
//  if (S.getNRVOCandidate() && S.getNRVOCandidate()->isNRVOVariable() &&
//      !Target.useGlobalsForAutomaticVariables()) {
//    // Apply the named return value optimization for this return statement,
//    // which means doing nothing: the appropriate result has already been
//    // constructed into the NRVO variable.
//
//    // If there is an NRVO flag for this variable, set it to 1 into indicate
//    // that the cleanup code should not destroy the variable.
//    if (llvm::Value *NRVOFlag = NRVOFlags[S.getNRVOCandidate()])
//      Builder.CreateStore(Builder.getTrue(), NRVOFlag);
//  } else if (!ReturnValue) {
//    // Make sure not to return anything, but evaluate the expression
//    // for side effects.
//    if (RV)
//      EmitAnyExpr(RV);
//  } else if (RV == 0) {
//    // Do nothing (return value is left uninitialized)
//  } else if (FnRetTy->isReferenceType()) {
//    // If this function returns a reference, take the address of the expression
//    // rather than the value.
//    RValue Result = EmitReferenceBindingToExpr(RV, /*InitializedDecl=*/0);
//    Builder.CreateStore(Result.getScalarVal(), ReturnValue);
//  } else if (!hasAggregateLLVMType(RV->getType())) {
//    Builder.CreateStore(EmitScalarExpr(RV), ReturnValue);
//  } else if (RV->getType().hasComplexAttr()) {
//    EmitComplexExprIntoAddr(RV, ReturnValue, false);
//  } else {
//    EmitAggExpr(RV, AggValueSlot::forAddr(ReturnValue, TypeInfo(), true));
//  }

  EmitBranchThroughCleanup(ReturnBlock);
}

void CodeGenFunction::EmitDefnCmd(const DefnCmd &S) {
  // As long as debug info is modeled with instructions, we have to ensure we
  // have a place to insert here and write the stop point here.
  if (getDebugInfo() && HaveInsertPoint())
    EmitStopPoint(&S);

  for (DefnCmd::const_defn_iterator I = S.defn_begin(), E = S.defn_end();
       I != E; ++I)
    EmitDefn(**I);
}

void CodeGenFunction::EmitBreakCmd(const BreakCmd &S) {
  assert(!BreakContinueStack.empty() && "break stmt not in a loop or switch!");

  // If this code is reachable then emit a stop point (if generating
  // debug info). We have to do this ourselves because we are on the
  // "simple" statement path.
  if (HaveInsertPoint())
    EmitStopPoint(&S);

  JumpDest Block = BreakContinueStack.back().BreakBlock;
  EmitBranchThroughCleanup(Block);
}

void CodeGenFunction::EmitContinueCmd(const ContinueCmd &S) {
  assert(!BreakContinueStack.empty() && "continue stmt not in a loop!");

  // If this code is reachable then emit a stop point (if generating
  // debug info). We have to do this ourselves because we are on the
  // "simple" statement path.
  if (HaveInsertPoint())
    EmitStopPoint(&S);

  JumpDest Block = BreakContinueStack.back().ContinueBlock;
  EmitBranchThroughCleanup(Block);
}

/// EmitCaseCmdRange - If case statement range is not too big then
/// add multiple cases to switch instruction, one for each value within
/// the range. If range is too big then emit "if" condition check.
void CodeGenFunction::EmitCaseCmdRange(const CaseCmd &S) {
//  assert(S.getRHS() && "Expected RHS value in CaseCmd");
//
//  llvm::APSInt LHS = S.getLHS()->EvaluateAsInt(getContext());
//  llvm::APSInt RHS = S.getRHS()->EvaluateAsInt(getContext());
//
//  // Emit the code for this case. We do this first to make sure it is
//  // properly chained from our predecessor before generating the
//  // switch machinery to enter this block.
//  EmitBlock(createBasicBlock("sw.bb"));
//  llvm::BasicBlock *CaseDest = Builder.GetInsertBlock();
//  EmitStmt(S.getSubStmt());
//
//  // If range is empty, do nothing.
//  if (LHS.isSigned() ? RHS.slt(LHS) : RHS.ult(LHS))
//    return;
//
//  llvm::APInt Range = RHS - LHS;
//  // FIXME: parameters such as this should not be hardcoded.
//  if (Range.ult(llvm::APInt(Range.getBitWidth(), 64))) {
//    // Range is small enough to add multiple switch instruction cases.
//    for (unsigned i = 0, e = Range.getZExtValue() + 1; i != e; ++i) {
//      SwitchInsn->addCase(Builder.getInt(LHS), CaseDest);
//      LHS++;
//    }
//    return;
//  }
//
//  // The range is too big. Emit "if" condition into a new block,
//  // making sure to save and restore the current insertion point.
//  llvm::BasicBlock *RestoreBB = Builder.GetInsertBlock();
//
//  // Push this test onto the chain of range checks (which terminates
//  // in the default basic block). The switch's default will be changed
//  // to the top of this chain after switch emission is complete.
//  llvm::BasicBlock *FalseDest = CaseRangeBlock;
//  CaseRangeBlock = createBasicBlock("sw.caserange");
//
//  CurFn->getBasicBlockList().push_back(CaseRangeBlock);
//  Builder.SetInsertPoint(CaseRangeBlock);
//
//  // Emit range check.
//  llvm::Value *Diff =
//    Builder.CreateSub(SwitchInsn->getCondition(), Builder.getInt(LHS),  "tmp");
//  llvm::Value *Cond =
//    Builder.CreateICmpULE(Diff, Builder.getInt(Range), "inbounds");
//  Builder.CreateCondBr(Cond, CaseDest, FalseDest);
//
//  // Restore the appropriate insertion point.
//  if (RestoreBB)
//    Builder.SetInsertPoint(RestoreBB);
//  else
//    Builder.ClearInsertionPoint();
}

void CodeGenFunction::EmitCaseCmd(const CaseCmd &S) {
  // Handle case ranges.
//  if (S.getRHS()) {
//    EmitCaseCmdRange(S);
//    return;
//  }
//
//  llvm::ConstantInt *CaseVal =
//    Builder.getInt(S.getLHS()->EvaluateAsInt(getContext()));
//
//  // If the body of the case is just a 'break', and if there was no fallthrough,
//  // try to not emit an empty block.
//  if (isa<BreakCmd>(S.getSubStmt())) {
//    JumpDest Block = BreakContinueStack.back().BreakBlock;
//
//    // Only do this optimization if there are no cleanups that need emitting.
//    if (isObviouslyBranchWithoutCleanups(Block)) {
//      SwitchInsn->addCase(CaseVal, Block.getBlock());
//
//      // If there was a fallthrough into this case, make sure to redirect it to
//      // the end of the switch as well.
//      if (Builder.GetInsertBlock()) {
//        Builder.CreateBr(Block.getBlock());
//        Builder.ClearInsertionPoint();
//      }
//      return;
//    }
//  }
//
//  EmitBlock(createBasicBlock("sw.bb"));
//  llvm::BasicBlock *CaseDest = Builder.GetInsertBlock();
//  SwitchInsn->addCase(CaseVal, CaseDest);
//
//  // Recursively emitting the statement is acceptable, but is not wonderful for
//  // code where we have many case statements nested together, i.e.:
//  //  case 1:
//  //    case 2:
//  //      case 3: etc.
//  // Handling this recursively will create a new block for each case statement
//  // that falls through to the next case which is IR intensive.  It also causes
//  // deep recursion which can run into stack depth limitations.  Handle
//  // sequential non-range case statements specially.
//  const CaseCmd *CurCase = &S;
//  const CaseCmd *NextCase = dyn_cast<CaseCmd>(S.getSubStmt());
//
//  // Otherwise, iteratively add consecutive cases to this switch stmt.
//  while (NextCase && NextCase->getRHS() == 0) {
//    CurCase = NextCase;
//    llvm::ConstantInt *CaseVal =
//      Builder.getInt(CurCase->getLHS()->EvaluateAsInt(getContext()));
//    SwitchInsn->addCase(CaseVal, CaseDest);
//    NextCase = dyn_cast<CaseCmd>(CurCase->getSubStmt());
//  }
//
//  // Normal default recursion for non-cases.
//  EmitCmd(CurCase->getSubStmt());
}

void CodeGenFunction::EmitOtherwiseCmd(const OtherwiseCmd &S) {
  llvm::BasicBlock *DefaultBlock = SwitchInsn->getDefaultDest();
  assert(DefaultBlock->empty() &&
         "EmitOtherwiseCmd: Default block already defined?");
  EmitBlock(DefaultBlock);
//  EmitStmt(S.getSubStmt());
}

/// CollectStatementsForCase - Given the body of a 'switch' statement and a
/// constant value that is being switched on, see if we can dead code eliminate
/// the body of the switch to a simple series of statements to emit.  Basically,
/// on a switch (5) we want to find these statements:
///    case 5:
///      printf(...);    <--
///      ++i;            <--
///      break;
///
/// and add them to the ResultStmts vector.  If it is unsafe to do this
/// transformation (for example, one of the elided statements contains a label
/// that might be jumped to), return CSFC_Failure.  If we handled it and 'S'
/// should include statements after it (e.g. the printf() line is a substmt of
/// the case) then return CSFC_FallThrough.  If we handled it and found a break
/// statement, then return CSFC_Success.
///
/// If Case is non-null, then we are looking for the specified case, checking
/// that nothing we jump over contains labels.  If Case is null, then we found
/// the case and are looking for the break.
///
/// If the recursive walk actually finds our Case, then we set FoundCase to
/// true.
///
enum CSFC_Result { CSFC_Failure, CSFC_FallThrough, CSFC_Success };
static CSFC_Result CollectStatementsForCase(const Stmt *S,
                                            const CaseCmd *Case,
                                            bool &FoundCase,
                              llvm::SmallVectorImpl<const Stmt*> &ResultStmts) {
  // If this is a null statement, just succeed.
  if (S == 0)
    return Case ? CSFC_Success : CSFC_FallThrough;
    
  // If this is the switchcase (case 4: or default) that we're looking for, then
  // we're in business.  Just add the substatement.
  if (const CaseCmd *SC = dyn_cast<CaseCmd>(S)) {
    if (S == Case) {
      FoundCase = true;
//      return CollectStatementsForCase(SC->getSubStmt(), 0, FoundCase,
//                                      ResultStmts);
      return CSFC_Failure;
    }
    
    // Otherwise, this is some other case or default statement, just ignore it.
//    return CollectStatementsForCase(SC->getSubStmt(), Case, FoundCase,
//                                    ResultStmts);
    return CSFC_Failure;
  }

  // If we are in the live part of the code and we found our break statement,
  // return a success!
  if (Case == 0 && isa<BreakCmd>(S))
    return CSFC_Success;
  
  // If this is a switch statement, then it might contain the SwitchCase, the
  // break, or neither.
  if (const BlockCmd *CS = dyn_cast<BlockCmd>(S)) {
    // Handle this as two cases: we might be looking for the SwitchCase (if so
    // the skipped statements must be skippable) or we might already have it.
	  BlockCmd::const_body_iterator I = CS->body_begin(), E = CS->body_end();
    if (Case) {
      // Keep track of whether we see a skipped declaration.  The code could be
      // using the declaration even if it is skipped, so we can't optimize out
      // the decl if the kept statements might refer to it.
      bool HadSkippedDecl = false;
      
      // If we're looking for the case, just see if we can skip each of the
      // substatements.
      for (; Case && I != E; ++I) {
        HadSkippedDecl |= isa<DefnCmd>(*I);
        
        switch (CollectStatementsForCase(*I, Case, FoundCase, ResultStmts)) {
        case CSFC_Failure: return CSFC_Failure;
        case CSFC_Success:
          // A successful result means that either 1) that the statement doesn't
          // have the case and is skippable, or 2) does contain the case value
          // and also contains the break to exit the switch.  In the later case,
          // we just verify the rest of the statements are elidable.
          if (FoundCase) {
            // If we found the case and skipped declarations, we can't do the
            // optimization.
            if (HadSkippedDecl)
              return CSFC_Failure;
            
            return CSFC_Success;
          }
          break;
        case CSFC_FallThrough:
          // If we have a fallthrough condition, then we must have found the
          // case started to include statements.  Consider the rest of the
          // statements in the compound statement as candidates for inclusion.
          assert(FoundCase && "Didn't find case but returned fallthrough?");
          // We recursively found Case, so we're not looking for it anymore.
          Case = 0;
            
          // If we found the case and skipped declarations, we can't do the
          // optimization.
          if (HadSkippedDecl)
            return CSFC_Failure;
          break;
        }
      }
    }

    // If we have statements in our range, then we know that the statements are
    // live and need to be added to the set of statements we're tracking.
    for (; I != E; ++I) {
      switch (CollectStatementsForCase(*I, 0, FoundCase, ResultStmts)) {
      case CSFC_Failure: return CSFC_Failure;
      case CSFC_FallThrough:
        // A fallthrough result means that the statement was simple and just
        // included in ResultStmt, keep adding them afterwards.
        break;
      case CSFC_Success:
        return CSFC_Success;
      }      
    }
    
    return Case ? CSFC_Success : CSFC_FallThrough;
  }

  // Okay, this is some other statement that we don't handle explicitly, like a
  // for statement or increment etc.  If we are skipping over this statement,
  // just verify it doesn't have labels, which would make it invalid to elide.
  if (Case) {
    return CSFC_Success;
  }
  
  // Otherwise, we want to include this statement.  Everything is cool with that
  // so long as it doesn't contain a break out of the switch we're in.
  // if (CodeGenFunction::containsBreak(S)) return CSFC_Failure;
  
  // Otherwise, everything is great.  Include the statement and tell the caller
  // that we fall through and include the next statement as well.
  ResultStmts.push_back(S);
  return CSFC_FallThrough;
}

/// FindCaseStatementsForValue - Find the case statement being jumped to and
/// then invoke CollectStatementsForCase to find the list of statements to emit
/// for a switch on constant.  See the comment above CollectStatementsForCase
/// for more details.
static bool FindCaseStatementsForValue(const SwitchCmd &S,
                                       const llvm::APInt &ConstantCondValue,
                                llvm::SmallVectorImpl<const Stmt*> &ResultStmts,
                                       ASTContext &C) {
  return false;
}

void CodeGenFunction::EmitSwitchCmd(const SwitchCmd &S) {

}

void CodeGenFunction::EmitTryCmd(const TryCmd &S) {

}

static std::string
SimplifyConstraint(const char *Constraint, const TargetInfo &Target,
                 llvm::SmallVectorImpl<TargetInfo::ConstraintInfo> *OutCons=0) {
  std::string Result;

  while (*Constraint) {
    switch (*Constraint) {
    default:
      //Result += Target.convertConstraint(Constraint);
      break;
    // Ignore these
    case '*':
    case '?':
    case '!':
    case '=': // Will see this and the following in mult-alt constraints.
    case '+':
      break;
    case ',':
      Result += "|";
      break;
    case 'g':
      Result += "imr";
      break;
    case '[': {
      assert(OutCons &&
             "Must pass output names to constraints with a symbolic name");
      unsigned Index;
      bool result = Target.resolveSymbolicName(Constraint,
                                               &(*OutCons)[0],
                                               OutCons->size(), Index);
      assert(result && "Could not resolve symbolic name"); (void)result;
      Result += llvm::utostr(Index);
      break;
    }
    }

    Constraint++;
  }

  return Result;
}
