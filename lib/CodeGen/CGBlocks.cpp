//===--- CGBlocks.cpp - Emit LLVM Code for declarations -------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit blocks.
//
//===----------------------------------------------------------------------===//

#include "CGDebugInfo.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGBlocks.h"
#include "llvm/Module.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Target/TargetData.h"
#include <algorithm>

using namespace mlang;
using namespace CodeGen;

struct CallMemsetLocalBlockObject : EHScopeStack::Cleanup {
  llvm::AllocaInst *BlockAddr;
  CharUnits BlockSize;
  
  CallMemsetLocalBlockObject(llvm::AllocaInst *blockAddr, 
                             CharUnits blocSize) 
    : BlockAddr(blockAddr), BlockSize(blocSize) {}
  
  void Emit(CodeGenFunction &CGF, bool isForEH) {
    CGF.Builder.CreateMemSet(BlockAddr, 
                             llvm::ConstantInt::get(CGF.Int8Ty, 0xCD), 
                             BlockSize.getQuantity(), 
                             BlockAddr->getAlignment());
  }
};

CGBlockInfo::CGBlockInfo(const BlockCmd *blockCmd, const char *N)
  : Name(N), CXXThisIndex(0), CanBeGlobal(false), NeedsCopyDispose(false),
    HasCXXObject(false), UsesStret(false), StructureType(0), Block(blockCmd) {
    
  // Skip asm prefix, if any.
  if (Name && Name[0] == '\01')
    ++Name;
}

// Anchor the vtable to this translation unit.
CodeGenModule::ByrefHelpers::~ByrefHelpers() {}

/// Build the given block as a global block.
static llvm::Constant *buildGlobalBlock(CodeGenModule &CGM,
                                        const CGBlockInfo &blockInfo,
                                        llvm::Constant *blockFn);

/// Build the helper function to copy a block.
static llvm::Constant *buildCopyHelper(CodeGenModule &CGM,
                                       const CGBlockInfo &blockInfo) {
  return CodeGenFunction(CGM).GenerateCopyHelperFunction(blockInfo);
}

/// Build the helper function to dipose of a block.
static llvm::Constant *buildDisposeHelper(CodeGenModule &CGM,
                                          const CGBlockInfo &blockInfo) {
  return CodeGenFunction(CGM).GenerateDestroyHelperFunction(blockInfo);
}

/// Build the block descriptor constant for a block.
static llvm::Constant *buildBlockDescriptor(CodeGenModule &CGM,
                                            const CGBlockInfo &blockInfo) {
  ASTContext &C = CGM.getContext();

  llvm::Type *ulong = CGM.getTypes().ConvertType(C.UInt64Ty);
  llvm::Type *i8p = CGM.getTypes().ConvertType(C.Int32Ty);

  llvm::SmallVector<llvm::Constant*, 6> elements;

  // reserved
  elements.push_back(llvm::ConstantInt::get(ulong, 0));

  // Size
  // FIXME: What is the right way to say this doesn't fit?  We should give
  // a user diagnostic in that case.  Better fix would be to change the
  // API to size_t.
  elements.push_back(llvm::ConstantInt::get(ulong,
                                            blockInfo.BlockSize.getQuantity()));

  // Optional copy/dispose helpers.
  if (blockInfo.NeedsCopyDispose) {
    // copy_func_helper_decl
    elements.push_back(buildCopyHelper(CGM, blockInfo));

    // destroy_func_decl
    elements.push_back(buildDisposeHelper(CGM, blockInfo));
  }

  // GC layout.
  elements.push_back(llvm::Constant::getNullValue(i8p));

  llvm::Constant *init = llvm::ConstantStruct::getAnon(elements);

  llvm::GlobalVariable *global =
    new llvm::GlobalVariable(CGM.getModule(), init->getType(), true,
                             llvm::GlobalValue::InternalLinkage,
                             init, "__block_descriptor_tmp");

  return llvm::ConstantExpr::getBitCast(global, CGM.getBlockDescriptorType());
}

