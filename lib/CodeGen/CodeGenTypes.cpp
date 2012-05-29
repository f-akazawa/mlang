//===--- CodeGenTypes.cpp - Type translation for LLVM CodeGen -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the code that handles AST -> LLVM type lowering.
//
//===----------------------------------------------------------------------===//

#include "CodeGenTypes.h"
#include "CGCall.h"
#include "CGOOPABI.h"
#include "CGRecordLayout.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/RecordLayout.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/Target/TargetData.h"
using namespace mlang;
using namespace CodeGen;

CodeGenTypes::CodeGenTypes(ASTContext &Ctx, llvm::Module& M,
                           const llvm::TargetData &TD, const ABIInfo &Info,
                           CGOOPABI &OOPABI, const CodeGenOptions &CGO)
  : Context(Ctx), Target(Ctx.Target), TheModule(M), TheTargetData(TD),
    TheABIInfo(Info), TheOOPABI(OOPABI), CodeGenOpts(CGO) {
}

CodeGenTypes::~CodeGenTypes() {
  for (llvm::DenseMap<const RawType *, CGRecordLayout *>::iterator
         I = CGRecordLayouts.begin(), E = CGRecordLayouts.end();
      I != E; ++I)
    delete I->second;

  for (llvm::FoldingSet<CGFunctionInfo>::iterator
       I = FunctionInfos.begin(), E = FunctionInfos.end(); I != E; )
    delete &*I++;
}

void CodeGenTypes::addRecordTypeName(const TypeDefn *RD, llvm::StructType *Ty,
                                     llvm::StringRef suffix) {
  llvm::SmallString<256> TypeName;
  llvm::raw_svector_ostream OS(TypeName);
  OS << RD->getKindName() << '.';
  
  // Name the codegen type after the typedef name
  // if there is no tag type name available
  if (RD->getIdentifier()) {
    // FIXME: We should not have to check for a null decl context here.
    // Right now we do it because the implicit Obj-C decls don't have one.
    if (RD->getDefnContext())
      OS << RD->getQualifiedNameAsString();
    else
      RD->printName(OS);
  } else
    OS << "anon";

  if (!suffix.empty())
    OS << suffix;

  Ty->setName(OS.str());
}

/// ConvertType - Convert the specified type to its LLVM form.
llvm::Type *CodeGenTypes::ConvertType(Type T, bool IsRecursive) {
  llvm::Type *Result = ConvertTypeRecursive(T);

  return Result;
}

llvm::Type *CodeGenTypes::ConvertTypeRecursive(Type T) {
  //T = Context.getCanonicalType(T);
	if(T.isNull())
		return llvm::Type::getInt8Ty(getLLVMContext());

  // See if type is already cached.
  llvm::DenseMap<const RawType *, llvm::Type *>::iterator
    I = TypeCache.find(T.getRawTypePtr());
  // If type is found in map and this is not a definition for a opaque
  // place holder type then use it. Otherwise, convert type T.
  if (I != TypeCache.end())
    return I->second;

  llvm::Type *ResultType = ConvertNewType(T);
  TypeCache.insert(std::make_pair(T.getRawTypePtr(),
                                  ResultType));
  return ResultType;
}

/// ConvertTypeForMem - Convert type T into a llvm::Type.  This differs from
/// ConvertType in that it is used to convert to the memory representation for
/// a type.  For example, the scalar representation for _Bool is i1, but the
/// memory representation is usually i8 or i32, depending on the target.
llvm::Type *CodeGenTypes::ConvertTypeForMem(Type T, bool IsRecursive){
  llvm::Type *R = ConvertType(T, IsRecursive);

  // If this is a non-bool type, don't map it.
  if (!R->isIntegerTy(1))
    return R;

  // Otherwise, return an integer of the target-specified size.
  return llvm::IntegerType::get(getLLVMContext(),
                                (unsigned)Context.getTypeSize(T));

}

/// UpdateCompletedType - When we find the full definition for a TagDecl,
/// replace the 'opaque' type we previously made for it if applicable.
void CodeGenTypes::UpdateCompletedType(const TypeDefn *TD) {

}

