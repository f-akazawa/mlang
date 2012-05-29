//===--- CGDecl.cpp - Emit LLVM Code for declarations ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Decl nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CGDebugInfo.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/CharUnits.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Intrinsics.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Type.h"
using namespace mlang;
using namespace CodeGen;


void CodeGenFunction::EmitDefn(const Defn &D) {
  switch (D.getKind()) {
  case Defn::TranslationUnit:
  case Defn::Namespace:
  case Defn::ClassMethod:
  case Defn::ClassConstructor:
  case Defn::ClassDestructor:
  case Defn::Member:
  case Defn::ParamVar:
  case Defn::Script:
    assert(0 && "Declaration should not be in declstmts!");
  case Defn::Function:  // void X();
  case Defn::Struct:    // struct/union/class X;
  case Defn::Cell:
  case Defn::UserClass: // struct/union/class X; [C++]
    // None of these decls require codegen support.
    return;

  case Defn::Var: {
    const VarDefn &VD = cast<VarDefn>(D);
//    assert(VD.isLocalVarDefn() &&
//           "Should not see file-scope variables inside a function!");
    return EmitVarDefn(VD);
  }
  }
}

/// EmitVarDecl - This method handles emission of any variable declaration
/// inside a function, including static vars etc.
void CodeGenFunction::EmitVarDefn(const VarDefn &D) {
  switch (D.getStorageClass()) {
  case SC_None:
  case SC_Local:
  case SC_Automatic:
    return EmitAutoVarDefn(D);
  case SC_Persistent: {
    llvm::GlobalValue::LinkageTypes Linkage = 
      llvm::GlobalValue::InternalLinkage;

    // If the function definition has some sort of weak linkage, its
    // static variables should also be weak so that they get properly
    // uniqued.  We can't do this in C, though, because there's no
    // standard way to agree on which variables are the same (i.e.
    // there's no mangling).
    if (getContext().getLangOptions().OOP)
      if (llvm::GlobalValue::isWeakForLinker(CurFn->getLinkage()))
        Linkage = CurFn->getLinkage();
    
    return EmitStaticVarDefn(D, Linkage);
  }
  case SC_Global:
    // Don't emit it now, allow it to be emitted lazily on its first use.
    return;
  }

  assert(0 && "Unknown storage class");
}

static std::string GetStaticDefnName(CodeGenFunction &CGF, const VarDefn &D,
                                     const char *Separator) {
  CodeGenModule &CGM = CGF.CGM;
  if (CGF.getContext().getLangOptions().OOP) {
    llvm::StringRef Name = CGM.getMangledName(&D);
    return Name.str();
  }
  
  std::string ContextName;
  if (!CGF.CurFuncDefn) {
    // Better be in a block declared in global scope.
    const NamedDefn *ND = cast<NamedDefn>(&D);
    const DefnContext *DC = ND->getDefnContext();
    if (const ScriptDefn *BD = dyn_cast<ScriptDefn>(DC)) {
      MangleBuffer Name;
      CGM.getBlockMangledName(GlobalDefn(), Name, BD);
      ContextName = Name.getString();
    }
    else
      assert(0 && "Unknown context for block static var decl");
  } else if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(CGF.CurFuncDefn)) {
    llvm::StringRef Name = CGM.getMangledName(FD);
    ContextName = Name.str();
  } else
    assert(0 && "Unknown context for static var decl");
  
  return ContextName + Separator + D.getNameAsString();
}

llvm::GlobalVariable *
CodeGenFunction::CreateStaticVarDefn(const VarDefn &D,
                                     const char *Separator,
                                     llvm::GlobalValue::LinkageTypes Linkage) {
  Type Ty = D.getType();
//  assert(Ty->isConstantSizeType() && "VLAs can't be static");

  std::string Name = GetStaticDefnName(*this, D, Separator);

  llvm::Type *LTy = CGM.getTypes().ConvertTypeForMem(Ty);
  llvm::GlobalVariable *GV =
    new llvm::GlobalVariable(CGM.getModule(), LTy,
                             /*Ty.isConstant(getContext())*/false, Linkage,
                             CGM.EmitNullConstant(D.getType()), Name, 0,
                             /*D.isThreadSpecified()*/false,
                             CGM.getContext().getTargetAddressSpace(Ty));
  GV->setAlignment(getContext().getDefnAlign(&D).getQuantity());
  if (Linkage != llvm::GlobalValue::InternalLinkage)
    GV->setVisibility(CurFn->getVisibility());
  return GV;
}

