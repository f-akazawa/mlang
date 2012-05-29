//===-- CodeGenFunction.h - Per-Function state for LLVM CodeGen -*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This is the internal per-function state used for llvm translation.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CODEGENFUNCTION_H_
#define MLANG_CODEGEN_CODEGENFUNCTION_H_

#include "mlang/AST/CharUnits.h"
#include "mlang/AST/CmdAll.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/Type.h"
#include "mlang/Basic/ABI.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ValueHandle.h"
#include "CodeGenModule.h"
#include "CGBuilder.h"
#include "CGValue.h"

namespace llvm {
  class BasicBlock;
  class LLVMContext;
  class MDNode;
  class Module;
  class SwitchInst;
  class Twine;
  class Value;
  class CallSite;
}

namespace mlang {
  class APValue;
  class ASTContext;
  class ClassDestructorDefn;
  class TryCmd;
  class Defn;
  class FunctionDefn;
  class FunctionProtoType;
  class ImplicitParamDefn;
  class TargetInfo;
  class TargetCodeGenInfo;
  class VarDefn;

namespace CodeGen {
class BlockFlags;
class BlockFieldFlags;
class CodeGenTypes;
class CGDebugInfo;
class CGFunctionInfo;
class CGRecordLayout;
class CGBlockInfo;
class CGOOPABI;

/// A branch fixup.  These are required when emitting a goto to a
/// label which hasn't been emitted yet.  The goto is optimistically
/// emitted as a branch to the basic block for the label, and (if it
/// occurs in a scope with non-trivial cleanups) a fixup is added to
/// the innermost cleanup.  When a (normal) cleanup is popped, any
/// unresolved fixups in that scope are threaded through the cleanup.
struct BranchFixup {
  /// The block containing the terminator which needs to be modified
  /// into a switch if this fixup is resolved into the current scope.
  /// If null, LatestBranch points directly to the destination.
  llvm::BasicBlock *OptimisticBranchBlock;

  /// The ultimate destination of the branch.
  ///
  /// This can be set to null to indicate that this fixup was
  /// successfully resolved.
  llvm::BasicBlock *Destination;

  /// The destination index value.
  unsigned DestinationIndex;

  /// The initial branch of the fixup.
  llvm::BranchInst *InitialBranch;
};

template <class T> struct InvariantValue {
  typedef T type;
  typedef T saved_type;
  static bool needsSaving(type value) { return false; }
  static saved_type save(CodeGenFunction &CGF, type value) { return value; }
  static type restore(CodeGenFunction &CGF, saved_type value) { return value; }
};

/// A metaprogramming class for ensuring that a value will dominate an
/// arbitrary position in a function.
template <class T> struct DominatingValue : InvariantValue<T> {};

template <class T, bool mightBeInstruction =
            llvm::is_base_of<llvm::Value, T>::value &&
            !llvm::is_base_of<llvm::Constant, T>::value &&
            !llvm::is_base_of<llvm::BasicBlock, T>::value>
struct DominatingPointer;
template <class T> struct DominatingPointer<T,false> : InvariantValue<T*> {};
// template <class T> struct DominatingPointer<T,true> at end of file

template <class T> struct DominatingValue<T*> : DominatingPointer<T> {};

enum CleanupKind {
  EHCleanup = 0x1,
  NormalCleanup = 0x2,
  NormalAndEHCleanup = EHCleanup | NormalCleanup,

  InactiveCleanup = 0x4,
  InactiveEHCleanup = EHCleanup | InactiveCleanup,
  InactiveNormalCleanup = NormalCleanup | InactiveCleanup,
  InactiveNormalAndEHCleanup = NormalAndEHCleanup | InactiveCleanup
};

/// A stack of scopes which respond to exceptions, including cleanups
/// and catch blocks.
class EHScopeStack {
public:
  /// A saved depth on the scope stack.  This is necessary because
  /// pushing scopes onto the stack invalidates iterators.
  class stable_iterator {
    friend class EHScopeStack;

    /// Offset from StartOfData to EndOfBuffer.
    ptrdiff_t Size;

    stable_iterator(ptrdiff_t Size) : Size(Size) {}

  public:
    static stable_iterator invalid() { return stable_iterator(-1); }
    stable_iterator() : Size(-1) {}

    bool isValid() const { return Size >= 0; }

    /// Returns true if this scope encloses I.
    /// Returns false if I is invalid.
    /// This scope must be valid.
    bool encloses(stable_iterator I) const { return Size <= I.Size; }

    /// Returns true if this scope strictly encloses I: that is,
    /// if it encloses I and is not I.
    /// Returns false is I is invalid.
    /// This scope must be valid.
    bool strictlyEncloses(stable_iterator I) const { return Size < I.Size; }

    friend bool operator==(stable_iterator A, stable_iterator B) {
      return A.Size == B.Size;
    }
    friend bool operator!=(stable_iterator A, stable_iterator B) {
      return A.Size != B.Size;
    }
  };

  /// Information for lazily generating a cleanup.  Subclasses must be
  /// POD-like: cleanups will not be destructed, and they will be
  /// allocated on the cleanup stack and freely copied and moved
  /// around.
  ///
  /// Cleanup implementations should generally be declared in an
  /// anonymous namespace.
  class Cleanup {
  public:
    // Anchor the construction vtable.  We use the destructor because
    // gcc gives an obnoxious warning if there are virtual methods
    // with an accessible non-virtual destructor.  Unfortunately,
    // declaring this destructor makes it non-trivial, but there
    // doesn't seem to be any other way around this warning.
    //
    // This destructor will never be called.
    virtual ~Cleanup();

    /// Emit the cleanup.  For normal cleanups, this is run in the
    /// same EH context as when the cleanup was pushed, i.e. the
    /// immediately-enclosing context of the cleanup scope.  For
    /// EH cleanups, this is run in a terminate context.
    ///
    // \param IsForEHCleanup true if this is for an EH cleanup, false
    ///  if for a normal cleanup.
    virtual void Emit(CodeGenFunction &CGF, bool IsForEHCleanup) = 0;
  };

  /// UnconditionalCleanupN stores its N parameters and just passes
	/// them to the real cleanup function.
	template<class T, class A0>
	class UnconditionalCleanup1: public Cleanup {
		A0 a0;
	public:
		UnconditionalCleanup1(A0 a0) :
			a0(a0) {
		}
		void Emit(CodeGenFunction &CGF, bool IsForEHCleanup) {
			T::Emit(CGF, IsForEHCleanup, a0);
		}
	};

	template<class T, class A0, class A1>
	class UnconditionalCleanup2: public Cleanup {
		A0 a0;
		A1 a1;
	public:
		UnconditionalCleanup2(A0 a0, A1 a1) :
			a0(a0), a1(a1) {
		}
		void Emit(CodeGenFunction &CGF, bool IsForEHCleanup) {
			T::Emit(CGF, IsForEHCleanup, a0, a1);
		}
	};

	template<class T, class A0, class A1, class A2>
	class UnconditionalCleanup3: public Cleanup {
		A0 a0;
		A1 a1;
		A2 a2;
	public:
		UnconditionalCleanup3(A0 a0, A1 a1, A2 a2) :
			a0(a0), a1(a1), a2(a2) {
		}
		void Emit(CodeGenFunction &CGF, bool IsForEHCleanup) {
			T::Emit(CGF, IsForEHCleanup, a0, a1, a2);
		}
	};

	/// ConditionalCleanupN stores the saved form of its N parameters,
	/// then restores them and performs the cleanup.
	template<class T, class A0>
	class ConditionalCleanup1: public Cleanup {
		typedef typename DominatingValue<A0>::saved_type A0_saved;
		A0_saved a0_saved;

		void Emit(CodeGenFunction &CGF, bool IsForEHCleanup) {
			A0 a0 = DominatingValue<A0>::restore(CGF, a0_saved);
			T::Emit(CGF, IsForEHCleanup, a0);
		}

	public:
		ConditionalCleanup1(A0_saved a0) :
			a0_saved(a0) {
		}
	};

	template<class T, class A0, class A1>
	class ConditionalCleanup2: public Cleanup {
		typedef typename DominatingValue<A0>::saved_type A0_saved;
		typedef typename DominatingValue<A1>::saved_type A1_saved;
		A0_saved a0_saved;
		A1_saved a1_saved;

		void Emit(CodeGenFunction &CGF, bool IsForEHCleanup) {
			A0 a0 = DominatingValue<A0>::restore(CGF, a0_saved);
			A1 a1 = DominatingValue<A1>::restore(CGF, a1_saved);
			T::Emit(CGF, IsForEHCleanup, a0, a1);
		}

	public:
		ConditionalCleanup2(A0_saved a0, A1_saved a1) :
			a0_saved(a0), a1_saved(a1) {
		}
	};

	template<class T, class A0, class A1, class A2>
	class ConditionalCleanup3: public Cleanup {
		typedef typename DominatingValue<A0>::saved_type A0_saved;
		typedef typename DominatingValue<A1>::saved_type A1_saved;
		typedef typename DominatingValue<A2>::saved_type A2_saved;
		A0_saved a0_saved;
		A1_saved a1_saved;
		A2_saved a2_saved;

		void Emit(CodeGenFunction &CGF, bool IsForEHCleanup) {
			A0 a0 = DominatingValue<A0>::restore(CGF, a0_saved);
			A1 a1 = DominatingValue<A1>::restore(CGF, a1_saved);
			A2 a2 = DominatingValue<A2>::restore(CGF, a2_saved);
			T::Emit(CGF, IsForEHCleanup, a0, a1, a2);
		}

	public:
		ConditionalCleanup3(A0_saved a0, A1_saved a1, A2_saved a2) :
			a0_saved(a0), a1_saved(a1), a2_saved(a2) {
		}
	};

private:
  // The implementation for this class is in CGException.h and
  // CGException.cpp; the definition is here because it's used as a
  // member of CodeGenFunction.

  /// The start of the scope-stack buffer, i.e. the allocated pointer
  /// for the buffer.  All of these pointers are either simultaneously
  /// null or simultaneously valid.
  char *StartOfBuffer;

  /// The end of the buffer.
  char *EndOfBuffer;

  /// The first valid entry in the buffer.
  char *StartOfData;

  /// The innermost normal cleanup on the stack.
  stable_iterator InnermostNormalCleanup;

  /// The innermost EH cleanup on the stack.
  stable_iterator InnermostEHCleanup;

  /// The number of catches on the stack.
  unsigned CatchDepth;

  /// The current EH destination index.  Reset to FirstCatchIndex
  /// whenever the last EH cleanup is popped.
  unsigned NextEHDestIndex;
  enum { FirstEHDestIndex = 1 };

  /// The current set of branch fixups.  A branch fixup is a jump to
  /// an as-yet unemitted label, i.e. a label for which we don't yet
  /// know the EH stack depth.  Whenever we pop a cleanup, we have
  /// to thread all the current branch fixups through it.
  ///
  /// Fixups are recorded as the Use of the respective branch or
  /// switch statement.  The use points to the final destination.
  /// When popping out of a cleanup, these uses are threaded through
  /// the cleanup and adjusted to point to the new cleanup.
  ///
  /// Note that branches are allowed to jump into protected scopes
  /// in certain situations;  e.g. the following code is legal:
  ///     struct A { ~A(); }; // trivial ctor, non-trivial dtor
  ///     goto foo;
  ///     A a;
  ///    foo:
  ///     bar();
  llvm::SmallVector<BranchFixup, 8> BranchFixups;