/*
  Purely notional variadic template describing the layout of a block.

  template <class _ResultType, class... _ParamTypes, class... _CaptureTypes>
  struct Block_literal {
    /// Initialized to one of:
    ///   extern void *_NSConcreteStackBlock[];
    ///   extern void *_NSConcreteGlobalBlock[];
    ///
    /// In theory, we could start one off malloc'ed by setting
    /// BLOCK_NEEDS_FREE, giving it a refcount of 1, and using
    /// this isa:
    ///   extern void *_NSConcreteMallocBlock[];
    struct objc_class *isa;

    /// These are the flags (with corresponding bit number) that the
    /// compiler is actually supposed to know about.
    ///  25. BLOCK_HAS_COPY_DISPOSE - indicates that the block
    ///   descriptor provides copy and dispose helper functions
    ///  26. BLOCK_HAS_CXX_OBJ - indicates that there's a captured
    ///   object with a nontrivial destructor or copy constructor
    ///  28. BLOCK_IS_GLOBAL - indicates that the block is allocated
    ///   as global memory
    ///  29. BLOCK_USE_STRET - indicates that the block function
    ///   uses stret, which objc_msgSend needs to know about
    ///  30. BLOCK_HAS_SIGNATURE - indicates that the block has an
    ///   @encoded signature string
    /// And we're not supposed to manipulate these:
    ///  24. BLOCK_NEEDS_FREE - indicates that the block has been moved
    ///   to malloc'ed memory
    ///  27. BLOCK_IS_GC - indicates that the block has been moved to
    ///   to GC-allocated memory
    /// Additionally, the bottom 16 bits are a reference count which
    /// should be zero on the stack.
    int flags;

    /// Reserved;  should be zero-initialized.
    int reserved;

    /// Function pointer generated from block literal.
    _ResultType (*invoke)(Block_literal *, _ParamTypes...);

    /// Block description metadata generated from block literal.
    struct Block_descriptor *block_descriptor;

    /// Captured values follow.
    _CapturesTypes captures...;
  };
 */

/// The number of fields in a block header.
const unsigned BlockHeaderSize = 5;

namespace {
  /// A chunk of data that we actually have to capture in the block.
  struct BlockLayoutChunk {
    CharUnits Alignment;
    CharUnits Size;
    const ScriptDefn::Capture *Capture; // null for 'this'
    llvm::Type *Type;

    BlockLayoutChunk(CharUnits align, CharUnits size,
                     const ScriptDefn::Capture *capture,
                     llvm::Type *type)
      : Alignment(align), Size(size), Capture(capture), Type(type) {}

    /// Tell the block info that this chunk has the given field index.
    void setIndex(CGBlockInfo &info, unsigned index) {
      if (!Capture)
        info.CXXThisIndex = index;
      else
        info.Captures[Capture->getVariable()]
          = CGBlockInfo::Capture::makeIndex(index);
    }
  };

  /// Order by descending alignment.
  bool operator<(const BlockLayoutChunk &left, const BlockLayoutChunk &right) {
    return left.Alignment > right.Alignment;
  }
}

/// Determines if the given type is safe for constant capture in C++.
static bool isSafeForCXXConstantCapture(Type type) {
//  const RecordType *recordType =
//    type->getBaseElementTypeUnsafe()->getAs<RecordType>();
//
//  // Only records can be unsafe.
//  if (!recordType) return true;
//
//  const CXXRecordDecl *record = cast<CXXRecordDecl>(recordType->getDecl());
//
//  // Maintain semantics for classes with non-trivial dtors or copy ctors.
//  if (!record->hasTrivialDestructor()) return false;
//  if (!record->hasTrivialCopyConstructor()) return false;
//
//  // Otherwise, we just have to make sure there aren't any mutable
//  // fields that might have changed since initialization.
//  return !record->hasMutableFields();
  return false;
}

/// It is illegal to modify a const object after initialization.
/// Therefore, if a const object has a constant initializer, we don't
/// actually need to keep storage for it in the block; we'll just
/// rematerialize it at the start of the block function.  This is
/// acceptable because we make no promises about address stability of
/// captured variables.
static llvm::Constant *tryCaptureAsConstant(CodeGenModule &CGM,
                                            const VarDefn *var) {
  Type type = var->getType();

  // We can only do this if the variable is const.
  //if (!type.isConstQualified()) return 0;

  // If the variable doesn't have any initializer (shouldn't this be
  // invalid?), it's not clear what we should do.  Maybe capture as
  // zero?
//  const Expr *init = var->getInit();
//  if (!init) return 0;
//
//  return CGM.EmitConstantExpr(init, var->getType());
  return 0;
}

/// Get the low bit of a nonzero character count.  This is the
/// alignment of the nth byte if the 0th byte is universally aligned.
static CharUnits getLowBit(CharUnits v) {
  return CharUnits::fromQuantity(v.getQuantity() & (~v.getQuantity() + 1));
}

