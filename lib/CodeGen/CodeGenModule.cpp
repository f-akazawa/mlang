//===--- CodeGenModule.cpp - Emit LLVM Code from ASTs for a Module --------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This coordinates the per-module state used while generating code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenModule.h"
#include "CGDebugInfo.h"
#include "CodeGenFunction.h"
#include "CodeGenTBAA.h"
#include "CGCall.h"
#include "CGOOPABI.h"
#include "TargetInfo.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Mangle.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/AST/Type.h"
#include "mlang/Basic/Builtins.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Basic/ConvertUTF.h"
#include "mlang/Diag/Diagnostic.h"
#include "llvm/CallingConv.h"
#include "llvm/Module.h"
#include "llvm/Intrinsics.h"
#include "llvm/LLVMContext.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Target/Mangler.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/ErrorHandling.h"
using namespace mlang;
using namespace CodeGen;

static CGOOPABI &createOOPABI(CodeGenModule &CGM) {
//  switch (CGM.getContext().Target.getGmatABI()) {
//  case GmatABI_Nvidia: return *CreateNVGmatABI(CGM);
//  case GmatABI_AMD: return *CreateAMDGmatABI(CGM);
//  }

  llvm_unreachable("invalid Gmat ABI kind");
  return *CreateNVGmatABI(CGM);
}

CodeGenModule::CodeGenModule(ASTContext &C, const CodeGenOptions &CGO,
                             llvm::Module &M, const llvm::TargetData &TD,
                             DiagnosticsEngine &diags)
  : Context(C), Features(C.getLangOptions()), CodeGenOpts(CGO), TheModule(M),
    TheTargetData(TD), TheTargetCodeGenInfo(0), Diags(diags),
    ABI(createOOPABI(*this)),
    Types(C, M, TD, getTargetCodeGenInfo().getABIInfo(), ABI, CGO),
    TBAA(0),
    VTables(*this), DebugInfo(0),
    CFConstantStringClassRef(0), ConstantStringClassRef(0),
    VMContext(M.getContext()),
    NSConcreteGlobalScriptDefn(0), NSConcreteStackScriptDefn(0),
    NSConcreteGlobalScript(0), NSConcreteStackScript(0),
    NSConcreteGlobalBlockDefn(0), NSConcreteStackBlockDefn(0),
    NSConcreteGlobalBlock(0), NSConcreteStackBlock(0),
    BlockObjectAssignDefn(0), BlockObjectDisposeDefn(0),
    BlockObjectAssign(0), BlockObjectDispose(0),
    BlockDescriptorType(0), GenericBlockLiteralType(0) {
  // Enable TBAA unless it's suppressed.
  if (!CodeGenOpts.RelaxedAliasing && CodeGenOpts.OptimizationLevel > 0)
    TBAA = new CodeGenTBAA(Context, VMContext, getLangOptions(),
                           ABI.getMangleContext());

  // If debug info or coverage generation is enabled, create the CGDebugInfo
  // object.
  if (CodeGenOpts.DebugInfo || CodeGenOpts.EmitGcovArcs ||
      CodeGenOpts.EmitGcovNotes) {
 //   DebugInfo = new CGDebugInfo(*this);
  }

  Block.GlobalUniqueCount = 0;

  // Initialize the type cache.
  llvm::LLVMContext &LLVMContext = M.getContext();
  VoidTy = llvm::Type::getVoidTy(LLVMContext);
  Int8Ty = llvm::Type::getInt8Ty(LLVMContext);
  Int32Ty = llvm::Type::getInt32Ty(LLVMContext);
  Int64Ty = llvm::Type::getInt64Ty(LLVMContext);
  PointerWidthInBits = C.Target.getPointerWidth(0);
  PointerAlignInBytes =
    C.toCharUnitsFromBits(C.Target.getPointerAlign(0)).getQuantity();
  IntTy = llvm::IntegerType::get(LLVMContext, C.Target.getIntWidth());
  IntPtrTy = llvm::IntegerType::get(LLVMContext, PointerWidthInBits);
  Int8PtrTy = Int8Ty->getPointerTo(0);
  Int8PtrPtrTy = Int8PtrTy->getPointerTo(0);
}

CodeGenModule::~CodeGenModule() {
  delete &ABI;
  delete TBAA;
//  delete DebugInfo;
}

void CodeGenModule::Release() {
  EmitDeferred();
//  EmitCXXGlobalInitFunc();
//  EmitCXXGlobalDtorFunc();
  EmitCtorList(GlobalCtors, "llvm.global_ctors");
  EmitCtorList(GlobalDtors, "llvm.global_dtors");
  EmitAnnotations();
  EmitLLVMUsed();

  //SimplifyPersonality();

  if (getCodeGenOpts().EmitDeclMetadata)
    EmitDefnMetadata();

  if (getCodeGenOpts().EmitGcovArcs || getCodeGenOpts().EmitGcovNotes)
    EmitCoverageFile();
}

void CodeGenModule::UpdateCompletedType(const TypeDefn *TD) {
  // Make sure that this type is translated.
  Types.UpdateCompletedType(TD);
//  if (DebugInfo)
//    DebugInfo->UpdateCompletedType(TD);
}

llvm::MDNode *CodeGenModule::getTBAAInfo(Type QTy) {
  if (!TBAA)
    return 0;
  return TBAA->getTBAAInfo(QTy);
}

void CodeGenModule::DecorateInstruction(llvm::Instruction *Inst,
                                        llvm::MDNode *TBAAInfo) {
  Inst->setMetadata(llvm::LLVMContext::MD_tbaa, TBAAInfo);
}

bool CodeGenModule::isTargetDarwin() const {
  return getContext().Target.getTriple().isOSDarwin();
}

void CodeGenModule::Error(SourceLocation loc, llvm::StringRef error) {
  unsigned diagID = getDiags().getCustomDiagID(DiagnosticsEngine::Error, error);
  getDiags().Report(Context.getFullLoc(loc), diagID);
}

/// ErrorUnsupported - Print out an error that codegen doesn't support the
/// specified stmt yet.
void CodeGenModule::ErrorUnsupported(const Stmt *S, const char *Type,
                                     bool OmitOnError) {
  if (OmitOnError && getDiags().hasErrorOccurred())
    return;
  unsigned DiagID = getDiags().getCustomDiagID(DiagnosticsEngine::Error,
                                               "cannot compile this %0 yet");
  std::string Msg = Type;
  getDiags().Report(Context.getFullLoc(S->getLocStart()), DiagID)
    << Msg << S->getSourceRange();
}

/// ErrorUnsupported - Print out an error that codegen doesn't support the
/// specified decl yet.
void CodeGenModule::ErrorUnsupported(const Defn *D, const char *Type,
                                     bool OmitOnError) {
  if (OmitOnError && getDiags().hasErrorOccurred())
    return;
  unsigned DiagID = getDiags().getCustomDiagID(DiagnosticsEngine::Error,
                                               "cannot compile this %0 yet");
  std::string Msg = Type;
  getDiags().Report(Context.getFullLoc(D->getLocation()), DiagID) << Msg;
}

llvm::ConstantInt *CodeGenModule::getSize(CharUnits size) {
  return llvm::ConstantInt::get(SizeTy, size.getQuantity());
}

llvm::Constant *
CodeGenModule::GetConstantArrayFromStringLiteral(const StringLiteral *E) {
//  assert(!E->getType()->isPointerType() && "Strings are always arrays");

  // Don't emit it as the address of the string, emit the string data itself
  // as an inline array.
//  if (E->getCharByteWidth() == 1) {
    SmallString<64> Str(E->getString());
    // Resize the string to the right size, which is indicated by its type.
 // const ConstantArrayType *CAT = Context.getAsConstantArrayType(E->getType());
 //   Str.resize(CAT->getSize().getZExtValue());
    return llvm::ConstantDataArray::getString(VMContext, Str, false);
//  }
#if 0
  llvm::ArrayType *AType =
    cast<llvm::ArrayType>(getTypes().ConvertType(E->getType()));
  llvm::Type *ElemTy = AType->getElementType();
  unsigned NumElements = AType->getNumElements();

  // Wide strings have either 2-byte or 4-byte elements.
  if (ElemTy->getPrimitiveSizeInBits() == 16) {
    SmallVector<uint16_t, 32> Elements;
    Elements.reserve(NumElements);
    for (unsigned i = 0, e = E->getLength(); i != e; ++i)
      Elements.push_back(E->getCodeUnit(i));
    Elements.resize(NumElements);
    return llvm::ConstantDataArray::get(VMContext, Elements);
  }

  assert(ElemTy->getPrimitiveSizeInBits() == 32);
  SmallVector<uint32_t, 32> Elements;
  Elements.reserve(NumElements);

  for (unsigned i = 0, e = E->getLength(); i != e; ++i)
    Elements.push_back(E->getCodeUnit(i));
  Elements.resize(NumElements);
  return llvm::ConstantDataArray::get(VMContext, Elements);
#endif
}
void CodeGenModule::setGlobalVisibility(llvm::GlobalValue *GV,
                                        const NamedDefn *D) const {
  // Internal definitions always have default visibility.
  if (GV->hasLocalLinkage()) {
    GV->setVisibility(llvm::GlobalValue::DefaultVisibility);
    return;
  }

  // Set visibility for definitions.
  NamedDefn::LinkageInfo LV = D->getLinkageAndVisibility();
  if (LV.visibilityExplicit() || !GV->hasAvailableExternallyLinkage())
    GV->setVisibility(GetLLVMVisibility(LV.visibility()));
}