  char *allocate(size_t Size);

  void *pushCleanup(CleanupKind K, size_t DataSize);

public:
  EHScopeStack() : StartOfBuffer(0), EndOfBuffer(0), StartOfData(0),
                   InnermostNormalCleanup(stable_end()),
                   InnermostEHCleanup(stable_end()),
                   CatchDepth(0), NextEHDestIndex(FirstEHDestIndex) {}
  ~EHScopeStack() { delete[] StartOfBuffer; }

  // Variadic templates would make this not terrible.

  /// Push a lazily-created cleanup on the stack.
  template <class T>
  void pushCleanup(CleanupKind Kind) {
    void *Buffer = pushCleanup(Kind, sizeof(T));
    Cleanup *Obj = new(Buffer) T();
    (void) Obj;
  }

  /// Push a lazily-created cleanup on the stack.
  template <class T, class A0>
  void pushCleanup(CleanupKind Kind, A0 a0) {
    void *Buffer = pushCleanup(Kind, sizeof(T));
    Cleanup *Obj = new(Buffer) T(a0);
    (void) Obj;
  }

  /// Push a lazily-created cleanup on the stack.
  template <class T, class A0, class A1>
  void pushCleanup(CleanupKind Kind, A0 a0, A1 a1) {
    void *Buffer = pushCleanup(Kind, sizeof(T));
    Cleanup *Obj = new(Buffer) T(a0, a1);
    (void) Obj;
  }

  /// Push a lazily-created cleanup on the stack.
  template <class T, class A0, class A1, class A2>
  void pushCleanup(CleanupKind Kind, A0 a0, A1 a1, A2 a2) {
    void *Buffer = pushCleanup(Kind, sizeof(T));
    Cleanup *Obj = new(Buffer) T(a0, a1, a2);
    (void) Obj;
  }

  /// Push a lazily-created cleanup on the stack.
  template <class T, class A0, class A1, class A2, class A3>
  void pushCleanup(CleanupKind Kind, A0 a0, A1 a1, A2 a2, A3 a3) {
    void *Buffer = pushCleanup(Kind, sizeof(T));
    Cleanup *Obj = new(Buffer) T(a0, a1, a2, a3);
    (void) Obj;
  }

  /// Push a lazily-created cleanup on the stack.
  template <class T, class A0, class A1, class A2, class A3, class A4>
  void pushCleanup(CleanupKind Kind, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4) {
    void *Buffer = pushCleanup(Kind, sizeof(T));
    Cleanup *Obj = new(Buffer) T(a0, a1, a2, a3, a4);
    (void) Obj;
  }

  // Feel free to add more variants of the following:

  /// Push a cleanup with non-constant storage requirements on the
  /// stack.  The cleanup type must provide an additional static method:
  ///   static size_t getExtraSize(size_t);
  /// The argument to this method will be the value N, which will also
  /// be passed as the first argument to the constructor.
  ///
  /// The data stored in the extra storage must obey the same
  /// restrictions as normal cleanup member data.
  ///
  /// The pointer returned from this method is valid until the cleanup
  /// stack is modified.
  template <class T, class A0, class A1, class A2>
  T *pushCleanupWithExtra(CleanupKind Kind, size_t N, A0 a0, A1 a1, A2 a2) {
    void *Buffer = pushCleanup(Kind, sizeof(T) + T::getExtraSize(N));
    return new (Buffer) T(N, a0, a1, a2);
  }

  /// Pops a cleanup scope off the stack.  This should only be called
  /// by CodeGenFunction::PopCleanupBlock.
  void popCleanup();

  /// Push a set of catch handlers on the stack.  The catch is
  /// uninitialized and will need to have the given number of handlers
  /// set on it.
  class EHCatchScope *pushCatch(unsigned NumHandlers);

  /// Pops a catch scope off the stack.
  void popCatch();

  /// Push an exceptions filter on the stack.
  class EHFilterScope *pushFilter(unsigned NumFilters);

  /// Pops an exceptions filter off the stack.
  void popFilter();

  /// Push a terminate handler on the stack.
  void pushTerminate();

  /// Pops a terminate handler off the stack.
  void popTerminate();

  /// Determines whether the exception-scopes stack is empty.
  bool empty() const { return StartOfData == EndOfBuffer; }

  bool requiresLandingPad() const {
    return (CatchDepth || hasEHCleanups());
  }

  /// Determines whether there are any normal cleanups on the stack.
  bool hasNormalCleanups() const {
    return InnermostNormalCleanup != stable_end();
  }

  /// Returns the innermost normal cleanup on the stack, or
  /// stable_end() if there are no normal cleanups.
  stable_iterator getInnermostNormalCleanup() const {
    return InnermostNormalCleanup;
  }
  stable_iterator getInnermostActiveNormalCleanup() const; // CGException.h

  /// Determines whether there are any EH cleanups on the stack.
  bool hasEHCleanups() const {
    return InnermostEHCleanup != stable_end();
  }

  /// Returns the innermost EH cleanup on the stack, or stable_end()
  /// if there are no EH cleanups.
  stable_iterator getInnermostEHCleanup() const {
    return InnermostEHCleanup;
  }
  stable_iterator getInnermostActiveEHCleanup() const; // CGException.h

  /// An unstable reference to a scope-stack depth.  Invalidated by
  /// pushes but not pops.
  class iterator;

  /// Returns an iterator pointing to the innermost EH scope.
  iterator begin() const;

  /// Returns an iterator pointing to the outermost EH scope.
  iterator end() const;

  /// Create a stable reference to the top of the EH stack.  The
  /// returned reference is valid until that scope is popped off the
  /// stack.
  stable_iterator stable_begin() const {
    return stable_iterator(EndOfBuffer - StartOfData);
  }

  /// Create a stable reference to the bottom of the EH stack.
  static stable_iterator stable_end() {
    return stable_iterator(0);
  }

  /// Translates an iterator into a stable_iterator.
  stable_iterator stabilize(iterator it) const;

  /// Finds the nearest cleanup enclosing the given iterator.
  /// Returns stable_iterator::invalid() if there are no such cleanups.
  stable_iterator getEnclosingEHCleanup(iterator it) const;

  /// Turn a stable reference to a scope depth into a unstable pointer
  /// to the EH stack.
  iterator find(stable_iterator save) const;

  /// Removes the cleanup pointed to by the given stable_iterator.
  void removeCleanup(stable_iterator save);

  /// Add a branch fixup to the current cleanup scope.
  BranchFixup &addBranchFixup() {
    assert(hasNormalCleanups() && "adding fixup in scope without cleanups");
    BranchFixups.push_back(BranchFixup());
    return BranchFixups.back();
  }

  unsigned getNumBranchFixups() const { return BranchFixups.size(); }
  BranchFixup &getBranchFixup(unsigned I) {
    assert(I < getNumBranchFixups());
    return BranchFixups[I];
  }

  /// Pops lazily-removed fixups from the end of the list.  This
  /// should only be called by procedures which have just popped a
  /// cleanup or resolved one or more fixups.
  void popNullFixups();

  /// Clears the branch-fixups list.  This should only be called by
  /// ResolveAllBranchFixups.
  void clearFixups() { BranchFixups.clear(); }

  /// Gets the next EH destination index.
  unsigned getNextEHDestIndex() { return NextEHDestIndex++; }
};


/// CodeGenFunction - This class organizes the per-function state that is used
/// while generating LLVM code.
class CodeGenFunction : public CodeGenTypeCache {
  CodeGenFunction(const CodeGenFunction&); // DO NOT IMPLEMENT
  void operator=(const CodeGenFunction&);  // DO NOT IMPLEMENT

  friend class CGOOPABI;
public:
  /// A jump destination is an abstract label, branching to which may
  /// require a jump out through normal cleanups.
  struct JumpDest {
    JumpDest() : Block(0), ScopeDepth(), Index(0) {}
    JumpDest(llvm::BasicBlock *Block,
             EHScopeStack::stable_iterator Depth,
             unsigned Index)
      : Block(Block), ScopeDepth(Depth), Index(Index) {}

    bool isValid() const { return Block != 0; }
    llvm::BasicBlock *getBlock() const { return Block; }
    EHScopeStack::stable_iterator getScopeDepth() const { return ScopeDepth; }
    unsigned getDestIndex() const { return Index; }

  private:
    llvm::BasicBlock *Block;
    EHScopeStack::stable_iterator ScopeDepth;
    unsigned Index;
  };

  /// An unwind destination is an abstract label, branching to which
  /// may require a jump out through EH cleanups.
  struct UnwindDest {
    UnwindDest() : Block(0), ScopeDepth(), Index(0) {}
    UnwindDest(llvm::BasicBlock *Block,
               EHScopeStack::stable_iterator Depth,
               unsigned Index)
      : Block(Block), ScopeDepth(Depth), Index(Index) {}

    bool isValid() const { return Block != 0; }
    llvm::BasicBlock *getBlock() const { return Block; }
    EHScopeStack::stable_iterator getScopeDepth() const { return ScopeDepth; }
    unsigned getDestIndex() const { return Index; }

  private:
    llvm::BasicBlock *Block;
    EHScopeStack::stable_iterator ScopeDepth;
    unsigned Index;
  };

  CodeGenModule &CGM;  // Per-module state.
  const TargetInfo &Target;

  typedef std::pair<llvm::Value *, llvm::Value *> ComplexPairTy;
  CGBuilderTy Builder;

  /// CurFuncDefn - Holds the Defn for the current function or ObjC method.
  /// This excludes BlockDefns.
  const Defn *CurFuncDefn;
  /// CurCodeDefn - This is the inner-most code context, which includes blocks.
  const Defn *CurCodeDefn;
  const CGFunctionInfo *CurFnInfo;
  Type FnRetTy;
  llvm::Function *CurFn;

  /// CurGD - The GlobalDefn for the current function being compiled.
  GlobalDefn CurGD;

  /// PrologueCleanupDepth - The cleanup depth enclosing all the
  /// cleanups associated with the parameters.
  EHScopeStack::stable_iterator PrologueCleanupDepth;

  /// ReturnBlock - Unified return block.
  JumpDest ReturnBlock;

  /// ReturnValue - The temporary alloca to hold the return value. This is null
  /// iff the function has no return value.
  llvm::Value *ReturnValue;

  /// RethrowBlock - Unified rethrow block.
  UnwindDest RethrowBlock;

  /// AllocaInsertPoint - This is an instruction in the entry block before which
  /// we prefer to insert allocas.
  llvm::AssertingVH<llvm::Instruction> AllocaInsertPt;

  bool CatchUndefined;

  /// In ARC, whether we should autorelease the return value.
  bool AutoreleaseResult;

  const CodeGen::CGBlockInfo *BlockInfo;
  llvm::Value *BlockPointer;

  /// \brief A mapping from NRVO variables to the flags used to indicate
  /// when the NRVO has been applied to this variable.
  llvm::DenseMap<const VarDefn *, llvm::Value *> NRVOFlags;

  EHScopeStack EHStack;

  /// i32s containing the indexes of the cleanup destinations.
  llvm::AllocaInst *NormalCleanupDest;
  llvm::AllocaInst *EHCleanupDest;