static void initializeForBlockHeader(CodeGenModule &CGM, CGBlockInfo &info,
                    llvm::SmallVectorImpl<llvm::Type*> &elementTypes) {
  ASTContext &C = CGM.getContext();

  // The header is basically a 'struct { void *; int; int; void *; void *; }'.
  CharUnits ptrSize, ptrAlign, intSize, intAlign;
  llvm::tie(ptrSize, ptrAlign) = C.getTypeInfoInChars(C.UInt32Ty);
  llvm::tie(intSize, intAlign) = C.getTypeInfoInChars(C.Int32Ty);

  // Are there crazy embedded platforms where this isn't true?
  assert(intSize <= ptrSize && "layout assumptions horribly violated");

  CharUnits headerSize = ptrSize;
  if (2 * intSize < ptrAlign) headerSize += ptrSize;
  else headerSize += 2 * intSize;
  headerSize += 2 * ptrSize;

  info.BlockAlign = ptrAlign;
  info.BlockSize = headerSize;

  assert(elementTypes.empty());
  llvm::Type *i8p = CGM.getTypes().ConvertType(C.UInt32Ty);
  llvm::Type *intTy = CGM.getTypes().ConvertType(C.Int32Ty);
  elementTypes.push_back(i8p);
  elementTypes.push_back(intTy);
  elementTypes.push_back(intTy);
  elementTypes.push_back(i8p);
  elementTypes.push_back(CGM.getBlockDescriptorType());

  assert(elementTypes.size() == BlockHeaderSize);
}

/// Compute the layout of the given block.  Attempts to lay the block
/// out with minimal space requirements.
static void computeBlockInfo(CodeGenModule &CGM, CGBlockInfo &info) {
  ASTContext &C = CGM.getContext();
  const ScriptDefn *block = info.getBlockDecl();

  llvm::SmallVector<llvm::Type*, 8> elementTypes;
  initializeForBlockHeader(CGM, info, elementTypes);

  if (!block->hasCaptures()) {
    info.StructureType =
      llvm::StructType::get(CGM.getLLVMContext(), elementTypes, true);
    info.CanBeGlobal = true;
    return;
  }

  // Collect the layout chunks.
  llvm::SmallVector<BlockLayoutChunk, 16> layout;
  layout.reserve(block->capture_end() - block->capture_begin());

  CharUnits maxFieldAlign;

  // Next, all the block captures.
  for (ScriptDefn::capture_const_iterator ci = block->capture_begin(),
         ce = block->capture_end(); ci != ce; ++ci) {
    const VarDefn *variable = ci->getVariable();

    if (ci->isByRef()) {
      // We have to copy/dispose of the __block reference.
      info.NeedsCopyDispose = true;

      // Just use void* instead of a pointer to the byref type.
      Type byRefPtrTy = C.UInt32Ty;

      llvm::Type *llvmType = CGM.getTypes().ConvertType(byRefPtrTy);
      std::pair<CharUnits,CharUnits> tinfo
        = CGM.getContext().getTypeInfoInChars(byRefPtrTy);
      maxFieldAlign = std::max(maxFieldAlign, tinfo.second);

      layout.push_back(BlockLayoutChunk(tinfo.second, tinfo.first,
                                        &*ci, llvmType));
      continue;
    }

    // Otherwise, build a layout chunk with the size and alignment of
    // the declaration.
    if (llvm::Constant *constant = tryCaptureAsConstant(CGM, variable)) {
      info.Captures[variable] = CGBlockInfo::Capture::makeConstant(constant);
      continue;
    }

    // If we have a lifetime qualifier, honor it for capture purposes.
    // That includes *not* copying it if it's __unsafe_unretained.
    if (ci->hasCopyExpr()) {
      info.NeedsCopyDispose = true;
      info.HasCXXObject = true;

    // And so do types with destructors.
    }
    CharUnits size = C.getTypeSizeInChars(variable->getType());
    CharUnits align = C.getDefnAlign(variable);
    maxFieldAlign = std::max(maxFieldAlign, align);

    llvm::Type *llvmType =
      CGM.getTypes().ConvertTypeForMem(variable->getType());

    layout.push_back(BlockLayoutChunk(align, size, &*ci, llvmType));
  }

  // If that was everything, we're done here.
  if (layout.empty()) {
    info.StructureType =
      llvm::StructType::get(CGM.getLLVMContext(), elementTypes, true);
    info.CanBeGlobal = true;
    return;
  }

  // Sort the layout by alignment.  We have to use a stable sort here
  // to get reproducible results.  There should probably be an
  // llvm::array_pod_stable_sort.
  std::stable_sort(layout.begin(), layout.end());

  CharUnits &blockSize = info.BlockSize;
  info.BlockAlign = std::max(maxFieldAlign, info.BlockAlign);

  // Assuming that the first byte in the header is maximally aligned,
  // get the alignment of the first byte following the header.
  CharUnits endAlign = getLowBit(blockSize);

  // If the end of the header isn't satisfactorily aligned for the
  // maximum thing, look for things that are okay with the header-end
  // alignment, and keep appending them until we get something that's
  // aligned right.  This algorithm is only guaranteed optimal if
  // that condition is satisfied at some point; otherwise we can get
  // things like:
  //   header                 // next byte has alignment 4
  //   something_with_size_5; // next byte has alignment 1
  //   something_with_alignment_8;
  // which has 7 bytes of padding, as opposed to the naive solution
  // which might have less (?).
  if (endAlign < maxFieldAlign) {
    llvm::SmallVectorImpl<BlockLayoutChunk>::iterator
      li = layout.begin() + 1, le = layout.end();

    // Look for something that the header end is already
    // satisfactorily aligned for.
    for (; li != le && endAlign < li->Alignment; ++li)
      ;

    // If we found something that's naturally aligned for the end of
    // the header, keep adding things...
    if (li != le) {
      llvm::SmallVectorImpl<BlockLayoutChunk>::iterator first = li;
      for (; li != le; ++li) {
        assert(endAlign >= li->Alignment);

        li->setIndex(info, elementTypes.size());
        elementTypes.push_back(li->Type);
        blockSize += li->Size;
        endAlign = getLowBit(blockSize);

        // ...until we get to the alignment of the maximum field.
        if (endAlign >= maxFieldAlign)
          break;
      }

      // Don't re-append everything we just appended.
      layout.erase(first, li);
    }
  }

  // At this point, we just have to add padding if the end align still
  // isn't aligned right.
  if (endAlign < maxFieldAlign) {
    CharUnits padding = maxFieldAlign - endAlign;

    elementTypes.push_back(llvm::ArrayType::get(CGM.Int8Ty,
                                                padding.getQuantity()));
    blockSize += padding;

    endAlign = getLowBit(blockSize);
    assert(endAlign >= maxFieldAlign);
  }

  // Slam everything else on now.  This works because they have
  // strictly decreasing alignment and we expect that size is always a
  // multiple of alignment.
  for (llvm::SmallVectorImpl<BlockLayoutChunk>::iterator
         li = layout.begin(), le = layout.end(); li != le; ++li) {
    assert(endAlign >= li->Alignment);
    li->setIndex(info, elementTypes.size());
    elementTypes.push_back(li->Type);
    blockSize += li->Size;
    endAlign = getLowBit(blockSize);
  }

  info.StructureType =
    llvm::StructType::get(CGM.getLLVMContext(), elementTypes, true);
}