llvm::StringRef CodeGenModule::getMangledName(GlobalDefn GD) {
  const NamedDefn *ND = cast<NamedDefn>(GD.getDefn());

  llvm::StringRef &Str = MangledDefnNames[GD.getCanonicalDefn()];
  if (!Str.empty())
    return Str;

  if (!getOOPABI().getMangleContext().shouldMangleDefnName(ND)) {
    IdentifierInfo *II = ND->getIdentifier();
    assert(II && "Attempt to mangle unnamed decl.");

    Str = II->getName();
    return Str;
  }
  
  llvm::SmallString<256> Buffer;
  llvm::raw_svector_ostream Out(Buffer);
  if (const ClassConstructorDefn *D = dyn_cast<ClassConstructorDefn>(ND))
    getOOPABI().getMangleContext().mangleCXXCtor(D, GD.getCtorType(), Out);
  else if (const ClassDestructorDefn *D = dyn_cast<ClassDestructorDefn>(ND))
    getOOPABI().getMangleContext().mangleCXXDtor(D, GD.getDtorType(), Out);
  else if (const ScriptDefn *BD = dyn_cast<ScriptDefn>(ND))
    getOOPABI().getMangleContext().mangleBlock(BD, Out);
  else
    getOOPABI().getMangleContext().mangleName(ND, Out);

  // Allocate space for the mangled name.
  Out.flush();
  size_t Length = Buffer.size();
  char *Name = MangledNamesAllocator.Allocate<char>(Length);
  std::copy(Buffer.begin(), Buffer.end(), Name);
  
  Str = llvm::StringRef(Name, Length);
  
  return Str;
}

void CodeGenModule::getBlockMangledName(GlobalDefn GD, MangleBuffer &Buffer,
                                        const ScriptDefn *BD) {
  MangleContext &MangleCtx = getOOPABI().getMangleContext();
  const Defn *D = GD.getDefn();
  llvm::raw_svector_ostream Out(Buffer.getBuffer());
  if (D == 0)
    MangleCtx.mangleGlobalBlock(BD, Out);
  else if (const ClassConstructorDefn *CD = dyn_cast<ClassConstructorDefn>(D))
    MangleCtx.mangleCtorBlock(CD, GD.getCtorType(), BD, Out);
  else if (const ClassDestructorDefn *DD = dyn_cast<ClassDestructorDefn>(D))
    MangleCtx.mangleDtorBlock(DD, GD.getDtorType(), BD, Out);
  else
    MangleCtx.mangleBlock(cast<DefnContext>(D), BD, Out);
}

llvm::GlobalValue *CodeGenModule::GetGlobalValue(llvm::StringRef Name) {
  return getModule().getNamedValue(Name);
}

/// AddGlobalCtor - Add a function to the list that will be called before
/// main() runs.
void CodeGenModule::AddGlobalCtor(llvm::Function * Ctor, int Priority) {
  // FIXME: Type coercion of void()* types.
  GlobalCtors.push_back(std::make_pair(Ctor, Priority));
}

/// AddGlobalDtor - Add a function to the list that will be called
/// when the module is unloaded.
void CodeGenModule::AddGlobalDtor(llvm::Function * Dtor, int Priority) {
  // FIXME: Type coercion of void()* types.
  GlobalDtors.push_back(std::make_pair(Dtor, Priority));
}

void CodeGenModule::EmitCtorList(const CtorList &Fns, const char *GlobalName) {
  // Ctor function type is void()*.
  llvm::FunctionType* CtorFTy = llvm::FunctionType::get(VoidTy, false);
  llvm::Type *CtorPFTy = llvm::PointerType::getUnqual(CtorFTy);

  // Get the type of a ctor entry, { i32, void ()* }.
  llvm::StructType *CtorStructTy =
    llvm::StructType::get(llvm::Type::getInt32Ty(VMContext),
                          llvm::PointerType::getUnqual(CtorFTy), NULL);

  // Construct the constructor and destructor arrays.
  std::vector<llvm::Constant*> Ctors;
  for (CtorList::const_iterator I = Fns.begin(), E = Fns.end(); I != E; ++I) {
    std::vector<llvm::Constant*> S;
    S.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(VMContext),
                I->second, false));
    S.push_back(llvm::ConstantExpr::getBitCast(I->first, CtorPFTy));
    Ctors.push_back(llvm::ConstantStruct::get(CtorStructTy, S));
  }

  if (!Ctors.empty()) {
    llvm::ArrayType *AT = llvm::ArrayType::get(CtorStructTy, Ctors.size());
    new llvm::GlobalVariable(TheModule, AT, false,
                             llvm::GlobalValue::AppendingLinkage,
                             llvm::ConstantArray::get(AT, Ctors),
                             GlobalName);
  }
}

void CodeGenModule::EmitAnnotations() {
  if (Annotations.empty())
    return;

  // Create a new global variable for the ConstantStruct in the Module.
  llvm::Constant *Array =
  llvm::ConstantArray::get(llvm::ArrayType::get(Annotations[0]->getType(),
                                                Annotations.size()),
                           Annotations);
  llvm::GlobalValue *gv =
  new llvm::GlobalVariable(TheModule, Array->getType(), false,
                           llvm::GlobalValue::AppendingLinkage, Array,
                           "llvm.global.annotations");
  gv->setSection("llvm.metadata");
}

llvm::GlobalValue::LinkageTypes
CodeGenModule::getFunctionLinkage(const FunctionDefn *D) {
  GVALinkage Linkage = getContext().GetGVALinkageForFunction(D);

  if (Linkage == GVA_Internal)
    return llvm::Function::InternalLinkage;
  
//  if (D->hasAttr<DLLExportAttr>())
//    return llvm::Function::DLLExportLinkage;
//
//  if (D->hasAttr<WeakAttr>())
//    return llvm::Function::WeakAnyLinkage;
  
  // In C99 mode, 'inline' functions are guaranteed to have a strong
  // definition somewhere else, so we can use available_externally linkage.
  if (Linkage == GVA_C99Inline)
    return llvm::Function::AvailableExternallyLinkage;
  
  // In C++, the compiler has to emit a definition in every translation unit
  // that references the function.  We should use linkonce_odr because
  // a) if all references in this translation unit are optimized away, we
  // don't need to codegen it.  b) if the function persists, it needs to be
  // merged with other definitions. c) C++ has the ODR, so we know the
  // definition is dependable.
//  if (Linkage == GVA_CXXInline || Linkage == GVA_TemplateInstantiation)
//    return !Context.getLangOptions().AppleKext
//             ? llvm::Function::LinkOnceODRLinkage
//             : llvm::Function::InternalLinkage;
  
  // An explicit instantiation of a template has weak linkage, since
  // explicit instantiations can occur in multiple translation units
  // and must all be equivalent. However, we are not allowed to
  // throw away these explicit instantiations.
//  if (Linkage == GVA_ExplicitTemplateInstantiation)
//    return !Context.getLangOptions().AppleKext
//             ? llvm::Function::WeakODRLinkage
//             : llvm::Function::InternalLinkage;
  
  // Otherwise, we have strong external linkage.
  assert(Linkage == GVA_StrongExternal);
  return llvm::Function::ExternalLinkage;
}

/// SetFunctionDefinitionAttributes - Set attributes for a global.
///
/// FIXME: This is currently only done for aliases and functions, but not for
/// variables (these details are set in EmitGlobalVarDefinition for variables).
void CodeGenModule::SetFunctionDefinitionAttributes(const FunctionDefn *D,
                                                    llvm::GlobalValue *GV) {
  SetCommonAttributes(D, GV);
}

void CodeGenModule::SetLLVMFunctionAttributes(const Defn *D,
                                              const CGFunctionInfo &Info,
                                              llvm::Function *F) {
  unsigned CallingConv;
  AttributeListType AttributeList;
  ConstructAttributeList(Info, D, AttributeList, CallingConv);
  F->setAttributes(llvm::AttrListPtr::get(AttributeList));
  F->setCallingConv(static_cast<llvm::CallingConv::ID>(CallingConv));
}

void CodeGenModule::SetLLVMFunctionAttributesForDefinition(const Defn *D,
                                                           llvm::Function *F) {
  if (CodeGenOpts.UnwindTables)
    F->setHasUWTable();

//  if (D->hasAttr<AlwaysInlineAttr>())
//    F->addFnAttr(llvm::Attribute::AlwaysInline);
//
//  if (D->hasAttr<NakedAttr>())
//    F->addFnAttr(llvm::Attribute::Naked);
//
//  if (D->hasAttr<NoInlineAttr>())
//    F->addFnAttr(llvm::Attribute::NoInline);

  if (isa<ClassConstructorDefn>(D) || isa<ClassDestructorDefn>(D))
    F->setUnnamedAddr(true);

  if (Features.getStackProtectorMode() == LangOptions::SSPOn)
    F->addFnAttr(llvm::Attribute::StackProtect);
  else if (Features.getStackProtectorMode() == LangOptions::SSPReq)
    F->addFnAttr(llvm::Attribute::StackProtectReq);
  
//  unsigned alignment = D->getMaxAlignment() / Context.getCharWidth();
//  if (alignment)
//    F->setAlignment(alignment);

  // C++ ABI requires 2-byte alignment for member functions.
  if (F->getAlignment() < 2 && isa<ClassMethodDefn>(D))
    F->setAlignment(2);
}

void CodeGenModule::SetCommonAttributes(const Defn *D,
                                        llvm::GlobalValue *GV) {
  if (const NamedDefn *ND = dyn_cast<NamedDefn>(D))
    setGlobalVisibility(GV, ND);
  else
    GV->setVisibility(llvm::GlobalValue::DefaultVisibility);

//  if (D->hasAttr<UsedAttr>())
//    AddUsedGlobal(GV);
//
//  if (const SectionAttr *SA = D->getAttr<SectionAttr>())
//    GV->setSection(SA->getName());

  getTargetCodeGenInfo().SetTargetAttributes(D, GV, *this);
}

void CodeGenModule::SetInternalFunctionAttributes(const Defn *D,
                                                  llvm::Function *F,
                                                  const CGFunctionInfo &FI) {
  SetLLVMFunctionAttributes(D, FI, F);
  SetLLVMFunctionAttributesForDefinition(D, F);

  F->setLinkage(llvm::Function::InternalLinkage);

  SetCommonAttributes(D, F);
}

