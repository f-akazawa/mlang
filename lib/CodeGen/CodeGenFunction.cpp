//===--- CodeGenFunction.cpp - Emit LLVM Code from ASTs for a Function ----===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This coordinates the per-function state used while generating code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGOOPABI.h"
#include "CGDebugInfo.h"
#include "CGException.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/AST/APValue.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/CmdAll.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Intrinsics.h"
using namespace mlang;
using namespace CodeGen;

CodeGenFunction::CodeGenFunction(CodeGenModule &cgm)
  : CodeGenTypeCache(cgm), CGM(cgm),
    Target(CGM.getContext().Target), Builder(cgm.getModule().getContext()),
    AutoreleaseResult(false), BlockInfo(0), BlockPointer(0),
    NormalCleanupDest(0), EHCleanupDest(0), NextCleanupDestIndex(1),
    ExceptionSlot(0),
    DebugInfo(0), DisableDebugInfo(false), DidCallStackSave(false),
    IndirectBranch(0), SwitchInsn(0), CaseRangeBlock(0), UnreachableBlock(0),
    ClassThisDefn(0), ClassThisValue(0), ClassVTTDefn(0), ClassVTTValue(0),
    OutermostConditional(0), TerminateLandingPad(0), TerminateHandler(0),
    TrapBB(0) {

  CatchUndefined = false/*getContext().getLangOptions().CatchUndefined*/;
  CGM.getOOPABI().getMangleContext().startNewFunction();
}

llvm::Type *CodeGenFunction::ConvertTypeForMem(Type T) {
  return CGM.getTypes().ConvertTypeForMem(T);
}

llvm::Type *CodeGenFunction::ConvertType(Type T) {
  return CGM.getTypes().ConvertType(T);
}

bool CodeGenFunction::hasAggregateLLVMType(Type T) {
  return T->isMatrixType() || T->isCellType() ||
		     T->isStructType() || T.hasComplexAttr();
}

void CodeGenFunction::EmitReturnBlock() {
  // For cleanliness, we try to avoid emitting the return block for
  // simple cases.
  llvm::BasicBlock *CurBB = Builder.GetInsertBlock();

  if (CurBB) {
    assert(!CurBB->getTerminator() && "Unexpected terminated block.");

    // We have a valid insert point, reuse it if it is empty or there are no
    // explicit jumps to the return block.
    if (CurBB->empty() || ReturnBlock.getBlock()->use_empty()) {
      ReturnBlock.getBlock()->replaceAllUsesWith(CurBB);
      delete ReturnBlock.getBlock();
    } else
      EmitBlock(ReturnBlock.getBlock());
    return;
  }

  // Otherwise, if the return block is the target of a single direct
  // branch then we can just put the code in that block instead. This
  // cleans up functions which started with a unified return block.
  if (ReturnBlock.getBlock()->hasOneUse()) {
    llvm::BranchInst *BI =
      dyn_cast<llvm::BranchInst>(*ReturnBlock.getBlock()->use_begin());
    if (BI && BI->isUnconditional() &&
        BI->getSuccessor(0) == ReturnBlock.getBlock()) {
      // Reset insertion point and delete the branch.
      Builder.SetInsertPoint(BI->getParent());
      BI->eraseFromParent();
      delete ReturnBlock.getBlock();
      return;
    }
  }

  // FIXME: We are at an unreachable point, there is no reason to emit the block
  // unless it has uses. However, we still need a place to put the debug
  // region.end for now.

  EmitBlock(ReturnBlock.getBlock());
}

static void EmitIfUsed(CodeGenFunction &CGF, llvm::BasicBlock *BB) {
  if (!BB) return;
  if (!BB->use_empty())
    return CGF.CurFn->getBasicBlockList().push_back(BB);
  delete BB;
}

