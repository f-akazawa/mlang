//===--- CodeGenTypes.h - Type translation for LLVM CodeGen -----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This is the code that handles AST -> LLVM type lowering.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CODEGENTYPES_H_
#define MLANG_CODEGEN_CODEGENTYPES_H_

#include "CGCall.h"
#include "mlang/AST/GlobalDefn.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseMap.h"
#include <vector>

namespace llvm {
  class FunctionType;
  class Module;
  class TargetData;
  class Type;
  class StructType;
  class LLVMContext;
}

namespace mlang {
  class ABIInfo;
  class ASTContext;
  class ClassConstructorDefn;
  class ClassDestructorDefn;
  class ClassMethodDefn;
  class CodeGenOptions;
  class MemberDefn;
  class FunctionProtoType;
//  class PointerType;
  class Type;
  class TypeDefn;
  class TargetInfo;
  class Type;

namespace CodeGen {
  class CGOOPABI;
  class CGRecordLayout;

/// CodeGenTypes - This class organizes the cross-module state that is used
/// while lowering AST types to LLVM types.
class CodeGenTypes {
  ASTContext &Context;
  const TargetInfo &Target;
  llvm::Module& TheModule;
  const llvm::TargetData& TheTargetData;
  const ABIInfo& TheABIInfo;
  CGOOPABI &TheOOPABI;
  const CodeGenOptions &CodeGenOpts;

  llvm::DenseMap<const RawType*, llvm::StructType *> TagDeclTypes;

  /// The opaque type map for Objective-C interfaces. All direct
  /// manipulation is done by the runtime interfaces, which are
  /// responsible for coercing to the appropriate type; these opaque
  /// types are never refined.
  // llvm::DenseMap<const ObjCInterfaceType*, llvm::Type *> InterfaceTypes;

  /// CGRecordLayouts - This maps llvm struct type with corresponding
  /// record layout info.
  llvm::DenseMap<const RawType*, CGRecordLayout *> CGRecordLayouts;

  /// FunctionInfos - Hold memoized CGFunctionInfo results.
  llvm::FoldingSet<CGFunctionInfo> FunctionInfos;

private:
  /// TypeCache - This map keeps cache of llvm::Types (through PATypeHolder)
  /// and maps llvm::Types to corresponding mlang::Type. llvm::PATypeHolder is
  /// used instead of llvm::Type because it allows us to bypass potential
  /// dangling type pointers due to type refinement on llvm side.
  llvm::DenseMap<const RawType *, llvm::Type *> TypeCache;

  /// ConvertNewType - Convert type T into a llvm::Type. Do not use this
  /// method directly because it does not do any type caching. This method
  /// is available only for ConvertType(). CovertType() is preferred
  /// interface to convert type T into a llvm::Type.
  llvm::Type *ConvertNewType(Type T);

  /// addRecordTypeName - Compute a name from the given record decl with an
  /// optional suffix and name the given LLVM type using it.
  void addRecordTypeName(const TypeDefn *RD, llvm::StructType *Ty,
                         llvm::StringRef suffix);

public:
  CodeGenTypes(ASTContext &Ctx, llvm::Module &M, const llvm::TargetData &TD,
               const ABIInfo &Info, CGOOPABI &OOPABI,
               const CodeGenOptions &Opts);
  ~CodeGenTypes();

  const llvm::TargetData &getTargetData() const { return TheTargetData; }
  const TargetInfo &getTarget() const { return Target; }
  ASTContext &getContext() const { return Context; }
  const ABIInfo &getABIInfo() const { return TheABIInfo; }
  CGOOPABI &getOOPABI() const { return TheOOPABI; }
  const CodeGenOptions &getCodeGenOpts() const { return CodeGenOpts; }
  llvm::LLVMContext &getLLVMContext() { return TheModule.getContext(); }

  /// ConvertType - Convert type T into a llvm::Type.
  llvm::Type *ConvertType(Type T, bool IsRecursive = false);
  llvm::Type *ConvertTypeRecursive(Type T);

  /// ConvertTypeForMem - Convert type T into a llvm::Type.  This differs from
  /// ConvertType in that it is used to convert to the memory representation for
  /// a type.  For example, the scalar representation for _Bool is i1, but the
  /// memory representation is usually i8 or i32, depending on the target.
  llvm::Type *ConvertTypeForMem(Type T, bool IsRecursive = false);
  llvm::Type *ConvertTypeForMemRecursive(Type T) {
    return ConvertTypeForMem(T, true);
  }

  /// GetFunctionType - Get the LLVM function type for \arg Info.
  llvm::FunctionType *GetFunctionType(const CGFunctionInfo &Info,
                                            bool IsVariadic,
                                            bool IsRecursive = false);