llvm::Type *CodeGenModule::getBlockDescriptorType() {
  if (BlockDescriptorType)
    return BlockDescriptorType;

  llvm::Type *UnsignedLongTy =
    getTypes().ConvertType(getContext().UInt64Ty);

  // struct __block_descriptor {
  //   unsigned long reserved;
  //   unsigned long block_size;
  //
  //   // later, the following will be added
  //
  //   struct {
  //     void (*copyHelper)();
  //     void (*copyHelper)();
  //   } helpers;                // !!! optional
  //
  //   const char *signature;   // the block signature
  //   const char *layout;      // reserved
  // };
  BlockDescriptorType =
    llvm::StructType::create("struct.__block_descriptor",
    		                  UnsignedLongTy, UnsignedLongTy, NULL);

  // Now form a pointer to that.
  BlockDescriptorType = llvm::PointerType::getUnqual(BlockDescriptorType);
  return BlockDescriptorType;
}

llvm::Type *CodeGenModule::getGenericBlockLiteralType() {
  if (GenericBlockLiteralType)
    return GenericBlockLiteralType;

  llvm::Type *BlockDescPtrTy = getBlockDescriptorType();

  // struct __block_literal_generic {
  //   void *__isa;
  //   int __flags;
  //   int __reserved;
  //   void (*__invoke)(void *);
  //   struct __block_descriptor *__descriptor;
  // };
  GenericBlockLiteralType = llvm::StructType::create(
		  "struct.__block_literal_generic", IntTy,
                                                  IntTy,
                                                  IntTy,
                                                  IntTy,
                                                  BlockDescPtrTy,
                                                  NULL);

  return GenericBlockLiteralType;
}


RValue CodeGenFunction::EmitBlockCallExpr(const FunctionCall* E,
                                          ReturnValueSlot ReturnValue) {
  // And call the block.
  // return EmitCall(FnInfo, Func, ReturnValue, Args);
	return RValue();
}