void CodeGenFunction::FinishFunction(SourceLocation EndLoc) {
  assert(BreakContinueStack.empty() &&
         "mismatched push/pop in break/continue stack!");

  // Emit function epilog (to return).
  EmitReturnBlock();

  EmitFunctionInstrumentation("__cyg_profile_func_exit");

  // Emit debug descriptor for function end.
  if (CGDebugInfo *DI = getDebugInfo()) {
//    DI->setLocation(EndLoc);
//    DI->EmitFunctionEnd(Builder);
  }

  EmitFunctionEpilog(*CurFnInfo);
//  EmitEndEHSpec(CurCodeDefn);

  assert(EHStack.empty() &&
         "did not remove all scopes from cleanup stack!");

  // If someone did an indirect goto, emit the indirect goto block at the end of
  // the function.
  if (IndirectBranch) {
    EmitBlock(IndirectBranch->getParent());
    Builder.ClearInsertionPoint();
  }
  
  // Remove the AllocaInsertPt instruction, which is just a convenience for us.
  llvm::Instruction *Ptr = AllocaInsertPt;
  AllocaInsertPt = 0;
  Ptr->eraseFromParent();
  
  // If someone took the address of a label but never did an indirect goto, we
  // made a zero entry PHI node, which is illegal, zap it now.
  if (IndirectBranch) {
    llvm::PHINode *PN = cast<llvm::PHINode>(IndirectBranch->getAddress());
    if (PN->getNumIncomingValues() == 0) {
      PN->replaceAllUsesWith(llvm::UndefValue::get(PN->getType()));
      PN->eraseFromParent();
    }
  }

  EmitIfUsed(*this, RethrowBlock.getBlock());
  EmitIfUsed(*this, TerminateLandingPad);
  EmitIfUsed(*this, TerminateHandler);
  EmitIfUsed(*this, UnreachableBlock);

  if (CGM.getCodeGenOpts().EmitDeclMetadata)
    EmitDefnMetadata();
}

/// ShouldInstrumentFunction - Return true if the current function should be
/// instrumented with __cyg_profile_func_* calls
bool CodeGenFunction::ShouldInstrumentFunction() {
  if (!CGM.getCodeGenOpts().InstrumentFunctions)
    return false;
//  if (CurFuncDefn->hasAttr<NoInstrumentFunctionAttr>())
//    return false;
  return true;
}

/// EmitFunctionInstrumentation - Emit LLVM code to call the specified
/// instrumentation function with the current function and the call site, if
/// function instrumentation is enabled.
void CodeGenFunction::EmitFunctionInstrumentation(const char *Fn) {
  if (!ShouldInstrumentFunction())
    return;

  llvm::PointerType *PointerTy;
  llvm::FunctionType *FunctionTy;
  std::vector<llvm::Type*> ProfileFuncArgs;

  // void __cyg_profile_func_{enter,exit} (void *this_fn, void *call_site);
  PointerTy = llvm::Type::getInt8PtrTy(getLLVMContext());
  ProfileFuncArgs.push_back(PointerTy);
  ProfileFuncArgs.push_back(PointerTy);
  FunctionTy = llvm::FunctionType::get(
    llvm::Type::getVoidTy(getLLVMContext()),
    ProfileFuncArgs, false);

  llvm::Constant *F = CGM.CreateRuntimeFunction(FunctionTy, Fn);
  llvm::CallInst *CallSite = Builder.CreateCall(
    CGM.getIntrinsic(llvm::Intrinsic::returnaddress),
    llvm::ConstantInt::get(Int32Ty, 0),
    "callsite");

  Builder.CreateCall2(F,
                      llvm::ConstantExpr::getBitCast(CurFn, PointerTy),
                      CallSite);
}

