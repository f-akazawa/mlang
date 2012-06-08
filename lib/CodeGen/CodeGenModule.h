//===--- CodeGenModule.h - Per-Module state for LLVM CodeGen ----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This is the internal per-translation-unit state used for llvm translation.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CODEGENMODULE_H_
#define MLANG_CODEGEN_CODEGENMODULE_H_

#include "mlang/Basic/LangOptions.h"
#include "clang/AST/Attr.h"
#include "mlang/AST/CharUnits.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/GlobalDefn.h"
#include "mlang/AST/Mangle.h"
#include "mlang/Basic/ABI.h"
#include "CGVTables.h"
#include "CodeGenTypes.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/ValueHandle.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {
  class Module;
  class Constant;
  class ConstantInt;
  class Function;
  class GlobalValue;
  class TargetData;
  class FunctionType;
  class LLVMContext;
}

namespace mlang {
  class TargetCodeGenInfo;
  class ASTContext;
  class FunctionDefn;
  class IdentifierInfo;
  class BlockCmd;
  class CharUnits;
  class Defn;
  class Expr;
  class MangleBuffer;
  class Stmt;
  class StringLiteral;
  class NamedDefn;
  class UnaryOperator;
  class ValueDefn;
  class VarDefn;
  class LangOptions;
  class CodeGenOptions;
  class DiagnosticsEngine;
//  class AnnotateAttr;
  class ClassDestructorDefn;

namespace CodeGen {

  class CodeGenFunction;
  class CodeGenTBAA;
  class CGOOPABI;
  class CGDebugInfo;


struct OrderGlobalInits {
  unsigned int priority;
  unsigned int lex_order;
  OrderGlobalInits(unsigned int p, unsigned int l) :
    priority(p), lex_order(l) {
  }

  bool operator==(const OrderGlobalInits &RHS) const {
    return priority == RHS.priority && lex_order == RHS.lex_order;
  }

  bool operator<(const OrderGlobalInits &RHS) const {
    if (priority < RHS.priority)
      return true;

    return priority == RHS.priority && lex_order < RHS.lex_order;
  }
};

struct CodeGenTypeCache {
  /// void
  llvm::Type *VoidTy;

  /// i8, i32, and i64
  llvm::IntegerType *Int8Ty, *Int32Ty, *Int64Ty;

  /// int
  llvm::IntegerType *IntTy;

  /// intptr_t, size_t, and ptrdiff_t, which we assume are the same size.
  union {
    llvm::IntegerType *IntPtrTy;
    llvm::IntegerType *SizeTy;
    llvm::IntegerType *PtrDiffTy;
  };

	/// void* in address space 0
	union {
		llvm::PointerType *VoidPtrTy;
		llvm::PointerType *Int8PtrTy;
	};

	/// void** in address space 0
	union {
		llvm::PointerType *VoidPtrPtrTy;
		llvm::PointerType *Int8PtrPtrTy;
	};

	/// The width of a pointer into the generic address space.
	unsigned char PointerWidthInBits;

	/// The alignment of a pointer into the generic address space.
	unsigned char PointerAlignInBytes;
};

/// CodeGenModule - This class organizes the cross-function state that is used
/// while generating LLVM code.
class CodeGenModule : public CodeGenTypeCache {
  CodeGenModule(const CodeGenModule&);  // DO NOT IMPLEMENT
  void operator=(const CodeGenModule&); // DO NOT IMPLEMENT

  typedef std::vector<std::pair<llvm::Constant*, int> > CtorList;

  ASTContext &Context;
  const LangOptions &Features;
  const CodeGenOptions &CodeGenOpts;
  llvm::Module &TheModule;
  const llvm::TargetData &TheTargetData;
  mutable const TargetCodeGenInfo *TheTargetCodeGenInfo;
  DiagnosticsEngine &Diags;
  CGOOPABI &ABI;
  CodeGenTypes Types;
  CodeGenTBAA *TBAA;

  /// VTables - Holds information about C++ vtables.
  CodeGenVTables VTables;
  friend class CodeGenVTables;

  CGDebugInfo* DebugInfo;

  // WeakRefReferences - A set of references that have only been seen via
  // a weakref so far. This is used to remove the weak of the reference if we ever
  // see a direct reference or a definition.
  llvm::SmallPtrSet<llvm::GlobalValue*, 10> WeakRefReferences;