void CodeGenModule::SetFunctionAttributes(GlobalDefn GD,
                                          llvm::Function *F,
                                          bool IsIncompleteFunction) {
  if (unsigned IID = F->getIntrinsicID()) {
    // If this is an intrinsic function, set the function's attributes
    // to the intrinsic's attributes.
    F->setAttributes(llvm::Intrinsic::getAttributes((llvm::Intrinsic::ID)IID));
    return;
  }

  const FunctionDefn *FD = cast<FunctionDefn>(GD.getDefn());

  if (!IsIncompleteFunction)
    SetLLVMFunctionAttributes(FD, getTypes().getFunctionInfo(GD), F);

  // Only a few attributes are set on declarations; these may later be
  // overridden by a definition.

//  if (FD->hasAttr<DLLImportAttr>()) {
//    F->setLinkage(llvm::Function::DLLImportLinkage);
//  } else if (FD->hasAttr<WeakAttr>() ||
//             FD->isWeakImported()) {
//    // "extern_weak" is overloaded in LLVM; we probably should have
//    // separate linkage types for this.
//    F->setLinkage(llvm::Function::ExternalWeakLinkage);
//  } else {
//    F->setLinkage(llvm::Function::ExternalLinkage);
//
//    NamedDefn::LinkageInfo LV = FD->getLinkageAndVisibility();
//    if (LV.linkage() == ExternalLinkage && LV.visibilityExplicit()) {
//      F->setVisibility(GetLLVMVisibility(LV.visibility()));
//    }
//  }

//  if (const SectionAttr *SA = FD->getAttr<SectionAttr>())
//    F->setSection(SA->getName());
}

void CodeGenModule::AddUsedGlobal(llvm::GlobalValue *GV) {
  assert(!GV->isDeclaration() &&
         "Only globals with definition can force usage.");
  LLVMUsed.push_back(GV);
}

void CodeGenModule::EmitLLVMUsed() {
  // Don't create llvm.used if there is no need.
  if (LLVMUsed.empty())
    return;

  llvm::Type *i8PTy = llvm::Type::getInt8PtrTy(VMContext);

  // Convert LLVMUsed to what ConstantArray needs.
  std::vector<llvm::Constant*> UsedArray;
  UsedArray.resize(LLVMUsed.size());
  for (unsigned i = 0, e = LLVMUsed.size(); i != e; ++i) {
    UsedArray[i] =
     llvm::ConstantExpr::getBitCast(cast<llvm::Constant>(&*LLVMUsed[i]),
                                      i8PTy);
  }

  if (UsedArray.empty())
    return;
  llvm::ArrayType *ATy = llvm::ArrayType::get(i8PTy, UsedArray.size());

  llvm::GlobalVariable *GV =
    new llvm::GlobalVariable(getModule(), ATy, false,
                             llvm::GlobalValue::AppendingLinkage,
                             llvm::ConstantArray::get(ATy, UsedArray),
                             "llvm.used");

  GV->setSection("llvm.metadata");
}

void CodeGenModule::EmitDeferred() {
  // Emit code for any potentially referenced deferred decls.  Since a
  // previously unused static decl may become used during the generation of code
  // for a static function, iterate until no  changes are made.

  while (!DeferredDefnsToEmit.empty() || !DeferredVTables.empty()) {
    if (!DeferredVTables.empty()) {
      const UserClassDefn *RD = DeferredVTables.back();
      DeferredVTables.pop_back();
//      getVTables().GenerateClassData(getVTableLinkage(RD), RD);
      continue;
    }

    GlobalDefn D = DeferredDefnsToEmit.back();
    DeferredDefnsToEmit.pop_back();

    // Check to see if we've already emitted this.  This is necessary
    // for a couple of reasons: first, decls can end up in the
    // deferred-decls queue multiple times, and second, decls can end
    // up with definitions in unusual ways (e.g. by an extern inline
    // function acquiring a strong function redefinition).  Just
    // ignore these cases.
    //
    // TODO: That said, looking this up multiple times is very wasteful.
    llvm::StringRef Name = "_script_yabin_"/*getMangledName(D)*/;
    llvm::GlobalValue *CGRef = GetGlobalValue(Name);
    assert(CGRef && "Deferred decl wasn't referenced?");

    if (!CGRef->isDeclaration())
      continue;

    // GlobalAlias::isDeclaration() defers to the aliasee, but for our
    // purposes an alias counts as a definition.
    if (isa<llvm::GlobalAlias>(CGRef))
      continue;

    // Otherwise, emit the definition and move on to the next one.
    EmitGlobalDefinition(D);
  }
}

/// EmitAnnotateAttr - Generate the llvm::ConstantStruct which contains the
/// annotation information for a given GlobalValue.  The annotation struct is
/// {i8 *, i8 *, i8 *, i32}.  The first field is a constant expression, the
/// GlobalValue being annotated.  The second field is the constant string
/// created from the AnnotateAttr's annotation.  The third field is a constant
/// string containing the name of the translation unit.  The fourth field is
/// the line number in the file of the annotated value declaration.
///
/// FIXME: this does not unique the annotation string constants, as llvm-gcc
///        appears to.
///
//llvm::Constant *CodeGenModule::EmitAnnotateAttr(llvm::GlobalValue *GV,
//                                                const AnnotateAttr *AA,
//                                                unsigned LineNo) {
//  llvm::Module *M = &getModule();
//
//  // get [N x i8] constants for the annotation string, and the filename string
//  // which are the 2nd and 3rd elements of the global annotation structure.
//  llvm::Type *SBP = llvm::Type::getInt8PtrTy(VMContext);
//  llvm::Constant *anno = llvm::ConstantArray::get(VMContext,
//                                                  AA->getAnnotation(), true);
//  llvm::Constant *unit = llvm::ConstantArray::get(VMContext,
//                                                  M->getModuleIdentifier(),
//                                                  true);
//
//  // Get the two global values corresponding to the ConstantArrays we just
//  // created to hold the bytes of the strings.
//  llvm::GlobalValue *annoGV =
//    new llvm::GlobalVariable(*M, anno->getType(), false,
//                             llvm::GlobalValue::PrivateLinkage, anno,
//                             GV->getName());
//  // translation unit name string, emitted into the llvm.metadata section.
//  llvm::GlobalValue *unitGV =
//    new llvm::GlobalVariable(*M, unit->getType(), false,
//                             llvm::GlobalValue::PrivateLinkage, unit,
//                             ".str");
//  unitGV->setUnnamedAddr(true);
//
//  // Create the ConstantStruct for the global annotation.
//  llvm::Constant *Fields[4] = {
//    llvm::ConstantExpr::getBitCast(GV, SBP),
//    llvm::ConstantExpr::getBitCast(annoGV, SBP),
//    llvm::ConstantExpr::getBitCast(unitGV, SBP),
//    llvm::ConstantInt::get(llvm::Type::getInt32Ty(VMContext), LineNo)
//  };
//  return llvm::ConstantStruct::getAnon(Fields);
//}

bool CodeGenModule::MayDeferGeneration(const NamedDefn *Global) {
  // Never defer when EmitAllDefns is specified.
//  if (Features.EmitAllDefns)
//    return false;

  return !getContext().DefnMustBeEmitted(Global);
}

//llvm::Constant *CodeGenModule::GetWeakRefReference(const ValueDefn *VD) {
//  const AliasAttr *AA = VD->getAttr<AliasAttr>();
//  assert(AA && "No alias?");
//
//  llvm::Type *DefnTy = getTypes().ConvertTypeForMem(VD->getType());
//
//  // See if there is already something with the target's name in the module.
//  llvm::GlobalValue *Entry = GetGlobalValue(AA->getAliasee());
//
//  llvm::Constant *Aliasee;
//  if (isa<llvm::FunctionType>(DefnTy))
//    Aliasee = GetOrCreateLLVMFunction(AA->getAliasee(), DefnTy, GlobalDefn(),
//                                      /*ForVTable=*/false);
//  else
//    Aliasee = GetOrCreateLLVMGlobal(AA->getAliasee(),
//                                    llvm::PointerType::getUnqual(DefnTy), 0);
//  if (!Entry) {
//    llvm::GlobalValue* F = cast<llvm::GlobalValue>(Aliasee);
//    F->setLinkage(llvm::Function::ExternalWeakLinkage);
//    WeakRefReferences.insert(F);
//  }
//
//  return Aliasee;
//}

void CodeGenModule::EmitGlobal(GlobalDefn GD) {
  const NamedDefn *Global = cast<NamedDefn>(GD.getDefn());

//  // Weak references don't produce any output by themselves.
//  if (Global->hasAttr<WeakRefAttr>())
//    return;
//
//  // If this is an alias definition (which otherwise looks like a declaration)
//  // emit it now.
//  if (Global->hasAttr<AliasAttr>())
//    return EmitAliasDefinition(GD);

  // Ignore declarations, they will be emitted on their first use.
  if(const ScriptDefn *SD = dyn_cast<ScriptDefn>(Global)) {
	  if (SD->getIdentifier()) {
		  llvm::StringRef Name = SD->getName();
		  if (Name == "_NSConcreteGlobalBlock") {
			  NSConcreteGlobalScriptDefn = SD;
		  } else if (Name == "_NSConcreteStackBlock") {
			  NSConcreteStackScriptDefn = SD;
		  }
	  }
  } else if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(Global)) {
    if (FD->getIdentifier()) {
      llvm::StringRef Name = FD->getName();
      if (Name == "_Block_object_assign") {
        BlockObjectAssignDefn = FD;
      } else if (Name == "_Block_object_dispose") {
        BlockObjectDisposeDefn = FD;
      }
    }
  } else {
    const VarDefn *VD = cast<VarDefn>(Global);
    //assert(VD->isFileVarDefn() && "Cannot emit local var decl as global.");

    if (VD->getIdentifier()) {
      llvm::StringRef Name = VD->getName();
      if (Name == "_NSConcreteGlobalBlock") {
        NSConcreteGlobalBlockDefn = VD;
      } else if (Name == "_NSConcreteStackBlock") {
        NSConcreteStackBlockDefn = VD;
      }
    }

//
//    if (VD->isThisDeclarationADefinition() != VarDefn::Definition)
//      return;
  }

  // Defer code generation when possible if this is a static definition, inline
  // function etc.  These we only want to emit if they are used.
  if (!MayDeferGeneration(Global)) {
    // Emit the definition if it can't be deferred.
    EmitGlobalDefinition(GD);
    return;
  }

  // If we're deferring emission of a C++ variable with an
  // initializer, remember the order in which it appeared in the file.
  if (getLangOptions().OOP && isa<VarDefn>(Global) &&
      cast<VarDefn>(Global)->hasInit()) {
    DelayedCXXInitPosition[Global] = CXXGlobalInits.size();
    CXXGlobalInits.push_back(0);
  }
  
  // If the value has already been used, add it directly to the
  // DeferredDefnsToEmit list.
  llvm::StringRef MangledName = getMangledName(GD);
  if (GetGlobalValue(MangledName))
    DeferredDefnsToEmit.push_back(GD);
  else {
    // Otherwise, remember that we saw a deferred decl with this name.  The
    // first use of the mangled name will cause it to move into
    // DeferredDefnsToEmit.
    DeferredDefns[MangledName] = GD;
  }
}

