//===--- CGDebugInfo.cpp - Emit Debug Information for a Module ------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This coordinates the debug information generation while generating code.
//
//===----------------------------------------------------------------------===//

#include "CGDebugInfo.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGBlocks.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/FileManager.h"
//#include "mlang/Basic/Version.h"
#include "mlang/Frontend/CodeGenOptions.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Intrinsics.h"
#include "llvm/Module.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/Support/Path.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
using namespace mlang;
using namespace mlang::CodeGen;

CGDebugInfo::CGDebugInfo(CodeGenModule &CGM)
  : CGM(CGM), DBuilder(CGM.getModule()),
    BlockLiteralGenericSet(false) {
  CreateCompileUnit();
}

CGDebugInfo::~CGDebugInfo() {
  assert(RegionStack.empty() && "Region stack mismatch, stack not empty!");
}

void CGDebugInfo::setLocation(SourceLocation Loc) {
  if (Loc.isValid())
    CurLoc = CGM.getContext().getSourceManager().getInstantiationLoc(Loc);
}

/// getContextDescriptor - Get context info for the decl.
llvm::DIDescriptor CGDebugInfo::getContextDescriptor(const Defn *Context) {
  if (!Context)
    return TheCU;

  llvm::DenseMap<const Defn *, llvm::WeakVH>::iterator
    I = RegionMap.find(Context);
  if (I != RegionMap.end())
    return llvm::DIDescriptor(dyn_cast_or_null<llvm::MDNode>(&*I->second));

  // Check namespace.
  if (const NamespaceDefn *NSDecl = dyn_cast<NamespaceDefn>(Context))
    return llvm::DIDescriptor(getOrCreateNameSpace(NSDecl));

  if (const TypeDefn *RDefn = dyn_cast<TypeDefn>(Context)) {
    llvm::DIType Ty = getOrCreateType(CGM.getContext().getTypeDefnType(RDefn),
                                      getOrCreateMainFile());
    return llvm::DIDescriptor(Ty);

  }
  return TheCU;
}

/// getFunctionName - Get function name for the given FunctionDefn. If the
/// name is constructred on demand (e.g. C++ destructor) then the name
/// is stored on the side.
llvm::StringRef CGDebugInfo::getFunctionName(const FunctionDefn *FD) {
  assert (FD && "Invalid FunctionDefn!");
  IdentifierInfo *FII = FD->getIdentifier();
  if (FII)
    return FII->getName();

  // Otherwise construct human readable name for debug info.
  std::string NS = FD->getNameAsString();

  // Copy this name on the side and use its reference.
  char *StrPtr = DebugInfoNames.Allocate<char>(NS.length());
  memcpy(StrPtr, NS.data(), NS.length());
  return llvm::StringRef(StrPtr, NS.length());
}

/// getHeteroContainerName - Get class name including template argument list.
llvm::StringRef 
CGDebugInfo::getHeteroContainerName(TypeDefn *RD) {
  std::string Buffer;

  Buffer = RD->getIdentifier()->getNameStart();
  PrintingPolicy Policy(CGM.getLangOptions());

  // Copy this name on the side and use its reference.
  char *StrPtr = DebugInfoNames.Allocate<char>(Buffer.length());
  memcpy(StrPtr, Buffer.data(), Buffer.length());
  return llvm::StringRef(StrPtr, Buffer.length());
}

/// getOrCreateFile - Get the file debug info descriptor for the input location.
llvm::DIFile CGDebugInfo::getOrCreateFile(SourceLocation Loc) {
  if (!Loc.isValid())
    // If Location is not valid then use main input file.
    return DBuilder.createFile(TheCU.getFilename(), TheCU.getDirectory());

  SourceManager &SM = CGM.getContext().getSourceManager();
  PresumedLoc PLoc = SM.getPresumedLoc(Loc);

  if (PLoc.isInvalid() || llvm::StringRef(PLoc.getFilename()).empty())
    // If the location is not valid then use main input file.
    return DBuilder.createFile(TheCU.getFilename(), TheCU.getDirectory());

  // Cache the results.
  const char *fname = PLoc.getFilename();
  llvm::DenseMap<const char *, llvm::WeakVH>::iterator it =
    DIFileCache.find(fname);

  if (it != DIFileCache.end()) {
    // Verify that the information still exists.
    if (&*it->second)
      return llvm::DIFile(cast<llvm::MDNode>(it->second));
  }

  llvm::DIFile F = DBuilder.createFile(PLoc.getFilename(), getCurrentDirname());

  DIFileCache[fname] = F;
  return F;

}

/// getOrCreateMainFile - Get the file info for main compile unit.
llvm::DIFile CGDebugInfo::getOrCreateMainFile() {
  return DBuilder.createFile(TheCU.getFilename(), TheCU.getDirectory());
}

/// getLineNumber - Get line number for the location. If location is invalid
/// then use current location.
unsigned CGDebugInfo::getLineNumber(SourceLocation Loc) {
  assert (CurLoc.isValid() && "Invalid current location!");
  SourceManager &SM = CGM.getContext().getSourceManager();
  PresumedLoc PLoc = SM.getPresumedLoc(Loc.isValid() ? Loc : CurLoc);
  return PLoc.isValid()? PLoc.getLine() : 0;
}

/// getColumnNumber - Get column number for the location. If location is 
/// invalid then use current location.
unsigned CGDebugInfo::getColumnNumber(SourceLocation Loc) {
  assert (CurLoc.isValid() && "Invalid current location!");
  SourceManager &SM = CGM.getContext().getSourceManager();
  PresumedLoc PLoc = SM.getPresumedLoc(Loc.isValid() ? Loc : CurLoc);
  return PLoc.isValid()? PLoc.getColumn() : 0;
}

llvm::StringRef CGDebugInfo::getCurrentDirname() {
  if (!CWDName.empty())
    return CWDName;
  char *CompDirnamePtr = NULL;
  llvm::sys::Path CWD = llvm::sys::Path::GetCurrentDirectory();
  CompDirnamePtr = DebugInfoNames.Allocate<char>(CWD.size());
  memcpy(CompDirnamePtr, CWD.c_str(), CWD.size());
  return CWDName = llvm::StringRef(CompDirnamePtr, CWD.size());
}

/// CreateCompileUnit - Create new compile unit.
void CGDebugInfo::CreateCompileUnit() {

  // Get absolute path name.
  SourceManager &SM = CGM.getContext().getSourceManager();
  std::string MainFileName = CGM.getCodeGenOpts().MainFileName;
  if (MainFileName.empty())
    MainFileName = "<unknown>";

  // The main file name provided via the "-main-file-name" option contains just
  // the file name itself with no path information. This file name may have had
  // a relative path, so we look into the actual file entry for the main
  // file to determine the real absolute path for the file.
  std::string MainFileDir;
  if (const FileEntry *MainFile = SM.getFileEntryForID(SM.getMainFileID())) {
    MainFileDir = MainFile->getDir()->getName();
    if (MainFileDir != ".")
      MainFileName = MainFileDir + "/" + MainFileName;
  }

  // Save filename string.
  char *FilenamePtr = DebugInfoNames.Allocate<char>(MainFileName.length());
  memcpy(FilenamePtr, MainFileName.c_str(), MainFileName.length());
  llvm::StringRef Filename(FilenamePtr, MainFileName.length());
  
  unsigned LangTag;
  const LangOptions &LO = CGM.getLangOptions();


  std::string Producer = "";

  // Figure out which version of the ObjC runtime we have.
  unsigned RuntimeVers = 0;

  // Create new compile unit.
  DBuilder.createCompileUnit(
    LangTag, Filename, getCurrentDirname(),
    Producer,
    LO.Optimize, CGM.getCodeGenOpts().DwarfDebugFlags, RuntimeVers);
  // FIXME - Eliminate TheCU.
  TheCU = llvm::DICompileUnit(DBuilder.getCU());
}