/// AddInitializerToStaticVarDecl - Add the initializer for 'D' to the
/// global variable that has already been created for it.  If the initializer
/// has a different type than GV does, this may free GV and return a different
/// one.  Otherwise it just returns GV.
llvm::GlobalVariable *
CodeGenFunction::AddInitializerToStaticVarDefn(const VarDefn &D,
                                               llvm::GlobalVariable *GV) {
  llvm::Constant *Init = CGM.EmitConstantExpr(D.getInit(), D.getType(), this);

  // If constant emission failed, then this should be a C++ static
  // initializer.
  if (!Init) {
    if (!getContext().getLangOptions().OOP)
      CGM.ErrorUnsupported(D.getInit(), "constant l-value expression");
    else if (Builder.GetInsertBlock()) {
      // Since we have a static initializer, this global variable can't 
      // be constant.
      GV->setConstant(false);

 //     EmitCXXGuardedInit(D, GV);
    }
    return GV;
  }

  // The initializer may differ in type from the global. Rewrite
  // the global to match the initializer.  (We have to do this
  // because some types, like unions, can't be completely represented
  // in the LLVM type system.)
  if (GV->getType()->getElementType() != Init->getType()) {
    llvm::GlobalVariable *OldGV = GV;
    
    GV = new llvm::GlobalVariable(CGM.getModule(), Init->getType(),
                                  OldGV->isConstant(),
                                  OldGV->getLinkage(), Init, "",
                                  /*InsertBefore*/ OldGV,
                                  /*D.isThreadSpecified()*/false,
                           CGM.getContext().getTargetAddressSpace(D.getType()));
    GV->setVisibility(OldGV->getVisibility());
    
    // Steal the name of the old global
    GV->takeName(OldGV);
    
    // Replace all uses of the old global with the new global
    llvm::Constant *NewPtrForOldDecl =
    llvm::ConstantExpr::getBitCast(GV, OldGV->getType());
    OldGV->replaceAllUsesWith(NewPtrForOldDecl);
    
    // Erase the old global, since it is no longer used.
    OldGV->eraseFromParent();
  }
  
  GV->setInitializer(Init);
  return GV;
}

void CodeGenFunction::EmitStaticVarDefn(const VarDefn &D,
                                      llvm::GlobalValue::LinkageTypes Linkage) {
  llvm::Value *&DMEntry = LocalDefnMap[&D];
  assert(DMEntry == 0 && "Defn already exists in localdeclmap!");

  llvm::GlobalVariable *GV = CreateStaticVarDefn(D, ".", Linkage);

  // Store into LocalDeclMap before generating initializer to handle
  // circular references.
  DMEntry = GV;

  // Local static block variables must be treated as globals as they may be
  // referenced in their RHS initializer block-literal expresion.
  CGM.setStaticLocalDeclAddress(&D, GV);

  // If this value has an initializer, emit it.
  if (D.getInit())
    GV = AddInitializerToStaticVarDefn(D, GV);

  GV->setAlignment(getContext().getDefnAlign(&D).getQuantity());

  // We may have to cast the constant because of the initializer
  // mismatch above.
  //
  // FIXME: It is really dangerous to store this in the map; if anyone
  // RAUW's the GV uses of this constant will be invalid.
  llvm::Type *LTy = CGM.getTypes().ConvertTypeForMem(D.getType());
  llvm::Type *LPtrTy =
    LTy->getPointerTo(CGM.getContext().getTargetAddressSpace(D.getType()));
  DMEntry = llvm::ConstantExpr::getBitCast(GV, LPtrTy);

  // Emit global variable debug descriptor for static vars.
  CGDebugInfo *DI = getDebugInfo();
  if (DI) {
    DI->setLocation(D.getLocation());
    DI->EmitGlobalVariable(static_cast<llvm::GlobalVariable *>(GV), &D);
  }
}

namespace {
  struct CallArrayDtor : EHScopeStack::Cleanup {
    CallArrayDtor(const ClassDestructorDefn *Dtor,
                  const ArrayType *Ty,
                  llvm::Value *Loc)
      : Dtor(Dtor), Ty(Ty), Loc(Loc) {}