llvm::Value *CodeGenFunction::GetAddrOfBlockDefn(const VarDefn *variable,
                                                 bool isByRef) {
  assert(BlockInfo && "evaluating block ref without block information?");
  const CGBlockInfo::Capture &capture = BlockInfo->getCapture(variable);

  // Handle constant captures.
  if (capture.isConstant()) return LocalDefnMap[variable];

  llvm::Value *addr =
    Builder.CreateStructGEP(LoadBlockStruct(), capture.getIndex(),
                            "block.capture.addr");

  if (isByRef) {
    // addr should be a void** right now.  Load, then cast the result
    // to byref*.

    addr = Builder.CreateLoad(addr);
    llvm::PointerType *byrefPointerType
      = llvm::PointerType::get(BuildByRefType(variable), 0);
    addr = Builder.CreateBitCast(addr, byrefPointerType,
                                 "byref.addr");

    // Follow the forwarding pointer.
    addr = Builder.CreateStructGEP(addr, 1, "byref.forwarding");
    addr = Builder.CreateLoad(addr, "byref.addr.forwarded");

    // Cast back to byref* and GEP over to the actual object.
    addr = Builder.CreateBitCast(addr, byrefPointerType);
    addr = Builder.CreateStructGEP(addr, getByRefValueLLVMField(variable),
                                   variable->getNameAsString());
  }

  if (variable->getType()->isReferenceType())
    addr = Builder.CreateLoad(addr, "ref.tmp");

  return addr;
}

llvm::Constant *
CodeGenModule::GetAddrOfGlobalBlock(const BlockCmd *blockExpr,
                                    const char *name) {
  CGBlockInfo blockInfo(blockExpr, name);

  // Compute information about the layout, etc., of this block.
  computeBlockInfo(*this, blockInfo);

  // Using that metadata, generate the actual block function.
  llvm::Constant *blockFn;
  {
    llvm::DenseMap<const Defn*, llvm::Value*> LocalDefnMap;
    blockFn = CodeGenFunction(*this).GenerateBlockFunction(GlobalDefn(),
                                                           blockInfo,
                                                           0, LocalDefnMap);
  }
  blockFn = llvm::ConstantExpr::getBitCast(blockFn, IntTy);

  return buildGlobalBlock(*this, blockInfo, blockFn);
}

static llvm::Constant *buildGlobalBlock(CodeGenModule &CGM,
                                        const CGBlockInfo &blockInfo,
                                        llvm::Constant *blockFn) {
  assert(blockInfo.CanBeGlobal);

  // Generate the constants for the block literal initializer.
  llvm::Constant *fields[BlockHeaderSize];

  // isa
  fields[0] = CGM.getNSConcreteGlobalBlock();

  // __flags
  BlockFlags flags = BLOCK_IS_GLOBAL | BLOCK_HAS_SIGNATURE;
  if (blockInfo.UsesStret) flags |= BLOCK_USE_STRET;
                                      
  fields[1] = llvm::ConstantInt::get(CGM.IntTy, flags.getBitMask());

  // Reserved
  fields[2] = llvm::Constant::getNullValue(CGM.IntTy);

  // Function
  fields[3] = blockFn;

  // Descriptor
  fields[4] = buildBlockDescriptor(CGM, blockInfo);

  llvm::Constant *init = llvm::ConstantStruct::getAnon(fields);

  llvm::GlobalVariable *literal =
    new llvm::GlobalVariable(CGM.getModule(),
                             init->getType(),
                             /*constant*/ true,
                             llvm::GlobalVariable::InternalLinkage,
                             init,
                             "__block_literal_global");
  literal->setAlignment(blockInfo.BlockAlign.getQuantity());

  // Return a constant of the appropriately-casted type.
  llvm::Type *requiredType = CGM.IntTy;
  return llvm::ConstantExpr::getBitCast(literal, requiredType);
}

llvm::Function *
CodeGenFunction::GenerateBlockFunction(GlobalDefn GD,
                                       const CGBlockInfo &blockInfo,
                                       const Defn *outerFnDecl,
                                       const DefnMapTy &ldm) {
  const ScriptDefn *blockDecl = blockInfo.getBlockDecl();

  // Check if we should generate debug info for this block function.
  if (CGM.getModuleDebugInfo())
    DebugInfo = CGM.getModuleDebugInfo();

  BlockInfo = &blockInfo;

  // Arrange for local static and local extern declarations to appear
  // to be local to this function as well, in case they're directly
  // referenced in a block.
  for (DefnMapTy::const_iterator i = ldm.begin(), e = ldm.end(); i != e; ++i) {
    const VarDefn *var = dyn_cast<VarDefn>(i->first);
    if (var)
      LocalDefnMap[var] = i->second;
  }

  // Begin building the function declaration.

  // Build the argument list.
  FunctionArgList args;

  // The first argument is the block pointer.  Just take it as a void*
  // and cast it later.
  Type selfTy = getContext().Int32Ty;

  return NULL;
}