  unsigned NextCleanupDestIndex;

  /// The exception slot.  All landing pads write the current
  /// exception pointer into this alloca.
  llvm::Value *ExceptionSlot;

  /// Emits a landing pad for the current EH stack.
  llvm::BasicBlock *EmitLandingPad();

  llvm::BasicBlock *getInvokeDestImpl();

  /// Set up the last cleaup that was pushed as a conditional
  /// full-expression cleanup.
  void initFullExprCleanup();

  template <class T>
  typename DominatingValue<T>::saved_type saveValueInCond(T value) {
	  return DominatingValue<T>::save(*this, value);
  }

public:
  // A struct holding information about a finally block's IR
  // generation.
  struct FinallyInfo {
	  /// Where the catchall's edge through the cleanup should go.
		JumpDest RethrowDest;

		/// A function to call to enter the catch.
		llvm::Constant *BeginCatchFn;

		/// An i1 variable indicating whether or not the @finally is
		/// running for an exception.
		llvm::AllocaInst *ForEHVar;

		/// An i8* variable into which the exception pointer to rethrow
		/// has been saved.
		llvm::AllocaInst *SavedExnVar;

	public:
		void enter(CodeGenFunction &CGF, const Stmt *Finally,
				llvm::Constant *beginCatchFn, llvm::Constant *endCatchFn,
				llvm::Constant *rethrowFn);
		void exit(CodeGenFunction &CGF);
  };

  /// pushFullExprCleanup - Push a cleanup to be run at the end of the
	/// current full-expression.  Safe against the possibility that
	/// we're currently inside a conditionally-evaluated expression.
	template<class T, class A0>
	void pushFullExprCleanup(CleanupKind kind, A0 a0) {
		// If we're not in a conditional branch, or if none of the
		// arguments requires saving, then use the unconditional cleanup.
		if (!isInConditionalBranch()) {
			typedef EHScopeStack::UnconditionalCleanup1<T, A0> CleanupType;
			return EHStack.pushCleanup<CleanupType> (kind, a0);
		}

		typename DominatingValue<A0>::saved_type a0_saved = saveValueInCond(a0);

		typedef EHScopeStack::ConditionalCleanup1<T, A0> CleanupType;
		EHStack.pushCleanup<CleanupType> (kind, a0_saved);
		initFullExprCleanup();
	}

	/// pushFullExprCleanup - Push a cleanup to be run at the end of the
	/// current full-expression.  Safe against the possibility that
	/// we're currently inside a conditionally-evaluated expression.
	template<class T, class A0, class A1>
	void pushFullExprCleanup(CleanupKind kind, A0 a0, A1 a1) {
		// If we're not in a conditional branch, or if none of the
		// arguments requires saving, then use the unconditional cleanup.
		if (!isInConditionalBranch()) {
			typedef EHScopeStack::UnconditionalCleanup2<T, A0, A1> CleanupType;
			return EHStack.pushCleanup<CleanupType> (kind, a0, a1);
		}

		typename DominatingValue<A0>::saved_type a0_saved = saveValueInCond(a0);
		typename DominatingValue<A1>::saved_type a1_saved = saveValueInCond(a1);

		typedef EHScopeStack::ConditionalCleanup2<T, A0, A1> CleanupType;
		EHStack.pushCleanup<CleanupType> (kind, a0_saved, a1_saved);
		initFullExprCleanup();
	}

	/// pushFullExprCleanup - Push a cleanup to be run at the end of the
	/// current full-expression.  Safe against the possibility that
	/// we're currently inside a conditionally-evaluated expression.
	template<class T, class A0, class A1, class A2>
	void pushFullExprCleanup(CleanupKind kind, A0 a0, A1 a1, A2 a2) {
		// If we're not in a conditional branch, or if none of the
		// arguments requires saving, then use the unconditional cleanup.
		if (!isInConditionalBranch()) {
			typedef EHScopeStack::UnconditionalCleanup3<T, A0, A1, A2>
					CleanupType;
			return EHStack.pushCleanup<CleanupType> (kind, a0, a1, a2);
		}

		typename DominatingValue<A0>::saved_type a0_saved = saveValueInCond(a0);
		typename DominatingValue<A1>::saved_type a1_saved = saveValueInCond(a1);
		typename DominatingValue<A2>::saved_type a2_saved = saveValueInCond(a2);

		typedef EHScopeStack::ConditionalCleanup3<T, A0, A1, A2> CleanupType;
		EHStack.pushCleanup<CleanupType> (kind, a0_saved, a1_saved, a2_saved);
		initFullExprCleanup();
	}

  /// PushDestructorCleanup - Push a cleanup to call the
  /// complete-object destructor of an object of the given type at the
  /// given address.  Does nothing if T is not a C++ class type with a
  /// non-trivial destructor.
  void PushDestructorCleanup(Type T, llvm::Value *Addr);

  /// PushDestructorCleanup - Push a cleanup to call the
  /// complete-object variant of the given destructor on the object at
  /// the given address.
  void PushDestructorCleanup(const ClassDestructorDefn *Dtor,
                             llvm::Value *Addr);

  /// PopCleanupBlock - Will pop the cleanup entry on the stack and
  /// process all branch fixups.
  void PopCleanupBlock(bool FallThroughIsBranchThrough = false);

  /// DeactivateCleanupBlock - Deactivates the given cleanup block.
  /// The block cannot be reactivated.  Pops it if it's the top of the
  /// stack.
  void DeactivateCleanupBlock(EHScopeStack::stable_iterator Cleanup);

  /// ActivateCleanupBlock - Activates an initially-inactive cleanup.
  /// Cannot be used to resurrect a deactivated cleanup.
  void ActivateCleanupBlock(EHScopeStack::stable_iterator Cleanup);

  /// \brief Enters a new scope for capturing cleanups, all of which
  /// will be executed once the scope is exited.
  class RunCleanupsScope {
    CodeGenFunction& CGF;
    EHScopeStack::stable_iterator CleanupStackDepth;
    bool OldDidCallStackSave;
    bool PerformCleanup;

    RunCleanupsScope(const RunCleanupsScope &); // DO NOT IMPLEMENT
    RunCleanupsScope &operator=(const RunCleanupsScope &); // DO NOT IMPLEMENT

  public:
    /// \brief Enter a new cleanup scope.
    explicit RunCleanupsScope(CodeGenFunction &CGF)
      : CGF(CGF), PerformCleanup(true)
    {
      CleanupStackDepth = CGF.EHStack.stable_begin();
      OldDidCallStackSave = CGF.DidCallStackSave;
      CGF.DidCallStackSave = false;
    }

    /// \brief Exit this cleanup scope, emitting any accumulated
    /// cleanups.
    ~RunCleanupsScope() {
      if (PerformCleanup) {
        CGF.DidCallStackSave = OldDidCallStackSave;
        CGF.PopCleanupBlocks(CleanupStackDepth);
      }
    }

    /// \brief Determine whether this scope requires any cleanups.
    bool requiresCleanups() const {
      return CGF.EHStack.stable_begin() != CleanupStackDepth;
    }

    /// \brief Force the emission of cleanups now, instead of waiting
    /// until this object is destroyed.
    void ForceCleanup() {
      assert(PerformCleanup && "Already forced cleanup");
      CGF.DidCallStackSave = OldDidCallStackSave;
      CGF.PopCleanupBlocks(CleanupStackDepth);
      PerformCleanup = false;
    }
  };

  /// PopCleanupBlocks - Takes the old cleanup stack size and emits
  /// the cleanup blocks that have been added.
  void PopCleanupBlocks(EHScopeStack::stable_iterator OldCleanupStackSize);

  void ResolveBranchFixups(llvm::BasicBlock *Target);

  /// The given basic block lies in the current EH scope, but may be a
  /// target of a potentially scope-crossing jump; get a stable handle
  /// to which we can perform this jump later.
  JumpDest getJumpDestInCurrentScope(llvm::BasicBlock *Target) {
    return JumpDest(Target,
                    EHStack.getInnermostNormalCleanup(),
                    NextCleanupDestIndex++);
  }

  /// The given basic block lies in the current EH scope, but may be a
  /// target of a potentially scope-crossing jump; get a stable handle
  /// to which we can perform this jump later.
  JumpDest getJumpDestInCurrentScope(const char *Name = 0) {
    return getJumpDestInCurrentScope(createBasicBlock(Name));
  }

  /// EmitBranchThroughCleanup - Emit a branch from the current insert
  /// block through the normal cleanup handling code (if any) and then
  /// on to \arg Dest.
  void EmitBranchThroughCleanup(JumpDest Dest);

  /// isObviouslyBranchWithoutCleanups - Return true if a branch to the
  /// specified destination obviously has no cleanups to run.  'false' is always
  /// a conservatively correct answer for this method.
  bool isObviouslyBranchWithoutCleanups(JumpDest Dest) const;

  /// EmitBranchThroughEHCleanup - Emit a branch from the current
  /// insert block through the EH cleanup handling code (if any) and
  /// then on to \arg Dest.
  void EmitBranchThroughEHCleanup(UnwindDest Dest);

  /// getRethrowDest - Returns the unified outermost-scope rethrow
  /// destination.
  UnwindDest getRethrowDest();

  /// An object to manage conditionally-evaluated expressions.
  class ConditionalEvaluation {
		llvm::BasicBlock *StartBB;

	public:
		ConditionalEvaluation(CodeGenFunction &CGF) :
			StartBB(CGF.Builder.GetInsertBlock()) {
		}

		void begin(CodeGenFunction &CGF) {
			assert(CGF.OutermostConditional != this);
			if (!CGF.OutermostConditional)
				CGF.OutermostConditional = this;
		}

		void end(CodeGenFunction &CGF) {
			assert(CGF.OutermostConditional != 0);
			if (CGF.OutermostConditional == this)
				CGF.OutermostConditional = 0;
		}

		/// Returns a block which will be executed prior to each
		/// evaluation of the conditional code.
		llvm::BasicBlock *getStartingBlock() const {
			return StartBB;
		}
	};

  /// isInConditionalBranch - Return true if we're currently emitting
  /// one branch or the other of a conditional expression.
  bool isInConditionalBranch() const { return OutermostConditional != 0; }

  /// An RAII object to record that we're evaluating a statement
  /// expression.
  class StmtExprEvaluation {
	  CodeGenFunction &CGF;

		/// We have to save the outermost conditional: cleanups in a
		/// statement expression aren't conditional just because the
		/// StmtExpr is.
		ConditionalEvaluation *SavedOutermostConditional;

	public:
		StmtExprEvaluation(CodeGenFunction &CGF) :
			CGF(CGF), SavedOutermostConditional(CGF.OutermostConditional) {
			CGF.OutermostConditional = 0;
		}

		~StmtExprEvaluation() {
			CGF.OutermostConditional = SavedOutermostConditional;
			CGF.EnsureInsertPoint();
		}
    };

  /// An object which temporarily prevents a value from being
  /// destroyed by aggressive peephole optimizations that assume that
  /// all uses of a value have been realized in the IR.
  class PeepholeProtection {
	  llvm::Instruction *Inst;
	  friend class CodeGenFunction;
  public:
	  PeepholeProtection() : Inst(0) {}
    };