  /// DeferredDefns - This contains all the decls which have definitions but
  /// which are deferred for emission and therefore should only be output if
  /// they are actually used.  If a decl is in this, then it is known to have
  /// not been referenced yet.
  llvm::StringMap<GlobalDefn> DeferredDefns;

  /// DeferredDefnsToEmit - This is a list of deferred decls which we have seen
  /// that *are* actually referenced. These get code generated when the module
  /// is done.
  std::vector<GlobalDefn> DeferredDefnsToEmit;

  /// LLVMUsed - List of global values which are required to be
  /// present in the object file; bitcast to i8*. This is used for
  /// forcing visibility of symbols which may otherwise be optimized
  /// out.
  std::vector<llvm::WeakVH> LLVMUsed;

  /// GlobalCtors - Store the list of global constructors and their respective
  /// priorities to be emitted when the translation unit is complete.
  CtorList GlobalCtors;

  /// GlobalDtors - Store the list of global destructors and their respective
  /// priorities to be emitted when the translation unit is complete.
  CtorList GlobalDtors;

  /// MangledDefnNames - A map of canonical GlobalDecls to their mangled names.
  llvm::DenseMap<GlobalDefn, llvm::StringRef> MangledDefnNames;
  llvm::BumpPtrAllocator MangledNamesAllocator;
  
  std::vector<llvm::Constant*> Annotations;

  llvm::StringMap<llvm::Constant*> CFConstantStringMap;
  llvm::StringMap<llvm::Constant*> ConstantStringMap;
  llvm::DenseMap<const Defn*, llvm::Value*> StaticLocalDefnMap;

  /// CXXGlobalInits - Global variables with initializers that need to run
  /// before main.
  std::vector<llvm::Constant*> CXXGlobalInits;

  /// When a C++ decl with an initializer is deferred, null is
  /// appended to CXXGlobalInits, and the index of that null is placed
  /// here so that the initializer will be performed in the correct
  /// order.
  llvm::DenseMap<const Defn*, unsigned> DelayedCXXInitPosition;
  
  /// - Global variables with initializers whose order of initialization
  /// is set by init_priority attribute.
  
  llvm::SmallVector<std::pair<OrderGlobalInits, llvm::Function*>, 8> 
    PrioritizedCXXGlobalInits;

  /// CXXGlobalDtors - Global destructor functions and arguments that need to
  /// run on termination.
  std::vector<std::pair<llvm::WeakVH,llvm::Constant*> > CXXGlobalDtors;

  /// CFConstantStringClassRef - Cached reference to the class for constant
  /// strings. This value has type int * but is actually an Obj-C class pointer.
  llvm::Constant *CFConstantStringClassRef;

  /// ConstantStringClassRef - Cached reference to the class for constant
  /// strings. This value has type int * but is actually an Obj-C class pointer.
  llvm::Constant *ConstantStringClassRef;

  llvm::LLVMContext &VMContext;

  /// @name Cache for Blocks Runtime Globals
  /// @{

  const ScriptDefn *NSConcreteGlobalScriptDefn;
  const ScriptDefn *NSConcreteStackScriptDefn;
  llvm::Constant *NSConcreteGlobalScript;
  llvm::Constant *NSConcreteStackScript;

  const VarDefn *NSConcreteGlobalBlockDefn;
  const VarDefn *NSConcreteStackBlockDefn;
  llvm::Constant *NSConcreteGlobalBlock;
  llvm::Constant *NSConcreteStackBlock;

  const FunctionDefn *BlockObjectAssignDefn;
  const FunctionDefn *BlockObjectDisposeDefn;
  llvm::Constant *BlockObjectAssign;
  llvm::Constant *BlockObjectDispose;

  llvm::Type *BlockDescriptorType;
  llvm::Type *GenericBlockLiteralType;

  struct {
    int GlobalUniqueCount;
  } Block;

  /// @}
public:
  CodeGenModule(ASTContext &C, const CodeGenOptions &CodeGenOpts,
                llvm::Module &M, const llvm::TargetData &TD,
                DiagnosticsEngine &Diags);

  ~CodeGenModule();

  /// Release - Finalize LLVM code generation.
  void Release();

  /// getOOPABI() - Return a reference to the configured C++ ABI.
  CGOOPABI &getOOPABI() { return ABI; }