    const ClassDestructorDefn *Dtor;
    const ArrayType *Ty;
    llvm::Value *Loc;

    void Emit(CodeGenFunction &CGF, bool IsForEH) {
      Type BaseElementTy = Ty->getElementType();
      llvm::Type *BasePtr = CGF.ConvertType(BaseElementTy);
      BasePtr = llvm::PointerType::getUnqual(BasePtr);
      llvm::Value *BaseAddrPtr = CGF.Builder.CreateBitCast(Loc, BasePtr);
//      CGF.EmitClassAggrDestructorCall(Dtor, Ty, BaseAddrPtr);
    }
  };

  struct CallVarDtor : EHScopeStack::Cleanup {
    CallVarDtor(const ClassDestructorDefn *Dtor,
                llvm::Value *NRVOFlag,
                llvm::Value *Loc)
      : Dtor(Dtor), NRVOFlag(NRVOFlag), Loc(Loc) {}

    const ClassDestructorDefn *Dtor;
    llvm::Value *NRVOFlag;
    llvm::Value *Loc;

    void Emit(CodeGenFunction &CGF, bool IsForEH) {
      // Along the exceptions path we always execute the dtor.
      bool NRVO = !IsForEH && NRVOFlag;

      llvm::BasicBlock *SkipDtorBB = 0;
      if (NRVO) {
        // If we exited via NRVO, we skip the destructor call.
        llvm::BasicBlock *RunDtorBB = CGF.createBasicBlock("nrvo.unused");
        SkipDtorBB = CGF.createBasicBlock("nrvo.skipdtor");
        llvm::Value *DidNRVO = CGF.Builder.CreateLoad(NRVOFlag, "nrvo.val");
        CGF.Builder.CreateCondBr(DidNRVO, SkipDtorBB, RunDtorBB);
        CGF.EmitBlock(RunDtorBB);
      }
          
//      CGF.EmitClassDestructorCall(Dtor, Dtor_Complete,
//                                /*ForVirtualBase=*/false, Loc);

      if (NRVO) CGF.EmitBlock(SkipDtorBB);
    }
  };

  struct CallStackRestore : EHScopeStack::Cleanup {
    llvm::Value *Stack;
    CallStackRestore(llvm::Value *Stack) : Stack(Stack) {}
    void Emit(CodeGenFunction &CGF, bool IsForEH) {
      llvm::Value *V = CGF.Builder.CreateLoad(Stack, "tmp");
      llvm::Value *F = CGF.CGM.getIntrinsic(llvm::Intrinsic::stackrestore);
      CGF.Builder.CreateCall(F, V);
    }
  };

  struct ExtendGCLifetime : EHScopeStack::Cleanup {
    const VarDefn &Var;
    ExtendGCLifetime(const VarDefn *var) : Var(*var) {}

    void Emit(CodeGenFunction &CGF, bool forEH) {
      // Compute the address of the local variable, in case it's a
      // byref or something.
      DefnRefExpr DRE(const_cast<VarDefn*>(&Var), Var.getType(), VK_LValue,
                      SourceLocation());
//      llvm::Value *value = CGF.EmitLoadOfScalar(CGF.EmitDefnRefLValue(&DRE));
//      CGF.EmitExtendGCLifetime(value);
    }
  };

  struct CallCleanupFunction : EHScopeStack::Cleanup {
    llvm::Constant *CleanupFn;
    const CGFunctionInfo &FnInfo;
    const VarDefn &Var;
    
    CallCleanupFunction(llvm::Constant *CleanupFn, const CGFunctionInfo *Info,
                        const VarDefn *Var)
      : CleanupFn(CleanupFn), FnInfo(*Info), Var(*Var) {}