void CodeGenFunction::StartFunction(GlobalDefn GD,
		                                Type &RetTy,
                                    llvm::Function *Fn,
                                    const FunctionArgList &Args,
                                    SourceLocation StartLoc) {
  const Defn *D = GD.getDefn();
  
  DidCallStackSave = false;
  CurCodeDefn = CurFuncDefn = D;
  FnRetTy = RetTy;
  CurFn = Fn;
  assert(CurFn->isDeclaration() && "Function already has body?");

  // Pass inline keyword to optimizer if it appears explicitly on any
  // declaration.
//  if (const FunctionDefn *FD = dyn_cast_or_null<FunctionDefn>(D))
//    for (FunctionDefn::redecl_iterator RI = FD->redecls_begin(),
//           RE = FD->redecls_end(); RI != RE; ++RI)
//      if (RI->isInlineSpecified()) {
//        Fn->addFnAttr(llvm::Attribute::InlineHint);
//        break;
//      }

  llvm::BasicBlock *EntryBB = createBasicBlock("entry", CurFn);

  // Create a marker to make it easy to insert allocas into the entryblock
  // later.  Don't create this with the builder, because we don't want it
  // folded.
  llvm::Value *Undef = llvm::UndefValue::get(Int32Ty);
  AllocaInsertPt = new llvm::BitCastInst(Undef, Int32Ty, "", EntryBB);
  if (Builder.isNamePreserving())
    AllocaInsertPt->setName("allocapt");

  ReturnBlock = getJumpDestInCurrentScope("return");

  Builder.SetInsertPoint(EntryBB);

  // Emit subprogram debug descriptor.
//  if (CGDebugInfo *DI = getDebugInfo()) {
//    // FIXME: what is going on here and why does it ignore all these
//    // interesting type properties?
//    Type FnType =
//      getContext().getFunctionType(RetTy, 0, 0,
//                                   FunctionProtoType::ExtProtoInfo());
//
//    DI->setLocation(StartLoc);
//    DI->EmitFunctionStart(GD, FnType, CurFn, Builder);
//  }

  if (ShouldInstrumentFunction())
	  EmitFunctionInstrumentation("__cyg_profile_func_enter");

//  if (CGM.getCodeGenOpts().InstrumentForProfiling)
//    EmitMCountInstrumentation();

  // FIXME: Leaked.
  // CC info is ignored, hopefully?
  CurFnInfo = &CGM.getTypes().getFunctionInfo(FnRetTy, Args,
                                              FunctionType::ExtInfo());

  if (RetTy.isNull()) {
	  // Void type; nothing to return.
	  ReturnValue = 0;
  } else if (CurFnInfo->getReturnInfo().getKind() == ABIArgInfo::Indirect &&
             hasAggregateLLVMType(CurFnInfo->getReturnType())) {
    // Indirect aggregate return; emit returned value directly into sret slot.
    // This reduces code size, and affects correctness in C++.
    ReturnValue = CurFn->arg_begin();
  } else {
    ReturnValue = CreateIRTemp(RetTy, "retval");
  }

//  EmitStartEHSpec(CurCodeDefn);
  EmitFunctionProlog(*CurFnInfo, CurFn, Args);

  if (D && isa<ClassMethodDefn>(D) && cast<ClassMethodDefn>(D)->isInstance())
    CGM.getOOPABI().EmitInstanceFunctionProlog(*this);

  // If any of the arguments have a variably modified type, make sure to
  // emit the type size.
//  for (FunctionArgList::const_iterator i = Args.begin(), e = Args.end();
//       i != e; ++i) {
//    Type Ty = i->getType();
//
////    if (Ty->isVariablyModifiedType())
////      EmitVLASize(Ty);
//  }
}

void CodeGenFunction::EmitFunctionBody(FunctionArgList &Args) {
  const FunctionDefn *FD = cast<FunctionDefn>(CurGD.getDefn());
  assert(FD->getBody());
  EmitStmt(FD->getBody());
}

/// Tries to mark the given function nounwind based on the
/// non-existence of any throwing calls within it.  We believe this is
/// lightweight enough to do at -O0.
static void TryMarkNoThrow(llvm::Function *F) {
  // LLVM treats 'nounwind' on a function as part of the type, so we
  // can't do this on functions that can be overwritten.
  if (F->mayBeOverridden()) return;

  for (llvm::Function::iterator FI = F->begin(), FE = F->end(); FI != FE; ++FI)
    for (llvm::BasicBlock::iterator
           BI = FI->begin(), BE = FI->end(); BI != BE; ++BI)
      if (llvm::CallInst *Call = dyn_cast<llvm::CallInst>(&*BI))
        if (!Call->doesNotThrow())
          return;
  F->setDoesNotThrow(true);
}