void CodeGenModule::EmitGlobalDefinition(GlobalDefn GD) {
  const NamedDefn *D = cast<NamedDefn>(GD.getDefn());

  PrettyStackTraceDefn CrashInfo(const_cast<NamedDefn *>(D), D->getLocation(),
                                 Context.getSourceManager(),
                                 "Generating code for declaration");
  
  if(const ScriptDefn *Script = dyn_cast<ScriptDefn>(D)) {
	  return EmitGlobalScriptDefinition(Script);
  }

  if (const FunctionDefn *Function = dyn_cast<FunctionDefn>(D)) {
    // At -O0, don't generate IR for functions with available_externally 
    // linkage.
    if (CodeGenOpts.OptimizationLevel == 0 && 
        /*!Function->hasAttr<AlwaysInlineAttr>() &&*/
        getFunctionLinkage(Function) 
                                  == llvm::Function::AvailableExternallyLinkage)
      return;

    if (const ClassMethodDefn *Method = dyn_cast<ClassMethodDefn>(D)) {
      // Make sure to emit the definition(s) before we emit the thunks.
      // This is necessary for the generation of certain thunks.
//      if (const ClassConstructorDefn *CD = dyn_cast<ClassConstructorDefn>(Method))
//        EmitClassConstructor(CD, GD.getCtorType());
//      else if (const ClassDestructorDefn *DD =dyn_cast<ClassDestructorDefn>(Method))
//        EmitClassDestructor(DD, GD.getDtorType());
//      else
//        EmitGlobalFunctionDefinition(GD);

//      if (Method->isVirtual())
//        getVTables().EmitThunks(GD);

      return;
    }

    return EmitGlobalFunctionDefinition(GD);
  }
  
  if (const VarDefn *VD = dyn_cast<VarDefn>(D))
    return EmitGlobalVarDefinition(VD);
  
  assert(0 && "Invalid argument to EmitGlobalDefinition()");
}

/// GetOrCreateLLVMFunction - If the specified mangled name is not in the
/// module, create and return an llvm Function with the specified type. If there
/// is something in the module with the specified name, return it potentially
/// bitcasted to the right type.
///
/// If D is non-null, it specifies a decl that correspond to this.  This is used
/// to set the attributes on the function when it is first created.
llvm::Constant *
CodeGenModule::GetOrCreateLLVMFunction(llvm::StringRef MangledName,
                                       llvm::Type *Ty,
                                       GlobalDefn D, bool ForVTable,
                                       llvm::Attributes ExtraAttrs) {
  // Lookup the entry, lazily creating it if necessary.
  llvm::GlobalValue *Entry = GetGlobalValue(MangledName);
  if (Entry) {
    if (WeakRefReferences.count(Entry)) {
      const FunctionDefn *FD = cast_or_null<FunctionDefn>(D.getDefn());

      //FIXME huyabin
      if (FD /*&& !FD->hasAttr<WeakAttr>()*/)
        Entry->setLinkage(llvm::Function::ExternalLinkage);

      WeakRefReferences.erase(Entry);
    }

    if (Entry->getType()->getElementType() == Ty)
      return Entry;

    // Make sure the result is of the correct type.
    llvm::Type *PTy = llvm::PointerType::getUnqual(Ty);
    return llvm::ConstantExpr::getBitCast(Entry, PTy);
  }

  // This function doesn't have a complete type (for example, the return
  // type is an incomplete struct). Use a fake type instead, and make
  // sure not to try to set attributes.
  bool IsIncompleteFunction = false;

  llvm::FunctionType *FTy;
  if (isa<llvm::FunctionType>(Ty)) {
    FTy = cast<llvm::FunctionType>(Ty);
  } else {
    FTy = llvm::FunctionType::get(VoidTy, false);
    IsIncompleteFunction = true;
  }
  
  llvm::Function *F = llvm::Function::Create(FTy,
                                             llvm::Function::ExternalLinkage,
                                             MangledName, &getModule());
  assert(F->getName() == MangledName && "name was uniqued!");

  if (D.getDefn())
    SetFunctionAttributes(D, F, IsIncompleteFunction);
  if (ExtraAttrs != llvm::Attribute::None)
    F->addFnAttr(ExtraAttrs);

  // This is the first use or definition of a mangled name.  If there is a
  // deferred decl with this name, remember that we need to emit it at the end
  // of the file.
  llvm::StringMap<GlobalDefn>::iterator DDI = DeferredDefns.find(MangledName);
  if (DDI != DeferredDefns.end()) {
    // Move the potentially referenced deferred decl to the DeferredDefnsToEmit
    // list, and remove it from DeferredDefns (since we don't need it anymore).
    DeferredDefnsToEmit.push_back(DDI->second);
    DeferredDefns.erase(DDI);

  // Otherwise, there are cases we have to worry about where we're
  // using a declaration for which we must emit a definition but where
  // we might not find a top-level definition:
  //   - member functions defined inline in their classes
  //   - friend functions defined inline in some class
  //   - special member functions with implicit definitions
  // If we ever change our AST traversal to walk into class methods,
  // this will be unnecessary.
  //
  // We also don't emit a definition for a function if it's going to be an entry
  // in a vtable, unless it's already marked as used.
  } else if (getLangOptions().OOP && D.getDefn()) {
    // Look for a declaration that's lexically in a record.
    const FunctionDefn *FD = cast<FunctionDefn>(D.getDefn());
//    do {
//      if (isa<UserClassDefn>(FD->getDefnContext())) {
//        if (FD->isImplicit() && !ForVTable) {
//          assert(FD->isUsed() && "Sema didn't mark implicit function as used!");
//          DeferredDefnsToEmit.push_back(D.getWithDecl(FD));
//          break;
//        } else {
          DeferredDefnsToEmit.push_back(D/*.getWithDefn(FD)*/);
//          break;
//        }
//      }
//      FD = FD->getPreviousDefinition();
//    } while (FD);
  }

  // Make sure the result is of the requested type.
  if (!IsIncompleteFunction) {
    assert(F->getType()->getElementType() == Ty);
    return F;
  }

  llvm::Type *PTy = llvm::PointerType::getUnqual(Ty);
  return llvm::ConstantExpr::getBitCast(F, PTy);
}

/// GetAddrOfFunction - Return the address of the given function.  If Ty is
/// non-null, then this function will use the specified type if it has to
/// create it (this occurs when we see a definition of the function).
llvm::Constant *CodeGenModule::GetAddrOfFunction(GlobalDefn GD,
                                                 llvm::Type *Ty,
                                                 bool ForVTable) {
  // If there was no specific requested type, just convert it now.
  if (!Ty)
    Ty = getTypes().ConvertType(cast<ValueDefn>(GD.getDefn())->getType());
  
  //FIXME huyabin
  llvm::StringRef MangledName = "_script_yabin_" /*+ getMangledName(GD)*/;
  return GetOrCreateLLVMFunction(MangledName, Ty, GD, ForVTable);
}

/// CreateRuntimeFunction - Create a new runtime function with the specified
/// type and name.
llvm::Constant *
CodeGenModule::CreateRuntimeFunction(llvm::FunctionType *FTy,
                                     llvm::StringRef Name,
                                     llvm::Attributes ExtraAttrs) {
  return GetOrCreateLLVMFunction(Name, FTy, GlobalDefn(), /*ForVTable=*/false,
                                 ExtraAttrs);
}

/// GetAddrOfFunction - Return the address of the given script.
llvm::Constant *CodeGenModule::GetAddrOfScript(const ScriptDefn *SD) {
  return NULL;
}

static bool DeclIsConstantGlobal(ASTContext &Context, const VarDefn *D,
                                 bool ConstantInit) {
//  if (!D->getType().isConstant(Context) && !D->getType()->isReferenceType())
//    return false;
//
//  if (Context.getLangOptions().CPlusPlus) {
//    if (const RecordType *Record
//          = Context.getBaseElementType(D->getType())->getAs<RecordType>())
//      return ConstantInit &&
//             cast<UserClassDefn>(Record->getDefn())->isPOD() &&
//             !cast<UserClassDefn>(Record->getDefn())->hasMutableFields();
//  }
  
  return true;
}

/// GetOrCreateLLVMGlobal - If the specified mangled name is not in the module,
/// create and return an llvm GlobalVariable with the specified type.  If there
/// is something in the module with the specified name, return it potentially
/// bitcasted to the right type.
///
/// If D is non-null, it specifies a decl that correspond to this.  This is used
/// to set the attributes on the global when it is first created.
llvm::Constant *
CodeGenModule::GetOrCreateLLVMGlobal(llvm::StringRef MangledName,
                                     llvm::PointerType *Ty,
                                     const VarDefn *D,
                                     bool UnnamedAddr) {
  // Lookup the entry, lazily creating it if necessary.
  llvm::GlobalValue *Entry = GetGlobalValue(MangledName);
  if (Entry) {
    if (WeakRefReferences.count(Entry)) {
//      if (D && !D->hasAttr<WeakAttr>())
//        Entry->setLinkage(llvm::Function::ExternalLinkage);

      WeakRefReferences.erase(Entry);
    }

    if (UnnamedAddr)
      Entry->setUnnamedAddr(true);

    if (Entry->getType() == Ty)
      return Entry;

    // Make sure the result is of the correct type.
    return llvm::ConstantExpr::getBitCast(Entry, Ty);
  }

  // This is the first use or definition of a mangled name.  If there is a
  // deferred decl with this name, remember that we need to emit it at the end
  // of the file.
  llvm::StringMap<GlobalDefn>::iterator DDI = DeferredDefns.find(MangledName);
  if (DDI != DeferredDefns.end()) {
    // Move the potentially referenced deferred decl to the DeferredDefnsToEmit
    // list, and remove it from DeferredDefns (since we don't need it anymore).
    DeferredDefnsToEmit.push_back(DDI->second);
    DeferredDefns.erase(DDI);
  }

  llvm::GlobalVariable *GV =
    new llvm::GlobalVariable(getModule(), Ty->getElementType(), false,
                             llvm::GlobalValue::ExternalLinkage,
                             0, MangledName, 0,
                             false, Ty->getAddressSpace());

  // Handle things which are present even on external declarations.
  if (D) {
    // FIXME: This code is overly simple and should be merged with other global
    // handling.
    GV->setConstant(DeclIsConstantGlobal(Context, D, false));

    // Set linkage and visibility in case we never see a definition.
    NamedDefn::LinkageInfo LV = D->getLinkageAndVisibility();
    if (LV.linkage() != ExternalLinkage) {
      // Don't set internal linkage on declarations.
    } else {
//      if (D->hasAttr<DLLImportAttr>())
//        GV->setLinkage(llvm::GlobalValue::DLLImportLinkage);
//      else if (D->hasAttr<WeakAttr>() || D->isWeakImported())
//        GV->setLinkage(llvm::GlobalValue::ExternalWeakLinkage);

      // Set visibility on a declaration only if it's explicit.
      if (LV.visibilityExplicit())
        GV->setVisibility(GetLLVMVisibility(LV.visibility()));
    }

//    GV->setThreadLocal(D->isThreadSpecified());
  }

  return GV;
}

