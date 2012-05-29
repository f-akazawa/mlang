//===--- CGDebugInfo.h - DebugInfo for LLVM CodeGen -------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This is the source level debug info generator for llvm translation.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CGDEBUGINFO_H_
#define MLANG_CODEGEN_CGDEBUGINFO_H_

#include "mlang/AST/Type.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/Basic/SourceLocation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Analysis/DIBuilder.h"
#include "llvm/Support/ValueHandle.h"
#include "llvm/Support/Allocator.h"

#include "CGBuilder.h"

namespace llvm {
  class MDNode;
}

namespace mlang {
  class VarDefn;
  class DefnRefExpr;
  class GlobalDefn;

namespace CodeGen {
  class CodeGenModule;
  class CodeGenFunction;

/// CGDebugInfo - This class gathers all debug information during compilation
/// and is responsible for emitting to llvm globals or pass directly to
/// the backend.
class CGDebugInfo {
  CodeGenModule &CGM;
  llvm::DIBuilder DBuilder;
  llvm::DICompileUnit TheCU;
  SourceLocation CurLoc, PrevLoc;
  llvm::DIType VTablePtrType;
  
  /// TypeCache - Cache of previously constructed Types.
  llvm::DenseMap<void *, llvm::WeakVH> TypeCache;

  bool BlockLiteralGenericSet;
  llvm::DIType BlockLiteralGeneric;

  std::vector<llvm::TrackingVH<llvm::MDNode> > RegionStack;
  llvm::DenseMap<const Defn *, llvm::WeakVH> RegionMap;
  // FnBeginRegionCount - Keep track of RegionStack counter at the beginning
  // of a function. This is used to pop unbalanced regions at the end of a
  // function.
  std::vector<unsigned> FnBeginRegionCount;

  /// LineDirectiveFiles - This stack is used to keep track of 
  /// scopes introduced by #line directives.
  std::vector<const char *> LineDirectiveFiles;

  /// DebugInfoNames - This is a storage for names that are
  /// constructed on demand. For example, C++ destructors, C++ operators etc..
  llvm::BumpPtrAllocator DebugInfoNames;
  llvm::StringRef CWDName;

  llvm::DenseMap<const char *, llvm::WeakVH> DIFileCache;
  llvm::DenseMap<const FunctionDefn *, llvm::WeakVH> SPCache;
  llvm::DenseMap<const NamespaceDefn *, llvm::WeakVH> NameSpaceCache;

  /// Helper functions for getOrCreateType.
  llvm::DIType CreateType(const SimpleNumericType *Ty);
  llvm::DIType CreateQualifiedType(Type Ty, llvm::DIFile F);
//  llvm::DIType CreateType(const PointerType *Ty, llvm::DIFile F);
  llvm::DIType CreateType(const FunctionType *Ty, llvm::DIFile F);
  llvm::DIType CreateType(const HeteroContainerType *Ty);
  llvm::DIType CreateType(const VectorType *Ty, llvm::DIFile F);
  llvm::DIType CreateType(const ArrayType *Ty, llvm::DIFile F);
  llvm::DIType CreateType(const LValueReferenceType *Ty, llvm::DIFile F);
  llvm::DIType CreateType(const RValueReferenceType *Ty, llvm::DIFile F);
//  llvm::DIType CreateType(const MemberPointerType *Ty, llvm::DIFile F);
  llvm::DIType getOrCreateMethodType(const ClassMethodDefn *Method,
                                     llvm::DIFile F);
  llvm::DIType getOrCreateVTablePtrType(llvm::DIFile F);
  llvm::DIType getOrCreateFunctionType(const Defn *D, Type FnType,
                                       llvm::DIFile F);
  llvm::DINameSpace getOrCreateNameSpace(const NamespaceDefn *N);
  llvm::DIType CreatePointeeType(Type PointeeTy, llvm::DIFile F);
  llvm::DIType CreatePointerLikeType(unsigned Tag,
                                     const RawType *Ty, Type PointeeTy,
                                     llvm::DIFile F);
  
  llvm::DISubprogram CreateClassMemberFunction(const ClassMethodDefn *Method,
                                             llvm::DIFile F,
                                             llvm::DIType RecordTy);
  
  void CollectClassMemberFunctions(const UserClassDefn *Defn,
                                 llvm::DIFile F,
                                 llvm::SmallVectorImpl<llvm::Value *> &E,
                                 llvm::DIType T);

  void CollectClassBases(const UserClassDefn *Defn,
                       llvm::DIFile F,
                       llvm::SmallVectorImpl<llvm::Value *> &EltTys,
                       llvm::DIType RecordTy);

  llvm::DIType createFieldType(llvm::StringRef name, Type type,
                               Expr *bitWidth, SourceLocation loc,
                               AccessSpecifier AS, uint64_t offsetInBits,
                               llvm::DIFile tunit,
                               llvm::DIDescriptor scope);

  void CollectRecordFields(const UserClassDefn *Defn, llvm::DIFile F,
                           llvm::SmallVectorImpl<llvm::Value *> &E,
                           llvm::DIType RecordTy);

  void CollectVTableInfo(const UserClassDefn *Defn,
                         llvm::DIFile F,
                         llvm::SmallVectorImpl<llvm::Value *> &EltTys);

public:
  CGDebugInfo(CodeGenModule &CGM);
  ~CGDebugInfo();