/*
    notes.push_back(HelperInfo());
    HelperInfo &note = notes.back();
    note.index = capture.getIndex();
    note.RequiresCopying = (ci->hasCopyExpr() || BlockRequiresCopying(type));
    note.cxxbar_import = ci->getCopyExpr();

    if (ci->isByRef()) {
      note.flag = BLOCK_FIELD_IS_BYREF;
      if (type.isObjCGCWeak())
        note.flag |= BLOCK_FIELD_IS_WEAK;
    } else if (type->isBlockPointerType()) {
      note.flag = BLOCK_FIELD_IS_BLOCK;
    } else {
      note.flag = BLOCK_FIELD_IS_OBJECT;
    }
 */

llvm::Constant *
CodeGenFunction::GenerateCopyHelperFunction(const CGBlockInfo &blockInfo) {
  ASTContext &C = getContext();

  FunctionArgList args;
  return NULL;
}

llvm::Constant *
CodeGenFunction::GenerateDestroyHelperFunction(const CGBlockInfo &blockInfo) {
  ASTContext &C = getContext();

  return NULL;
}

namespace {

/// Emits the copy/dispose helper functions for a __block object of id type.
class ObjectByrefHelpers : public CodeGenModule::ByrefHelpers {
  BlockFieldFlags Flags;

public:
  ObjectByrefHelpers(CharUnits alignment, BlockFieldFlags flags)
    : ByrefHelpers(alignment), Flags(flags) {}

  void emitCopy(CodeGenFunction &CGF, llvm::Value *destField,
                llvm::Value *srcField) {
    destField = CGF.Builder.CreateBitCast(destField, CGF.Int32Ty);

    srcField = CGF.Builder.CreateBitCast(srcField, CGF.VoidPtrPtrTy);
    llvm::Value *srcValue = CGF.Builder.CreateLoad(srcField);

    unsigned flags = (Flags | BLOCK_BYREF_CALLER).getBitMask();

    llvm::Value *flagsVal = llvm::ConstantInt::get(CGF.Int32Ty, flags);
    llvm::Value *fn = CGF.CGM.getBlockObjectAssign();
    CGF.Builder.CreateCall3(fn, destField, srcValue, flagsVal);
  }

  void emitDispose(CodeGenFunction &CGF, llvm::Value *field) {
    field = CGF.Builder.CreateBitCast(field, CGF.Int8PtrTy->getPointerTo(0));
    llvm::Value *value = CGF.Builder.CreateLoad(field);

    CGF.BuildBlockRelease(value, Flags | BLOCK_BYREF_CALLER);
  }

  void profileImpl(llvm::FoldingSetNodeID &id) const {
    id.AddInteger(Flags.getBitMask());
  }
};

/// Emits the copy/dispose helpers for an ARC __block __weak variable.
class ARCWeakByrefHelpers : public CodeGenModule::ByrefHelpers {
public:
  ARCWeakByrefHelpers(CharUnits alignment) : ByrefHelpers(alignment) {}

  void emitCopy(CodeGenFunction &CGF, llvm::Value *destField,
                llvm::Value *srcField) {
//    CGF.EmitARCMoveWeak(destField, srcField);
  }

  void emitDispose(CodeGenFunction &CGF, llvm::Value *field) {
//    CGF.EmitARCDestroyWeak(field);
  }

  void profileImpl(llvm::FoldingSetNodeID &id) const {
    // 0 is distinguishable from all pointers and byref flags
    id.AddInteger(0);
  }
};

/// Emits the copy/dispose helpers for an ARC __block __strong variable
/// that's not of block-pointer type.
class ARCStrongByrefHelpers : public CodeGenModule::ByrefHelpers {
public:
  ARCStrongByrefHelpers(CharUnits alignment) : ByrefHelpers(alignment) {}

  void emitCopy(CodeGenFunction &CGF, llvm::Value *destField,
                llvm::Value *srcField) {
    // Do a "move" by copying the value and then zeroing out the old
    // variable.

    llvm::Value *value = CGF.Builder.CreateLoad(srcField);
    llvm::Value *null =
      llvm::ConstantPointerNull::get(cast<llvm::PointerType>(value->getType()));
    CGF.Builder.CreateStore(value, destField);
    CGF.Builder.CreateStore(null, srcField);
  }

  void emitDispose(CodeGenFunction &CGF, llvm::Value *field) {
    llvm::Value *value = CGF.Builder.CreateLoad(field);
//    CGF.EmitARCRelease(value, /*precise*/ false);
  }