    void Emit(CodeGenFunction &CGF, bool IsForEH) {
      DefnRefExpr DRE(const_cast<VarDefn*>(&Var), Var.getType(), VK_LValue,
                      SourceLocation());
      // Compute the address of the local variable, in case it's a byref
      // or something.
      llvm::Value *Addr = CGF.EmitDefnRefLValue(&DRE).getAddress();

      // In some cases, the type of the function argument will be different from
      // the type of the pointer. An example of this is
      // void f(void* arg);
      // __attribute__((cleanup(f))) void *g;
      //
      // To fix this we insert a bitcast here.
      Type ArgTy = FnInfo.arg_begin()->type;
      llvm::Value *Arg =
        CGF.Builder.CreateBitCast(Addr, CGF.ConvertType(ArgTy));

      CallArgList Args;
//      Args.add(RValue::get(Arg),
//               CGF.getContext().getPointerType(Var.getType()));
      CGF.EmitCall(FnInfo, CleanupFn, ReturnValueSlot(), Args);
    }
  };
}

static bool isAccessedBy(const VarDefn &var, const Stmt *s) {
  if (const Expr *e = dyn_cast<Expr>(s)) {
    // Skip the most common kinds of expressions that make
    // hierarchy-walking expensive.
    s = e = e->IgnoreParenCasts();

    if (const DefnRefExpr *ref = dyn_cast<DefnRefExpr>(e))
      return (ref->getDefn() == &var);
  }

  for (Stmt::const_child_range children = s->children(); children; ++children)
    if (isAccessedBy(var, *children))
      return true;

  return false;
}

static bool isAccessedBy(const ValueDefn *defn, const Expr *e) {
  if (!defn) return false;
  if (!isa<VarDefn>(defn)) return false;
  const VarDefn *var = cast<VarDefn>(defn);
  return isAccessedBy(*var, e);
}

static void drillIntoBlockVariable(CodeGenFunction &CGF,
                                   LValue &lvalue,
                                   const VarDefn *var) {
  //lvalue.setAddress(CGF.BuildBlockByrefAddress(lvalue.getAddress(), var));
}

void CodeGenFunction::EmitScalarInit(const Expr *init,
                                     const ValueDefn *D,
                                     LValue lvalue,
                                     bool capturedByInit) {
  llvm::Value *value = EmitScalarExpr(init);
  if (capturedByInit)
	  drillIntoBlockVariable(*this, lvalue, cast<VarDefn>(D));

  EmitStoreThroughLValue(RValue::get(value), lvalue);
  return;
}

/// EmitScalarInit - Initialize the given lvalue with the given object.
void CodeGenFunction::EmitScalarInit(llvm::Value *init, LValue lvalue) {
  return EmitStoreThroughLValue(RValue::get(init), lvalue);
}

/// canEmitInitWithFewStoresAfterMemset - Decide whether we can emit the
/// non-zero parts of the specified initializer with equal or fewer than
/// NumStores scalar stores.
static bool canEmitInitWithFewStoresAfterMemset(llvm::Constant *Init,
                                                unsigned &NumStores) {
  // Zero and Undef never requires any extra stores.
  if (isa<llvm::ConstantAggregateZero>(Init) ||
      isa<llvm::ConstantPointerNull>(Init) ||
      isa<llvm::UndefValue>(Init))
    return true;
  if (isa<llvm::ConstantInt>(Init) || isa<llvm::ConstantFP>(Init) ||
      isa<llvm::ConstantVector>(Init) || isa<llvm::BlockAddress>(Init) ||
      isa<llvm::ConstantExpr>(Init))
    return Init->isNullValue() || NumStores--;

  // See if we can emit each element.
  if (isa<llvm::ConstantArray>(Init) || isa<llvm::ConstantStruct>(Init)) {
    for (unsigned i = 0, e = Init->getNumOperands(); i != e; ++i) {
      llvm::Constant *Elt = cast<llvm::Constant>(Init->getOperand(i));
      if (!canEmitInitWithFewStoresAfterMemset(Elt, NumStores))
        return false;
    }
    return true;
  }
  
  // Anything else is hard and scary.
  return false;
}