  llvm::Value *getStaticLocalDeclAddress(const VarDefn *VD) {
    return StaticLocalDefnMap[VD];
  }
  void setStaticLocalDeclAddress(const VarDefn *D,
                             llvm::GlobalVariable *GV) {
    StaticLocalDefnMap[D] = GV;
  }

  CGDebugInfo *getModuleDebugInfo() { return DebugInfo; }

  ASTContext &getContext() const { return Context; }
  const CodeGenOptions &getCodeGenOpts() const { return CodeGenOpts; }
  const LangOptions &getLangOptions() const { return Features; }
  llvm::Module &getModule() const { return TheModule; }
  CodeGenTypes &getTypes() { return Types; }
  CodeGenVTables &getVTables() { return VTables; }
  DiagnosticsEngine &getDiags() const { return Diags; }
  const llvm::TargetData &getTargetData() const { return TheTargetData; }
  const TargetInfo &getTarget() const { return Context.Target; }
  llvm::LLVMContext &getLLVMContext() { return VMContext; }
  const TargetCodeGenInfo &getTargetCodeGenInfo();
  bool isTargetDarwin() const;

  bool shouldUseTBAA() const { return TBAA != 0; }

  llvm::MDNode *getTBAAInfo(Type QTy);

  static void DecorateInstruction(llvm::Instruction *Inst,
                                  llvm::MDNode *TBAAInfo);

  /// getSize - Emit the given number of characters as a value of type size_t.
  llvm::ConstantInt *getSize(CharUnits numChars);

  /// setGlobalVisibility - Set the visibility for the given LLVM
  /// GlobalValue.
  void setGlobalVisibility(llvm::GlobalValue *GV, const NamedDefn *D) const;

  /// TypeVisibilityKind - The kind of global variable that is passed to
  /// setTypeVisibility
  enum TypeVisibilityKind {
    TVK_ForVTT,
    TVK_ForVTable,
    TVK_ForConstructionVTable,
    TVK_ForRTTI,
    TVK_ForRTTIName
  };

  /// setTypeVisibility - Set the visibility for the given global
  /// value which holds information about a type.
  void setTypeVisibility(llvm::GlobalValue *GV, const UserClassDefn *D,
                         TypeVisibilityKind TVK) const;

  static llvm::GlobalValue::VisibilityTypes GetLLVMVisibility(Visibility V) {
    switch (V) {
    case DefaultVisibility:   return llvm::GlobalValue::DefaultVisibility;
    case HiddenVisibility:    return llvm::GlobalValue::HiddenVisibility;
    case ProtectedVisibility: return llvm::GlobalValue::ProtectedVisibility;
    }
    llvm_unreachable("unknown visibility!");
    return llvm::GlobalValue::DefaultVisibility;
  }

  llvm::Constant *GetAddrOfGlobal(GlobalDefn GD) {
    if (isa<ClassConstructorDefn>(GD.getDefn()))
      return GetAddrOfCXXConstructor(cast<ClassConstructorDefn>(GD.getDefn()),
                                     GD.getCtorType());
    else if (isa<ClassDestructorDefn>(GD.getDefn()))
      return GetAddrOfCXXDestructor(cast<ClassDestructorDefn>(GD.getDefn()),
                                     GD.getDtorType());
    else if (isa<FunctionDefn>(GD.getDefn()))
      return GetAddrOfFunction(GD);
    else
      return GetAddrOfGlobalVar(cast<VarDefn>(GD.getDefn()));
  }

  /// CreateOrReplaceCXXRuntimeVariable - Will return a global variable of the given
  /// type. If a variable with a different type already exists then a new
  /// variable with the right type will be created and all uses of the old
  /// variable will be replaced with a bitcast to the new variable.
  llvm::GlobalVariable *
  CreateOrReplaceCXXRuntimeVariable(llvm::StringRef Name, llvm::Type *Ty,
                                    llvm::GlobalValue::LinkageTypes Linkage);

  /// GetAddrOfGlobalVar - Return the llvm::Constant for the address of the
  /// given global variable.  If Ty is non-null and if the global doesn't exist,
  /// then it will be greated with the specified type instead of whatever the
  /// normal requested type would be.
  llvm::Constant *GetAddrOfGlobalVar(const VarDefn *D,
                                     llvm::Type *Ty = 0);


  /// GetAddrOfFunction - Return the address of the given function.  If Ty is
  /// non-null, then this function will use the specified type if it has to
  /// create it.
  llvm::Constant *GetAddrOfFunction(GlobalDefn GD,
                                    llvm::Type *Ty = 0,
                                    bool ForVTable = false);