llvm::GlobalVariable *
CodeGenModule::CreateOrReplaceCXXRuntimeVariable(llvm::StringRef Name, 
                                      llvm::Type *Ty,
                                      llvm::GlobalValue::LinkageTypes Linkage) {
  llvm::GlobalVariable *GV = getModule().getNamedGlobal(Name);
  llvm::GlobalVariable *OldGV = 0;

  
  if (GV) {
    // Check if the variable has the right type.
    if (GV->getType()->getElementType() == Ty)
      return GV;

    // Because C++ name mangling, the only way we can end up with an already
    // existing global with the same name is if it has been declared extern "C".
      assert(GV->isDeclaration() && "Declaration has wrong type!");
    OldGV = GV;
  }
  
  // Create a new variable.
  GV = new llvm::GlobalVariable(getModule(), Ty, /*isConstant=*/true,
                                Linkage, 0, Name);
  
  if (OldGV) {
    // Replace occurrences of the old variable if needed.
    GV->takeName(OldGV);
    
    if (!OldGV->use_empty()) {
      llvm::Constant *NewPtrForOldDecl =
      llvm::ConstantExpr::getBitCast(GV, OldGV->getType());
      OldGV->replaceAllUsesWith(NewPtrForOldDecl);
    }
    
    OldGV->eraseFromParent();
  }
  
  return GV;
}

/// GetAddrOfGlobalVar - Return the llvm::Constant for the address of the
/// given global variable.  If Ty is non-null and if the global doesn't exist,
/// then it will be greated with the specified type instead of whatever the
/// normal requested type would be.
llvm::Constant *CodeGenModule::GetAddrOfGlobalVar(const VarDefn *D,
                                                  llvm::Type *Ty) {
  assert(D->hasGlobalStorage() && "Not a global variable");
  Type ASTTy = D->getType();
  if (Ty == 0)
    Ty = getTypes().ConvertTypeForMem(ASTTy);

  llvm::PointerType *PTy =
    llvm::PointerType::get(Ty, getContext().getTargetAddressSpace(ASTTy));

  llvm::StringRef MangledName = getMangledName(D);
  return GetOrCreateLLVMGlobal(MangledName, PTy, D);
}

/// CreateRuntimeVariable - Create a new runtime global variable with the
/// specified type and name.
llvm::Constant *
CodeGenModule::CreateRuntimeVariable(llvm::Type *Ty,
                                     llvm::StringRef Name) {
  return GetOrCreateLLVMGlobal(Name, llvm::PointerType::getUnqual(Ty), 0,
                               true);
}

void CodeGenModule::EmitTentativeDefinition(const VarDefn *D) {
  assert(!D->getInit() && "Cannot emit definite definitions here!");

  if (MayDeferGeneration(D)) {
    // If we have not seen a reference to this variable yet, place it
    // into the deferred declarations table to be emitted if needed
    // later.
    llvm::StringRef MangledName = getMangledName(D);
    if (!GetGlobalValue(MangledName)) {
      DeferredDefns[MangledName] = D;
      return;
    }
  }

  // The tentative definition is the only definition.
  EmitGlobalVarDefinition(D);
}

void CodeGenModule::EmitVTable(UserClassDefn *Class, bool DefinitionRequired) {
  if (DefinitionRequired) {
//    getVTables().GenerateClassData(getVTableLinkage(Class), Class);
  }
}

llvm::GlobalVariable::LinkageTypes 
CodeGenModule::getVTableLinkage(const UserClassDefn *RD) {
  if (RD->getLinkage() != ExternalLinkage)
    return llvm::GlobalVariable::InternalLinkage;

  // Silence GCC warning.
  return llvm::GlobalVariable::LinkOnceODRLinkage;
}

CharUnits CodeGenModule::GetTargetTypeStoreSize(llvm::Type *Ty) const {
    return Context.toCharUnitsFromBits(
      TheTargetData.getTypeStoreSizeInBits(Ty));
}

void CodeGenModule::EmitGlobalVarDefinition(const VarDefn *D) {
  llvm::Constant *Init = 0;
  Type ASTTy = D->getType();
  bool NonConstInit = false;

  const Expr *InitExpr = D->getAnyInitializer();
  
  if (!InitExpr) {
    // This is a tentative definition; tentative definitions are
    // implicitly initialized with { 0 }.
    //
    // Note that tentative definitions are only emitted at the end of
    // a translation unit, so they should never have incomplete
    // type. In addition, EmitTentativeDefinition makes sure that we
    // never attempt to emit a tentative definition if a real one
    // exists. A use may still exists, however, so we still may need
    // to do a RAUW.
    //assert(!ASTTy->isIncompleteType() && "Unexpected incomplete type");
    Init = EmitNullConstant(D->getType());
  } else {
    Init = EmitConstantExpr(InitExpr, D->getType());       
    if (!Init) {
      Type T = InitExpr->getType();
      if (D->getType()->isReferenceType())
        T = D->getType();
      
      if (getLangOptions().OOP) {
        Init = EmitNullConstant(T);
        NonConstInit = true;
      } else {
        ErrorUnsupported(D, "static initializer");
        Init = llvm::UndefValue::get(getTypes().ConvertType(T));
      }
    } else {
      // We don't need an initializer, so remove the entry for the delayed
      // initializer position (just in case this entry was delayed).
      if (getLangOptions().OOP)
        DelayedCXXInitPosition.erase(D);
    }
  }

  llvm::Type* InitType = Init->getType();
  llvm::Constant *Entry = GetAddrOfGlobalVar(D, InitType);

  // Strip off a bitcast if we got one back.
  if (llvm::ConstantExpr *CE = dyn_cast<llvm::ConstantExpr>(Entry)) {
    assert(CE->getOpcode() == llvm::Instruction::BitCast ||
           // all zero index gep.
           CE->getOpcode() == llvm::Instruction::GetElementPtr);
    Entry = CE->getOperand(0);
  }

  // Entry is now either a Function or GlobalVariable.
  llvm::GlobalVariable *GV = dyn_cast<llvm::GlobalVariable>(Entry);

  // We have a definition after a declaration with the wrong type.
  // We must make a new GlobalVariable* and update everything that used OldGV
  // (a declaration or tentative definition) with the new GlobalVariable*
  // (which will be a definition).
  //
  // This happens if there is a prototype for a global (e.g.
  // "extern int x[];") and then a definition of a different type (e.g.
  // "int x[10];"). This also happens when an initializer has a different type
  // from the type of the global (this happens with unions).
  if (GV == 0 ||
      GV->getType()->getElementType() != InitType ||
      GV->getType()->getAddressSpace() !=
        getContext().getTargetAddressSpace(ASTTy)) {

    // Move the old entry aside so that we'll create a new one.
    Entry->setName(llvm::StringRef());

    // Make a new global with the correct type, this is now guaranteed to work.
    GV = cast<llvm::GlobalVariable>(GetAddrOfGlobalVar(D, InitType));

    // Replace all uses of the old global with the new global
    llvm::Constant *NewPtrForOldDecl =
        llvm::ConstantExpr::getBitCast(GV, Entry->getType());
    Entry->replaceAllUsesWith(NewPtrForOldDecl);

    // Erase the old global, since it is no longer used.
    cast<llvm::GlobalValue>(Entry)->eraseFromParent();
  }

//  if (const AnnotateAttr *AA = D->getAttr<AnnotateAttr>()) {
//    SourceManager &SM = Context.getSourceManager();
//    AddAnnotation(EmitAnnotateAttr(GV, AA,
//                              SM.getInstantiationLineNumber(D->getLocation())));
//  }

  GV->setInitializer(Init);

  // If it is safe to mark the global 'constant', do so now.
  GV->setConstant(false);
  if (!NonConstInit && DeclIsConstantGlobal(Context, D, true))
    GV->setConstant(true);

  GV->setAlignment(getContext().getDefnAlign(D).getQuantity());
  
  // Set the llvm linkage type as appropriate.
  llvm::GlobalValue::LinkageTypes Linkage = 
    GetLLVMLinkageVarDefinition(D, GV);
  GV->setLinkage(Linkage);
  if (Linkage == llvm::GlobalVariable::CommonLinkage)
    // common vars aren't constant even if declared const.
    GV->setConstant(false);

  SetCommonAttributes(D, GV);

  // Emit the initializer function if necessary.
//  if (NonConstInit)
//    EmitCXXGlobalVarDeclInitFunc(D, GV);

  // Emit global variable debug information.
  if (CGDebugInfo *DI = getModuleDebugInfo()) {
//    DI->setLocation(D->getLocation());
//    DI->EmitGlobalVariable(GV, D);
  }
}