/// emitStoresForInitAfterMemset - For inits that
/// canEmitInitWithFewStoresAfterMemset returned true for, emit the scalar
/// stores that would be required.
static void emitStoresForInitAfterMemset(llvm::Constant *Init, llvm::Value *Loc,
                                         bool isVolatile, CGBuilderTy &Builder) {
  // Zero doesn't require any stores.
  if (isa<llvm::ConstantAggregateZero>(Init) ||
      isa<llvm::ConstantPointerNull>(Init) ||
      isa<llvm::UndefValue>(Init))
    return;
  
  if (isa<llvm::ConstantInt>(Init) || isa<llvm::ConstantFP>(Init) ||
      isa<llvm::ConstantVector>(Init) || isa<llvm::BlockAddress>(Init) ||
      isa<llvm::ConstantExpr>(Init)) {
    if (!Init->isNullValue())
      Builder.CreateStore(Init, Loc, isVolatile);
    return;
  }
  
  assert((isa<llvm::ConstantStruct>(Init) || isa<llvm::ConstantArray>(Init)) &&
         "Unknown value type!");
  
  for (unsigned i = 0, e = Init->getNumOperands(); i != e; ++i) {
    llvm::Constant *Elt = cast<llvm::Constant>(Init->getOperand(i));
    if (Elt->isNullValue()) continue;
    
    // Otherwise, get a pointer to the element and emit it.
    emitStoresForInitAfterMemset(Elt, Builder.CreateConstGEP2_32(Loc, 0, i),
                                 isVolatile, Builder);
  }
}


/// shouldUseMemSetPlusStoresToInitialize - Decide whether we should use memset
/// plus some stores to initialize a local variable instead of using a memcpy
/// from a constant global.  It is beneficial to use memset if the global is all
/// zeros, or mostly zeros and large.
static bool shouldUseMemSetPlusStoresToInitialize(llvm::Constant *Init,
                                                  uint64_t GlobalSize) {
  // If a global is all zeros, always use a memset.
  if (isa<llvm::ConstantAggregateZero>(Init)) return true;


  // If a non-zero global is <= 32 bytes, always use a memcpy.  If it is large,
  // do it if it will require 6 or fewer scalar stores.
  // TODO: Should budget depends on the size?  Avoiding a large global warrants
  // plopping in more stores.
  unsigned StoreBudget = 6;
  uint64_t SizeLimit = 32;
  
  return GlobalSize > SizeLimit && 
         canEmitInitWithFewStoresAfterMemset(Init, StoreBudget);
}


/// EmitAutoVarDecl - Emit code and set up an entry in LocalDeclMap for a
/// variable declaration with auto, register, or no storage class specifier.
/// These turn into simple stack objects, or GlobalValues depending on target.
void CodeGenFunction::EmitAutoVarDefn(const VarDefn &D) {
  AutoVarEmission emission = EmitAutoVarAlloca(D);
  EmitAutoVarInit(emission);
  EmitAutoVarCleanups(emission);
}