/// CreateType - Get the Basic type from the cache or create a new
/// one if necessary.
llvm::DIType CGDebugInfo::CreateType(const SimpleNumericType *BT) {
  unsigned Encoding = 0;
  const char *BTName = NULL;
  switch (BT->getKind()) {
  default:
  case SimpleNumericType::UInt8:
  case SimpleNumericType::UInt16:
  case SimpleNumericType::UInt32:
  case SimpleNumericType::UInt64:   Encoding = llvm::dwarf::DW_ATE_unsigned; break;
  case SimpleNumericType::Int8:
  case SimpleNumericType::Int16:
  case SimpleNumericType::Int32:
  case SimpleNumericType::Int64:    Encoding = llvm::dwarf::DW_ATE_signed; break;
  case SimpleNumericType::Logical:  Encoding = llvm::dwarf::DW_ATE_boolean; break;
  case SimpleNumericType::Single:
  case SimpleNumericType::Double:   Encoding = llvm::dwarf::DW_ATE_float; break;
  }

  BTName = BT->getName();

  // Bit size, align and offset of the type.
  uint64_t Size = CGM.getContext().getTypeSize(BT);
  uint64_t Align = CGM.getContext().getTypeAlign(BT);
  llvm::DIType DbgTy = 
    DBuilder.createBasicType(BTName, Size, Align, Encoding);
  return DbgTy;
}

/// CreateCVRType - Get the qualified type from the cache or create
/// a new one if necessary.
llvm::DIType CGDebugInfo::CreateQualifiedType(Type Ty, llvm::DIFile Unit) {
//  QualifierCollector Qc;
//  const RawType *T = Qc.strip(Ty);
//
//  // Ignore these qualifiers for now.
//  Qc.removeObjCGCAttr();
//  Qc.removeAddressSpace();
//  Qc.removeObjCLifetime();
//
//  // We will create one Derived type for one qualifier and recurse to handle any
//  // additional ones.
//  unsigned Tag;
//  if (Qc.hasConst()) {
//    Tag = llvm::dwarf::DW_TAG_const_type;
//    Qc.removeConst();
//  } else if (Qc.hasVolatile()) {
//    Tag = llvm::dwarf::DW_TAG_volatile_type;
//    Qc.removeVolatile();
//  } else if (Qc.hasRestrict()) {
//    Tag = llvm::dwarf::DW_TAG_restrict_type;
//    Qc.removeRestrict();
//  } else {
//    assert(Qc.empty() && "Unknown type qualifier for debug info");
//    return getOrCreateType(Type(T, 0), Unit);
//  }
//
//  llvm::DIType FromTy = getOrCreateType(Qc.apply(CGM.getContext(), T), Unit);
//
//  // No need to fill in the Name, Line, Size, Alignment, Offset in case of
//  // CVR derived types.
//  llvm::DIType DbgTy = DBuilder.createQualifiedType(Tag, FromTy);
//
//  return DbgTy;
  return llvm::DIType();
}

/// CreatePointeeType - Create PointTee type. If Pointee is a record
/// then emit record's fwd if debug info size reduction is enabled.
llvm::DIType CGDebugInfo::CreatePointeeType(Type PointeeTy,
                                            llvm::DIFile Unit) {
  if (!CGM.getCodeGenOpts().LimitDebugInfo)
    return getOrCreateType(PointeeTy, Unit);

  if (const HeteroContainerType *RTy = dyn_cast<HeteroContainerType>(PointeeTy)) {
    TypeDefn *RD = RTy->getDefn();
    llvm::DIFile DefUnit = getOrCreateFile(RD->getLocation());
    unsigned Line = getLineNumber(RD->getLocation());
    llvm::DIDescriptor FDContext =
      getContextDescriptor(cast<Defn>(RD->getDefnContext()));

    if (RD->isStruct())
      return DBuilder.createStructType(FDContext, RD->getName(), DefUnit,
                                       Line, 0, 0, llvm::DIType::FlagFwdDecl,
                                       llvm::DIArray());
//    else if (RD->isUnion())
//      return DBuilder.createUnionType(FDContext, RD->getName(), DefUnit,
//                                      Line, 0, 0, llvm::DIType::FlagFwdDecl,
//                                      llvm::DIArray());
    else {
      assert(RD->isClassdef() && "Unknown RecordType!");
      return DBuilder.createClassType(FDContext, RD->getName(), DefUnit,
                                      Line, 0, 0, 0, llvm::DIType::FlagFwdDecl,
                                      llvm::DIType(), llvm::DIArray());
    }
  }
  return getOrCreateType(PointeeTy, Unit);

}

llvm::DIType CGDebugInfo::CreatePointerLikeType(unsigned Tag,
                                                const RawType *Ty,
                                                Type PointeeTy,
                                                llvm::DIFile Unit) {

  if (Tag == llvm::dwarf::DW_TAG_reference_type)
    return DBuilder.createReferenceType(CreatePointeeType(PointeeTy, Unit));
                                    
  // Bit size, align and offset of the type.
  // Size is always the size of a pointer. We can't use getTypeSize here
  // because that does not return the correct value for references.
  unsigned AS = CGM.getContext().getTargetAddressSpace(PointeeTy);
  uint64_t Size = CGM.getContext().Target.getPointerWidth(AS);
  uint64_t Align = CGM.getContext().getTypeAlign(Ty);

  return 
    DBuilder.createPointerType(CreatePointeeType(PointeeTy, Unit), Size, Align);
}

llvm::DIType CGDebugInfo::CreateType(const FunctionType *Ty,
                                     llvm::DIFile Unit) {
  llvm::SmallVector<llvm::Value *, 16> EltTys;

  // Add the result type at least.
  EltTys.push_back(getOrCreateType(Ty->getResultType(), Unit));

  // Set up remainder of arguments if there is a prototype.
  // FIXME: IF NOT, HOW IS THIS REPRESENTED?  llvm-gcc doesn't represent '...'!
  if (isa<FunctionNoProtoType>(Ty))
    EltTys.push_back(DBuilder.createUnspecifiedParameter());
  else if (const FunctionProtoType *FTP = dyn_cast<FunctionProtoType>(Ty)) {
    for (unsigned i = 0, e = FTP->getNumArgs(); i != e; ++i)
      EltTys.push_back(getOrCreateType(FTP->getArgType(i), Unit));
  }

  llvm::DIArray EltTypeArray = DBuilder.getOrCreateArray(EltTys);

  llvm::DIType DbgTy = DBuilder.createSubroutineType(Unit, EltTypeArray);
  return DbgTy;
}