  llvm::FunctionType *GetFunctionType(GlobalDefn GD);

  /// isFuncTypeConvertible - Utility to check whether a function type can
  /// be converted to an LLVM type (i.e. doesn't depend on an incomplete tag
  /// type).
  bool isFuncTypeConvertible(const FunctionType *FT);
  bool isFuncTypeArgumentConvertible(Type Ty);

  /// GetFunctionTypeForVTable - Get the LLVM function type for use in a vtable,
  /// given a ClassMethodDefn. If the method to has an incomplete return type,
  /// and/or incomplete argument types, this will return the opaque type.
  llvm::Type *GetFunctionTypeForVTable(GlobalDefn GD);

  const CGRecordLayout &getCGRecordLayout(const TypeDefn*);

  /// addBaseSubobjectTypeName - Add a type name for the base subobject of the
  /// given record layout.
  void addBaseSubobjectTypeName(const UserClassDefn *RD,
                                const CGRecordLayout &layout);
  /// UpdateCompletedType - When we find the full definition for a TypeDefn,
  /// replace the 'opaque' type we previously made for it if applicable.
  void UpdateCompletedType(const TypeDefn *TD);

  /// getFunctionInfo - Get the function info for the specified function decl.
  const CGFunctionInfo &getFunctionInfo(GlobalDefn GD);

  const CGFunctionInfo &getFunctionInfo(const FunctionDefn *FD);
  const CGFunctionInfo &getFunctionInfo(const ClassMethodDefn *MD);
  const CGFunctionInfo &getFunctionInfo(const ClassConstructorDefn *D,
                                        ClassCtorType Type);
  const CGFunctionInfo &getFunctionInfo(const ClassDestructorDefn *D,
                                        ClassDtorType Type);

  const CGFunctionInfo &getFunctionInfo(const CallArgList &Args,
                                        const FunctionType *Ty) {
    return getFunctionInfo(Ty->getResultType(), Args,
                           Ty->getExtInfo());
  }

  const CGFunctionInfo &getFunctionInfo(const FunctionProtoType *Ty,
                                        bool IsRecursive = false);
  const CGFunctionInfo &getFunctionInfo(const FunctionNoProtoType *Ty,
                                        bool IsRecursive = false);

  /// getFunctionInfo - Get the function info for a member function of
  /// the given type.  This is used for calls through member function
  /// pointers.
  const CGFunctionInfo &getFunctionInfo(const UserClassDefn *RD,
                                        const FunctionProtoType *FTP);

  /// getFunctionInfo - Get the function info for a function described by a
  /// return type and argument types. If the calling convention is not
  /// specified, the "C" calling convention will be used.
  const CGFunctionInfo &getFunctionInfo(Type ResTy,
                                        const CallArgList &Args,
                                        const FunctionType::ExtInfo &Info);
  const CGFunctionInfo &getFunctionInfo(Type ResTy,
                                        const FunctionArgList &Args,
                                        const FunctionType::ExtInfo &Info);

  /// Retrieves the ABI information for the given function signature.
  ///
  /// \param ArgTys - must all actually be canonical as params
  const CGFunctionInfo &getFunctionInfo(Type RetTy,
                               const llvm::SmallVectorImpl<Type> &ArgTys,
                                        const FunctionType::ExtInfo &Info,
                                        bool IsRecursive = false);

  /// \brief Compute a new LLVM record layout object for the given record.
  CGRecordLayout *ComputeRecordLayout(const TypeDefn *D);

public:  // These are internal details of CGT that shouldn't be used externally.
  /// ConvertTagDeclType - Lay out a tagged decl type like struct or union or
  /// enum.
  llvm::Type *ConvertTypeDefnType(const TypeDefn *TD);

  /// GetExpandedTypes - Expand the type \arg Ty into the LLVM
  /// argument types it would be passed as on the provided vector \arg
  /// ArgTys. See ABIArgInfo::Expand.
  void GetExpandedTypes(Type Ty, llvm::SmallVectorImpl<llvm::Type*> &ArgTys,
                        bool IsRecursive);

  /// IsZeroInitializable - Return whether a type can be
  /// zero-initialized (in the C++ sense) with an LLVM zeroinitializer.
  bool isZeroInitializable(Type T);

  /// IsZeroInitializable - Return whether a record type can be
  /// zero-initialized (in the C++ sense) with an LLVM zeroinitializer.
  bool isZeroInitializable(const UserClassDefn *RD);
};

}  // end namespace CodeGen
}  // end namespace mlang

#endif /* MLANG_CODEGEN_CODEGENTYPES_H_ */