static llvm::Type* getTypeForFormat(llvm::LLVMContext &VMContext,
                                          const llvm::fltSemantics &format) {
  if (&format == &llvm::APFloat::IEEEsingle)
    return llvm::Type::getFloatTy(VMContext);
  if (&format == &llvm::APFloat::IEEEdouble)
    return llvm::Type::getDoubleTy(VMContext);
  if (&format == &llvm::APFloat::IEEEquad)
    return llvm::Type::getFP128Ty(VMContext);
  if (&format == &llvm::APFloat::PPCDoubleDouble)
    return llvm::Type::getPPC_FP128Ty(VMContext);
  if (&format == &llvm::APFloat::x87DoubleExtended)
    return llvm::Type::getX86_FP80Ty(VMContext);
  assert(0 && "Unknown float format!");
  return 0;
}

llvm::Type *CodeGenTypes::ConvertNewType(Type T) {
  const mlang::RawType &Ty = *T.getRawTypePtr();

  switch (Ty.getTypeClass()) {
#define TYPE(Class, Base)
#define ABSTRACT_TYPE(Class, Base)
#define NON_CANONICAL_TYPE(Class, Base) case RawType::Class:
#define DEPENDENT_TYPE(Class, Base) case RawType::Class:
#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(Class, Base) case RawType::Class:
#include "mlang/AST/TypeNodes.def"
    llvm_unreachable("Non-canonical or dependent types aren't possible.");
    break;

  case RawType::SimpleNumeric: {
    switch (cast<SimpleNumericType>(Ty).getKind()) {
//    case SimpleNumericType::Void:
//      // LLVM void type can only be used as the result of a function call.  Just
//      // map to the same as char.
//      return llvm::Type::getInt8Ty(getLLVMContext());

    case SimpleNumericType::Logical:
      // Note that we always return bool as i1 for use as a scalar type.
      return llvm::Type::getInt1Ty(getLLVMContext());

    case SimpleNumericType::Char:
    case SimpleNumericType::Int8:
    case SimpleNumericType::UInt8:
    case SimpleNumericType::Int16:
    case SimpleNumericType::UInt16:
    case SimpleNumericType::Int32:
    case SimpleNumericType::UInt32:
    case SimpleNumericType::Int64:
    case SimpleNumericType::UInt64:
      return llvm::IntegerType::get(getLLVMContext(),
        static_cast<unsigned>(Context.getTypeSize(T)));

    case SimpleNumericType::Single:
    case SimpleNumericType::Double:
      return getTypeForFormat(getLLVMContext(),
                              Context.getFloatTypeSemantics(T));
    }
    llvm_unreachable("Unknown builtin type!");
    break;
  }
  case RawType::LValueReference:
  case RawType::RValueReference: {
    const ReferenceType &RTy = cast<ReferenceType>(Ty);
    Type ETy = RTy.getPointeeType();
    llvm::Type *PointeeType = ConvertTypeForMem(ETy);
    unsigned AS = Context.getTargetAddressSpace(ETy);
    return llvm::PointerType::get(PointeeType, AS);
  }
  case RawType::NDArray: {
    const NDArrayType &A = cast<NDArrayType>(Ty);
    llvm::Type *EltTy = ConvertTypeForMemRecursive(A.getElementType());
    return llvm::ArrayType::get(EltTy, A.getSize()/*.getZExtValue()*/);
  }
  case RawType::Vector: {
    const VectorType &VT = cast<VectorType>(Ty);
    return llvm::VectorType::get(ConvertTypeRecursive(VT.getElementType()),
                                 VT.getNumElements());
  }
  case RawType::Matrix: {
      const MatrixType &MT = cast<MatrixType>(Ty);
      return llvm::ArrayType::get(ConvertTypeRecursive(MT.getElementType()),
                                   MT.getSize());
    }
  case RawType::FunctionHandle:
  case RawType::FunctionNoProto:
  case RawType::FunctionProto: {
    // The function type can be built; call the appropriate routines to
    // build it.
    const CGFunctionInfo *FI;
    bool isVariadic;
    if (const FunctionProtoType *FPT = dyn_cast<FunctionProtoType>(&Ty)) {
      FI = &getFunctionInfo(FPT, true /*Recursive*/);
      isVariadic = FPT->isVariadic();
    } else {
      const FunctionNoProtoType *FNPT = cast<FunctionNoProtoType>(&Ty);
      FI = &getFunctionInfo(FNPT, true /*Recursive*/);
      isVariadic = true;
    }

    return GetFunctionType(*FI, isVariadic, true);
  }

  case RawType::Struct:
  case RawType::Cell:
  case RawType::Map:
  case RawType::Classdef: {
    const TypeDefn *TD = cast<HeteroContainerType>(Ty).getDefn();
    llvm::Type *Res = ConvertTypeDefnType(TD);

//    if (const RecordDecl *RD = dyn_cast<RecordDecl>(TD))
//      addRecordTypeName(RD, Res, llvm::StringRef());
    return Res;
  }
  }

  // FIXME: implement.
  // FIXME huyabin
  return NULL;
}