void CodeGenFunction::GenerateCode(GlobalDefn GD, llvm::Function *Fn) {

  const FunctionDefn *FD = cast<FunctionDefn>(GD.getDefn());
  
  // Check if we should generate debug info for this function.
  if (CGM.getModuleDebugInfo() /*&& !FD->hasAttr<NoDebugAttr>()*/)
    DebugInfo = CGM.getModuleDebugInfo();

  FunctionArgList Args;
  Type ResTy = FD->getResultType();

  CurGD = GD;
  if (isa<ClassMethodDefn>(FD) && cast<ClassMethodDefn>(FD)->isInstance())
    CGM.getOOPABI().BuildInstanceFunctionParams(*this, ResTy, Args);

  if (FD->getNumParams()) {
    const FunctionProtoType* FProto = FD->getType()->getAs<FunctionProtoType>();
    assert(FProto && "Function def must have prototype!");

    for (unsigned i = 0, e = FD->getNumParams(); i != e; ++i)
      Args.push_back(FD->getParamDefn(i));
  }

  SourceRange BodyRange;
  if (Stmt *Body = FD->getBody()) BodyRange = Body->getSourceRange();

  // Emit the standard function prologue.
  StartFunction(GD, ResTy, Fn, Args, BodyRange.getBegin());

  // Generate the body of the function.
  if (isa<ClassDestructorDefn>(FD)) {
//    EmitDestructorBody(Args);
  } else if (isa<ClassConstructorDefn>(FD)) {
//    EmitConstructorBody(Args);
  } else
    EmitFunctionBody(Args);

  // Emit the standard function epilogue.
  FinishFunction(BodyRange.getEnd());

  // If we haven't marked the function nothrow through other means, do
  // a quick pass now to see if we can.
  if (!CurFn->doesNotThrow())
    TryMarkNoThrow(CurFn);
}

/// ConstantFoldsToSimpleInteger - If the specified expression does not fold
/// to a constant, or if it does but contains a label, return false.  If it
/// constant folds return true and set the boolean result in Result.
bool CodeGenFunction::ConstantFoldsToSimpleInteger(const Expr *Cond,
                                                   bool &ResultBool) {
  llvm::APInt ResultInt;
  if (!ConstantFoldsToSimpleInteger(Cond, ResultInt))
    return false;

  ResultBool = ResultInt.getBoolValue();
  return true;
}

/// ConstantFoldsToSimpleInteger - If the specified expression does not fold
/// to a constant, or if it does but contains a label, return false.  If it
/// constant folds return true and set the folded value.
bool CodeGenFunction::
ConstantFoldsToSimpleInteger(const Expr *Cond, llvm::APInt &ResultInt) {
  // FIXME: Rename and handle conversion of other evaluatable things
  // to bool.
  Expr::EvalResult Result;
  if (!Cond->Evaluate(Result, getContext()) || !Result.Val.isInt() ||
      Result.HasSideEffects)
    return false;  // Not foldable, not integer or not fully evaluatable.

  ResultInt = Result.Val.getInt();
  return true;
}