llvm::DIType CGDebugInfo::createFieldType(llvm::StringRef name,
                                          Type type,
                                          Expr *bitWidth,
                                          SourceLocation loc,
                                          AccessSpecifier AS,
                                          uint64_t offsetInBits,
                                          llvm::DIFile tunit,
                                          llvm::DIDescriptor scope) {
  llvm::DIType debugType = getOrCreateType(type, tunit);

  // Get the location for the field.
  llvm::DIFile file = getOrCreateFile(loc);
  unsigned line = getLineNumber(loc);

  uint64_t sizeInBits = 0;
  unsigned alignInBits = 0;
//  if (!type->isIncompleteArrayType()) {
//    llvm::tie(sizeInBits, alignInBits) = CGM.getContext().getTypeInfo(type);

    if (bitWidth)
      sizeInBits = bitWidth->EvaluateAsInt(CGM.getContext()).getZExtValue();
 // }

  unsigned flags = 0;
  if (AS == mlang::AS_private)
    flags |= llvm::DIDescriptor::FlagPrivate;
  else if (AS == mlang::AS_protected)
    flags |= llvm::DIDescriptor::FlagProtected;

  return DBuilder.createMemberType(scope, name, file, line, sizeInBits,
                                   alignInBits, offsetInBits, flags, debugType);
}

/// CollectRecordFields - A helper function to collect debug info for
/// record fields. This is used while creating debug info entry for a Record.
void CGDebugInfo::
CollectRecordFields(const UserClassDefn *record, llvm::DIFile tunit,
                    llvm::SmallVectorImpl<llvm::Value *> &elements,
                    llvm::DIType RecordTy) {
}

/// getOrCreateMethodType - ClassMethodDefn type is a FunctionType. This
/// function type is not updated to include implicit "this" pointer. Use this
/// routine to get a method type which includes "this" pointer.
llvm::DIType
CGDebugInfo::getOrCreateMethodType(const ClassMethodDefn *Method,
                                   llvm::DIFile Unit) {
  llvm::DIType FnTy
    = getOrCreateType(Type(Method->getType()->getAs<FunctionProtoType>(),
                               0),
                      Unit);
  
  // Add "this" pointer.

  llvm::DIArray Args = llvm::DICompositeType(FnTy).getTypeArray();
  assert (Args.getNumElements() && "Invalid number of arguments!");

  llvm::SmallVector<llvm::Value *, 16> Elts;

  // First element is always return type. For 'void' functions it is NULL.
  Elts.push_back(Args.getElement(0));

//  if (!Method->isStatic())
//  {
//        // "this" pointer is always first argument.
//        Type ThisPtr = Method->getThisType(CGM.getContext());
//        llvm::DIType ThisPtrType =
//          DBuilder.createArtificialType(getOrCreateType(ThisPtr, Unit));
//
//        TypeCache[ThisPtr.getAsOpaquePtr()] = ThisPtrType;
//        Elts.push_back(ThisPtrType);
//    }

  // Copy rest of the arguments.
  for (unsigned i = 1, e = Args.getNumElements(); i != e; ++i)
    Elts.push_back(Args.getElement(i));

  llvm::DIArray EltTypeArray = DBuilder.getOrCreateArray(Elts);

  return DBuilder.createSubroutineType(Unit, EltTypeArray);
}

/// isFunctionLocalClass - Return true if CXXRecordDecl is defined 
/// inside a function.
static bool isFunctionLocalClass(const UserClassDefn *RD) {
  if (const UserClassDefn *NRD =
      dyn_cast<UserClassDefn>(RD->getDefnContext()))
    return isFunctionLocalClass(NRD);
  else if (isa<FunctionDefn>(RD->getDefnContext()))
    return true;
  return false;
  
}
/// CreateClassMemberFunction - A helper function to create a DISubprogram for
/// a single member function GlobalDecl.
llvm::DISubprogram
CGDebugInfo::CreateClassMemberFunction(const ClassMethodDefn *Method,
                                     llvm::DIFile Unit,
                                     llvm::DIType RecordTy) {
  bool IsCtorOrDtor = 
    isa<ClassConstructorDefn>(Method) || isa<ClassDestructorDefn>(Method);
  
  llvm::StringRef MethodName = getFunctionName(Method);
  llvm::DIType MethodTy = getOrCreateMethodType(Method, Unit);
  
  // Since a single ctor/dtor corresponds to multiple functions, it doesn't
  // make sense to give a single ctor/dtor a linkage name.
  llvm::StringRef MethodLinkageName;
  if (!IsCtorOrDtor && !isFunctionLocalClass(
		  cast<UserClassDefn>(Method->getParent())))
    MethodLinkageName = CGM.getMangledName(Method);

  // Get the location for the method.
  llvm::DIFile MethodDefUnit = getOrCreateFile(Method->getLocation());
  unsigned MethodLine = getLineNumber(Method->getLocation());

  // Collect virtual method info.
  llvm::DIType ContainingType;
  unsigned Virtuality = 0; 
  unsigned VIndex = 0;
  
//  if (Method->isVirtual()) {
//    if (Method->isPure())
//      Virtuality = llvm::dwarf::DW_VIRTUALITY_pure_virtual;
//    else
//      Virtuality = llvm::dwarf::DW_VIRTUALITY_virtual;
//
//    // It doesn't make sense to give a virtual destructor a vtable index,
//    // since a single destructor has two entries in the vtable.
//    if (!isa<ClassDestructorDefn>(Method))
//      VIndex = CGM.getVTables().getMethodVTableIndex(Method);
//    ContainingType = RecordTy;
//  }

  unsigned Flags = 0;
  if (Method->isImplicit())
    Flags |= llvm::DIDescriptor::FlagArtificial;
  AccessSpecifier Access = Method->getAccess();
  if (Access == mlang::AS_private)
    Flags |= llvm::DIDescriptor::FlagPrivate;
  else if (Access == mlang::AS_protected)
    Flags |= llvm::DIDescriptor::FlagProtected;
  if (const ClassConstructorDefn *CXXC = dyn_cast<ClassConstructorDefn>(Method)) {
//    if (CXXC->isExplicit())
//      Flags |= llvm::DIDescriptor::FlagExplicit;
  }
//  if (Method->hasPrototype())
    Flags |= llvm::DIDescriptor::FlagPrototyped;
    
  llvm::DISubprogram SP =
    DBuilder.createMethod(RecordTy , MethodName, MethodLinkageName, 
                          MethodDefUnit, MethodLine,
                          MethodTy, /*isLocalToUnit=*/false, 
                          /* isDefinition=*/ false,
                          Virtuality, VIndex, ContainingType,
                          Flags, CGM.getLangOptions().Optimize);
  
  SPCache[Method] = llvm::WeakVH(SP);

  return SP;
}

/// CollectCXXMemberFunctions - A helper function to collect debug info for
/// C++ member functions.This is used while creating debug info entry for 
/// a Record.
void CGDebugInfo::
CollectClassMemberFunctions(const UserClassDefn *RD, llvm::DIFile Unit,
                          llvm::SmallVectorImpl<llvm::Value *> &EltTys,
                          llvm::DIType RecordTy) {
  for(UserClassDefn::method_iterator I = RD->method_begin(),
        E = RD->method_end(); I != E; ++I) {
    const ClassMethodDefn *Method = *I;
    
//    if (Method->isImplicit() && !Method->isUsed())
//      continue;

    EltTys.push_back(CreateClassMemberFunction(Method, Unit, RecordTy));
  }
}                                 