llvm::GlobalValue::LinkageTypes
CodeGenModule::GetLLVMLinkageVarDefinition(const VarDefn *D,
                                           llvm::GlobalVariable *GV) {
//  GVALinkage Linkage = getContext().GetGVALinkageForVariable(D);
//  if (Linkage == GVA_Internal)
//    return llvm::Function::InternalLinkage;
//  else if (D->hasAttr<DLLImportAttr>())
//    return llvm::Function::DLLImportLinkage;
//  else if (D->hasAttr<DLLExportAttr>())
//    return llvm::Function::DLLExportLinkage;
//  else if (D->hasAttr<WeakAttr>()) {
//    if (GV->isConstant())
//      return llvm::GlobalVariable::WeakODRLinkage;
//    else
//      return llvm::GlobalVariable::WeakAnyLinkage;
//  } else if (Linkage == GVA_TemplateInstantiation ||
//             Linkage == GVA_ExplicitTemplateInstantiation)
//    return llvm::GlobalVariable::WeakODRLinkage;
//  else if (!getLangOptions().CPlusPlus &&
//           ((!CodeGenOpts.NoCommon && !D->getAttr<NoCommonAttr>()) ||
//             D->getAttr<CommonAttr>()) &&
//           !D->hasExternalStorage() && !D->getInit() &&
//           !D->getAttr<SectionAttr>() && !D->isThreadSpecified() &&
//           !D->getAttr<WeakImportAttr>()) {
//    // Thread local vars aren't considered common linkage.
//    return llvm::GlobalVariable::CommonLinkage;
//  }
  return llvm::GlobalVariable::ExternalLinkage;
}

/// ReplaceUsesOfNonProtoTypeWithRealFunction - This function is called when we
/// implement a function with no prototype, e.g. "int foo() {}".  If there are
/// existing call uses of the old function in the module, this adjusts them to
/// call the new function directly.
///
/// This is not just a cleanup: the always_inline pass requires direct calls to
/// functions to be able to inline them.  If there is a bitcast in the way, it
/// won't inline them.  Instcombine normally deletes these calls, but it isn't
/// run at -O0.
static void ReplaceUsesOfNonProtoTypeWithRealFunction(llvm::GlobalValue *Old,
                                                      llvm::Function *NewFn) {
  // If we're redefining a global as a function, don't transform it.
  llvm::Function *OldFn = dyn_cast<llvm::Function>(Old);
  if (OldFn == 0) return;

  llvm::Type *NewRetTy = NewFn->getReturnType();
  llvm::SmallVector<llvm::Value*, 4> ArgList;

  for (llvm::Value::use_iterator UI = OldFn->use_begin(), E = OldFn->use_end();
       UI != E; ) {
    // TODO: Do invokes ever occur in C code?  If so, we should handle them too.
    llvm::Value::use_iterator I = UI++; // Increment before the CI is erased.
    llvm::CallInst *CI = dyn_cast<llvm::CallInst>(*I);
    if (!CI) continue; // FIXME: when we allow Invoke, just do CallSite CS(*I)
    llvm::CallSite CS(CI);
    if (!CI || !CS.isCallee(I)) continue;

    // If the return types don't match exactly, and if the call isn't dead, then
    // we can't transform this call.
    if (CI->getType() != NewRetTy && !CI->use_empty())
      continue;

    // If the function was passed too few arguments, don't transform.  If extra
    // arguments were passed, we silently drop them.  If any of the types
    // mismatch, we don't transform.
    unsigned ArgNo = 0;
    bool DontTransform = false;
    for (llvm::Function::arg_iterator AI = NewFn->arg_begin(),
         E = NewFn->arg_end(); AI != E; ++AI, ++ArgNo) {
      if (CS.arg_size() == ArgNo ||
          CS.getArgument(ArgNo)->getType() != AI->getType()) {
        DontTransform = true;
        break;
      }
    }
    if (DontTransform)
      continue;

    // Okay, we can transform this.  Create the new call instruction and copy
    // over the required information.
    ArgList.append(CS.arg_begin(), CS.arg_begin() + ArgNo);
    llvm::CallInst *NewCall = llvm::CallInst::Create(NewFn, ArgList, "", CI);
    ArgList.clear();
    if (!NewCall->getType()->isVoidTy())
      NewCall->takeName(CI);
    NewCall->setAttributes(CI->getAttributes());
    NewCall->setCallingConv(CI->getCallingConv());

    // Finally, remove the old call, replacing any uses with the new one.
    if (!CI->use_empty())
      CI->replaceAllUsesWith(NewCall);

    // Copy debug location attached to CI.
    if (!CI->getDebugLoc().isUnknown())
      NewCall->setDebugLoc(CI->getDebugLoc());
    CI->eraseFromParent();
  }
}

// Convert ScriptDefn to a GlobalDefn, which contains a FuntionDefn that has
// the void f(void) style.
static GlobalDefn ConvertScriptToGlobalFuntion(CodeGenModule &CGM, ScriptDefn* SD) {
  ASTContext &Ctx = CGM.getContext();
  FunctionType::ExtInfo *Info = new FunctionType::ExtInfo(true,
		  false, (unsigned)0, CC_Default, false);
  Type Ty = Ctx.getFunctionNoProtoType(Type(), *Info);
  SourceLocation Loc = SD->getLocStart();
  DefinitionName Name = SD->getDefnName();
  FunctionDefn *FD = FunctionDefn::Create(Ctx, (DefnContext *)0, Loc, Name,
		  Ty);
  BlockCmd *body = SD->getBlockBody();
  FD->setBody(body);
  return GlobalDefn(FD);
}

void CodeGenModule::EmitGlobalScriptDefinition(const ScriptDefn* SD) {
	GlobalDefn GD = ConvertScriptToGlobalFuntion(*this, const_cast<ScriptDefn*>(SD));
	return EmitGlobalFunctionDefinition(GD);
}

void CodeGenModule::EmitGlobalFunctionDefinition(GlobalDefn GD) {
  const FunctionDefn *D = cast<FunctionDefn>(GD.getDefn());

  // Compute the function info and LLVM type.
  const CGFunctionInfo &FI = getTypes().getFunctionInfo(GD);
  bool variadic = false;
  if (const FunctionProtoType *fpt = D->getType()->getAs<FunctionProtoType>())
    variadic = fpt->isVariadic();
  llvm::FunctionType *Ty = getTypes().GetFunctionType(FI, variadic, false);

  // Get or create the prototype for the function.
  llvm::Constant *Entry = GetAddrOfFunction(GD, Ty);

  // Strip off a bitcast if we got one back.
  if (llvm::ConstantExpr *CE = dyn_cast<llvm::ConstantExpr>(Entry)) {
    assert(CE->getOpcode() == llvm::Instruction::BitCast);
    Entry = CE->getOperand(0);
  }


  if (cast<llvm::GlobalValue>(Entry)->getType()->getElementType() != Ty) {
    llvm::GlobalValue *OldFn = cast<llvm::GlobalValue>(Entry);

    // If the types mismatch then we have to rewrite the definition.
    assert(OldFn->isDeclaration() &&
           "Shouldn't replace non-declaration");

    // F is the Function* for the one with the wrong type, we must make a new
    // Function* and update everything that used F (a declaration) with the new
    // Function* (which will be a definition).
    //
    // This happens if there is a prototype for a function
    // (e.g. "int f()") and then a definition of a different type
    // (e.g. "int f(int x)").  Move the old function aside so that it
    // doesn't interfere with GetAddrOfFunction.
    OldFn->setName(llvm::StringRef());
    llvm::Function *NewFn = cast<llvm::Function>(GetAddrOfFunction(GD, Ty));

    // If this is an implementation of a function without a prototype, try to
    // replace any existing uses of the function (which may be calls) with uses
    // of the new function
    if (D->getType()->isFunctionNoProtoType()) {
      ReplaceUsesOfNonProtoTypeWithRealFunction(OldFn, NewFn);
      OldFn->removeDeadConstantUsers();
    }

    // Replace uses of F with the Function we will endow with a body.
    if (!Entry->use_empty()) {
      llvm::Constant *NewPtrForOldDecl =
        llvm::ConstantExpr::getBitCast(NewFn, Entry->getType());
      Entry->replaceAllUsesWith(NewPtrForOldDecl);
    }

    // Ok, delete the old function now, which is dead.
    OldFn->eraseFromParent();

    Entry = NewFn;
  }

  // We need to set linkage and visibility on the function before
  // generating code for it because various parts of IR generation
  // want to propagate this information down (e.g. to local static
  // declarations).
  llvm::Function *Fn = cast<llvm::Function>(Entry);
  setFunctionLinkage(D, Fn);

  // FIXME: this is redundant with part of SetFunctionDefinitionAttributes
  setGlobalVisibility(Fn, D);

  CodeGenFunction(*this).GenerateCode(GD, Fn);

  SetFunctionDefinitionAttributes(D, Fn);
  SetLLVMFunctionAttributesForDefinition(D, Fn);

//  if (const ConstructorAttr *CA = D->getAttr<ConstructorAttr>())
//    AddGlobalCtor(Fn, CA->getPriority());
//  if (const DestructorAttr *DA = D->getAttr<DestructorAttr>())
//    AddGlobalDtor(Fn, DA->getPriority());
}