/// EmitBranchOnBoolExpr - Emit a branch on a boolean condition (e.g. for an if
/// statement) to the specified blocks.  Based on the condition, this might try
/// to simplify the codegen of the conditional based on the branch.
///
void CodeGenFunction::EmitBranchOnBoolExpr(const Expr *Cond,
                                           llvm::BasicBlock *TrueBlock,
                                           llvm::BasicBlock *FalseBlock) {
	Cond = Cond->IgnoreParens();

	if (const BinaryOperator *CondBOp = dyn_cast<BinaryOperator>(Cond)) {
		// Handle X && Y in a condition.
		if (CondBOp->getOpcode() == BO_LAnd) {
			// If we have "1 && X", simplify the code.  "0 && X" would have constant
			// folded if the case was simple enough.
			bool ConstantBool = false;
			if (ConstantFoldsToSimpleInteger(CondBOp->getLHS(), ConstantBool)
					&& ConstantBool) {
				// br(1 && X) -> br(X).
				return EmitBranchOnBoolExpr(CondBOp->getRHS(), TrueBlock,
						FalseBlock);
			}

			// If we have "X && 1", simplify the code to use an uncond branch.
			// "X && 0" would have been constant folded to 0.
			if (ConstantFoldsToSimpleInteger(CondBOp->getRHS(), ConstantBool)
					&& ConstantBool) {
				// br(X && 1) -> br(X).
				return EmitBranchOnBoolExpr(CondBOp->getLHS(), TrueBlock,
						FalseBlock);
			}

			// Emit the LHS as a conditional.  If the LHS conditional is false, we
			// want to jump to the FalseBlock.
			llvm::BasicBlock *LHSTrue = createBasicBlock("land.lhs.true");

			ConditionalEvaluation eval(*this);
			EmitBranchOnBoolExpr(CondBOp->getLHS(), LHSTrue, FalseBlock);
			EmitBlock(LHSTrue);

			// Any temporaries created here are conditional.
			eval.begin(*this);
			EmitBranchOnBoolExpr(CondBOp->getRHS(), TrueBlock, FalseBlock);
			eval.end(*this);

			return;
		}

		if (CondBOp->getOpcode() == BO_LOr) {
			// If we have "0 || X", simplify the code.  "1 || X" would have constant
			// folded if the case was simple enough.
			bool ConstantBool = false;
			if (ConstantFoldsToSimpleInteger(CondBOp->getLHS(), ConstantBool)
					&& !ConstantBool) {
				// br(0 || X) -> br(X).
				return EmitBranchOnBoolExpr(CondBOp->getRHS(), TrueBlock,
						FalseBlock);
			}

			// If we have "X || 0", simplify the code to use an uncond branch.
			// "X || 1" would have been constant folded to 1.
			if (ConstantFoldsToSimpleInteger(CondBOp->getRHS(), ConstantBool)
					&& !ConstantBool) {
				// br(X || 0) -> br(X).
				return EmitBranchOnBoolExpr(CondBOp->getLHS(), TrueBlock,
						FalseBlock);
			}

			// Emit the LHS as a conditional.  If the LHS conditional is true, we
			// want to jump to the TrueBlock.
			llvm::BasicBlock *LHSFalse = createBasicBlock("lor.lhs.false");

			ConditionalEvaluation eval(*this);
			EmitBranchOnBoolExpr(CondBOp->getLHS(), TrueBlock, LHSFalse);
			EmitBlock(LHSFalse);

			// Any temporaries created here are conditional.
			eval.begin(*this);
			EmitBranchOnBoolExpr(CondBOp->getRHS(), TrueBlock, FalseBlock);
			eval.end(*this);

			return;
		}
	}

	if (const UnaryOperator *CondUOp = dyn_cast<UnaryOperator>(Cond)) {
		// br(!x, t, f) -> br(x, f, t)
		if (CondUOp->getOpcode() == UO_LNot)
			return EmitBranchOnBoolExpr(CondUOp->getSubExpr(), FalseBlock,
					TrueBlock);
	}

	// Emit the code with the fully general case.
	llvm::Value *CondV = EvaluateExprAsBool(Cond);
	Builder.CreateCondBr(CondV, TrueBlock, FalseBlock);
}

/// ErrorUnsupported - Print out an error that codegen doesn't support the
/// specified stmt yet.
void CodeGenFunction::ErrorUnsupported(const Stmt *S, const char *Type,
                                       bool OmitOnError) {
  CGM.ErrorUnsupported(S, Type, OmitOnError);
}

void
CodeGenFunction::EmitNullInitialization(llvm::Value *DestPtr, Type Ty) {
  // Ignore empty classes in C++.
  if (getContext().getLangOptions().OOP) {
    if (const HeteroContainerType *RT = Ty->getAs<HeteroContainerType>()) {
      if (cast<UserClassDefn>(RT->getDefn())->isEmpty())
        return;
    }
  }

  // Cast the dest ptr to the appropriate i8 pointer type.
  unsigned DestAS =
    cast<llvm::PointerType>(DestPtr->getType())->getAddressSpace();
  llvm::Type *BP = Builder.getInt8PtrTy(DestAS);
  if (DestPtr->getType() != BP)
    DestPtr = Builder.CreateBitCast(DestPtr, BP, "tmp");

  // Get size and alignment info for this aggregate.
  std::pair<uint64_t, unsigned> TypeInfo = getContext().getTypeSizeInfo(Ty);
  uint64_t Size = TypeInfo.first / 8;
  unsigned Align = TypeInfo.second / 8;

  // Don't bother emitting a zero-byte memset.
  if (Size == 0)
    return;

  llvm::ConstantInt *SizeVal = llvm::ConstantInt::get(IntPtrTy, Size);

  // If the type contains a pointer to data member we can't memset it to zero.
  // Instead, create a null constant and copy it to the destination.
  if (!CGM.getTypes().isZeroInitializable(Ty)) {
    llvm::Constant *NullConstant = CGM.EmitNullConstant(Ty);

    llvm::GlobalVariable *NullVariable = 
      new llvm::GlobalVariable(CGM.getModule(), NullConstant->getType(),
                               /*isConstant=*/true, 
                               llvm::GlobalVariable::PrivateLinkage,
                               NullConstant, llvm::Twine());
    llvm::Value *SrcPtr =
      Builder.CreateBitCast(NullVariable, Builder.getInt8PtrTy());

    // FIXME: variable-size types?

    // Get and call the appropriate llvm.memcpy overload.
    Builder.CreateMemCpy(DestPtr, SrcPtr, SizeVal, Align, false);
    return;
  } 
  
  // Otherwise, just memset the whole thing to zero.  This is legal
  // because in LLVM, all default initializers (other than the ones we just
  // handled above) are guaranteed to have a bit pattern of all zeros.

  // FIXME: Handle variable sized types.
  Builder.CreateMemSet(DestPtr, Builder.getInt8(0), SizeVal, Align, false);
}