/// CollectClassBases - A helper function to collect debug info for
/// C++ base classes. This is used while creating debug info entry for 
/// a Record.
void CGDebugInfo::
CollectClassBases(const UserClassDefn *RD, llvm::DIFile Unit,
                llvm::SmallVectorImpl<llvm::Value *> &EltTys,
                llvm::DIType RecordTy) {

  const ASTRecordLayout &RL = CGM.getContext().getASTRecordLayout(RD);
  for (UserClassDefn::base_class_const_iterator BI = RD->bases_begin(),
         BE = RD->bases_end(); BI != BE; ++BI) {
    unsigned BFlags = 0;
    uint64_t BaseOffset;
    
    const UserClassDefn *Base =
      cast<UserClassDefn>(BI->getType()->getAs<ClassdefType>()->getDefn());
    
    if (BI->isVirtual()) {
      // virtual base offset offset is -ve. The code generator emits dwarf
      // expression where it expects +ve number.
      BaseOffset = 0;
      //FIXME huyabin
        //0 - CGM.getVTables().getVirtualBaseOffsetOffset(RD, Base).getQuantity();
      BFlags = llvm::DIDescriptor::FlagVirtual;
    } else
      BaseOffset = RL.getBaseClassOffsetInBits(Base);
    // FIXME: Inconsistent units for BaseOffset. It is in bytes when
    // BI->isVirtual() and bits when not.
    
    AccessSpecifier Access = BI->getAccessSpecifier();
    if (Access == mlang::AS_private)
      BFlags |= llvm::DIDescriptor::FlagPrivate;
    else if (Access == mlang::AS_protected)
      BFlags |= llvm::DIDescriptor::FlagProtected;
    
    llvm::DIType DTy = 
      DBuilder.createInheritance(RecordTy,                                     
                                 getOrCreateType(BI->getType(), Unit),
                                 BaseOffset, BFlags);
    EltTys.push_back(DTy);
  }
}

/// getOrCreateVTablePtrType - Return debug info descriptor for vtable.
llvm::DIType CGDebugInfo::getOrCreateVTablePtrType(llvm::DIFile Unit) {
  if (VTablePtrType.isValid())
    return VTablePtrType;

  ASTContext &Context = CGM.getContext();

  /* Function type */
  llvm::Value *STy = getOrCreateType(Context.Int32Ty, Unit);
  llvm::DIArray SElements = DBuilder.getOrCreateArray(STy);
  llvm::DIType SubTy = DBuilder.createSubroutineType(Unit, SElements);
  unsigned Size = Context.getTypeSize(Context.Int32Ty);
  llvm::DIType vtbl_ptr_type = DBuilder.createPointerType(SubTy, Size, 0,
                                                          "__vtbl_ptr_type");
  VTablePtrType = DBuilder.createPointerType(vtbl_ptr_type, Size);
  return VTablePtrType;
}

/// getVTableName - Get vtable name for the given Class.
llvm::StringRef CGDebugInfo::getVTableName(const UserClassDefn *RD) {
  // Otherwise construct gdb compatible name name.
  std::string Name = "_vptr$" + RD->getNameAsString();

  // Copy this name on the side and use its reference.
  char *StrPtr = DebugInfoNames.Allocate<char>(Name.length());
  memcpy(StrPtr, Name.data(), Name.length());
  return llvm::StringRef(StrPtr, Name.length());
}


/// CollectVTableInfo - If the C++ class has vtable info then insert appropriate
/// debug info entry in EltTys vector.
void CGDebugInfo::
CollectVTableInfo(const UserClassDefn *RD, llvm::DIFile Unit,
                  llvm::SmallVectorImpl<llvm::Value *> &EltTys) {
  const ASTRecordLayout &RL = CGM.getContext().getASTRecordLayout(RD);

  // If there is a primary base then it will hold vtable info.
  if (RL.getPrimaryBase())
    return;

  // If this class is not dynamic then there is not any vtable info to collect.
//  if (!RD->isDynamicClass())
//    return;

  unsigned Size = CGM.getContext().getTypeSize(CGM.getContext().Int32Ty);
  llvm::DIType VPTR
    = DBuilder.createMemberType(Unit, getVTableName(RD), Unit,
                                0, Size, 0, 0, 0, 
                                getOrCreateVTablePtrType(Unit));
  EltTys.push_back(VPTR);
}

/// getOrCreateRecordType - Emit record type's standalone debug info. 
llvm::DIType CGDebugInfo::getOrCreateRecordType(Type RTy,
                                                SourceLocation Loc) {
  llvm::DIType T =  getOrCreateType(RTy, getOrCreateFile(Loc));
  DBuilder.retainType(T);
  return T;
}

/// CreateType - get structure or union type.
llvm::DIType CGDebugInfo::CreateType(const HeteroContainerType *Ty) {
  TypeDefn *RD = Ty->getDefn();
  llvm::DIFile Unit = getOrCreateFile(RD->getLocation());

  // Get overall information about the record type for the debug info.
  llvm::DIFile DefUnit = getOrCreateFile(RD->getLocation());
  unsigned Line = getLineNumber(RD->getLocation());

  // Records and classes and unions can all be recursive.  To handle them, we
  // first generate a debug descriptor for the struct as a forward declaration.
  // Then (if it is a definition) we go through and get debug info for all of
  // its members.  Finally, we create a descriptor for the complete type (which
  // may refer to the forward decl if the struct is recursive) and replace all
  // uses of the forward declaration with the final definition.
  llvm::DIDescriptor FDContext =
    getContextDescriptor(cast<Defn>(RD->getDefnContext()));

  llvm::DIType FwdDecl = DBuilder.createTemporaryType(DefUnit);

  llvm::MDNode *MN = FwdDecl;
  llvm::TrackingVH<llvm::MDNode> FwdDeclNode = MN;
  // Otherwise, insert it into the TypeCache so that recursive uses will find
  // it.
  TypeCache[Type(Ty, 0).getAsOpaquePtr()] = FwdDecl;
  // Push the struct on region stack.
  RegionStack.push_back(FwdDeclNode);
  RegionMap[Ty->getDefn()] = llvm::WeakVH(FwdDecl);

  // Convert all the elements.
  llvm::SmallVector<llvm::Value *, 16> EltTys;

  const UserClassDefn *CXXDecl = dyn_cast<UserClassDefn>(RD);
  if (CXXDecl) {
    CollectClassBases(CXXDecl, Unit, EltTys, FwdDecl);
    CollectVTableInfo(CXXDecl, Unit, EltTys);
  }
  
  // Collect static variables with initializers.
  for (UserClassDefn::defn_iterator I = RD->defns_begin(), E = RD->defns_end();
       I != E; ++I)
    if (const VarDefn *V = dyn_cast<VarDefn>(*I)) {
      if (const Expr *Init = V->getInit()) {
        Expr::EvalResult Result;
        if (Init->Evaluate(Result, CGM.getContext()) && Result.Val.isInt()) {
          llvm::ConstantInt *CI 
            = llvm::ConstantInt::get(CGM.getLLVMContext(), Result.Val.getInt());
          
          // Create the descriptor for static variable.
          llvm::DIFile VUnit = getOrCreateFile(V->getLocation());
          llvm::StringRef VName = V->getName();
          llvm::DIType VTy = getOrCreateType(V->getType(), VUnit);
          // Do not use DIGlobalVariable for enums.
          if (VTy.getTag() != llvm::dwarf::DW_TAG_enumeration_type) {
            DBuilder.createStaticVariable(FwdDecl, VName, VName, VUnit,
                                          getLineNumber(V->getLocation()),
                                          VTy, true, CI);
          }
        }
      }
    }

  CollectRecordFields(cast<UserClassDefn>(RD), Unit, EltTys, FwdDecl);
  llvm::DIArray TParamsArray;
  if (CXXDecl) {
    CollectClassMemberFunctions(CXXDecl, Unit, EltTys, FwdDecl);
  }

  RegionStack.pop_back();
  llvm::DenseMap<const Defn *, llvm::WeakVH>::iterator RI =
    RegionMap.find(Ty->getDefn());
  if (RI != RegionMap.end())
    RegionMap.erase(RI);

  llvm::DIDescriptor RDContext =  
    getContextDescriptor(cast<Defn>(RD->getDefnContext()));
  llvm::StringRef RDName = RD->getName();
  uint64_t Size = CGM.getContext().getTypeSize(Ty);
  uint64_t Align = CGM.getContext().getTypeAlign(Ty);
  llvm::DIArray Elements = DBuilder.getOrCreateArray(EltTys);
  llvm::MDNode *RealDecl = NULL;

  if (CXXDecl) {
    RDName = getHeteroContainerName(RD);
     // A class's primary base or the class itself contains the vtable.
    llvm::MDNode *ContainingType = NULL;
    const ASTRecordLayout &RL = CGM.getContext().getASTRecordLayout(RD);
    if (const UserClassDefn *PBase = RL.getPrimaryBase()) {
      // Seek non virtual primary base root.
      while (1) {
        const ASTRecordLayout &BRL = CGM.getContext().getASTRecordLayout(PBase);
        const UserClassDefn *PBT = BRL.getPrimaryBase();
        if (PBT /*&& !BRL.isPrimaryBaseVirtual()*/)
          PBase = PBT;
        else 
          break;
      }
      ContainingType = 
        getOrCreateType(Type(PBase->getTypeForDefn(), 0), Unit);
    }
//    else if (CXXDecl->isDynamicClass())
//      ContainingType = FwdDecl;

   RealDecl = DBuilder.createClassType(RDContext, RDName, DefUnit, Line,
                                       Size, Align, 0, 0, llvm::DIType(),
                                       Elements, ContainingType,
                                       TParamsArray);
  } else 
    RealDecl = DBuilder.createStructType(RDContext, RDName, DefUnit, Line,
                                         Size, Align, 0, Elements);

  // Now that we have a real decl for the struct, replace anything using the
  // old decl with the new one.  This will recursively update the debug info.
  llvm::DIType(FwdDeclNode).replaceAllUsesWith(RealDecl);
  RegionMap[RD] = llvm::WeakVH(RealDecl);
  return llvm::DIType(RealDecl);
}