/// EmitAutoVarAlloca - Emit the alloca and debug information for a
/// local variable.  Does not emit initalization or destruction.
CodeGenFunction::AutoVarEmission
CodeGenFunction::EmitAutoVarAlloca(const VarDefn &D) {
  Type Ty = D.getType();

  AutoVarEmission emission(D);

  CharUnits alignment = getContext().getDefnAlign(&D);
  emission.Alignment = alignment;

  llvm::Value *DeclPtr;
//  if (Ty->isConstantSizeType()) {
//    if (!Target.useGlobalsForAutomaticVariables()) {
//      bool NRVO = getContext().getLangOptions().ElideConstructors &&
//                  D.isNRVOVariable();

      // If this value is a POD array or struct with a statically
      // determinable constant initializer, there are optimizations we
      // can do.
      // TODO: we can potentially constant-evaluate non-POD structs and
      // arrays as long as the initialization is trivial (e.g. if they
      // have a non-trivial destructor, but not a non-trivial constructor).
//      if (D.getInit() &&
//          (Ty->isArrayType() || Ty->isRecordType()) &&
//          (Ty.isPODType(getContext()) ||
//           getContext().getBaseElementType(Ty)->isObjCObjectPointerType()) &&
//          D.getInit()->isConstantInitializer(getContext(), false)) {
//
//        // If the variable's a const type, and it's neither an NRVO
//        // candidate nor a __block variable, emit it as a global instead.
//        if (CGM.getCodeGenOpts().MergeAllConstants && Ty.isConstQualified() &&
//            !NRVO && !isByRef) {
//          EmitStaticVarDecl(D, llvm::GlobalValue::InternalLinkage);
//
//          emission.Address = 0; // signal this condition to later callbacks
//          assert(emission.wasEmittedAsGlobal());
//          return emission;
//        }
//
//        // Otherwise, tell the initialization code that we're in this case.
//        emission.IsConstantAggregate = true;
//      }
      
      // A normal fixed sized variable becomes an alloca in the entry block,
      // unless it's an NRVO variable.
//      llvm::Type *LTy = ConvertTypeForMem(Ty);
//
//      if (NRVO) {
        // The named return value optimization: allocate this variable in the
        // return slot, so that we can elide the copy when returning this
        // variable (C++0x [class.copy]p34).
//        DeclPtr = ReturnValue;
//
//        if (const HeteroContainerType *RecordTy = Ty->getAs<HeteroContainerType>()) {
//          if (!cast<UserClassDefn>(RecordTy->getDefn())->hasTrivialDestructor()) {
//            // Create a flag that is used to indicate when the NRVO was applied
//            // to this variable. Set it to zero to indicate that NRVO was not
//            // applied.
//            llvm::Value *Zero = Builder.getFalse();
//            llvm::Value *NRVOFlag = CreateTempAlloca(Zero->getType(), "nrvo");
//            EnsureInsertPoint();
//            Builder.CreateStore(Zero, NRVOFlag);
//
//            // Record the NRVO flag for this variable.
//            NRVOFlags[&D] = NRVOFlag;
//            emission.NRVOFlag = NRVOFlag;
//          }
//        }
//      } else {
//        if (isByRef)
//          LTy = BuildByRefType(&D);
//
//        llvm::AllocaInst *Alloc = CreateTempAlloca(LTy);
//        Alloc->setName(D.getNameAsString());
//
//        CharUnits allocaAlignment = alignment;
//        if (isByRef)
//          allocaAlignment = std::max(allocaAlignment,
//              getContext().toCharUnitsFromBits(Target.getPointerAlign(0)));
//        Alloc->setAlignment(allocaAlignment.getQuantity());
//        DeclPtr = Alloc;
//      }
//    } else {
//      // Targets that don't support recursion emit locals as globals.
//      const char *Class =
//        D.getStorageClass() == SC_Register ? ".reg." : ".auto.";
//      DeclPtr = CreateStaticVarDecl(D, Class,
//                                    llvm::GlobalValue::InternalLinkage);
//    }
//  } else {
//    EnsureInsertPoint();
//
//    if (!DidCallStackSave) {
//      // Save the stack.
//      llvm::Value *Stack = CreateTempAlloca(Int8PtrTy, "saved_stack");
//
//      llvm::Value *F = CGM.getIntrinsic(llvm::Intrinsic::stacksave);
//      llvm::Value *V = Builder.CreateCall(F);
//
//      Builder.CreateStore(V, Stack);
//
//      DidCallStackSave = true;
//
//      // Push a cleanup block and restore the stack there.
//      // FIXME: in general circumstances, this should be an EH cleanup.
//      EHStack.pushCleanup<CallStackRestore>(NormalCleanup, Stack);
//    }

//    llvm::Value *elementCount;
//    Type elementType;
//    llvm::tie(elementCount, elementType) = getVLASize(Ty);
//
//    llvm::Type *llvmTy = ConvertTypeForMem(elementType);
//
//    // Allocate memory for the array.
//    llvm::AllocaInst *vla = Builder.CreateAlloca(llvmTy, elementCount, "vla");
//    vla->setAlignment(alignment.getQuantity());
//
//    DeclPtr = vla;
//  }

  llvm::Value *&DMEntry = LocalDefnMap[&D];
  assert(DMEntry == 0 && "Defn already exists in localdeclmap!");
  DMEntry = DeclPtr;
  emission.Address = DeclPtr;

  // Emit debug info for local var declaration.
  if (HaveInsertPoint())
    if (CGDebugInfo *DI = getDebugInfo()) {
      DI->setLocation(D.getLocation());
      if (Target.useGlobalsForAutomaticVariables()) {
        DI->EmitGlobalVariable(static_cast<llvm::GlobalVariable *>(DeclPtr), &D);
      } else
        DI->EmitDeclareOfAutoVariable(&D, DeclPtr, Builder);
    }

  return emission;
}