//llvm::Value *CodeGenFunction::GetVLASize(const VariableArrayType *VAT) {
//  llvm::Value *&SizeEntry = VLASizeMap[VAT->getSizeExpr()];
//
//  assert(SizeEntry && "Did not emit size for type");
//  return SizeEntry;
//}

//llvm::Value *CodeGenFunction::EmitVLASize(Type Ty) {
//  assert(Ty->isVariablyModifiedType() &&
//         "Must pass variably modified type to EmitVLASizes!");
//
//  EnsureInsertPoint();
//
//  if (const VariableArrayType *VAT = getContext().getAsVariableArrayType(Ty)) {
//    // unknown size indication requires no size computation.
//    if (!VAT->getSizeExpr())
//      return 0;
//    llvm::Value *&SizeEntry = VLASizeMap[VAT->getSizeExpr()];
//
//    if (!SizeEntry) {
//      llvm::Type *SizeTy = ConvertType(getContext().getSizeType());
//
//      // Get the element size;
//      Type ElemTy = VAT->getElementType();
//      llvm::Value *ElemSize;
//      if (ElemTy->isVariableArrayType())
//        ElemSize = EmitVLASize(ElemTy);
//      else
//        ElemSize = llvm::ConstantInt::get(SizeTy,
//            getContext().getTypeSizeInChars(ElemTy).getQuantity());
//
//      llvm::Value *NumElements = EmitScalarExpr(VAT->getSizeExpr());
//      NumElements = Builder.CreateIntCast(NumElements, SizeTy, false, "tmp");
//
//      SizeEntry = Builder.CreateMul(ElemSize, NumElements);
//    }
//
//    return SizeEntry;
//  }
//
//  if (const ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
//    EmitVLASize(AT->getElementType());
//    return 0;
//  }
//
//  if (const ParenType *PT = dyn_cast<ParenType>(Ty)) {
//    EmitVLASize(PT->getInnerType());
//    return 0;
//  }
//
//  const PointerType *PT = Ty->getAs<PointerType>();
//  assert(PT && "unknown VM type!");
//  EmitVLASize(PT->getPointeeType());
//  return 0;
//}

//llvm::Value* CodeGenFunction::EmitVAListRef(const Expr* E) {
////  if (getContext().getBuiltinVaListType()->isArrayType())
////    return EmitScalarExpr(E);
//  return EmitLValue(E).getAddress();
//}

void CodeGenFunction::EmitDefnRefExprDbgValue(const DefnRefExpr *E,
                                              llvm::Constant *Init) {
  assert (Init && "Invalid DefnRefExpr initializer!");
  if (CGDebugInfo *Dbg = getDebugInfo()) {
//    Dbg->EmitGlobalVariable(E->getDefn(), Init);
  }
}

CodeGenFunction::PeepholeProtection
CodeGenFunction::protectFromPeepholes(RValue rvalue) {
  // At the moment, the only aggressive peephole we do in IR gen
  // is trunc(zext) folding, but if we add more, we can easily
  // extend this protection.

  if (!rvalue.isScalar()) return PeepholeProtection();
  llvm::Value *value = rvalue.getScalarVal();
  if (!isa<llvm::ZExtInst>(value)) return PeepholeProtection();

  // Just make an extra bitcast.
  assert(HaveInsertPoint());
  llvm::Instruction *inst = new llvm::BitCastInst(value, value->getType(), "",
                                                  Builder.GetInsertBlock());

  PeepholeProtection protection;
  protection.Inst = inst;
  return protection;
}

void CodeGenFunction::unprotectFromPeepholes(PeepholeProtection protection) {
  if (!protection.Inst) return;

  // In theory, we could try to duplicate the peepholes now, but whatever.
  protection.Inst->eraseFromParent();
}