  /// An RAII object to set (and then clear) a mapping for an OpaqueValueExpr.
  class OpaqueValueMapping {
		CodeGenFunction &CGF;
		const OpaqueValueExpr *OpaqueValue;
		bool BoundLValue;
		CodeGenFunction::PeepholeProtection Protection;

	public:
		static bool shouldBindAsLValue(const Expr *expr) {
			return expr->isLValue() || expr->getType()->isHeteroContainerType();
		}

		OpaqueValueMapping(CodeGenFunction &CGF,
				const OpaqueValueExpr *opaqueValue, LValue lvalue) :
			CGF(CGF), OpaqueValue(opaqueValue), BoundLValue(true) {
			assert(opaqueValue && "no opaque value expression!");
			assert(shouldBindAsLValue(opaqueValue));
			initLValue(lvalue);
		}

		OpaqueValueMapping(CodeGenFunction &CGF,
				const OpaqueValueExpr *opaqueValue, RValue rvalue) :
			CGF(CGF), OpaqueValue(opaqueValue), BoundLValue(false) {
			assert(opaqueValue && "no opaque value expression!");
			assert(!shouldBindAsLValue(opaqueValue));
			initRValue(rvalue);
		}

		void pop() {
			assert(OpaqueValue && "mapping already popped!");
			popImpl();
			OpaqueValue = 0;
		}

		~OpaqueValueMapping() {
			if (OpaqueValue)
				popImpl();
		}

	private:
		void popImpl() {
			if (BoundLValue)
				CGF.OpaqueLValues.erase(OpaqueValue);
			else {
				CGF.OpaqueRValues.erase(OpaqueValue);
//				CGF.unprotectFromPeepholes(Protection);
			}
		}

		void init(const OpaqueValueExpr *ov, const Expr *e) {
			OpaqueValue = ov;
			BoundLValue = shouldBindAsLValue(ov);
			assert(BoundLValue == shouldBindAsLValue(e)
					&& "inconsistent expression value kinds!");
			if (BoundLValue)
				initLValue(CGF.EmitLValue(e));
			else
				initRValue(CGF.EmitAnyExpr(e));
		}

		void initLValue(const LValue &lv) {
			CGF.OpaqueLValues.insert(std::make_pair(OpaqueValue, lv));
		}

		void initRValue(const RValue &rv) {
			// Work around an extremely aggressive peephole optimization in
			// EmitScalarConversion which assumes that all other uses of a
			// value are extant.
			Protection = CGF.protectFromPeepholes(rv);
			CGF.OpaqueRValues.insert(std::make_pair(OpaqueValue, rv));
		}
	};

  /// getByrefValueFieldNumber - Given a declaration, returns the LLVM field
  /// number that holds the value.
  unsigned getByRefValueLLVMField(const ValueDefn *VD) const;

  /// BuildBlockByrefAddress - Computes address location of the
  /// variable which is declared as __block.
  llvm::Value *BuildBlockByrefAddress(llvm::Value *BaseAddr,
                                      const VarDefn *V);

private:
  CGDebugInfo *DebugInfo;
  bool DisableDebugInfo;

  /// DidCallStackSave - Whether llvm.stacksave has been called. Used to avoid
  /// calling llvm.stacksave for multiple VLAs in the same scope.
  bool DidCallStackSave;

  /// IndirectBranch - The first time an indirect goto is seen we create a block
  /// with an indirect branch.  Every time we see the address of a label taken,
  /// we add the label to the indirect goto.  Every subsequent indirect goto is
  /// codegen'd as a jump to the IndirectBranch's basic block.
  llvm::IndirectBrInst *IndirectBranch;

  /// LocalDefnMap - This keeps track of the LLVM allocas or globals for local C
  /// decls.
  typedef  llvm::DenseMap<const Defn*, llvm::Value*> DefnMapTy;
  DefnMapTy LocalDefnMap;

  /// LabelMap - This keeps track of the LLVM basic block for each C label.
  // FIXME yabin llvm::DenseMap<const LabelDecl*, JumpDest> LabelMap;

  // BreakContinueStack - This keeps track of where break and continue
  // statements should jump to.
  struct BreakContinue {
    BreakContinue(JumpDest Break, JumpDest Continue)
      : BreakBlock(Break), ContinueBlock(Continue) {}

    JumpDest BreakBlock;
    JumpDest ContinueBlock;
  };
  llvm::SmallVector<BreakContinue, 8> BreakContinueStack;

  /// SwitchInsn - This is nearest current switch instruction. It is null if if
  /// current context is not in a switch.
  llvm::SwitchInst *SwitchInsn;

  /// CaseRangeBlock - This block holds if condition check for last case
  /// statement range in current switch instruction.
  llvm::BasicBlock *CaseRangeBlock;

  /// OpaqueLValues - Keeps track of the current set of opaque value
  /// expressions.
  llvm::DenseMap<const OpaqueValueExpr *, LValue> OpaqueLValues;
  llvm::DenseMap<const OpaqueValueExpr *, RValue> OpaqueRValues;

  // VLASizeMap - This keeps track of the associated size for each VLA type.
  // We track this by the size expression rather than the type itself because
  // in certain situations, like a const qualifier applied to an VLA typedef,
  // multiple VLA types can share the same size expression.
  // FIXME: Maybe this could be a stack of maps that is pushed/popped as we
  // enter/leave scopes.
  llvm::DenseMap<const Expr*, llvm::Value*> VLASizeMap;

  /// A block containing a single 'unreachable' instruction.  Created
  /// lazily by getUnreachableBlock().
  llvm::BasicBlock *UnreachableBlock;

  /// ClassThisDefn - When generating code for a C++ member function,
  /// this will hold the implicit 'this' declaration.
  ImplicitParamDefn *ClassThisDefn;
  llvm::Value *ClassThisValue;

  /// ClassVTTDefn - When generating code for a base object constructor or
  /// base object destructor with virtual bases, this will hold the implicit
  /// VTT parameter.
  ImplicitParamDefn *ClassVTTDefn;
  llvm::Value *ClassVTTValue;

  /// OutermostConditional - Points to the outermost active
  /// conditional control.  This is used so that we know if a
  /// temporary should be destroyed conditionally.
  ConditionalEvaluation *OutermostConditional;

  /// ByrefValueInfoMap - For each __block variable, contains a pair of the LLVM
  /// type as well as the field number that contains the actual data.
  llvm::DenseMap<const ValueDefn *, std::pair<llvm::Type *,
                                              unsigned> > ByRefValueInfo;

  llvm::BasicBlock *TerminateLandingPad;
  llvm::BasicBlock *TerminateHandler;
  llvm::BasicBlock *TrapBB;

public:
  CodeGenFunction(CodeGenModule &cgm);

  CodeGenTypes &getTypes() const { return CGM.getTypes(); }
  ASTContext &getContext() const { return CGM.getContext(); }
  CGDebugInfo *getDebugInfo() {
	  if (DisableDebugInfo)
		  return NULL;
	  return DebugInfo;
  }

  void disableDebugInfo() { DisableDebugInfo = true; }
  void enableDebugInfo() { DisableDebugInfo = false; }

  bool shouldUseFusedARCCalls() {
	  return CGM.getCodeGenOpts().OptimizationLevel == 0;
  }

  const LangOptions &getLangOptions() const { return CGM.getLangOptions(); }

  /// Returns a pointer to the function's exception object slot, which
  /// is assigned in every landing pad.
  llvm::Value *getExceptionSlot();

  llvm::Value *getNormalCleanupDestSlot();
  llvm::Value *getEHCleanupDestSlot();

  llvm::BasicBlock *getUnreachableBlock() {
    if (!UnreachableBlock) {
      UnreachableBlock = createBasicBlock("unreachable");
      new llvm::UnreachableInst(getLLVMContext(), UnreachableBlock);
    }
    return UnreachableBlock;
  }

  llvm::BasicBlock *getInvokeDest() {
    if (!EHStack.requiresLandingPad()) return 0;
    return getInvokeDestImpl();
  }

  llvm::LLVMContext &getLLVMContext() {
	  return CGM.getLLVMContext();
  }

  //===--------------------------------------------------------------------===//
  //                                  Classdef
  //===--------------------------------------------------------------------===//
  void GenerateClassMethod(const ClassMethodDefn *MD);

  void StartClassMethod(const ClassMethodDefn *MD,
                       const UserClassDefn *CD);

  /// GenerateClassGetter - Synthesize an class property getter function.
  void GenerateClassGetter(UserClassDefn *IMP,
                          const ClassPropertyDefn *PD);

  /// GenerateClassSetter - Synthesize an class property setter function
  /// for the given property.
  void GenerateClassSetter(UserClassDefn *IMP,
                          const ClassPropertyDefn *PID);

  bool IndirectClassSetterArg(const CGFunctionInfo &FI);

  //===--------------------------------------------------------------------===//
  //                                  Block Bits
  //===--------------------------------------------------------------------===//
  llvm::Value *BuildBlockLiteralTmp(const BlockCmd *);
  llvm::Constant *BuildDescriptorBlockDefn(const BlockCmd *,
                                           const CGBlockInfo &Info,
                                           llvm::StructType *,
                                           llvm::Constant *BlockVarLayout);

  llvm::Function *GenerateBlockFunction(GlobalDefn GD,
                                        const CGBlockInfo &Info,
                                        const Defn *OuterFuncDefn,
                                        const DefnMapTy &ldm);

  llvm::Constant *GenerateCopyHelperFunction(const CGBlockInfo &blockInfo);
  llvm::Constant *GenerateDestroyHelperFunction(const CGBlockInfo &blockInfo);

  void BuildBlockRelease(llvm::Value *DeclPtr, BlockFieldFlags flags);

  class AutoVarEmission;

  void emitByrefStructureInit(const AutoVarEmission &emission);
  void enterByrefCleanup(const AutoVarEmission &emission);

  llvm::Value *LoadBlockStruct() {
	  assert(BlockPointer && "no block pointer set!");
	  return BlockPointer;
  }

//  void AllocateBlockClassThisPointer(const ClassThisExpr *E);
  void AllocateBlockDefn(const DefnRefExpr *E);
  llvm::Value *GetAddrOfBlockDefn(const DefnRefExpr *E) {
    return GetAddrOfBlockDefn(cast<VarDefn>(E->getDefn()), /*E->isByRef()*/false);
  }
  llvm::Value *GetAddrOfBlockDefn(const VarDefn *D, bool ByRef);
  llvm::Type *BuildByRefType(const VarDefn *D);

  void GenerateCode(GlobalDefn GD, llvm::Function *Fn);
  void StartFunction(GlobalDefn GD, Type &RetTy,
                     llvm::Function *Fn,
                     const FunctionArgList &Args,
                     SourceLocation StartLoc);

  void EmitConstructorBody(FunctionArgList &Args);
  void EmitDestructorBody(FunctionArgList &Args);
  void EmitFunctionBody(FunctionArgList &Args);

  /// EmitReturnBlock - Emit the unified return block, trying to avoid its
  /// emission when possible.
  void EmitReturnBlock();

  /// FinishFunction - Complete IR generation of the current function. It is
  /// legal to call this function even if there is no current insertion point.
  void FinishFunction(SourceLocation EndLoc=SourceLocation());

  /// GenerateThunk - Generate a thunk for the given method.
  void GenerateThunk(llvm::Function *Fn, GlobalDefn GD, const ThunkInfo &Thunk);