/// Determines whether the given __block variable is potentially
/// captured by the given expression.
static bool isCapturedBy(const VarDefn &var, const Expr *e) {
  // Skip the most common kinds of expressions that make
  // hierarchy-walking expensive.
  e = e->IgnoreParenCasts();

  if (const BlockCmd *be = dyn_cast<BlockCmd>(e)) {
    const ScriptDefn *block = be->getContainerDefn();
    for (ScriptDefn::capture_const_iterator i = block->capture_begin(),
           e = block->capture_end(); i != e; ++i) {
      if (i->getVariable() == &var)
        return true;
    }

    // No need to walk into the subexpressions.
    return false;
  }

  for (Stmt::const_child_range children = e->children(); children; ++children)
    if (isCapturedBy(var, cast<Expr>(*children)))
      return true;

  return false;
}

void CodeGenFunction::EmitAutoVarInit(const AutoVarEmission &emission) {
  assert(emission.Variable && "emission was not valid!");

  // If this was emitted as a global constant, we're done.
  if (emission.wasEmittedAsGlobal()) return;

  const VarDefn &D = *emission.Variable;
  Type type = D.getType();

  // If this local has an initializer, emit it now.
  const Expr *Init = D.getInit();

  // If we are at an unreachable point, we don't need to emit the initializer
  // unless it contains a label.
  if (!HaveInsertPoint()) {
    if (!Init) return;
    EnsureInsertPoint();
  }

  // Initialize the structure of a __block variable.
  if (emission.IsByRef)
    emitByrefStructureInit(emission);

  if (!Init) return;

  CharUnits alignment = emission.Alignment;

  // Check whether this is a byref variable that's potentially
  // captured and moved by its own initializer.  If so, we'll need to
  // emit the initializer first, then copy into the variable.
  bool capturedByInit = emission.IsByRef && isCapturedBy(D, Init);

  llvm::Value *Loc =
    capturedByInit ? emission.Address : emission.getObjectAddress(*this);

  if (!emission.IsConstantAggregate) {
    LValue lv = MakeAddrLValue(Loc, type, alignment.getQuantity());
    lv.setNonGC(true);
    return EmitExprAsInit(Init, &D, lv, capturedByInit);
  }

  // If this is a simple aggregate initialization, we can optimize it
  // in various ways.
  assert(!capturedByInit && "constant init contains a capturing block?");

  bool isVolatile = /*type.isVolatileQualified()*/false;

  llvm::Constant *constant = CGM.EmitConstantExpr(D.getInit(), type, this);
  assert(constant != 0 && "Wasn't a simple constant init?");

  llvm::Value *SizeVal =
    llvm::ConstantInt::get(IntPtrTy, 
                           getContext().getTypeSizeInChars(type).getQuantity());

  llvm::Type *BP = Int8PtrTy;
  if (Loc->getType() != BP)
    Loc = Builder.CreateBitCast(Loc, BP, "tmp");

  // If the initializer is all or mostly zeros, codegen with memset then do
  // a few stores afterward.
  if (shouldUseMemSetPlusStoresToInitialize(constant, 
                CGM.getTargetData().getTypeAllocSize(constant->getType()))) {
    Builder.CreateMemSet(Loc, llvm::ConstantInt::get(Int8Ty, 0), SizeVal,
                         alignment.getQuantity(), isVolatile);
    if (!constant->isNullValue()) {
      Loc = Builder.CreateBitCast(Loc, constant->getType()->getPointerTo());
      emitStoresForInitAfterMemset(constant, Loc, isVolatile, Builder);
    }
  } else {
    // Otherwise, create a temporary global with the initializer then 
    // memcpy from the global to the alloca.
    std::string Name = GetStaticDefnName(*this, D, ".");
    llvm::GlobalVariable *GV =
      new llvm::GlobalVariable(CGM.getModule(), constant->getType(), true,
                               llvm::GlobalValue::InternalLinkage,
                               constant, Name, 0, false, 0);
    GV->setAlignment(alignment.getQuantity());
    GV->setUnnamedAddr(true);
        
    llvm::Value *SrcPtr = GV;
    if (SrcPtr->getType() != BP)
      SrcPtr = Builder.CreateBitCast(SrcPtr, BP, "tmp");

    Builder.CreateMemCpy(Loc, SrcPtr, SizeVal, alignment.getQuantity(),
                         isVolatile);
  }
}