  /// GetAddrOfFunction - Return the address of the given script.
  llvm::Constant *GetAddrOfScript(const ScriptDefn *SD);

  /// GetAddrOfRTTIDescriptor - Get the address of the RTTI descriptor 
  /// for the given type.
  llvm::Constant *GetAddrOfRTTIDescriptor(Type Ty, bool ForEH = false);

  /// GetAddrOfThunk - Get the address of the thunk for the given global decl.
  llvm::Constant *GetAddrOfThunk(GlobalDefn GD, const ThunkInfo &Thunk);

  /// GetWeakRefReference - Get a reference to the target of VD.
  llvm::Constant *GetWeakRefReference(const ValueDefn *VD);

  /// GetNonVirtualBaseClassOffset - Returns the offset from a derived class to 
  /// a class. Returns null if the offset is 0. 
//  llvm::Constant *
//  GetNonVirtualBaseClassOffset(const UserClassDefn *ClassDecl,
//                               CastExpr::path_const_iterator PathBegin,
//                               CastExpr::path_const_iterator PathEnd);

  /// A pair of helper functions for a __block variable.
  class ByrefHelpers : public llvm::FoldingSetNode {
  public:
    llvm::Constant *CopyHelper;
    llvm::Constant *DisposeHelper;

    /// The alignment of the field.  This is important because
    /// different offsets to the field within the byref struct need to
    /// have different helper functions.
    CharUnits Alignment;

    ByrefHelpers(CharUnits alignment) : Alignment(alignment) {}
    virtual ~ByrefHelpers();

    void Profile(llvm::FoldingSetNodeID &id) const {
      id.AddInteger(Alignment.getQuantity());
      profileImpl(id);
    }
    virtual void profileImpl(llvm::FoldingSetNodeID &id) const = 0;

    virtual bool needsCopy() const { return true; }
    virtual void emitCopy(CodeGenFunction &CGF,
                          llvm::Value *dest, llvm::Value *src) = 0;

    virtual bool needsDispose() const { return true; }
    virtual void emitDispose(CodeGenFunction &CGF, llvm::Value *field) = 0;
  };

  llvm::FoldingSet<ByrefHelpers> ByrefHelpersCache;

  /// getUniqueBlockCount - Fetches the global unique block count.
  int getUniqueBlockCount() { return ++Block.GlobalUniqueCount; }

  /// getBlockDescriptorType - Fetches the type of a generic block
  /// descriptor.
  llvm::Type *getBlockDescriptorType();

  /// getGenericBlockLiteralType - The type of a generic block literal.
  llvm::Type *getGenericBlockLiteralType();

  /// GetAddrOfGlobalBlock - Gets the address of a block which
  /// requires no captures.
  llvm::Constant *GetAddrOfGlobalBlock(const BlockCmd *BE, const char *);
  
  /// GetStringForStringLiteral - Return the appropriate bytes for a string
  /// literal, properly padded to match the literal type. If only the address of
  /// a constant is needed consider using GetAddrOfConstantStringLiteral.
  std::string GetStringForStringLiteral(const StringLiteral *E);

  /// GetAddrOfConstantCFString - Return a pointer to a constant CFString object
  /// for the given string.
  llvm::Constant *GetAddrOfConstantCFString(const StringLiteral *Literal);
  
  /// GetAddrOfConstantString - Return a pointer to a constant NSString object
  /// for the given string. Or a user defined String object as defined via
  /// -fconstant-string-class=class_name option.
  llvm::Constant *GetAddrOfConstantString(const StringLiteral *Literal);

  /// GetConstantArrayFromStringLiteral - Return a constant array for the given
  /// string.
  llvm::Constant *GetConstantArrayFromStringLiteral(const StringLiteral *E);

  /// GetAddrOfConstantStringFromLiteral - Return a pointer to a constant array
  /// for the given string literal.
  llvm::Constant *GetAddrOfConstantStringFromLiteral(const StringLiteral *S);

  /// GetAddrOfConstantString - Returns a pointer to a character array
  /// containing the literal. This contents are exactly that of the given
  /// string, i.e. it will not be null terminated automatically; see
  /// GetAddrOfConstantCString. Note that whether the result is actually a
  /// pointer to an LLVM constant depends on Feature.WriteableStrings.
  ///
  /// The result has pointer to array type.
  ///
  /// \param GlobalName If provided, the name to use for the global
  /// (if one is created).
  llvm::Constant *GetAddrOfConstantString(llvm::StringRef Str,
                                          const char *GlobalName=0);