llvm::DIType CGDebugInfo::CreateType(const VectorType *Ty,
                                     llvm::DIFile Unit) {
  llvm::DIType ElementTy = getOrCreateType(Ty->getElementType(), Unit);
  int64_t NumElems = Ty->getNumElements();
  int64_t LowerBound = 0;
  if (NumElems == 0)
    // If number of elements are not known then this is an unbounded array.
    // Use Low = 1, Hi = 0 to express such arrays.
    LowerBound = 1;
  else
    --NumElems;

  llvm::Value *Subscript = DBuilder.getOrCreateSubrange(LowerBound, NumElems);
  llvm::DIArray SubscriptArray = DBuilder.getOrCreateArray(Subscript);

  uint64_t Size = CGM.getContext().getTypeSize(Ty);
  uint64_t Align = CGM.getContext().getTypeAlign(Ty);

  return
    DBuilder.createVectorType(Size, Align, ElementTy, SubscriptArray);
}

llvm::DIType CGDebugInfo::CreateType(const ArrayType *Ty,
                                     llvm::DIFile Unit) {
  uint64_t Size;
  uint64_t Align;

  // Size and align of the whole array, not the element type.
  Size = CGM.getContext().getTypeSize(Ty);
  Align = CGM.getContext().getTypeAlign(Ty);

  // Add the dimensions of the array.  FIXME: This loses CV qualifiers from
  // interior arrays, do we care?  Why aren't nested arrays represented the
  // obvious/recursive way?
  llvm::SmallVector<llvm::Value *, 8> Subscripts;
  Type EltTy(Ty, 0);
  while ((Ty = dyn_cast<ArrayType>(EltTy))) {
	  int64_t UpperBound = 0;
		int64_t LowerBound = 0;
		// This is an unbounded array. Use Low = 1, Hi = 0 to express such
		// arrays.
		LowerBound = 1;

		// FIXME: Verify this is right for VLAs.
		Subscripts.push_back(DBuilder.getOrCreateSubrange(LowerBound,
				UpperBound));
		EltTy = Ty->getElementType();
  }


  llvm::DIArray SubscriptArray = DBuilder.getOrCreateArray(Subscripts);

  llvm::DIType DbgTy = 
    DBuilder.createArrayType(Size, Align, getOrCreateType(EltTy, Unit),
                             SubscriptArray);
  return DbgTy;
}

llvm::DIType CGDebugInfo::CreateType(const LValueReferenceType *Ty, 
                                     llvm::DIFile Unit) {
  return CreatePointerLikeType(llvm::dwarf::DW_TAG_reference_type, 
                               Ty, Ty->getPointeeType(), Unit);
}

llvm::DIType CGDebugInfo::CreateType(const RValueReferenceType *Ty, 
                                     llvm::DIFile Unit) {
  return CreatePointerLikeType(llvm::dwarf::DW_TAG_rvalue_reference_type, 
                               Ty, Ty->getPointeeType(), Unit);
}

/// getOrCreateType - Get the type from the cache or create a new
/// one if necessary.
llvm::DIType CGDebugInfo::getOrCreateType(Type Ty,
                                          llvm::DIFile Unit) {
  if (Ty.isNull())
    return llvm::DIType();

  // Check for existing entry.
  llvm::DenseMap<void *, llvm::WeakVH>::iterator it =
    TypeCache.find(Ty.getAsOpaquePtr());
  if (it != TypeCache.end()) {
    // Verify that the debug info still exists.
    if (&*it->second)
      return llvm::DIType(cast<llvm::MDNode>(it->second));
  }

  // Otherwise create the type.
  llvm::DIType Res = CreateTypeNode(Ty, Unit);

  // And update the type cache.
  TypeCache[Ty.getAsOpaquePtr()] = Res;  
  return Res;
}

/// CreateTypeNode - Create a new debug type node.
llvm::DIType CGDebugInfo::CreateTypeNode(Type Ty,
                                         llvm::DIFile Unit) {
  // Handle qualifiers, which recursively handles what they refer to.
  if (Ty.hasTypeInfo())
    return CreateQualifiedType(Ty, Unit);

  const char *Diag = 0;
  
  // Work out details of type.
  switch (Ty->getTypeClass()) {
  // FIXME: Handle these.
  case RawType::Vector:
    return CreateType(cast<VectorType>(Ty), Unit);
  case RawType::SimpleNumeric:
	  return CreateType(cast<SimpleNumericType>(Ty));
  case RawType::Struct:
  case RawType::Cell:
  case RawType::Map:
  case RawType::Classdef:
    return CreateType(cast<HeteroContainerType>(Ty));
  case RawType::FunctionHandle:
  case RawType::FunctionProto:
  case RawType::FunctionNoProto:
    return CreateType(cast<FunctionType>(Ty), Unit);
  case RawType::NDArray:
  case RawType::Matrix:
    return CreateType(cast<ArrayType>(Ty), Unit);
  case RawType::LValueReference:
    return CreateType(cast<LValueReferenceType>(Ty), Unit);
  case RawType::RValueReference:
    return CreateType(cast<RValueReferenceType>(Ty), Unit);
  }
  
  assert(Diag && "Fall through without a diagnostic?");
  unsigned DiagID = CGM.getDiags().getCustomDiagID(Diagnostic::Error,
                               "debug information for %0 is not yet supported");
  CGM.getDiags().Report(DiagID)
    << Diag;
  return llvm::DIType();
}