  void GenerateVarArgsThunk(llvm::Function *Fn, const CGFunctionInfo &FnInfo,
                            GlobalDefn GD, const ThunkInfo &Thunk);

  void EmitCtorPrologue(const ClassConstructorDefn *CD, ClassCtorType Type,
                        FunctionArgList &Args);

  /// InitializeVTablePointer - Initialize the vtable pointer of the given
  /// subobject.
  ///
//  void InitializeVTablePointer(BaseSubobject Base,
//                               const UserClassDefn *NearestVBase,
//                               uint64_t OffsetFromNearestVBase,
//                               llvm::Constant *VTable,
//                               const UserClassDefn *VTableClass);

  typedef llvm::SmallPtrSet<const UserClassDefn *, 4> VisitedVirtualBasesSetTy;
//  void InitializeVTablePointers(BaseSubobject Base,
//                                const UserClassDefn *NearestVBase,
//                                uint64_t OffsetFromNearestVBase,
//                                bool BaseIsNonVirtualPrimaryBase,
//                                llvm::Constant *VTable,
//                                const UserClassDefn *VTableClass,
//                                VisitedVirtualBasesSetTy& VBases);

//  void InitializeVTablePointers(const UserClassDefn *ClassDefn);

  /// GetVTablePtr - Return the Value of the vtable pointer member pointed
  /// to by This.
//  llvm::Value *GetVTablePtr(llvm::Value *This, llvm::Type *Ty);

  /// EnterDtorCleanups - Enter the cleanups necessary to complete the
  /// given phase of destruction for a destructor.  The end result
  /// should call destructors on members and base classes in reverse
  /// order of their construction.
  void EnterDtorCleanups(const ClassDestructorDefn *Dtor, ClassDtorType Type);

  /// ShouldInstrumentFunction - Return true if the current function should be
  /// instrumented with __cyg_profile_func_* calls
  bool ShouldInstrumentFunction();

  /// EmitFunctionInstrumentation - Emit LLVM code to call the specified
  /// instrumentation function with the current function and the call site, if
  /// function instrumentation is enabled.
  void EmitFunctionInstrumentation(const char *Fn);

  /// EmitFunctionProlog - Emit the target specific LLVM code to load the
  /// arguments for the given function. This is also responsible for naming the
  /// LLVM function arguments.
  void EmitFunctionProlog(const CGFunctionInfo &FI,
                          llvm::Function *Fn,
                          const FunctionArgList &Args);

  /// EmitFunctionEpilog - Emit the target specific LLVM code to return the
  /// given temporary.
  void EmitFunctionEpilog(const CGFunctionInfo &FI);

  /// EmitStartEHSpec - Emit the start of the exception spec.
  void EmitStartEHSpec(const Defn *D);

  /// EmitEndEHSpec - Emit the end of the exception spec.
  void EmitEndEHSpec(const Defn *D);

  /// getTerminateLandingPad - Return a landing pad that just calls terminate.
  llvm::BasicBlock *getTerminateLandingPad();

  /// getTerminateHandler - Return a handler (not a landing pad, just
  /// a catch handler) that just calls terminate.  This is used when
  /// a terminate scope encloses a try.
  llvm::BasicBlock *getTerminateHandler();

  llvm::Type *ConvertTypeForMem(Type T);
  llvm::Type *ConvertType(Type T);
  llvm::Type *ConvertType(const TypeDefn *T) {
    return ConvertType(getContext().getTypeDefnType(T));
  }

  /// hasAggregateLLVMType - Return true if the specified AST type will map into
  /// an aggregate LLVM type or is void.
  static bool hasAggregateLLVMType(Type T);

  /// createBasicBlock - Create an LLVM basic block.
  llvm::BasicBlock *createBasicBlock(llvm::StringRef Name="",
                                     llvm::Function *Parent=0,
                                     llvm::BasicBlock *InsertBefore=0) {
#ifdef NDEBUG
    return llvm::BasicBlock::Create(getLLVMContext(), "", Parent, InsertBefore);
#else
    return llvm::BasicBlock::Create(getLLVMContext(), Name, Parent, InsertBefore);
#endif
  }

  /// getBasicBlockForLabel - Return the LLVM basicblock that the specified
  /// label maps to.
  // JumpDest getJumpDestForLabel(const LabelStmt *S);

  /// SimplifyForwardingBlocks - If the given basic block is only a branch to
  /// another basic block, simplify it. This assumes that no other code could
  /// potentially reference the basic block.
  void SimplifyForwardingBlocks(llvm::BasicBlock *BB);

  /// EmitBlock - Emit the given block \arg BB and set it as the insert point,
  /// adding a fall-through branch from the current insert block if
  /// necessary. It is legal to call this function even if there is no current
  /// insertion point.
  ///
  /// IsFinished - If true, indicates that the caller has finished emitting
  /// branches to the given block and does not expect to emit code into it. This
  /// means the block can be ignored if it is unreachable.
  void EmitBlock(llvm::BasicBlock *BB, bool IsFinished=false);

  /// EmitBranch - Emit a branch to the specified basic block from the current
  /// insert block, taking care to avoid creation of branches from dummy
  /// blocks. It is legal to call this function even if there is no current
  /// insertion point.
  ///
  /// This function clears the current insertion point. The caller should follow
  /// calls to this function with calls to Emit*Block prior to generation new
  /// code.
  void EmitBranch(llvm::BasicBlock *Block);

  /// HaveInsertPoint - True if an insertion point is defined. If not, this
  /// indicates that the current code being emitted is unreachable.
  bool HaveInsertPoint() const {
    return Builder.GetInsertBlock() != 0;
  }

  /// EnsureInsertPoint - Ensure that an insertion point is defined so that
  /// emitted IR has a place to go. Note that by definition, if this function
  /// creates a block then that block is unreachable; callers may do better to
  /// detect when no insertion point is defined and simply skip IR generation.
  void EnsureInsertPoint() {
    if (!HaveInsertPoint())
      EmitBlock(createBasicBlock());
  }

  /// ErrorUnsupported - Print out an error that codegen doesn't support the
  /// specified stmt yet.
  void ErrorUnsupported(const Stmt *S, const char *Type,
                        bool OmitOnError=false);

  //===--------------------------------------------------------------------===//
  //                                  Helpers
  //===--------------------------------------------------------------------===//

  LValue MakeAddrLValue(llvm::Value *V, Type T, unsigned Alignment = 0) {
    return LValue::MakeAddr(V, T, Alignment, getContext(),
                            CGM.getTBAAInfo(T));
  }

  /// CreateTempAlloca - This creates a alloca and inserts it into the entry
  /// block. The caller is responsible for setting an appropriate alignment on
  /// the alloca.
  llvm::AllocaInst *CreateTempAlloca(llvm::Type *Ty,
                                     const llvm::Twine &Name = "tmp");

  /// InitTempAlloca - Provide an initial value for the given alloca.
  void InitTempAlloca(llvm::AllocaInst *Alloca, llvm::Value *Value);

  /// CreateIRTemp - Create a temporary IR object of the given type, with
  /// appropriate alignment. This routine should only be used when an temporary
  /// value needs to be stored into an alloca (for example, to avoid explicit
  /// PHI construction), but the type is the IR type, not the type appropriate
  /// for storing in memory.
  llvm::AllocaInst *CreateIRTemp(Type T, const llvm::Twine &Name = "tmp");

  /// CreateMemTemp - Create a temporary memory object of the given type, with
  /// appropriate alignment.
  llvm::AllocaInst *CreateMemTemp(Type T, const llvm::Twine &Name = "tmp");

  /// CreateAggTemp - Create a temporary memory object for the given
  /// aggregate type.
  AggValueSlot CreateAggTemp(Type T, const llvm::Twine &Name = "tmp") {
    return AggValueSlot::forAddr(CreateMemTemp(T, Name), false, false);
  }

  /// EvaluateExprAsBool - Perform the usual unary conversions on the specified
  /// expression and compare the result against zero, returning an Int1Ty value.
  llvm::Value *EvaluateExprAsBool(const Expr *E);

  /// EmitIgnoredExpr - Emit an expression in a context which ignores the result.
  void EmitIgnoredExpr(const Expr *E);

  /// EmitAnyExpr - Emit code to compute the specified expression which can have
  /// any type.  The result is returned as an RValue struct.  If this is an
  /// aggregate expression, the aggloc/agglocvolatile arguments indicate where
  /// the result should be returned.
  ///
  /// \param IgnoreResult - True if the resulting value isn't used.
  RValue EmitAnyExpr(const Expr *E,
                     AggValueSlot AggSlot = AggValueSlot::ignored(),
                     bool IgnoreResult = false);

  // EmitVAListRef - Emit a "reference" to a va_list; this is either the address
  // or the value of the expression, depending on how va_list is defined.
  llvm::Value *EmitVAListRef(const Expr *E);

  /// EmitAnyExprToTemp - Similary to EmitAnyExpr(), however, the result will
  /// always be accessible even if no aggregate location is provided.
  RValue EmitAnyExprToTemp(const Expr *E);

  /// EmitsAnyExprToMem - Emits the code necessary to evaluate an
  /// arbitrary expression into the given memory location.
  void EmitAnyExprToMem(const Expr *E, llvm::Value *Location,
                        bool IsLocationVolatile,
                        bool IsInitializer);

  /// EmitAggregateCopy - Emit an aggrate copy.
  ///
  /// \param isVolatile - True iff either the source or the destination is
  /// volatile.
  void EmitAggregateCopy(llvm::Value *DestPtr, llvm::Value *SrcPtr,
                         Type EltTy, bool isVolatile=false);

  /// StartBlock - Start new block named N. If insert block is a dummy block
  /// then reuse it.
  void StartBlock(const char *N);

  /// GetAddrOfStaticLocalVar - Return the address of a static local variable.
  llvm::Constant *GetAddrOfStaticLocalVar(const VarDefn *BVD) {
    return cast<llvm::Constant>(GetAddrOfLocalVar(BVD));
  }

  /// GetAddrOfLocalVar - Return the address of a local variable.
  llvm::Value *GetAddrOfLocalVar(const VarDefn *VD) {
    llvm::Value *Res = LocalDefnMap[VD];
    assert(Res && "Invalid argument to GetAddrOfLocalVar(), no decl!");
    return Res;
  }

  /// getOpaqueLValueMapping - Given an opaque value expression (which
	/// must be mapped to an l-value), return its mapping.
	const LValue &getOpaqueLValueMapping(const OpaqueValueExpr *e) {
		assert(OpaqueValueMapping::shouldBindAsLValue(e));

		llvm::DenseMap<const OpaqueValueExpr*, LValue>::iterator it =
				OpaqueLValues.find(e);
		assert(it != OpaqueLValues.end() && "no mapping for opaque value!");
		return it->second;
	}

	/// getOpaqueRValueMapping - Given an opaque value expression (which
	/// must be mapped to an r-value), return its mapping.
	const RValue &getOpaqueRValueMapping(const OpaqueValueExpr *e) {
		assert(!OpaqueValueMapping::shouldBindAsLValue(e));

		llvm::DenseMap<const OpaqueValueExpr*, RValue>::iterator it =
				OpaqueRValues.find(e);
		assert(it != OpaqueRValues.end() && "no mapping for opaque value!");
		return it->second;
	}