  void profileImpl(llvm::FoldingSetNodeID &id) const {
    // 1 is distinguishable from all pointers and byref flags
    id.AddInteger(1);
  }
};

/// Emits the copy/dispose helpers for a __block variable with a
/// nontrivial copy constructor or destructor.
class CXXByrefHelpers : public CodeGenModule::ByrefHelpers {
  Type VarType;
  const Expr *CopyExpr;

public:
  CXXByrefHelpers(CharUnits alignment, Type type,
                  const Expr *copyExpr)
    : ByrefHelpers(alignment), VarType(type), CopyExpr(copyExpr) {}

  bool needsCopy() const { return CopyExpr != 0; }
  void emitCopy(CodeGenFunction &CGF, llvm::Value *destField,
                llvm::Value *srcField) {
    if (!CopyExpr) return;
//    CGF.EmitSynthesizedCXXCopyCtor(destField, srcField, CopyExpr);
  }

  void emitDispose(CodeGenFunction &CGF, llvm::Value *field) {
    EHScopeStack::stable_iterator cleanupDepth = CGF.EHStack.stable_begin();
//    CGF.PushDestructorCleanup(VarType, field);
    CGF.PopCleanupBlocks(cleanupDepth);
  }

  void profileImpl(llvm::FoldingSetNodeID &id) const {
    id.AddPointer(VarType.getAsOpaquePtr());
  }
};
} // end anonymous namespace

static llvm::Constant *
generateByrefCopyHelper(CodeGenFunction &CGF,
                        llvm::StructType &byrefType,
                        CodeGenModule::ByrefHelpers &byrefInfo) {
  ASTContext &Context = CGF.getContext();

  Type R = Context.UInt32Ty;

  return NULL;
}

/// Build the copy helper for a __block variable.
static llvm::Constant *buildByrefCopyHelper(CodeGenModule &CGM,
                                            llvm::StructType &byrefType,
                                            CodeGenModule::ByrefHelpers &info) {
  CodeGenFunction CGF(CGM);
  return generateByrefCopyHelper(CGF, byrefType, info);
}

/// Generate code for a __block variable's dispose helper.
static llvm::Constant *
generateByrefDisposeHelper(CodeGenFunction &CGF,
                           llvm::StructType &byrefType,
                           CodeGenModule::ByrefHelpers &byrefInfo) {
  ASTContext &Context = CGF.getContext();
  Type R = Context.Int32Ty;

  FunctionArgList args;
  return NULL;
}

/// Build the dispose helper for a __block variable.
static llvm::Constant *buildByrefDisposeHelper(CodeGenModule &CGM,
                                               llvm::StructType &byrefType,
                                               CodeGenModule::ByrefHelpers &info) {
  CodeGenFunction CGF(CGM);
  return generateByrefDisposeHelper(CGF, byrefType, info);
}

/// 
template <class T> static T *buildByrefHelpers(CodeGenModule &CGM,
                                               llvm::StructType &byrefTy,
                                               T &byrefInfo) {
  // Increase the field's alignment to be at least pointer alignment,
  // since the layout of the byref struct will guarantee at least that.
  byrefInfo.Alignment = std::max(byrefInfo.Alignment,
                              CharUnits::fromQuantity(CGM.PointerAlignInBytes));

  llvm::FoldingSetNodeID id;
  byrefInfo.Profile(id);

  void *insertPos;
  CodeGenModule::ByrefHelpers *node
    = CGM.ByrefHelpersCache.FindNodeOrInsertPos(id, insertPos);
  if (node) return static_cast<T*>(node);

  byrefInfo.CopyHelper = buildByrefCopyHelper(CGM, byrefTy, byrefInfo);
  byrefInfo.DisposeHelper = buildByrefDisposeHelper(CGM, byrefTy, byrefInfo);

  T *copy = new (CGM.getContext()) T(byrefInfo);
  CGM.ByrefHelpersCache.InsertNode(copy, insertPos);
  return copy;
}

CodeGenModule::ByrefHelpers *
CodeGenFunction::buildByrefHelpers(llvm::StructType &byrefType,
                                   const AutoVarEmission &emission) {
  const VarDefn &var = *emission.Variable;
  Type type = var.getType();

  return 0;
}

unsigned CodeGenFunction::getByRefValueLLVMField(const ValueDefn *VD) const {
  assert(ByRefValueInfo.count(VD) && "Did not find value!");
  
  return ByRefValueInfo.find(VD)->second.second;
}

llvm::Value *CodeGenFunction::BuildBlockByrefAddress(llvm::Value *BaseAddr,
                                                     const VarDefn *V) {
  llvm::Value *Loc = Builder.CreateStructGEP(BaseAddr, 1, "forwarding");
  Loc = Builder.CreateLoad(Loc);
  Loc = Builder.CreateStructGEP(Loc, getByRefValueLLVMField(V),
                                V->getNameAsString());
  return Loc;
}