  /// setLocation - Update the current source location. If \arg loc is
  /// invalid it is ignored.
  void setLocation(SourceLocation Loc);

  /// EmitStopPoint - Emit a call to llvm.dbg.stoppoint to indicate a change of
  /// source line.
  void EmitStopPoint(CGBuilderTy &Builder);

  /// EmitFunctionStart - Emit a call to llvm.dbg.function.start to indicate
  /// start of a new function.
  void EmitFunctionStart(GlobalDefn GD, Type FnType,
                         llvm::Function *Fn, CGBuilderTy &Builder);

  /// EmitFunctionEnd - Constructs the debug code for exiting a function.
  void EmitFunctionEnd(CGBuilderTy &Builder);

  /// UpdateLineDirectiveRegion - Update region stack only if #line directive
  /// has introduced scope change.
  void UpdateLineDirectiveRegion(CGBuilderTy &Builder);

  /// EmitRegionStart - Emit a call to llvm.dbg.region.start to indicate start
  /// of a new block.
  void EmitRegionStart(CGBuilderTy &Builder);

  /// EmitRegionEnd - Emit call to llvm.dbg.region.end to indicate end of a
  /// block.
  void EmitRegionEnd(CGBuilderTy &Builder);

  /// EmitDeclareOfAutoVariable - Emit call to llvm.dbg.declare for an automatic
  /// variable declaration.
  void EmitDeclareOfAutoVariable(const VarDefn *Decl, llvm::Value *AI,
                                 CGBuilderTy &Builder);

  /// EmitDeclareOfBlockDeclRefVariable - Emit call to llvm.dbg.declare for an
  /// imported variable declaration in a block.
  void EmitDeclareOfBlockDeclRefVariable(const DefnRefExpr *BDRE,
                                         llvm::Value *AI,
                                         CGBuilderTy &Builder,
                                         CodeGenFunction *CGF);

  /// EmitDeclareOfArgVariable - Emit call to llvm.dbg.declare for an argument
  /// variable declaration.
  void EmitDeclareOfArgVariable(const VarDefn *Decl, llvm::Value *AI,
		                            unsigned ArgNo, CGBuilderTy &Builder);

  /// EmitGlobalVariable - Emit information about a global variable.
  void EmitGlobalVariable(llvm::GlobalVariable *GV, const VarDefn *Decl);

  /// EmitGlobalVariable - Emit global variable's debug info.
  void EmitGlobalVariable(const ValueDefn *VD, llvm::Constant *Init);

  /// getOrCreateRecordType - Emit record type's standalone debug info. 
  llvm::DIType getOrCreateRecordType(Type Ty, SourceLocation L);
private:
  /// EmitDeclare - Emit call to llvm.dbg.declare for a variable declaration.
  void EmitDeclare(const VarDefn *decl, unsigned Tag, llvm::Value *AI,
		               unsigned ArgNo, CGBuilderTy &Builder);

  // EmitTypeForVarWithBlocksAttr - Build up structure info for the byref.  
  // See BuildByRefType.
  llvm::DIType EmitTypeForVarWithBlocksAttr(const ValueDefn *VD,
                                            uint64_t *OffSet);

  /// getContextDescriptor - Get context info for the decl.
  llvm::DIDescriptor getContextDescriptor(const Defn *Defn);

  /// getCurrentDirname - Return current directory name.
  llvm::StringRef getCurrentDirname();

  /// CreateCompileUnit - Create new compile unit.
  void CreateCompileUnit();

  /// getOrCreateFile - Get the file debug info descriptor for the input 
  /// location.
  llvm::DIFile getOrCreateFile(SourceLocation Loc);

  /// getOrCreateMainFile - Get the file info for main compile unit.
  llvm::DIFile getOrCreateMainFile();

  /// getOrCreateType - Get the type from the cache or create a new type if
  /// necessary.
  llvm::DIType getOrCreateType(Type Ty, llvm::DIFile F);

  /// CreateTypeNode - Create type metadata for a source language type.
  llvm::DIType CreateTypeNode(Type Ty, llvm::DIFile F);

  /// CreateMemberType - Create new member and increase Offset by FType's size.
  llvm::DIType CreateMemberType(llvm::DIFile Unit, Type FType,
                                llvm::StringRef Name, uint64_t *Offset);

  /// getFunctionDeclaration - Return debug info descriptor to describe method
  /// declaration for the given method definition.
  llvm::DISubprogram getFunctionDefinition(const Defn *D);

  /// getFunctionName - Get function name for the given FunctionDecl. If the
  /// name is constructred on demand (e.g. C++ destructor) then the name
  /// is stored on the side.
  llvm::StringRef getFunctionName(const FunctionDefn *FD);

  /// getHeteroContainerName - Heterogeneous container type name.
  llvm::StringRef getHeteroContainerName(TypeDefn *RD);

  /// getVTableName - Get vtable name for the given Class.
  llvm::StringRef getVTableName(const UserClassDefn *Defn);

  /// getLineNumber - Get line number for the location. If location is invalid
  /// then use current location.
  unsigned getLineNumber(SourceLocation Loc);

  /// getColumnNumber - Get column number for the location. If location is 
  /// invalid then use current location.
  unsigned getColumnNumber(SourceLocation Loc);

  void UpdateCompletedType(const TypeDefn *TD);
};
} // namespace CodeGen
} // namespace mlang


#endif /* MLANG_CODEGEN_CGDEBUGINFO_H_ */