  /// getAccessedFieldNo - Given an encoded value and a result number, return
  /// the input field number being accessed.
  static unsigned getAccessedFieldNo(unsigned Idx, const llvm::Constant *Elts);

  llvm::BasicBlock *GetIndirectGotoBlock();

  /// EmitNullInitialization - Generate code to set a value of the given type to
  /// null, If the type contains data member pointers, they will be initialized
  /// to -1 in accordance with the Itanium C++ ABI.
  void EmitNullInitialization(llvm::Value *DestPtr, Type Ty);

  // EmitVAArg - Generate code to get an argument from the passed in pointer
  // and update it accordingly. The return value is a pointer to the argument.
  // FIXME: We should be able to get rid of this method and use the va_arg
  // instruction in LLVM instead once it works well enough.
  llvm::Value *EmitVAArg(llvm::Value *VAListAddr, Type Ty);

  /// EmitVLASize - Generate code for any VLA size expressions that might occur
  /// in a variably modified type. If Ty is a VLA, will return the value that
  /// corresponds to the size in bytes of the VLA type. Will return 0 otherwise.
  ///
  /// This function can be called with a null (unreachable) insert point.
  // llvm::Value *EmitVLASize(Type Ty);

  // GetVLASize - Returns an LLVM value that corresponds to the size in bytes
  // of a variable length array type.
  // llvm::Value *GetVLASize(const VariableArrayType *);

  /// LoadClassThis - Load the value of 'this'. This function is only valid while
  /// generating code for an C++ member function.
  llvm::Value *LoadClassThis() {
    assert(ClassThisValue && "no 'this' value for this function");
    return ClassThisValue;
  }

  /// LoadClassVTT - Load the VTT parameter to base constructors/destructors have
  /// virtual bases.
  llvm::Value *LoadClassVTT() {
    assert(ClassVTTValue && "no VTT value for this function");
    return ClassVTTValue;
  }

  /// GetAddressOfBaseOfCompleteClass - Convert the given pointer to a
  /// complete class to the given direct base.
//  llvm::Value *
//  GetAddressOfDirectBaseInCompleteClass(llvm::Value *Value,
//                                        const UserClassDefn *Derived,
//                                        const UserClassDefn *Base,
//                                        bool BaseIsVirtual);
//
//  /// GetAddressOfBaseClass - This function will add the necessary delta to the
//  /// load of 'this' and returns address of the base class.
//  llvm::Value *GetAddressOfBaseClass(llvm::Value *Value,
//                                     const UserClassDefn *Derived,
//                                     CastExpr::path_const_iterator PathBegin,
//                                     CastExpr::path_const_iterator PathEnd,
//                                     bool NullCheckValue);
//
//  llvm::Value *GetAddressOfDerivedClass(llvm::Value *Value,
//                                        const UserClassDefn *Derived,
//                                        CastExpr::path_const_iterator PathBegin,
//                                        CastExpr::path_const_iterator PathEnd,
//                                        bool NullCheckValue);
//
//  llvm::Value *GetVirtualBaseClassOffset(llvm::Value *This,
//                                         const UserClassDefn *ClassDefn,
//                                         const UserClassDefn *BaseClassDefn);

  void EmitDelegateClassConstructorCall(const ClassConstructorDefn *Ctor,
                                      ClassCtorType CtorType,
                                      const FunctionArgList &Args);
  void EmitClassConstructorCall(const ClassConstructorDefn *D, ClassCtorType Type,
                              bool ForVirtualBase, llvm::Value *This,
                              FunctionCall::const_arg_iterator ArgBeg,
                              FunctionCall::const_arg_iterator ArgEnd);
  
  void EmitSynthesizedClassCopyCtorCall(const ClassConstructorDefn *D,
                              llvm::Value *This, llvm::Value *Src,
                              FunctionCall::const_arg_iterator ArgBeg,
                              FunctionCall::const_arg_iterator ArgEnd);

//  void EmitClassAggrConstructorCall(const ClassConstructorDefn *D,
//                                  const ConstantArrayType *ArrayTy,
//                                  llvm::Value *ArrayPtr,
//                                  FunctionCall::const_arg_iterator ArgBeg,
//                                  FunctionCall::const_arg_iterator ArgEnd,
//                                  bool ZeroInitialization = false);

  void EmitClassAggrConstructorCall(const ClassConstructorDefn *D,
                                  llvm::Value *NumElements,
                                  llvm::Value *ArrayPtr,
                                  FunctionCall::const_arg_iterator ArgBeg,
                                  FunctionCall::const_arg_iterator ArgEnd,
                                  bool ZeroInitialization = false);

  void EmitClassAggrDestructorCall(const ClassDestructorDefn *D,
                                 const ArrayType *Array,
                                 llvm::Value *This);

  void EmitClassAggrDestructorCall(const ClassDestructorDefn *D,
                                 llvm::Value *NumElements,
                                 llvm::Value *This);

  llvm::Function *GenerateClassAggrDestructorHelper(const ClassDestructorDefn *D,
                                                  const ArrayType *Array,
                                                  llvm::Value *This);

  void EmitClassDestructorCall(const ClassDestructorDefn *D, ClassDtorType Type,
                             bool ForVirtualBase, llvm::Value *This);

//  void EmitNewArrayInitializer(const ClassNewExpr *E, llvm::Value *NewPtr,
//                               llvm::Value *NumElements);

//  void EmitClassTemporary(const ClassTemporary *Temporary, llvm::Value *Ptr);

//  llvm::Value *EmitClassNewExpr(const ClassNewExpr *E);
//  void EmitClassDeleteExpr(const ClassDeleteExpr *E);

//  void EmitDeleteCall(const FunctionDefn *DeleteFD, llvm::Value *Ptr,
//                      Type DeleteTy);

//  llvm::Value* EmitClassTypeidExpr(const ClassTypeidExpr *E);
//  llvm::Value *EmitDynamicCast(llvm::Value *V, const ClassDynamicCastExpr *DCE);

  void EmitCheck(llvm::Value *, unsigned Size);

  llvm::Value *EmitScalarPrePostIncDec(const UnaryOperator *E, LValue LV,
                                       bool isInc, bool isPre);
  ComplexPairTy EmitComplexPrePostIncDec(const UnaryOperator *E, LValue LV,
                                         bool isInc, bool isPre);
  //===--------------------------------------------------------------------===//
  //                            Declaration Emission
  //===--------------------------------------------------------------------===//

  /// EmitDefn - Emit a declaration.
  ///
  /// This function can be called with a null (unreachable) insert point.
  void EmitDefn(const Defn &D);

  /// EmitVarDefn - Emit a local variable declaration.
  ///
  /// This function can be called with a null (unreachable) insert point.
  void EmitVarDefn(const VarDefn &D);

  void EmitScalarInit(const Expr *init, const ValueDefn *D,
                      LValue lvalue, bool capturedByInit);
  void EmitScalarInit(llvm::Value *init, LValue lvalue);

  typedef void SpecialInitFn(CodeGenFunction &Init, const VarDefn &D,
                             llvm::Value *Address);
  /// EmitAutoVarDefn - Emit an auto variable declaration.
	///
	/// This function can be called with a null (unreachable) insert point.
	void EmitAutoVarDefn(const VarDefn &D);

	class AutoVarEmission {
		friend class CodeGenFunction;

		const VarDefn *Variable;

		/// The alignment of the variable.
		CharUnits Alignment;

		/// The address of the alloca.  Null if the variable was emitted
		/// as a global constant.
		llvm::Value *Address;

		llvm::Value *NRVOFlag;

		/// True if the variable is a __block variable.
		bool IsByRef;

		/// True if the variable is of aggregate type and has a constant
		/// initializer.
		bool IsConstantAggregate;

		struct Invalid {
		};
		AutoVarEmission(Invalid) :
			Variable(0) {
		}

		AutoVarEmission(const VarDefn &variable) :
			Variable(&variable), Address(0), NRVOFlag(0), IsByRef(false),
					IsConstantAggregate(false) {
		}

		bool wasEmittedAsGlobal() const {
			return Address == 0;
		}

	public:
		static AutoVarEmission invalid() {
			return AutoVarEmission(Invalid());
		}

		/// Returns the address of the object within this declaration.
		/// Note that this does not chase the forwarding pointer for
		/// __block decls.
		llvm::Value *getObjectAddress(CodeGenFunction &CGF) const {
			if (!IsByRef)
				return Address;

			return CGF.Builder.CreateStructGEP(Address,
					CGF.getByRefValueLLVMField(Variable),
					Variable->getNameAsString());
		}
	};
	AutoVarEmission EmitAutoVarAlloca(const VarDefn &var);
	void EmitAutoVarInit(const AutoVarEmission &emission);
	void EmitAutoVarCleanups(const AutoVarEmission &emission);

  void EmitStaticVarDefn(const VarDefn &D,
                         llvm::GlobalValue::LinkageTypes Linkage);

  /// EmitParmDefn - Emit a ParamVarDefn or an ImplicitParamDefn.
  void EmitParmDefn(const VarDefn &D, llvm::Value *Arg, unsigned ArgNo);

  void EmitExprAsInit(const Expr *init, const ValueDefn *D,
                      LValue lvalue, bool capturedByInit);
  /// protectFromPeepholes - Protect a value that we're intending to
  /// store to the side, but which will probably be used later, from
  /// aggressive peepholing optimizations that might delete it.
  ///
  /// Pass the result to unprotectFromPeepholes to declare that
  /// protection is no longer required.
  ///
  /// There's no particular reason why this shouldn't apply to
  /// l-values, it's just that no existing peepholes work on pointers.
  PeepholeProtection protectFromPeepholes(RValue rvalue);
  void unprotectFromPeepholes(PeepholeProtection protection);

  //===--------------------------------------------------------------------===//
  //                             Statement Emission
  //===--------------------------------------------------------------------===//

  /// EmitStopPoint - Emit a debug stoppoint if we are emitting debug info.
  void EmitStopPoint(const Stmt *S);

  /// EmitStmt - Emit the code for the statement \arg S. It is legal to call
  /// this function even if there is no current insertion point.
  ///
  /// This function may clear the current insertion point; callers should use
  /// EnsureInsertPoint if they wish to subsequently generate code without first
  /// calling EmitBlock, EmitBranch, or EmitStmt.
  void EmitStmt(const Stmt *S);

  /// EmitSimpleStmt - Try to emit a "simple" statement which does not
  /// necessarily require an insertion point or debug information; typically
  /// because the statement amounts to a jump or a container of other
  /// statements.
  ///
  /// \return True if the statement was handled.
  bool EmitSimpleStmt(const Stmt *S);

  RValue EmitBlockCmd(const BlockCmd &S, bool GetLast = false,
                       AggValueSlot AVS = AggValueSlot::ignored());

  void EmitIfCmd(const IfCmd &S);
  void EmitWhileCmd(const WhileCmd &S);
  void EmitForCmd(const ForCmd &S);
  void EmitReturnCmd(const ReturnCmd &S);
  void EmitDefnCmd(const DefnCmd &S);
  void EmitBreakCmd(const BreakCmd &S);
  void EmitContinueCmd(const ContinueCmd &S);
  void EmitSwitchCmd(const SwitchCmd &S);
  void EmitOtherwiseCmd(const OtherwiseCmd &S);
  void EmitCaseCmd(const CaseCmd &S);
  void EmitCaseCmdRange(const CaseCmd &S);