  /// GetAddrOfConstantCString - Returns a pointer to a character array
  /// containing the literal and a terminating '\0' character. The result has
  /// pointer to array type.
  ///
  /// \param GlobalName If provided, the name to use for the global (if one is
  /// created).
  llvm::Constant *GetAddrOfConstantCString(const std::string &str,
                                           const char *GlobalName=0);

  /// GetAddrOfCXXConstructor - Return the address of the constructor of the
  /// given type.
  llvm::GlobalValue *GetAddrOfCXXConstructor(const ClassConstructorDefn *ctor,
                                             ClassCtorType ctorType,
                                             const CGFunctionInfo *fnInfo = 0);

  /// GetAddrOfCXXDestructor - Return the address of the constructor of the
  /// given type.
  llvm::GlobalValue *GetAddrOfCXXDestructor(const ClassDestructorDefn *dtor,
                                            ClassDtorType dtorType,
                                            const CGFunctionInfo *fnInfo = 0);

  /// getBuiltinLibFunction - Given a builtin id for a function like
  /// "__builtin_fabsf", return a Function* for "fabsf".
  llvm::Value *getBuiltinLibFunction(const FunctionDefn *FD,
                                     unsigned BuiltinID);

  llvm::Function *getIntrinsic(unsigned IID,
		                           llvm::ArrayRef<llvm::Type*> Tys=
                                     llvm::ArrayRef<llvm::Type*>());

  /// EmitTopLevelDefn - Emit code for a single top level declaration.
  void EmitTopLevelDefn(Defn *D);

  /// AddUsedGlobal - Add a global which should be forced to be
  /// present in the object file; these are emitted to the llvm.used
  /// metadata global.
  void AddUsedGlobal(llvm::GlobalValue *GV);

  void AddAnnotation(llvm::Constant *C) { Annotations.push_back(C); }

  /// AddCXXDtorEntry - Add a destructor and object to add to the C++ global
  /// destructor function.
  void AddCXXDtorEntry(llvm::Constant *DtorFn, llvm::Constant *Object) {
    CXXGlobalDtors.push_back(std::make_pair(DtorFn, Object));
  }

  /// CreateRuntimeFunction - Create a new runtime function with the specified
  /// type and name.
  llvm::Constant *CreateRuntimeFunction(llvm::FunctionType *Ty,
                                        llvm::StringRef Name,
                                        llvm::Attributes ExtraAttrs =
                                          llvm::Attribute::None);
  /// CreateRuntimeVariable - Create a new runtime global variable with the
  /// specified type and name.
  llvm::Constant *CreateRuntimeVariable(llvm::Type *Ty,
                                        llvm::StringRef Name);

  ///@name Custom Blocks Runtime Interfaces
  ///@{

  llvm::Constant *getNSConcreteGlobalBlock();
  llvm::Constant *getNSConcreteStackBlock();
  llvm::Constant *getBlockObjectAssign();
  llvm::Constant *getBlockObjectDispose();

  ///@}

  // UpdateCompleteType - Make sure that this type is translated.
  void UpdateCompletedType(const TypeDefn *TD);

  llvm::Constant *getMemberPointerConstant(const UnaryOperator *e);

  /// EmitConstantExpr - Try to emit the given expression as a
  /// constant; returns 0 if the expression cannot be emitted as a
  /// constant.
  llvm::Constant *EmitConstantExpr(const Expr *E, Type DestType,
                                   CodeGenFunction *CGF = 0);

  /// EmitNullConstant - Return the result of value-initializing the given
  /// type, i.e. a null expression of the given type.  This is usually,
  /// but not always, an LLVM null constant.
  llvm::Constant *EmitNullConstant(Type T);

//  llvm::Constant *EmitAnnotateAttr(llvm::GlobalValue *GV,
//                                   const AnnotateAttr *AA, unsigned LineNo);

  /// Error - Emit a general error that something can't be done.
  void Error(SourceLocation loc, llvm::StringRef error);

  /// ErrorUnsupported - Print out an error that codegen doesn't support the
  /// specified stmt yet.
  /// \param OmitOnError - If true, then this error should only be emitted if no
  /// other errors have been reported.
  void ErrorUnsupported(const Stmt *S, const char *Type,
                        bool OmitOnError=false);