/// CreateMemberType - Create new member and increase Offset by FType's size.
llvm::DIType CGDebugInfo::CreateMemberType(llvm::DIFile Unit, Type FType,
                                           llvm::StringRef Name,
                                           uint64_t *Offset) {
  llvm::DIType FieldTy = CGDebugInfo::getOrCreateType(FType, Unit);
  uint64_t FieldSize = CGM.getContext().getTypeSize(FType);
  unsigned FieldAlign = CGM.getContext().getTypeAlign(FType);
  llvm::DIType Ty = DBuilder.createMemberType(Unit, Name, Unit, 0,
                                              FieldSize, FieldAlign,
                                              *Offset, 0, FieldTy);
  *Offset += FieldSize;
  return Ty;
}

/// getFunctionDeclaration - Return debug info descriptor to describe method
/// declaration for the given method definition.
llvm::DISubprogram CGDebugInfo::getFunctionDefinition(const Defn *D) {
  const FunctionDefn *FD = dyn_cast<FunctionDefn>(D);
  if (!FD) return llvm::DISubprogram();

  // Setup context.
  getContextDescriptor(cast<Defn>(D->getDefnContext()));

  llvm::DenseMap<const FunctionDefn *, llvm::WeakVH>::iterator
    MI = SPCache.find(FD);
  if (MI != SPCache.end()) {
    llvm::DISubprogram SP(dyn_cast_or_null<llvm::MDNode>(&*MI->second));
    if (SP.isSubprogram() && !llvm::DISubprogram(SP).isDefinition())
      return SP;
  }

  return llvm::DISubprogram();
}

// getOrCreateFunctionType - Construct DIType. If it is a c++ method, include
// implicit parameter "this".
llvm::DIType CGDebugInfo::getOrCreateFunctionType(const Defn * D, Type FnType,
                                                  llvm::DIFile F) {
  if (const ClassMethodDefn *Method = dyn_cast<ClassMethodDefn>(D))
    return getOrCreateMethodType(Method, F);

  return getOrCreateType(FnType, F);
}

/// EmitFunctionStart - Constructs the debug code for entering a function -
/// "llvm.dbg.func.start.".
void CGDebugInfo::EmitFunctionStart(GlobalDefn GD, Type FnType,
                                    llvm::Function *Fn,
                                    CGBuilderTy &Builder) {

  llvm::StringRef Name;
  llvm::StringRef LinkageName;

  FnBeginRegionCount.push_back(RegionStack.size());

  const Defn *D = GD.getDefn();

  unsigned Flags = 0;
  llvm::DIFile Unit = getOrCreateFile(CurLoc);
  llvm::DIDescriptor FDContext(Unit);
  if (const FunctionDefn *FD = dyn_cast<FunctionDefn>(D)) {
    // If there is a DISubprogram for  this function available then use it.
    llvm::DenseMap<const FunctionDefn *, llvm::WeakVH>::iterator
      FI = SPCache.find(FD);
    if (FI != SPCache.end()) {
      llvm::DIDescriptor SP(dyn_cast_or_null<llvm::MDNode>(&*FI->second));
      if (SP.isSubprogram() && llvm::DISubprogram(SP).isDefinition()) {
        llvm::MDNode *SPN = SP;
        RegionStack.push_back(SPN);
        RegionMap[D] = llvm::WeakVH(SP);
        return;
      }
    }
    Name = getFunctionName(FD);
    // Use mangled name as linkage name for c/c++ functions.
    if (!Fn->hasInternalLinkage())
      LinkageName = CGM.getMangledName(GD);
    if (LinkageName == Name)
      LinkageName = llvm::StringRef();
    if (const UserClassDefn *RDecl =
             dyn_cast_or_null<UserClassDefn>(FD->getDefnContext()))
      FDContext = getContextDescriptor(cast<Defn>(RDecl->getDefnContext()));

  } else {
    // Use llvm function name.
    Name = Fn->getName();
    Flags |= llvm::DIDescriptor::FlagPrototyped;
  }
  if (!Name.empty() && Name[0] == '\01')
    Name = Name.substr(1);

  // It is expected that CurLoc is set before using EmitFunctionStart.
  // Usually, CurLoc points to the left bracket location of compound
  // statement representing function body.
  unsigned LineNo = getLineNumber(CurLoc);
  if (D->isImplicit())
    Flags |= llvm::DIDescriptor::FlagArtificial;
//  llvm::DISubprogram SPDecl = getFunctionDefinition(D);
//  llvm::DISubprogram SP =
//    DBuilder.createFunction(FDContext, Name, LinkageName, Unit,
//                            LineNo, getOrCreateFunctionType(D, FnType, Unit),
//                            Fn->hasInternalLinkage(), true/*definition*/,
//                            Flags, CGM.getLangOptions().Optimize, Fn,
//                            TParamsArray, SPDecl);
//
//  // Push function on region stack.
//  llvm::MDNode *SPN = SP;
//  RegionStack.push_back(SPN);
//  RegionMap[D] = llvm::WeakVH(SP);

  // Clear stack used to keep track of #line directives.
  LineDirectiveFiles.clear();
}


void CGDebugInfo::EmitStopPoint(CGBuilderTy &Builder) {
  if (CurLoc.isInvalid() || CurLoc.isMacroID()) return;

  // Don't bother if things are the same as last time.
  SourceManager &SM = CGM.getContext().getSourceManager();
  if (CurLoc == PrevLoc
       || (SM.getInstantiationLineNumber(CurLoc) ==
           SM.getInstantiationLineNumber(PrevLoc)
           && SM.isFromSameFile(CurLoc, PrevLoc)))
    // New Builder may not be in sync with CGDebugInfo.
    if (!Builder.getCurrentDebugLocation().isUnknown())
      return;

  // Update last state.
  PrevLoc = CurLoc;

  llvm::MDNode *Scope = RegionStack.back();
  Builder.SetCurrentDebugLocation(llvm::DebugLoc::get(getLineNumber(CurLoc),
                                                      getColumnNumber(CurLoc),
                                                      Scope));
}