  llvm::Constant *getUnwindResumeOrRethrowFn();
  void EnterTryCmd(const TryCmd &S, bool IsFnTryBlock = false);
  void ExitTryCmd(const TryCmd &S, bool IsFnTryBlock = false);

  void EmitTryCmd(const TryCmd &S);

  //===--------------------------------------------------------------------===//
  //                         LValue Expression Emission
  //===--------------------------------------------------------------------===//

  /// GetUndefRValue - Get an appropriate 'undef' rvalue for the given type.
  RValue GetUndefRValue(Type Ty);

  /// EmitUnsupportedRValue - Emit a dummy r-value using the type of E
  /// and issue an ErrorUnsupported style diagnostic (using the
  /// provided Name).
  RValue EmitUnsupportedRValue(const Expr *E,
                               const char *Name);

  /// EmitUnsupportedLValue - Emit a dummy l-value using the type of E and issue
  /// an ErrorUnsupported style diagnostic (using the provided Name).
  LValue EmitUnsupportedLValue(const Expr *E,
                               const char *Name);

  /// EmitLValue - Emit code to compute a designator that specifies the location
  /// of the expression.
  ///
  /// This can return one of two things: a simple address or a bitfield
  /// reference.  In either case, the LLVM Value* in the LValue structure is
  /// guaranteed to be an LLVM pointer type.
  ///
  /// If this returns a bitfield reference, nothing about the pointee type of
  /// the LLVM value is known: For example, it may not be a pointer to an
  /// integer.
  ///
  /// If this returns a normal address, and if the lvalue's C type is fixed
  /// size, this method guarantees that the returned pointer type will point to
  /// an LLVM type of the same size of the lvalue's type.  If the lvalue has a
  /// variable length type, this is not possible.
  ///
  LValue EmitLValue(const Expr *E);

  /// EmitCheckedLValue - Same as EmitLValue but additionally we generate
  /// checking code to guard against undefined behavior.  This is only
  /// suitable when we know that the address will be used to access the
  /// object.
  LValue EmitCheckedLValue(const Expr *E);

  /// EmitToMemory - Change a scalar value from its value
  /// representation to its in-memory representation.
  llvm::Value *EmitToMemory(llvm::Value *Value, Type Ty);

  /// EmitFromMemory - Change a scalar value from its memory
  /// representation to its value representation.
  llvm::Value *EmitFromMemory(llvm::Value *Value, Type Ty);

  /// EmitLoadOfScalar - Load a scalar value from an address, taking
  /// care to appropriately convert from the memory representation to
  /// the LLVM value representation.
  llvm::Value *EmitLoadOfScalar(llvm::Value *Addr, bool Volatile,
                                unsigned Alignment, Type Ty,
                                llvm::MDNode *TBAAInfo = 0);

  /// EmitStoreOfScalar - Store a scalar value to an address, taking
  /// care to appropriately convert from the memory representation to
  /// the LLVM value representation.
  void EmitStoreOfScalar(llvm::Value *Value, llvm::Value *Addr,
                         bool Volatile, unsigned Alignment, Type Ty,
                         llvm::MDNode *TBAAInfo = 0);

  /// EmitStoreOfScalar - Store a scalar value to an address, taking
  /// care to appropriately convert from the memory representation to
  /// the LLVM value representation.  The l-value must be a simple
  /// l-value.
  void EmitStoreOfScalar(llvm::Value *value, LValue lvalue);

  /// EmitLoadOfLValue - Given an expression that represents a value lvalue,
  /// this method emits the address of the lvalue, then loads the result as an
  /// rvalue, returning the rvalue.
  RValue EmitLoadOfLValue(LValue V, Type LVType);
  RValue EmitLoadOfExtVectorElementLValue(LValue V, Type LVType);
  RValue EmitLoadOfPropertyRefLValue(LValue LV,
                                 ReturnValueSlot Return = ReturnValueSlot());

  /// EmitStoreThroughLValue - Store the specified rvalue into the specified
  /// lvalue, where both are guaranteed to the have the same type, and that type
  /// is 'Ty'.
  void EmitStoreThroughLValue(RValue Src, LValue Dst);
  void EmitStoreThroughExtVectorComponentLValue(RValue Src, LValue Dst);
  void EmitStoreThroughPropertyRefLValue(RValue Src, LValue Dst);

  /// Emit an l-value for an assignment (simple or compound) of complex type.
  LValue EmitComplexAssignmentLValue(const BinaryOperator *E);
  LValue EmitComplexCompoundAssignmentLValue(const CompoundAssignOperator *E);

  // Note: only availabe for agg return types
  LValue EmitBinaryOperatorLValue(const BinaryOperator *E);
  LValue EmitCompoundAssignmentLValue(const CompoundAssignOperator *E);
  // Note: only available for agg return types
  LValue EmitFunctionCallLValue(const FunctionCall *E);
  // Note: only available for agg return types
  LValue EmitDefnRefLValue(const DefnRefExpr *E);
  LValue EmitStringLiteralLValue(const StringLiteral *E);
  LValue EmitUnaryOpLValue(const UnaryOperator *E);
  LValue EmitArrayIndexExpr(const ArrayIndex *E);
//  LValue EmitExtVectorElementExpr(const ExtVectorElementExpr *E);
  LValue EmitMemberExpr(const MemberExpr *E);
  LValue EmitImaginaryLiteralLValue(const ImaginaryLiteral *E);
//  LValue EmitCastLValue(const CastExpr *E);
//  LValue EmitNullInitializationLValue(const ClassScalarValueInitExpr *E);
//  LValue EmitLValueForAnonRecordField(llvm::Value* Base,
//                                      const IndirectFieldDefn* Field,
//                                      unsigned CVRQualifiers);
  LValue EmitLValueForField(llvm::Value* Base, const MemberDefn* Field,
                            unsigned CVRQualifiers);

  /// EmitLValueForFieldInitialization - Like EmitLValueForField, except that
  /// if the Field is a reference, this will return the address of the reference
  /// and not the address of the value stored in the reference.
  LValue EmitLValueForFieldInitialization(llvm::Value* Base,
                                          const MemberDefn* Field,
                                          unsigned CVRQualifiers);

  LValue EmitBlockDefnRefLValue(const DefnRefExpr *E);

//  LValue EmitClassConstructLValue(const ClassConstructExpr *E);
//  LValue EmitClassBindTemporaryLValue(const ClassBindTemporaryExpr *E);
//  LValue EmitExprWithCleanupsLValue(const ExprWithCleanups *E);
//  LValue EmitClassTypeidLValue(const ClassTypeidExpr *E);

  // LValue EmitStmtExprLValue(const StmtExpr *E);
//  LValue EmitPointerToDataMemberBinaryExpr(const BinaryOperator *E);
  void   EmitDefnRefExprDbgValue(const DefnRefExpr *E, llvm::Constant *Init);
  //===--------------------------------------------------------------------===//
  //                         Scalar Expression Emission
  //===--------------------------------------------------------------------===//

  /// EmitCall - Generate a call of the given function, expecting the given
  /// result type, and using the given argument list which specifies both the
  /// LLVM arguments and the types they were derived from.
  ///
  /// \param TargetDefn - If given, the decl of the function in a direct call;
  /// used to set attributes on the call (noreturn, etc.).
  RValue EmitCall(const CGFunctionInfo &FnInfo,
                  llvm::Value *Callee,
                  ReturnValueSlot ReturnValue,
                  const CallArgList &Args,
                  const Defn *TargetDefn = 0,
                  llvm::Instruction **callOrInvoke = 0);

  RValue EmitCall(Type FnType, llvm::Value *Callee,
                  ReturnValueSlot ReturnValue,
                  FunctionCall::const_arg_iterator ArgBeg,
                  FunctionCall::const_arg_iterator ArgEnd,
                  const Defn *TargetDefn = 0);
  RValue EmitFunctionCall(const FunctionCall *E,
                      ReturnValueSlot ReturnValue = ReturnValueSlot());

  llvm::CallSite EmitCallOrInvoke(llvm::Value *Callee,
		                              llvm::ArrayRef<llvm::Value *> Args,
                                  const llvm::Twine &Name = "");

  llvm::Value *BuildVirtualCall(const ClassMethodDefn *MD, llvm::Value *This,
                                llvm::Type *Ty);
  llvm::Value *BuildVirtualCall(const ClassDestructorDefn *DD, ClassDtorType Type,
                                llvm::Value *This, llvm::Type *Ty);

  RValue EmitClassMemberCall(const ClassMethodDefn *MD,
                           llvm::Value *Callee,
                           ReturnValueSlot ReturnValue,
                           llvm::Value *This,
                           llvm::Value *VTT,
                           FunctionCall::const_arg_iterator ArgBeg,
                           FunctionCall::const_arg_iterator ArgEnd);
//  RValue EmitClassMemberCallExpr(const ClassMemberCallExpr *E,
//                               ReturnValueSlot ReturnValue);
//  RValue EmitClassMemberPointerCallExpr(const ClassMemberCallExpr *E,
//                                      ReturnValueSlot ReturnValue);
//
//  RValue EmitClassOperatorMemberCallExpr(const ClassOperatorCallExpr *E,
//                                       const ClassMethodDefn *MD,
//                                       ReturnValueSlot ReturnValue);


  RValue EmitBuiltinExpr(const FunctionDefn *FD,
                         unsigned BuiltinID, const FunctionCall *E);

  RValue EmitBlockCallExpr(const FunctionCall *E, ReturnValueSlot ReturnValue);

  /// EmitTargetBuiltinExpr - Emit the given builtin call. Returns 0 if the call
  /// is unhandled by the current target.
  llvm::Value *EmitTargetBuiltinExpr(unsigned BuiltinID, const FunctionCall *E);

  llvm::Value *EmitARMBuiltinExpr(unsigned BuiltinID, const FunctionCall *E);
  llvm::Value *EmitNeonCall(llvm::Function *F,
                            llvm::SmallVectorImpl<llvm::Value*> &O,
                            const char *name,
                            unsigned shift = 0, bool rightshift = false);
  llvm::Value *EmitNeonSplat(llvm::Value *V, llvm::Constant *Idx);
  llvm::Value *EmitNeonShiftVector(llvm::Value *V, llvm::Type *Ty,
                                   bool negateForRightShift);

  llvm::Value *BuildVector(const llvm::SmallVectorImpl<llvm::Value*> &Ops);
  llvm::Value *EmitX86BuiltinExpr(unsigned BuiltinID, const FunctionCall *E);
  llvm::Value *EmitPPCBuiltinExpr(unsigned BuiltinID, const FunctionCall *E);

  /// EmitReferenceBindingToExpr - Emits a reference binding to the passed in
  /// expression. Will emit a temporary variable if E is not an LValue.
  RValue EmitReferenceBindingToExpr(const Expr* E,
                                    const NamedDefn *InitializedDefn);

  //===--------------------------------------------------------------------===//
  //                           Expression Emission
  //===--------------------------------------------------------------------===//

  // Expressions are broken into three classes: scalar, complex, aggregate.

  /// EmitScalarExpr - Emit the computation of the specified expression of LLVM
  /// scalar type, returning the result.
  llvm::Value *EmitScalarExpr(const Expr *E , bool IgnoreResultAssign = false);