  /// ErrorUnsupported - Print out an error that codegen doesn't support the
  /// specified decl yet.
  /// \param OmitOnError - If true, then this error should only be emitted if no
  /// other errors have been reported.
  void ErrorUnsupported(const Defn *D, const char *Type,
                        bool OmitOnError=false);

  /// SetInternalFunctionAttributes - Set the attributes on the LLVM
  /// function for the given decl and function info. This applies
  /// attributes necessary for handling the ABI as well as user
  /// specified attributes like section.
  void SetInternalFunctionAttributes(const Defn *D, llvm::Function *F,
                                     const CGFunctionInfo &FI);

  /// SetLLVMFunctionAttributes - Set the LLVM function attributes
  /// (sext, zext, etc).
  void SetLLVMFunctionAttributes(const Defn *D,
                                 const CGFunctionInfo &Info,
                                 llvm::Function *F);

  /// SetLLVMFunctionAttributesForDefinition - Set the LLVM function attributes
  /// which only apply to a function definintion.
  void SetLLVMFunctionAttributesForDefinition(const Defn *D, llvm::Function *F);

  /// ReturnTypeUsesSRet - Return true iff the given type uses 'sret' when used
  /// as a return type.
  bool ReturnTypeUsesSRet(const CGFunctionInfo &FI);

  /// ReturnTypeUsesSret - Return true iff the given type uses 'fpret' when used
  /// as a return type.
  bool ReturnTypeUsesFPRet(Type ResultType);

  /// ConstructAttributeList - Get the LLVM attributes and calling convention to
  /// use for a particular function type.
  ///
  /// \param Info - The function type information.
  /// \param TargetDecl - The decl these attributes are being constructed
  /// for. If supplied the attributes applied to this decl may contribute to the
  /// function attributes and calling convention.
  /// \param PAL [out] - On return, the attribute list to use.
  /// \param CallingConv [out] - On return, the LLVM calling convention to use.
  void ConstructAttributeList(const CGFunctionInfo &Info,
                              const Defn *TargetDecl,
                              AttributeListType &PAL,
                              unsigned &CallingConv);

  llvm::StringRef getMangledName(GlobalDefn GD);
  void getBlockMangledName(GlobalDefn GD, MangleBuffer &Buffer,
                           const ScriptDefn *BD);

  void EmitTentativeDefinition(const VarDefn *D);

  void EmitVTable(UserClassDefn *Class, bool DefinitionRequired);

  llvm::GlobalVariable::LinkageTypes
  getFunctionLinkage(const FunctionDefn *FD);

  void setFunctionLinkage(const FunctionDefn *FD, llvm::GlobalValue *V) {
    V->setLinkage(getFunctionLinkage(FD));
  }

  /// getVTableLinkage - Return the appropriate linkage for the vtable, VTT,
  /// and type information of the given class.
  llvm::GlobalVariable::LinkageTypes getVTableLinkage(const UserClassDefn *RD);

  /// GetTargetTypeStoreSize - Return the store size, in character units, of
  /// the given LLVM type.
  CharUnits GetTargetTypeStoreSize(llvm::Type *Ty) const;
  
  /// GetLLVMLinkageVarDefinition - Returns LLVM linkage for a global 
  /// variable.
  llvm::GlobalValue::LinkageTypes 
  GetLLVMLinkageVarDefinition(const VarDefn *D,
                              llvm::GlobalVariable *GV);
  
  std::vector<const UserClassDefn*> DeferredVTables;

private:
  llvm::GlobalValue *GetGlobalValue(llvm::StringRef Ref);

  llvm::Constant *GetOrCreateLLVMFunction(llvm::StringRef MangledName,
                                          llvm::Type *Ty,
                                          GlobalDefn D,
                                          bool ForVTable,
                                          llvm::Attributes ExtraAttrs =
                                            llvm::Attribute::None);
  llvm::Constant *GetOrCreateLLVMGlobal(llvm::StringRef MangledName,
                                        llvm::PointerType *PTy,
                                        const VarDefn *D,
                                        bool UnnamedAddr = false);

  /// SetCommonAttributes - Set attributes which are common to any
  /// form of a global definition (alias, Objective-C method,
  /// function, global variable).
  ///
  /// NOTE: This should only be called for definitions.
  void SetCommonAttributes(const Defn *D, llvm::GlobalValue *GV);