/// BuildByRefType - This routine changes a __block variable declared as T x
///   into:
///
///      struct {
///        void *__isa;
///        void *__forwarding;
///        int32_t __flags;
///        int32_t __size;
///        void *__copy_helper;       // only if needed
///        void *__destroy_helper;    // only if needed
///        char padding[X];           // only if needed
///        T x;
///      } x
///
llvm::Type *CodeGenFunction::BuildByRefType(const VarDefn *D) {
  std::pair<llvm::Type *, unsigned> &Info = ByRefValueInfo[D];
  if (Info.first)
    return Info.first;

  Type Ty = D->getType();

  llvm::SmallVector<llvm::Type *, 8> types;

  llvm::StructType *ByRefType =
    llvm::StructType::create(getLLVMContext(),
                             "struct.__block_byref_" + D->getNameAsString());

  // void *__isa;
  types.push_back(Int8PtrTy);

  // void *__forwarding;
  types.push_back(llvm::PointerType::getUnqual(ByRefType));

  // int32_t __flags;
  types.push_back(Int32Ty);

  // int32_t __size;
  types.push_back(Int32Ty);

//  bool HasCopyAndDispose = getContext().BlockRequiresCopying(Ty);
//  if (HasCopyAndDispose) {
//    /// void *__copy_helper;
//    types.push_back(Int8PtrTy);
//
//    /// void *__destroy_helper;
//    types.push_back(Int8PtrTy);
//  }

  bool Packed = false;
  CharUnits Align = getContext().getDefnAlign(D);
  if (Align > getContext().toCharUnitsFromBits(Target.getPointerAlign(0))) {
    // We have to insert padding.
    
    // The struct above has 2 32-bit integers.
    unsigned CurrentOffsetInBytes = 4 * 2;
    
    // And either 2 or 4 pointers.
    CurrentOffsetInBytes += (/*HasCopyAndDispose ? 4 :*/ 2) *
      CGM.getTargetData().getTypeAllocSize(Int8PtrTy);
    
    // Align the offset.
    unsigned AlignedOffsetInBytes = 
      llvm::RoundUpToAlignment(CurrentOffsetInBytes, Align.getQuantity());
    
    unsigned NumPaddingBytes = AlignedOffsetInBytes - CurrentOffsetInBytes;
    if (NumPaddingBytes > 0) {
      llvm::Type *Ty = llvm::Type::getInt8Ty(getLLVMContext());
      // FIXME: We need a sema error for alignment larger than the minimum of
      // the maximal stack alignment and the alignment of malloc on the system.
      if (NumPaddingBytes > 1)
        Ty = llvm::ArrayType::get(Ty, NumPaddingBytes);
    
      types.push_back(Ty);

      // We want a packed struct.
      Packed = true;
    }
  }

  // T x;
  types.push_back(ConvertTypeForMem(Ty));
  
  ByRefType->setBody(types, Packed);
  
  Info.first = ByRefType;
  
  Info.second = types.size() - 1;
  
  return Info.first;
}

/// Initialize the structural components of a __block variable, i.e.
/// everything but the actual object.
void CodeGenFunction::emitByrefStructureInit(const AutoVarEmission &emission) {

}

void CodeGenFunction::BuildBlockRelease(llvm::Value *V, BlockFieldFlags flags) {
  llvm::Value *F = CGM.getBlockObjectDispose();
  llvm::Value *N;
  V = Builder.CreateBitCast(V, Int8PtrTy);
  N = llvm::ConstantInt::get(Int32Ty, flags.getBitMask());
  Builder.CreateCall2(F, V, N);
}

namespace {
  struct CallBlockRelease : EHScopeStack::Cleanup {
    llvm::Value *Addr;
    CallBlockRelease(llvm::Value *Addr) : Addr(Addr) {}

    void Emit(CodeGenFunction &CGF, bool IsForEH) {
      // Should we be passing FIELD_IS_WEAK here?
      CGF.BuildBlockRelease(Addr, BLOCK_FIELD_IS_BYREF);
    }
  };
}

/// Enter a cleanup to destroy a __block variable.  Note that this
/// cleanup should be a no-op if the variable hasn't left the stack
/// yet; if a cleanup is required for the variable itself, that needs
/// to be done externally.
void CodeGenFunction::enterByrefCleanup(const AutoVarEmission &emission) {
  // We don't enter this cleanup if we're in pure-GC mode.
  if (CGM.getLangOptions().getGCMode() == LangOptions::GCOnly)
    return;

  EHStack.pushCleanup<CallBlockRelease>(NormalAndEHCleanup, emission.Address);
}