  /// EmitScalarConversion - Emit a conversion from the specified type to the
  /// specified destination type, both of which are LLVM scalar types.
  llvm::Value *EmitScalarConversion(llvm::Value *Src, Type SrcTy,
                                    Type DstTy);

  /// EmitComplexToScalarConversion - Emit a conversion from the specified
  /// complex type to the specified destination type, where the destination type
  /// is an LLVM scalar type.
  llvm::Value *EmitComplexToScalarConversion(ComplexPairTy Src, Type SrcTy,
                                             Type DstTy);


  /// EmitAggExpr - Emit the computation of the specified expression
  /// of aggregate type.  The result is computed into the given slot,
  /// which may be null to indicate that the value is not needed.
  void EmitAggExpr(const Expr *E, AggValueSlot AS, bool IgnoreResult = false);

  /// EmitAggExprToLValue - Emit the computation of the specified expression of
  /// aggregate type into a temporary LValue.
  LValue EmitAggExprToLValue(const Expr *E);

  /// EmitGCMemmoveCollectable - Emit special API for structs with object
  /// pointers.
  void EmitGCMemmoveCollectable(llvm::Value *DestPtr, llvm::Value *SrcPtr,
                                Type Ty);

  /// EmitComplexExpr - Emit the computation of the specified expression of
  /// complex type, returning the result.
  ComplexPairTy EmitComplexExpr(const Expr *E,
                                bool IgnoreReal = false,
                                bool IgnoreImag = false);

  /// EmitComplexExprIntoAddr - Emit the computation of the specified expression
  /// of complex type, storing into the specified Value*.
  void EmitComplexExprIntoAddr(const Expr *E, llvm::Value *DestAddr,
                               bool DestIsVolatile);

  /// StoreComplexToAddr - Store a complex number into the specified address.
  void StoreComplexToAddr(ComplexPairTy V, llvm::Value *DestAddr,
                          bool DestIsVolatile);
  /// LoadComplexFromAddr - Load a complex number from the specified address.
  ComplexPairTy LoadComplexFromAddr(llvm::Value *SrcAddr, bool SrcIsVolatile);

  /// CreateStaticVarDefn - Create a zero-initialized LLVM global for
  /// a static local variable.
  llvm::GlobalVariable *CreateStaticVarDefn(const VarDefn &D,
                                            const char *Separator,
                                       llvm::GlobalValue::LinkageTypes Linkage);

  /// AddInitializerToStaticVarDefn - Add the initializer for 'D' to the
  /// global variable that has already been created for it.  If the initializer
  /// has a different type than GV does, this may free GV and return a different
  /// one.  Otherwise it just returns GV.
  llvm::GlobalVariable *
  AddInitializerToStaticVarDefn(const VarDefn &D,
                                llvm::GlobalVariable *GV);


  /// EmitClassGlobalVarDefnInit - Create the initializer for a C++
  /// variable with global storage.
  void EmitClassGlobalVarDefnInit(const VarDefn &D, llvm::Constant *DefnPtr);

  /// EmitClassGlobalDtorRegistration - Emits a call to register the global ptr
  /// with the C++ runtime so that its destructor will be called at exit.
  void EmitClassGlobalDtorRegistration(llvm::Constant *DtorFn,
                                     llvm::Constant *DefnPtr);

  /// Emit code in this function to perform a guarded variable
  /// initialization.  Guarded initializations are used when it's not
  /// possible to prove that an initialization will be done exactly
  /// once, e.g. with a static local variable or a static data member
  /// of a class template.
  void EmitClassGuardedInit(const VarDefn &D, llvm::GlobalVariable *DefnPtr);

  /// GenerateClassGlobalInitFunc - Generates code for initializing global
  /// variables.
  void GenerateClassGlobalInitFunc(llvm::Function *Fn,
                                 llvm::Constant **Defns,
                                 unsigned NumDefns);

  /// GenerateClassGlobalDtorFunc - Generates code for destroying global
  /// variables.
  void GenerateClassGlobalDtorFunc(llvm::Function *Fn,
                                 const std::vector<std::pair<llvm::WeakVH,
                                   llvm::Constant*> > &DtorsAndObjects);

  void GenerateClassGlobalVarDefnInitFunc(llvm::Function *Fn, const VarDefn *D,
                                        llvm::GlobalVariable *Addr);

//  void EmitClassConstructExpr(const ClassConstructExpr *E, AggValueSlot Dest);
  
  void EmitSynthesizedClassCopyCtor(llvm::Value *Dest, llvm::Value *Src,
                                  const Expr *Exp);

//  RValue EmitExprWithCleanups(const ExprWithCleanups *E,
//                              AggValueSlot Slot =AggValueSlot::ignored());
//
//  void EmitClassThrowExpr(const ClassThrowExpr *E);

  //===--------------------------------------------------------------------===//
  //                             Internal Helpers
  //===--------------------------------------------------------------------===//

  /// ConstantFoldsToSimpleInteger - If the specified expression does not fold
  /// to a constant, or if it does but contains a label, return 0.  If it
  /// constant folds to 'true' and does not contain a label, return 1, if it
  /// constant folds to 'false' and does not contain a label, return -1.
  bool ConstantFoldsToSimpleInteger(const Expr *Cond, bool &ResultBool);
  bool ConstantFoldsToSimpleInteger(const Expr *Cond, llvm::APInt &ResultInt);

  /// EmitBranchOnBoolExpr - Emit a branch on a boolean condition (e.g. for an
  /// if statement) to the specified blocks.  Based on the condition, this might
  /// try to simplify the codegen of the conditional based on the branch.
  void EmitBranchOnBoolExpr(const Expr *Cond, llvm::BasicBlock *TrueBlock,
                            llvm::BasicBlock *FalseBlock);

  /// getTrapBB - Create a basic block that will call the trap intrinsic.  We'll
  /// generate a branch around the created basic block as necessary.
  llvm::BasicBlock *getTrapBB();

  /// EmitCallArg - Emit a single call argument.
  void EmitCallArg(CallArgList &args, const Expr *E, Type ArgType);

  /// EmitDelegateCallArg - We are performing a delegate call; that
  /// is, the current function is delegating to another one.  Produce
  /// a r-value suitable for passing the given parameter.
  void EmitDelegateCallArg(CallArgList &args, const VarDefn *Param);

private:
  void EmitReturnOfRValue(RValue RV, Type Ty);

  /// ExpandTypeFromArgs - Reconstruct a structure of type \arg Ty
  /// from function arguments into \arg Dst. See ABIArgInfo::Expand.
  ///
  /// \param AI - The first function argument of the expansion.
  /// \return The argument following the last expanded function
  /// argument.
  llvm::Function::arg_iterator
  ExpandTypeFromArgs(Type Ty, LValue Dst,
                     llvm::Function::arg_iterator AI);

  /// ExpandTypeToArgs - Expand an RValue \arg Src, with the LLVM type for \arg
  /// Ty, into individual arguments on the provided vector \arg Args. See
  /// ABIArgInfo::Expand.
  void ExpandTypeToArgs(Type Ty, RValue Src,
                        llvm::SmallVector<llvm::Value*, 16> &Args);

  // EmitCallArgs - Emit call arguments for a function.
  // The CallArgTypeInfo parameter is used for iterating over the known
  // argument types of the function being called.
  template<typename T>
  void EmitCallArgs(CallArgList& Args, const T* CallArgTypeInfo,
                    FunctionCall::const_arg_iterator ArgBeg,
                    FunctionCall::const_arg_iterator ArgEnd) {
      FunctionCall::const_arg_iterator Arg = ArgBeg;

    // First, use the argument types that the type info knows about
    if (CallArgTypeInfo) {
      for (typename T::arg_type_iterator I = CallArgTypeInfo->arg_type_begin(),
           E = CallArgTypeInfo->arg_type_end(); I != E; ++I, ++Arg) {
        assert(Arg != ArgEnd && "Running over edge of argument list!");
        Type ArgType = *I;
        EmitCallArg(Args, *Arg, ArgType);
      }

      // Either we've emitted all the call args, or we have a call to a
      // variadic function.
      assert((Arg == ArgEnd || CallArgTypeInfo->isVariadic()) &&
             "Extra arguments in non-variadic function!");

    }

    // If we still have any arguments, emit them using the type of the argument.
    for (; Arg != ArgEnd; ++Arg) {
    	EmitCallArg(Args, *Arg, Arg->getType());
    }
  }

  const TargetCodeGenInfo &getTargetHooks() const {
    return CGM.getTargetCodeGenInfo();
  }

  void EmitDefnMetadata();

  CodeGenModule::ByrefHelpers *
  buildByrefHelpers(llvm::StructType &byrefType,
                    const AutoVarEmission &emission);
};

/// Helper class with most of the code for saving a value for a
/// conditional expression cleanup.
struct DominatingLLVMValue {
  typedef llvm::PointerIntPair<llvm::Value*, 1, bool> saved_type;

  /// Answer whether the given value needs extra work to be saved.
  static bool needsSaving(llvm::Value *value) {
    // If it's not an instruction, we don't need to save.
    if (!isa<llvm::Instruction>(value)) return false;

    // If it's an instruction in the entry block, we don't need to save.
    llvm::BasicBlock *block = cast<llvm::Instruction>(value)->getParent();
    return (block != &block->getParent()->getEntryBlock());
  }

  /// Try to save the given value.
  static saved_type save(CodeGenFunction &CGF, llvm::Value *value) {
    if (!needsSaving(value)) return saved_type(value, false);

    // Otherwise we need an alloca.
    llvm::Value *alloca =
      CGF.CreateTempAlloca(value->getType(), "cond-cleanup.save");
    CGF.Builder.CreateStore(value, alloca);

    return saved_type(alloca, true);
  }

  static llvm::Value *restore(CodeGenFunction &CGF, saved_type value) {
    if (!value.getInt()) return value.getPointer();
    return CGF.Builder.CreateLoad(value.getPointer());
  }
};

/// A partial specialization of DominatingValue for llvm::Values that
/// might be llvm::Instructions.
template <class T> struct DominatingPointer<T,true> : DominatingLLVMValue {
  typedef T *type;
  static type restore(CodeGenFunction &CGF, saved_type value) {
    return static_cast<T*>(DominatingLLVMValue::restore(CGF, value));
  }
};

/// A specialization of DominatingValue for RValue.
template <> struct DominatingValue<RValue> {
  typedef RValue type;
  class saved_type {
    enum Kind { ScalarLiteral, ScalarAddress, AggregateLiteral,
                AggregateAddress, ComplexAddress };

    llvm::Value *Value;
    Kind K;
    saved_type(llvm::Value *v, Kind k) : Value(v), K(k) {}

  public:
    static bool needsSaving(RValue value);
    static saved_type save(CodeGenFunction &CGF, RValue value);
    RValue restore(CodeGenFunction &CGF);

    // implementations in CGExprCXX.cpp
  };

  static bool needsSaving(type value) {
    return saved_type::needsSaving(value);
  }
  static saved_type save(CodeGenFunction &CGF, type value) {
    return saved_type::save(CGF, value);
  }
  static type restore(CodeGenFunction &CGF, saved_type value) {
    return value.restore(CGF);
  }
};

}  // end namespace CodeGen
}  // end namespace mlang

#endif /* MLANG_CODEGEN_CODEGENFUNCTION_H_ */