  /// SetFunctionDefinitionAttributes - Set attributes for a global definition.
  void SetFunctionDefinitionAttributes(const FunctionDefn *D,
                                       llvm::GlobalValue *GV);

  /// SetFunctionAttributes - Set function attributes for a function
  /// declaration.
  void SetFunctionAttributes(GlobalDefn GD,
                             llvm::Function *F,
                             bool IsIncompleteFunction);

  /// EmitGlobal - Emit code for a singal global function or var decl. Forward
  /// declarations are emitted lazily.
  void EmitGlobal(GlobalDefn D);
  void EmitGlobalDefinition(GlobalDefn D);
  void EmitGlobalScriptDefinition(const ScriptDefn* D);
  void EmitGlobalFunctionDefinition(GlobalDefn GD);
  void EmitGlobalVarDefinition(const VarDefn *D);
  void EmitAliasDefinition(GlobalDefn GD);
  
  // C++ related functions.

  bool TryEmitDefinitionAsAlias(GlobalDefn Alias, GlobalDefn Target);
  bool TryEmitBaseDestructorAsAlias(const ClassDestructorDefn *D);

  void EmitNamespace(const NamespaceDefn *D);
  //void EmitLinkageSpec(const LinkageSpecDefn *D);

  /// EmitClassConstructors - Emit constructors (base, complete) from a
  /// C++ constructor Defn.
  void EmitClassConstructors(const ClassConstructorDefn *D);

  /// EmitClassConstructor - Emit a single constructor with the given type from
  /// a C++ constructor Defn.
  void EmitClassConstructor(const ClassConstructorDefn *D, ClassCtorType Type);

  /// EmitClassDestructors - Emit destructors (base, complete) from a
  /// C++ destructor Defn.
  void EmitClassDestructors(const ClassDestructorDefn *D);

  /// EmitClassDestructor - Emit a single destructor with the given type from
  /// a C++ destructor Defn.
  void EmitClassDestructor(const ClassDestructorDefn *D, ClassDtorType Type);

  /// EmitCXXGlobalInitFunc - Emit the function that initializes C++ globals.
  void EmitCXXGlobalInitFunc();

  /// EmitCXXGlobalDtorFunc - Emit the function that destroys C++ globals.
  void EmitCXXGlobalDtorFunc();

  void EmitCXXGlobalVarDeclInitFunc(const VarDefn *D,
                                    llvm::GlobalVariable *Addr);

  // FIXME: Hardcoding priority here is gross.
  void AddGlobalCtor(llvm::Function *Ctor, int Priority=65535);
  void AddGlobalDtor(llvm::Function *Dtor, int Priority=65535);

  /// EmitCtorList - Generates a global array of functions and priorities using
  /// the given list and name. This array will have appending linkage and is
  /// suitable for use as a LLVM constructor or destructor array.
  void EmitCtorList(const CtorList &Fns, const char *GlobalName);

  void EmitAnnotations(void);

  /// EmitFundamentalRTTIDescriptor - Emit the RTTI descriptors for the
  /// given type.
  void EmitFundamentalRTTIDescriptor(Type Type);

  /// EmitFundamentalRTTIDescriptors - Emit the RTTI descriptors for the
  /// builtin types.
  void EmitFundamentalRTTIDescriptors();

  /// EmitDeferred - Emit any needed decls for which code generation
  /// was deferred.
  void EmitDeferred(void);

  /// EmitLLVMUsed - Emit the llvm.used metadata used to force
  /// references to global which may otherwise be optimized out.
  void EmitLLVMUsed(void);

  void EmitDefnMetadata();

  /// EmitCoverageFile - Emit the llvm.gcov metadata used to tell LLVM where
  /// to emit the .gcno and .gcda files in a way that persists in .bc files.
  void EmitCoverageFile();

  /// MayDeferGeneration - Determine if the given decl can be emitted
  /// lazily; this is only relevant for definitions. The given decl
  /// must be either a function or var decl.
  bool MayDeferGeneration(const NamedDefn *D);

  /// SimplifyPersonality - Check whether we can use a "simpler", more
  /// core exceptions personality function.
  void SimplifyPersonality();
};
}  // end namespace CodeGen
}  // end namespace mlang

#endif /* MLANG_CODEGEN_CODEGENMODULE_H_ */