/// UpdateLineDirectiveRegion - Update region stack only if #line directive
/// has introduced scope change.
void CGDebugInfo::UpdateLineDirectiveRegion(CGBuilderTy &Builder) {
  if (CurLoc.isInvalid() || CurLoc.isMacroID() ||
      PrevLoc.isInvalid() || PrevLoc.isMacroID())
    return;
  SourceManager &SM = CGM.getContext().getSourceManager();
  PresumedLoc PCLoc = SM.getPresumedLoc(CurLoc);
  PresumedLoc PPLoc = SM.getPresumedLoc(PrevLoc);

  if (PCLoc.isInvalid() || PPLoc.isInvalid() ||
      !strcmp(PPLoc.getFilename(), PCLoc.getFilename()))
    return;

  // If #line directive stack is empty then we are entering a new scope.
  if (LineDirectiveFiles.empty()) {
    EmitRegionStart(Builder);
    LineDirectiveFiles.push_back(PCLoc.getFilename());
    return;
  }

  assert (RegionStack.size() >= LineDirectiveFiles.size()
          && "error handling  #line regions!");

  bool SeenThisFile = false;
  // Chek if current file is already seen earlier.
  for(std::vector<const char *>::iterator I = LineDirectiveFiles.begin(),
        E = LineDirectiveFiles.end(); I != E; ++I)
    if (!strcmp(PCLoc.getFilename(), *I)) {
      SeenThisFile = true;
      break;
    }

  // If #line for this file is seen earlier then pop out #line regions.
  if (SeenThisFile) {
    while (!LineDirectiveFiles.empty()) {
      const char *LastFile = LineDirectiveFiles.back();
      RegionStack.pop_back();
      LineDirectiveFiles.pop_back();
      if (!strcmp(PPLoc.getFilename(), LastFile))
        break;
    }
    return;
  } 

  // .. otherwise insert new #line region.
  EmitRegionStart(Builder);
  LineDirectiveFiles.push_back(PCLoc.getFilename());

  return;
}
/// EmitRegionStart- Constructs the debug code for entering a declarative
/// region - "llvm.dbg.region.start.".
void CGDebugInfo::EmitRegionStart(CGBuilderTy &Builder) {
  llvm::DIDescriptor D =
    DBuilder.createLexicalBlock(RegionStack.empty() ? 
                                llvm::DIDescriptor() : 
                                llvm::DIDescriptor(RegionStack.back()),
                                getOrCreateFile(CurLoc),
                                getLineNumber(CurLoc), 
                                getColumnNumber(CurLoc));
  llvm::MDNode *DN = D;
  RegionStack.push_back(DN);
}

/// EmitRegionEnd - Constructs the debug code for exiting a declarative
/// region - "llvm.dbg.region.end."
void CGDebugInfo::EmitRegionEnd(CGBuilderTy &Builder) {
  assert(!RegionStack.empty() && "Region stack mismatch, stack empty!");

  // Provide an region stop point.
  EmitStopPoint(Builder);

  RegionStack.pop_back();
}

/// EmitFunctionEnd - Constructs the debug code for exiting a function.
void CGDebugInfo::EmitFunctionEnd(CGBuilderTy &Builder) {
  assert(!RegionStack.empty() && "Region stack mismatch, stack empty!");
  unsigned RCount = FnBeginRegionCount.back();
  assert(RCount <= RegionStack.size() && "Region stack mismatch");

  // Pop all regions for this function.
  while (RegionStack.size() != RCount)
    EmitRegionEnd(Builder);
  FnBeginRegionCount.pop_back();
}

// EmitTypeForVarWithBlocksAttr - Build up structure info for the byref.  
// See BuildByRefType.
llvm::DIType CGDebugInfo::EmitTypeForVarWithBlocksAttr(const ValueDefn *VD,
                                                       uint64_t *XOffset) {

  llvm::SmallVector<llvm::Value *, 5> EltTys;
  Type FType;
  uint64_t FieldSize, FieldOffset;
  unsigned FieldAlign;
  
  llvm::DIFile Unit = getOrCreateFile(VD->getLocation());
  Type Type = VD->getType();

  FieldOffset = 0;
//  FType = CGM.getContext().getPointerType(CGM.getContext().VoidTy);
//  EltTys.push_back(CreateMemberType(Unit, FType, "__isa", &FieldOffset));
//  EltTys.push_back(CreateMemberType(Unit, FType, "__forwarding", &FieldOffset));
  FType = CGM.getContext().Int32Ty;
  EltTys.push_back(CreateMemberType(Unit, FType, "__flags", &FieldOffset));
  EltTys.push_back(CreateMemberType(Unit, FType, "__size", &FieldOffset));

//  bool HasCopyAndDispose = CGM.getContext().BlockRequiresCopying(Type);
//  if (HasCopyAndDispose) {
//    FType = CGM.getContext().getPointerType(CGM.getContext().VoidTy);
//    EltTys.push_back(CreateMemberType(Unit, FType, "__copy_helper",
//                                      &FieldOffset));
//    EltTys.push_back(CreateMemberType(Unit, FType, "__destroy_helper",
//                                      &FieldOffset));
//  }
  
  CharUnits Align = CGM.getContext().getDefnAlign(VD);
  if (Align > CGM.getContext().toCharUnitsFromBits(
        CGM.getContext().Target.getPointerAlign(0))) {
    CharUnits FieldOffsetInBytes 
      = CGM.getContext().toCharUnitsFromBits(FieldOffset);
    CharUnits AlignedOffsetInBytes
      = FieldOffsetInBytes.RoundUpToAlignment(Align);
    CharUnits NumPaddingBytes
      = AlignedOffsetInBytes - FieldOffsetInBytes;
    
    if (NumPaddingBytes.isPositive()) {
      llvm::APInt pad(32, NumPaddingBytes.getQuantity());
//      FType = CGM.getContext().getType(CGM.getContext().CharTy,
//                                                    pad, ArrayType::Normal, 0);
      EltTys.push_back(CreateMemberType(Unit, FType, "", &FieldOffset));
    }
  }
  
  FType = Type;
  llvm::DIType FieldTy = CGDebugInfo::getOrCreateType(FType, Unit);
  FieldSize = CGM.getContext().getTypeSize(FType);
  FieldAlign = CGM.getContext().toBits(Align);

  *XOffset = FieldOffset;  
  FieldTy = DBuilder.createMemberType(Unit, VD->getName(), Unit,
                                      0, FieldSize, FieldAlign,
                                      FieldOffset, 0, FieldTy);
  EltTys.push_back(FieldTy);
  FieldOffset += FieldSize;
  
  llvm::DIArray Elements = DBuilder.getOrCreateArray(EltTys);
  
  unsigned Flags = llvm::DIDescriptor::FlagBlockByrefStruct;
  
  return DBuilder.createStructType(Unit, "", Unit, 0, FieldOffset, 0, Flags,
                                   Elements);
}