//void CodeGenModule::EmitAliasDefinition(GlobalDefn GD) {
//  const ValueDefn *D = cast<ValueDefn>(GD.getDefn());
////  const AliasAttr *AA = D->getAttr<AliasAttr>();
////  assert(AA && "Not an alias?");
//
//  llvm::StringRef MangledName = getMangledName(GD);
//
//  // If there is a definition in the module, then it wins over the alias.
//  // This is dubious, but allow it to be safe.  Just ignore the alias.
//  llvm::GlobalValue *Entry = GetGlobalValue(MangledName);
//  if (Entry && !Entry->isDeclaration())
//    return;
//
//  llvm::Type *DeclTy = getTypes().ConvertTypeForMem(D->getType());
//
//  // Create a reference to the named value.  This ensures that it is emitted
//  // if a deferred decl.
//  llvm::Constant *Aliasee;
////  if (isa<llvm::FunctionType>(DeclTy))
////    Aliasee = GetOrCreateLLVMFunction(AA->getAliasee(), DeclTy, GlobalDefn(),
////                                      /*ForVTable=*/false);
////  else
////    Aliasee = GetOrCreateLLVMGlobal(AA->getAliasee(),
////                                    llvm::PointerType::getUnqual(DeclTy), 0);
//
//  // Create the new alias itself, but don't set a name yet.
//  llvm::GlobalValue *GA =
//    new llvm::GlobalAlias(Aliasee->getType(),
//                          llvm::Function::ExternalLinkage,
//                          "", Aliasee, &getModule());
//
//  if (Entry) {
//    assert(Entry->isDeclaration());
//
//    // If there is a declaration in the module, then we had an extern followed
//    // by the alias, as in:
//    //   extern int test6();
//    //   ...
//    //   int test6() __attribute__((alias("test7")));
//    //
//    // Remove it and replace uses of it with the alias.
//    GA->takeName(Entry);
//
//    Entry->replaceAllUsesWith(llvm::ConstantExpr::getBitCast(GA,
//                                                          Entry->getType()));
//    Entry->eraseFromParent();
//  } else {
//    GA->setName(MangledName);
//  }
//
//  // Set attributes which are particular to an alias; this is a
//  // specialization of the attributes which may be set on a global
//  // variable/function.
////  if (D->hasAttr<DLLExportAttr>()) {
////    if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(D)) {
////      // The dllexport attribute is ignored for undefined symbols.
////      if (FD->hasBody())
////        GA->setLinkage(llvm::Function::DLLExportLinkage);
////    } else {
////      GA->setLinkage(llvm::Function::DLLExportLinkage);
////    }
////  } else if (D->hasAttr<WeakAttr>() ||
////             D->hasAttr<WeakRefAttr>() ||
////             D->isWeakImported()) {
////    GA->setLinkage(llvm::Function::WeakAnyLinkage);
////  }
//
//  SetCommonAttributes(D, GA);
//}

/// getBuiltinLibFunction - Given a builtin id for a function like
/// "__builtin_fabsf", return a Function* for "fabsf".
llvm::Value *CodeGenModule::getBuiltinLibFunction(const FunctionDefn *FD,
                                                  unsigned BuiltinID) {
  assert((Context.BuiltinInfo.isLibFunction(BuiltinID) ||
          Context.BuiltinInfo.isPredefinedLibFunction(BuiltinID)) &&
         "isn't a lib fn");

  // Get the name, skip over the __builtin_ prefix (if necessary).
  llvm::StringRef Name;
  GlobalDefn D(FD);

  // If the builtin has been declared explicitly with an assembler label,
  // use the mangled name. This differs from the plain label on platforms
  // that prefix labels.
//  if (FD->hasAttr<AsmLabelAttr>())
//    Name = getMangledName(D);
//  else
	if (Context.BuiltinInfo.isLibFunction(BuiltinID))
    Name = Context.BuiltinInfo.GetName(BuiltinID) + 10;
  else
    Name = Context.BuiltinInfo.GetName(BuiltinID);


  llvm::FunctionType *Ty =
    cast<llvm::FunctionType>(getTypes().ConvertType(FD->getType()));

  return GetOrCreateLLVMFunction(Name, Ty, D, /*ForVTable=*/false);
}

llvm::Function *CodeGenModule::getIntrinsic(unsigned IID,
		llvm::ArrayRef<llvm::Type*> Tys) {
  return llvm::Intrinsic::getDeclaration(&getModule(),
                                         (llvm::Intrinsic::ID)IID, Tys);
}

//static llvm::StringMapEntry<llvm::Constant*> &
//GetConstantCFStringEntry(llvm::StringMap<llvm::Constant*> &Map,
//                         const StringLiteral *Literal,
//                         bool TargetIsLSB,
//                         bool &IsUTF16,
//                         unsigned &StringLength) {
//  llvm::StringRef String = Literal->getString();
//  unsigned NumBytes = String.size();
//
//  // Check for simple case.
//  if (!Literal->containsNonAsciiOrNull()) {
//    StringLength = NumBytes;
//    return Map.GetOrCreateValue(String);
//  }
//
//  // Otherwise, convert the UTF8 literals into a byte string.
//  llvm::SmallVector<UTF16, 128> ToBuf(NumBytes);
//  const UTF8 *FromPtr = (UTF8 *)String.data();
//  UTF16 *ToPtr = &ToBuf[0];
//
//  (void)ConvertUTF8toUTF16(&FromPtr, FromPtr + NumBytes,
//                           &ToPtr, ToPtr + NumBytes,
//                           strictConversion);
//
//  // ConvertUTF8toUTF16 returns the length in ToPtr.
//  StringLength = ToPtr - &ToBuf[0];
//
//  // Render the UTF-16 string into a byte array and convert to the target byte
//  // order.
//  //
//  // FIXME: This isn't something we should need to do here.
//  llvm::SmallString<128> AsBytes;
//  AsBytes.reserve(StringLength * 2);
//  for (unsigned i = 0; i != StringLength; ++i) {
//    unsigned short Val = ToBuf[i];
//    if (TargetIsLSB) {
//      AsBytes.push_back(Val & 0xFF);
//      AsBytes.push_back(Val >> 8);
//    } else {
//      AsBytes.push_back(Val >> 8);
//      AsBytes.push_back(Val & 0xFF);
//    }
//  }
//  // Append one extra null character, the second is automatically added by our
//  // caller.
//  AsBytes.push_back(0);
//
//  IsUTF16 = true;
//  return Map.GetOrCreateValue(llvm::StringRef(AsBytes.data(), AsBytes.size()));
//}

static llvm::StringMapEntry<llvm::Constant*> &
GetConstantStringEntry(llvm::StringMap<llvm::Constant*> &Map,
		       const StringLiteral *Literal,
		       unsigned &StringLength)
{
	llvm::StringRef String = Literal->getString();
	StringLength = String.size();
	return Map.GetOrCreateValue(String);
}

//llvm::Constant *
//CodeGenModule::GetAddrOfConstantString(const StringLiteral *Literal) {
//  unsigned StringLength = 0;
//  llvm::StringMapEntry<llvm::Constant*> &Entry =
//    GetConstantStringEntry(CFConstantStringMap, Literal, StringLength);
//
//  if (llvm::Constant *C = Entry.getValue())
//    return C;
//
//  llvm::Constant *Zero =
//  llvm::Constant::getNullValue(llvm::Type::getInt32Ty(VMContext));
//  llvm::Constant *Zeros[] = { Zero, Zero };
//
//  Type NSTy = getContext().getNSConstantStringType();
//
//  llvm::StructType *STy =
//  cast<llvm::StructType>(getTypes().ConvertType(NSTy));
//
//  std::vector<llvm::Constant*> Fields(3);
//
//  // Class pointer.
//  Fields[0] = ConstantStringClassRef;
//
//  // String pointer.
//  llvm::Constant *C = llvm::ConstantArray::get(VMContext, Entry.getKey().str());
//
//  llvm::GlobalValue::LinkageTypes Linkage;
//  bool isConstant;
//  Linkage = llvm::GlobalValue::PrivateLinkage;
//  isConstant = /*!Features.WritableStrings*/false;
//
//  llvm::GlobalVariable *GV =
//  new llvm::GlobalVariable(getModule(), C->getType(), isConstant, Linkage, C,
//                           ".str");
//  GV->setUnnamedAddr(true);
//  CharUnits Align = getContext().getTypeAlignInChars(getContext().CharTy);
//  GV->setAlignment(Align.getQuantity());
//  Fields[1] = llvm::ConstantExpr::getGetElementPtr(GV, Zeros, 2);
//
//  // String length.
//  llvm::Type *Ty = getTypes().ConvertType(getContext().UInt32Ty);
//  Fields[2] = llvm::ConstantInt::get(Ty, StringLength);
//
//  // The struct.
//  C = llvm::ConstantStruct::get(STy, Fields);
//  GV = new llvm::GlobalVariable(getModule(), C->getType(), true,
//                                llvm::GlobalVariable::PrivateLinkage, C,
//                                "_unnamed_nsstring_");
//  // FIXME. Fix section.
//  if (const char *Sect =
//        getContext().Target.getNSStringSection())
//    GV->setSection(Sect);
//  Entry.setValue(GV);
//
//  return GV;
//}

/// GetStringForStringLiteral - Return the appropriate bytes for a
/// string literal, properly padded to match the literal type.
std::string CodeGenModule::GetStringForStringLiteral(const StringLiteral *E) {
//  const ASTContext &Context = getContext();
//  const VectorType *CAT =
//    Context.getAsVectorType(E->getType());
//  assert(CAT && "String isn't pointer or array!");
//
//  // Resize the string to the right size.
//  uint64_t RealLen = CAT->getSize().getZExtValue();
//
//  if (E->isWide())
//    RealLen *= Context.Target.getWCharWidth() / Context.getCharWidth();

  std::string Str = E->getString().str();
//  Str.resize(RealLen, '\0');

  return Str;
}

/// GetAddrOfConstantStringFromLiteral - Return a pointer to a
/// constant array for the given string literal.
llvm::Constant *
CodeGenModule::GetAddrOfConstantStringFromLiteral(const StringLiteral *S) {
  // FIXME: This can be more efficient.
  // FIXME: We shouldn't need to bitcast the constant in the wide string case.
  llvm::Constant *C = GetAddrOfConstantString(GetStringForStringLiteral(S));
  if (S->isWide()) {
    llvm::Type *DestTy =
        llvm::PointerType::getUnqual(getTypes().ConvertType(S->getType()));
    C = llvm::ConstantExpr::getBitCast(C, DestTy);
  }
  return C;
}

/// GenerateWritableString -- Creates storage for a string literal.
static llvm::Constant *GenerateStringLiteral(llvm::StringRef str,
                                             bool constant,
                                             CodeGenModule &CGM,
                                             const char *GlobalName) {
  // Create Constant for this string literal. Don't add a '\0'.
  llvm::Constant *C =
      llvm::ConstantDataArray::getString(CGM.getLLVMContext(), str, false);
  // Create a global variable for this string
  llvm::GlobalVariable *GV =
    new llvm::GlobalVariable(CGM.getModule(), C->getType(), constant,
                             llvm::GlobalValue::PrivateLinkage,
                             C, GlobalName);
  GV->setAlignment(1);
  GV->setUnnamedAddr(true);
  return GV;
}