/// Emit an expression as an initializer for a variable at the given
/// location.  The expression is not necessarily the normal
/// initializer for the variable, and the address is not necessarily
/// its normal location.
///
/// \param init the initializing expression
/// \param var the variable to act as if we're initializing
/// \param loc the address to initialize; its type is a pointer
///   to the LLVM mapping of the variable's type
/// \param alignment the alignment of the address
/// \param capturedByInit true if the variable is a __block variable
///   whose address is potentially changed by the initializer
void CodeGenFunction::EmitExprAsInit(const Expr *init,
                                     const ValueDefn *D,
                                     LValue lvalue,
                                     bool capturedByInit) {
  Type type = D->getType();

  if (type->isReferenceType()) {
//    RValue rvalue = EmitReferenceBindingToExpr(init, D);
//    if (capturedByInit)
//      drillIntoBlockVariable(*this, lvalue, cast<VarDefn>(D));
//    EmitStoreThroughLValue(rvalue, lvalue);
  } else if (!hasAggregateLLVMType(type)) {
    EmitScalarInit(init, D, lvalue, capturedByInit);
  } else if (type.hasComplexAttr()) {
    ComplexPairTy complex = EmitComplexExpr(init);
    if (capturedByInit)
      drillIntoBlockVariable(*this, lvalue, cast<VarDefn>(D));
    StoreComplexToAddr(complex, lvalue.getAddress(), /*lvalue.isVolatile()*/false);
  } else {
    // TODO: how can we delay here if D is captured by its initializer?
    EmitAggExpr(init, AggValueSlot::forLValue(lvalue, true, false));
  }
}

void CodeGenFunction::EmitAutoVarCleanups(const AutoVarEmission &emission) {
  assert(emission.Variable && "emission was not valid!");

  // If this was emitted as a global constant, we're done.
  if (emission.wasEmittedAsGlobal()) return;

  const VarDefn &D = *emission.Variable;

  // Handle C++ or ARC destruction of variables.
  if (getLangOptions().OOP) {
    Type type = D.getType();
    //Type baseType = getContext().getBaseElementType(type);
  }

  // If this is a block variable, call _Block_object_destroy
  // (on the unforwarded address).
  if (emission.IsByRef)
    enterByrefCleanup(emission);
}

namespace {
  /// A cleanup to perform a release of an object at the end of a
  /// function.  This is used to balance out the incoming +1 of a
  /// ns_consumed argument when we can't reasonably do that just by
  /// not doing the initial retain for a __block argument.
  struct ConsumeARCParameter : EHScopeStack::Cleanup {
    ConsumeARCParameter(llvm::Value *param) : Param(param) {}

    llvm::Value *Param;

    void Emit(CodeGenFunction &CGF, bool IsForEH) {
      //CGF.EmitARCRelease(Param, /*precise*/ false);
    }
  };
}

/// Emit an alloca (or GlobalValue depending on target)
/// for the specified parameter and set up LocalDeclMap.
void CodeGenFunction::EmitParmDefn(const VarDefn &D, llvm::Value *Arg,
                                   unsigned ArgNo) {
  // FIXME: Why isn't ImplicitParamDecl a ParmVarDecl?
  assert((isa<ParamVarDefn>(D) ) &&
         "Invalid argument to EmitParmDecl");

  Arg->setName(D.getName());

  Type Ty = D.getType();

  llvm::Value *DeclPtr;
  // If this is an aggregate or variable sized value, reuse the input pointer.
  if (!Ty->isArrayType() ||
      CodeGenFunction::hasAggregateLLVMType(Ty)) {
    DeclPtr = Arg;
  } else {
    // Otherwise, create a temporary to hold the value.
    DeclPtr = CreateMemTemp(Ty, D.getName() + ".addr");

    bool doStore = true;

    // Store the initial value into the alloca.
    if (doStore) {
      LValue lv = MakeAddrLValue(DeclPtr, Ty,
                                 getContext().getDefnAlign(&D).getQuantity());
//      EmitStoreOfScalar(Arg, lv);
    }
  }

  llvm::Value *&DMEntry = LocalDefnMap[&D];
  assert(DMEntry == 0 && "Defn already exists in localdeclmap!");
  DMEntry = DeclPtr;

  // Emit debug info for param declaration.
  if (CGDebugInfo *DI = getDebugInfo())
    DI->EmitDeclareOfArgVariable(&D, DeclPtr, ArgNo, Builder);
}