/// ConvertTagDeclType - Lay out a tagged decl type like struct or union or
/// enum.
llvm::Type *CodeGenTypes::ConvertTypeDefnType(const TypeDefn *TD) {
  // TagDecl's are not necessarily unique, instead use the (clang)
  // type connected to the decl.
  const RawType *Key =
    Context.getTypeDefnType(TD).getRawTypePtr();
  llvm::DenseMap<const RawType*, llvm::StructType *>::iterator TDTI =
    TagDeclTypes.find(Key);

  // If we've already compiled this tag type, use the previous definition.
  if (TDTI != TagDeclTypes.end())
    return TDTI->second;

  const TypeDefn *RD = cast<const TypeDefn>(TD);

  // Force conversion of non-virtual base classes recursively.
  if (const UserClassDefn *RD = dyn_cast<UserClassDefn>(TD)) {
    for (UserClassDefn::base_class_const_iterator i = RD->bases_begin(),
         e = RD->bases_end(); i != e; ++i) {
      if (!i->isVirtual()) {
        const UserClassDefn *Base =
          cast<UserClassDefn>(i->getType()->getAs<HeteroContainerType>()->getDefn());
        ConvertTypeDefnType(Base);
      }
    }
  }

  // Layout fields.
  CGRecordLayout *Layout = ComputeRecordLayout(RD);

  CGRecordLayouts[Key] = Layout;
  llvm::Type *ResultType = Layout->getLLVMType();


  return NULL;
}

/// getCGRecordLayout - Return record layout info for the given record decl.
const CGRecordLayout &
CodeGenTypes::getCGRecordLayout(const TypeDefn *RD) {
  const RawType *Key = Context.getTypeDefnType(RD).getRawTypePtr();

  const CGRecordLayout *Layout = CGRecordLayouts.lookup(Key);
  if (!Layout) {
    // Compute the type information.
    ConvertTypeDefnType(RD);

    // Now try again.
    Layout = CGRecordLayouts.lookup(Key);
  }

  assert(Layout && "Unable to find record layout information for type");
  return *Layout;
}

void CodeGenTypes::addBaseSubobjectTypeName(const UserClassDefn *RD,
                                            const CGRecordLayout &layout) {
  llvm::StringRef suffix;
  if (layout.getBaseSubobjectLLVMType() != layout.getLLVMType())
    suffix = ".base";

  addRecordTypeName(RD, layout.getBaseSubobjectLLVMType(), suffix);
}

bool CodeGenTypes::isZeroInitializable(Type T) {
  // No need to check for member pointers when not compiling C++.
  if (!Context.getLangOptions().OOP)
    return true;
  
  //T = Context.getBaseElementType(T);
  
  // Records are non-zero-initializable if they contain any
  // non-zero-initializable subobjects.
  if (const HeteroContainerType *RT = T->getAs<HeteroContainerType>()) {
    const UserClassDefn *RD = cast<UserClassDefn>(RT->getDefn());
    return isZeroInitializable(RD);
  }

  // Everything else is okay.
  return true;
}

bool CodeGenTypes::isZeroInitializable(const UserClassDefn *RD) {
  return getCGRecordLayout(RD).isZeroInitializable();
}

bool CodeGenTypes::isFuncTypeConvertible(const FunctionType *FT) {
	return true;
}