/// GetAddrOfConstantString - Returns a pointer to a character array
/// containing the literal. This contents are exactly that of the
/// given string, i.e. it will not be null terminated automatically;
/// see GetAddrOfConstantCString. Note that whether the result is
/// actually a pointer to an LLVM constant depends on
/// Feature.WriteableStrings.
///
/// The result has pointer to array type.
llvm::Constant *CodeGenModule::GetAddrOfConstantString(llvm::StringRef Str,
                                                       const char *GlobalName) {
  bool IsConstant = /*!Features.WritableStrings*/false;

  // Get the default prefix if a name wasn't specified.
  if (!GlobalName)
    GlobalName = ".str";

  // Don't share any string literals if strings aren't constant.
  if (!IsConstant)
    return GenerateStringLiteral(Str, false, *this, GlobalName);

  llvm::StringMapEntry<llvm::Constant *> &Entry =
    ConstantStringMap.GetOrCreateValue(Str);

  if (Entry.getValue())
    return Entry.getValue();

  // Create a global variable for this.
  llvm::Constant *C = GenerateStringLiteral(Str, true, *this, GlobalName);
  Entry.setValue(C);
  return C;
}

/// GetAddrOfConstantCString - Returns a pointer to a character
/// array containing the literal and a terminating '\0'
/// character. The result has pointer to array type.
llvm::Constant *CodeGenModule::GetAddrOfConstantCString(const std::string &Str,
                                                        const char *GlobalName){
  llvm::StringRef StrWithNull(Str.c_str(), Str.size() + 1);
  return GetAddrOfConstantString(StrWithNull, GlobalName);
}

/// EmitNamespace - Emit all declarations in a namespace.
void CodeGenModule::EmitNamespace(const NamespaceDefn *ND) {
  for (TypeDefn::defn_iterator I = ND->defns_begin(), E = ND->defns_end();
       I != E; ++I)
    EmitTopLevelDefn(*I);
}

/// EmitTopLevelDefn - Emit code for a single top level declaration.
void CodeGenModule::EmitTopLevelDefn(Defn *D) {
  // If an error has occurred, stop code generation, but continue
  // parsing and semantic analysis (to ensure all warnings and errors
  // are emitted).
  if (Diags.hasErrorOccurred())
    return;

  // Ignore dependent declarations.
//  if (D->getDefnContext() && D->getDefnContext()->isDependentContext())
//    return;

  switch (D->getKind()) {
  case Defn::Script:
	  EmitGlobal(cast<ScriptDefn>(D));
	  break;
  case Defn::ClassMethod:
  case Defn::Function:
    EmitGlobal(cast<FunctionDefn>(D));
    break;
      
  case Defn::Var:
    EmitGlobal(cast<VarDefn>(D));
    break;

  // C++ Decls
  case Defn::Namespace:
    EmitNamespace(cast<NamespaceDefn>(D));
    break;
  case Defn::ClassConstructor:
//    EmitClassConstructors(cast<ClassConstructorDefn>(D));
    break;
  case Defn::ClassDestructor:
//    EmitClassDestructors(cast<ClassDestructorDefn>(D));
    break;
  default:
    // Make sure we handled everything we should, every other kind is a
    // non-top-level decl.  FIXME: Would be nice to have an isTopLevelDeclKind
    // function. Need to recode Defn::Kind to do that easily.
    assert(isa<TypeDefn>(D) && "Unsupported decl kind");
  }
}

/// Turns the given pointer into a constant.
static llvm::Constant *GetPointerConstant(llvm::LLVMContext &Context,
                                          const void *Ptr) {
  uintptr_t PtrInt = reinterpret_cast<uintptr_t>(Ptr);
  llvm::Type *i64 = llvm::Type::getInt64Ty(Context);
  return llvm::ConstantInt::get(i64, PtrInt);
}

static void EmitGlobalDefnMetadata(CodeGenModule &CGM,
                                   llvm::NamedMDNode *&GlobalMetadata,
                                   GlobalDefn D,
                                   llvm::GlobalValue *Addr) {
  if (!GlobalMetadata)
    GlobalMetadata =
      CGM.getModule().getOrInsertNamedMetadata("mlang.global.decl.ptrs");

  // TODO: should we report variant information for ctors/dtors?
  llvm::Value *Ops[] = {
    Addr,
    GetPointerConstant(CGM.getLLVMContext(), D.getDefn())
  };
  GlobalMetadata->addOperand(llvm::MDNode::get(CGM.getLLVMContext(), Ops));
}

/// Emits metadata nodes associating all the global values in the
/// current module with the Decls they came from.  This is useful for
/// projects using IR gen as a subroutine.
///
/// Since there's currently no way to associate an MDNode directly
/// with an llvm::GlobalValue, we create a global named metadata
/// with the name 'mlang.global.decl.ptrs'.
void CodeGenModule::EmitDefnMetadata() {
  llvm::NamedMDNode *GlobalMetadata = 0;

  // StaticLocalDefnMap
  for (llvm::DenseMap<GlobalDefn,llvm::StringRef>::iterator
         I = MangledDefnNames.begin(), E = MangledDefnNames.end();
       I != E; ++I) {
    llvm::GlobalValue *Addr = getModule().getNamedValue(I->second);
    EmitGlobalDefnMetadata(*this, GlobalMetadata, I->first, Addr);
  }
}

/// Emits metadata nodes for all the local variables in the current
/// function.
void CodeGenFunction::EmitDefnMetadata() {
  if (LocalDefnMap.empty()) return;

  llvm::LLVMContext &Context = getLLVMContext();

  // Find the unique metadata ID for this name.
  unsigned DeclPtrKind = Context.getMDKindID("mlang.defn.ptr");

  llvm::NamedMDNode *GlobalMetadata = 0;

  for (llvm::DenseMap<const Defn*, llvm::Value*>::iterator
         I = LocalDefnMap.begin(), E = LocalDefnMap.end(); I != E; ++I) {
    const Defn *D = I->first;
    llvm::Value *Addr = I->second;

    if (llvm::AllocaInst *Alloca = dyn_cast<llvm::AllocaInst>(Addr)) {
      llvm::Value *DAddr = GetPointerConstant(getLLVMContext(), D);
      Alloca->setMetadata(DeclPtrKind, llvm::MDNode::get(Context, DAddr));
    } else if (llvm::GlobalValue *GV = dyn_cast<llvm::GlobalValue>(Addr)) {
      GlobalDefn GD = GlobalDefn(cast<VarDefn>(D));
      EmitGlobalDefnMetadata(CGM, GlobalMetadata, GD, GV);
    }
  }
}

void CodeGenModule::EmitCoverageFile() {
  if (!getCodeGenOpts().CoverageFile.empty()) {
    if (llvm::NamedMDNode *CUNode = TheModule.getNamedMetadata("llvm.dbg.cu")) {
      llvm::NamedMDNode *GCov = TheModule.getOrInsertNamedMetadata("llvm.gcov");
      llvm::LLVMContext &Ctx = TheModule.getContext();
      llvm::MDString *CoverageFile =
          llvm::MDString::get(Ctx, getCodeGenOpts().CoverageFile);
      for (int i = 0, e = CUNode->getNumOperands(); i != e; ++i) {
        llvm::MDNode *CU = CUNode->getOperand(i);
        llvm::Value *node[] = { CoverageFile, CU };
        llvm::MDNode *N = llvm::MDNode::get(Ctx, node);
        GCov->addOperand(N);
      }
    }
  }
}

///@name Custom Runtime Function Interfaces
///@{
//
// FIXME: These can be eliminated once we can have clients just get the required
// AST nodes from the builtin tables.

llvm::Constant *CodeGenModule::getBlockObjectDispose() {
  if (BlockObjectDispose)
    return BlockObjectDispose;

  // If we saw an explicit defn, use that.
  if (BlockObjectDisposeDefn) {
    return BlockObjectDispose = GetAddrOfFunction(
      BlockObjectDisposeDefn,
      getTypes().GetFunctionType(BlockObjectDisposeDefn));
  }

  // Otherwise construct the function by hand.
  llvm::Type *args[] = { Int8PtrTy, Int32Ty };
  llvm::FunctionType *fty
    = llvm::FunctionType::get(VoidTy, args, false);
  return BlockObjectDispose =
    CreateRuntimeFunction(fty, "_Block_object_dispose");
}

llvm::Constant *CodeGenModule::getBlockObjectAssign() {
  if (BlockObjectAssign)
    return BlockObjectAssign;

  // If we saw an explicit defn, use that.
  if (BlockObjectAssignDefn) {
    return BlockObjectAssign = GetAddrOfFunction(
      BlockObjectAssignDefn,
      getTypes().GetFunctionType(BlockObjectAssignDefn));
  }

  // Otherwise construct the function by hand.
  llvm::Type *args[] = { Int8PtrTy, Int8PtrTy, Int32Ty };
  llvm::FunctionType *fty
    = llvm::FunctionType::get(VoidTy, args, false);
  return BlockObjectAssign =
    CreateRuntimeFunction(fty, "_Block_object_assign");
}

llvm::Constant *CodeGenModule::getNSConcreteGlobalBlock() {
  if (NSConcreteGlobalBlock)
    return NSConcreteGlobalBlock;

  // If we saw an explicit defn, use that.
  if (NSConcreteGlobalBlockDefn) {
    return NSConcreteGlobalBlock = GetAddrOfGlobalVar(
      NSConcreteGlobalBlockDefn,
      getTypes().ConvertType(NSConcreteGlobalBlockDefn->getType()));
  }

  // Otherwise construct the variable by hand.
  return NSConcreteGlobalBlock =
    CreateRuntimeVariable(Int8PtrTy, "_NSConcreteGlobalBlock");
}

llvm::Constant *CodeGenModule::getNSConcreteStackBlock() {
  if (NSConcreteStackBlock)
    return NSConcreteStackBlock;

  // If we saw an explicit decl, use that.
  if (NSConcreteStackBlockDefn) {
    return NSConcreteStackBlock = GetAddrOfGlobalVar(
      NSConcreteStackBlockDefn,
      getTypes().ConvertType(NSConcreteStackBlockDefn->getType()));
  }

  // Otherwise construct the variable by hand.
  return NSConcreteStackBlock =
    CreateRuntimeVariable(Int8PtrTy, "_NSConcreteStackBlock");
}

///@}