/// EmitDeclare - Emit local variable declaration debug info.
void CGDebugInfo::EmitDeclare(const VarDefn *VD, unsigned Tag,
                              llvm::Value *Storage, 
                              unsigned ArgNo, CGBuilderTy &Builder) {
  assert(!RegionStack.empty() && "Region stack mismatch, stack empty!");

  llvm::DIFile Unit = getOrCreateFile(VD->getLocation());
  llvm::DIType Ty;
  uint64_t XOffset = 0;
//  if (VD->hasAttr<BlocksAttr>())
//    Ty = EmitTypeForVarWithBlocksAttr(VD, &XOffset);
//  else
    Ty = getOrCreateType(VD->getType(), Unit);

  // If there is not any debug info for type then do not emit debug info
  // for this variable.
  if (!Ty)
    return;

  if (llvm::Argument *Arg = dyn_cast<llvm::Argument>(Storage)) {
    // If Storage is an aggregate returned as 'sret' then let debugger know
    // about this.
    if (Arg->hasStructRetAttr())
      Ty = DBuilder.createReferenceType(Ty);
//    else if (UserClassDefn *Record = VD->getType()->getAsUserClassDefn()) {
//      // If an aggregate variable has non trivial destructor or non trivial copy
//      // constructor than it is pass indirectly. Let debug info know about this
//      // by using reference of the aggregate type as a argument type.
//      if (!Record->hasTrivialCopyConstructor() || !Record->hasTrivialDestructor())
//        Ty = DBuilder.createReferenceType(Ty);
//    }
  }
      
  // Get location information.
  unsigned Line = getLineNumber(VD->getLocation());
  unsigned Column = getColumnNumber(VD->getLocation());
  unsigned Flags = 0;
  if (VD->isImplicit())
    Flags |= llvm::DIDescriptor::FlagArtificial;
  llvm::MDNode *Scope = RegionStack.back();
    
  llvm::StringRef Name = VD->getName();
  if (!Name.empty()) {
//    if (VD->hasAttr<BlocksAttr>()) {
//      CharUnits offset = CharUnits::fromQuantity(32);
//      llvm::SmallVector<llvm::Value *, 9> addr;
//      llvm::Type *Int64Ty = llvm::Type::getInt64Ty(CGM.getLLVMContext());
//      addr.push_back(llvm::ConstantInt::get(Int64Ty, llvm::DIBuilder::OpPlus));
//      // offset of __forwarding field
//      offset = CGM.getContext().toCharUnitsFromBits(
//        CGM.getContext().Target.getPointerWidth(0));
//      addr.push_back(llvm::ConstantInt::get(Int64Ty, offset.getQuantity()));
//      addr.push_back(llvm::ConstantInt::get(Int64Ty, llvm::DIBuilder::OpDeref));
//      addr.push_back(llvm::ConstantInt::get(Int64Ty, llvm::DIBuilder::OpPlus));
//      // offset of x field
//      offset = CGM.getContext().toCharUnitsFromBits(XOffset);
//      addr.push_back(llvm::ConstantInt::get(Int64Ty, offset.getQuantity()));
//
//      // Create the descriptor for the variable.
//      llvm::DIVariable D =
//        DBuilder.createComplexVariable(Tag,
//                                       llvm::DIDescriptor(RegionStack.back()),
//                                       VD->getName(), Unit, Line, Ty,
//                                       addr, ArgNo);
//
//      // Insert an llvm.dbg.declare into the current block.
//      llvm::Instruction *Call =
//        DBuilder.insertDeclare(Storage, D, Builder.GetInsertBlock());
//
//      Call->setDebugLoc(llvm::DebugLoc::get(Line, Column, Scope));
//      return;
//    }
      // Create the descriptor for the variable.
    llvm::DIVariable D =
      DBuilder.createLocalVariable(Tag, llvm::DIDescriptor(Scope), 
                                   Name, Unit, Line, Ty, 
                                   CGM.getLangOptions().Optimize, Flags, ArgNo);
    
    // Insert an llvm.dbg.declare into the current block.
    llvm::Instruction *Call =
      DBuilder.insertDeclare(Storage, D, Builder.GetInsertBlock());
    
    Call->setDebugLoc(llvm::DebugLoc::get(Line, Column, Scope));
    return;
  }
  
  // If VD is an anonymous union then Storage represents value for
  // all union fields.
//  if (const RecordType *RT = dyn_cast<RecordType>(VD->getType())) {
//    const RecordDecl *RD = cast<RecordDecl>(RT->getDefn());
//    if (RD->isUnion()) {
//      for (RecordDecl::field_iterator I = RD->field_begin(),
//             E = RD->field_end();
//           I != E; ++I) {
//        FieldDecl *Field = *I;
//        llvm::DIType FieldTy = getOrCreateType(Field->getType(), Unit);
//        llvm::StringRef FieldName = Field->getName();
//
//        // Ignore unnamed fields. Do not ignore unnamed records.
//        if (FieldName.empty() && !isa<RecordType>(Field->getType()))
//          continue;
//
//        // Use VarDecl's Tag, Scope and Line number.
//        llvm::DIVariable D =
//          DBuilder.createLocalVariable(Tag, llvm::DIDescriptor(Scope),
//                                       FieldName, Unit, Line, FieldTy,
//                                       CGM.getLangOptions().Optimize, Flags,
//                                       ArgNo);
//
//        // Insert an llvm.dbg.declare into the current block.
//        llvm::Instruction *Call =
//          DBuilder.insertDeclare(Storage, D, Builder.GetInsertBlock());
//
//        Call->setDebugLoc(llvm::DebugLoc::get(Line, Column, Scope));
//      }
//    }
//  }
}

void CGDebugInfo::EmitDeclareOfAutoVariable(const VarDefn *VD,
                                            llvm::Value *Storage,
                                            CGBuilderTy &Builder) {
  EmitDeclare(VD, llvm::dwarf::DW_TAG_auto_variable, Storage, 0, Builder);
}

/// EmitDeclareOfArgVariable - Emit call to llvm.dbg.declare for an argument
/// variable declaration.
void CGDebugInfo::EmitDeclareOfArgVariable(const VarDefn *VD, llvm::Value *AI,
                                           unsigned ArgNo,
                                           CGBuilderTy &Builder) {
  EmitDeclare(VD, llvm::dwarf::DW_TAG_arg_variable, AI, ArgNo, Builder);
}

/// EmitGlobalVariable - Emit information about a global variable.
void CGDebugInfo::EmitGlobalVariable(llvm::GlobalVariable *Var,
                                     const VarDefn *D) {
}

/// EmitGlobalVariable - Emit global variable's debug info.
void CGDebugInfo::EmitGlobalVariable(const ValueDefn *VD,
                                     llvm::Constant *Init) {
  // Create the descriptor for the variable.
  llvm::DIFile Unit = getOrCreateFile(VD->getLocation());
  llvm::StringRef Name = VD->getName();
  llvm::DIType Ty = getOrCreateType(VD->getType(), Unit);

  // Do not use DIGlobalVariable for enums.
  if (Ty.getTag() == llvm::dwarf::DW_TAG_enumeration_type)
    return;
  DBuilder.createStaticVariable(Unit, Name, Name, Unit,
                                getLineNumber(VD->getLocation()),
                                Ty, true, Init);
}

/// getOrCreateNamesSpace - Return namespace descriptor for the given
/// namespace decl.
llvm::DINameSpace 
CGDebugInfo::getOrCreateNameSpace(const NamespaceDefn *NSDecl) {
  llvm::DenseMap<const NamespaceDefn *, llvm::WeakVH>::iterator I =
    NameSpaceCache.find(NSDecl);
  if (I != NameSpaceCache.end())
    return llvm::DINameSpace(cast<llvm::MDNode>(I->second));
  
  unsigned LineNo = getLineNumber(NSDecl->getLocation());
  llvm::DIFile FileD = getOrCreateFile(NSDecl->getLocation());
  llvm::DIDescriptor Context = 
    getContextDescriptor(dyn_cast<Defn>(NSDecl->getDefnContext()));
  llvm::DINameSpace NS =
    DBuilder.createNameSpace(Context, NSDecl->getName(), FileD, LineNo);
  NameSpaceCache[NSDecl] = llvm::WeakVH(NS);
  return NS;
}

/// UpdateCompletedType - Update type cache because the type is now
/// translated.
void CGDebugInfo::UpdateCompletedType(const TypeDefn *TD) {
  Type Ty = CGM.getContext().getTypeDefnType(TD);

  // If the type exist in type cache then remove it from the cache.
  // There is no need to prepare debug info for the completed type
  // right now. It will be generated on demand lazily.
  llvm::DenseMap<void *, llvm::WeakVH>::iterator it =
    TypeCache.find(Ty.getAsOpaquePtr());
  if (it != TypeCache.end()) 
    TypeCache.erase(it);
}
