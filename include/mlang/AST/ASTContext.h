//===--- ASTContext.h - Context to hold long-lived AST nodes ----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTContext interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_ASTCONTEXT_H_
#define MLANG_AST_ASTCONTEXT_H_

#include "mlang/Basic/IdentifierTable.h"
#include "mlang/Basic/LangOptions.h"
#include "mlang/Diag/PartialDiagnostic.h"
#include "mlang/AST/DefinitionName.h"
#include "mlang/AST/PrettyPrinter.h"
#include "mlang/AST/NestedNameSpecifier.h"
#include "mlang/AST/Type.h"
#include "mlang/AST/UsuallyTinyPtrVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/Allocator.h"
#include <vector>

namespace llvm {
  struct fltSemantics;
  class raw_ostream;
}

namespace mlang {
  class ASTMutationListener;
  class ASTRecordLayout;
  class CharUnits;
  class ClassMethodDefn;
  class Defn;
  class DefinitionNameTable;
  class DiagnosticsEngine;
  class Expr;
  class ExternalASTSource;
  class FileManager;
  class FunctionDefn;
  class IdentifierTable;
  class NamedDefn;
  class SourceManager;
  class StoredDefnsMap;
  class TargetInfo;
  class TranslationUnitDefn;
  class TypeDefn;
  class UserClassDefn;
  class VarDefn;

  namespace Builtin { class Context; }

/// ASTContext - This class holds long-lived AST nodes (such as types and
/// defns) that can be referred to throughout the semantic analysis of a file.
class ASTContext : public llvm::RefCountedBase<ASTContext> {
// To avoid MS warning
  ASTContext &this_() { return *this; }

  mutable std::vector<RawType*> Types;
  mutable llvm::FoldingSet<ExtTypeInfo> ExtTypeInfoNodes;
  mutable llvm::FoldingSet<FunctionHandleType> FunctionHandleTypes;
  mutable llvm::FoldingSet<LValueReferenceType> LValueReferenceTypes;
  mutable llvm::FoldingSet<RValueReferenceType> RValueReferenceTypes;
  mutable llvm::FoldingSet<NDArrayType> NDArrayTypes;
  mutable llvm::FoldingSet<MatrixType> MatrixTypes;
  mutable llvm::FoldingSet<VectorType> VectorTypes;
  mutable llvm::FoldingSet<FunctionNoProtoType> FunctionNoProtoTypes;
  mutable llvm::ContextualFoldingSet<FunctionProtoType, ASTContext&>
      FunctionProtoTypes;

  /// \brief The set of nested name specifiers.
    ///
  /// This set is managed by the NestedNameSpecifier class.
  llvm::FoldingSet<NestedNameSpecifier> NestedNameSpecifiers;
  friend class NestedNameSpecifier;

  /// ASTRecordLayouts - A cache mapping from Defns to ASTRecordLayouts.
  ///  This is lazily created.  This is intentionally not serialized.
  ///  These Defns can be StructDefn, CellDefn and UserClassDefn.
  llvm::DenseMap<const Defn*, const ASTRecordLayout*> ASTRecordLayouts;

  /// KeyFunctions - A cache mapping from UserClassDefns to key functions.
  llvm::DenseMap<const UserClassDefn*, const ClassMethodDefn*> KeyFunctions;

  /// \brief Mapping from __block VarDefns to their copy initialization expr.
  llvm::DenseMap<const VarDefn*, Expr*> BlockVarCopyInits;

  /// TranslationUnitDefn - A script file and all the imported functions
  ///  and external user defined functions (for normal compilation purpose).
  TranslationUnitDefn *TUDefn;

  /// SourceMgr - The associated SourceManager object.
  SourceManager &SourceMgr;

  /// LangOpts - The language options used to create the AST associated with
  ///  this ASTContext object.
  LangOptions LangOpts;

  /// \brief The allocator used to create AST objects.
  ///
  /// AST objects are never destructed; rather, all memory associated with the
  /// AST objects will be released when the ASTContext itself is destroyed.
  mutable llvm::BumpPtrAllocator BumpAlloc;

  /// \brief Allocator for partial diagnostics.
  PartialDiagnostic::StorageAllocator DiagAllocator;

  // TODO this is to be implemented
  friend class ASTDefnReader;

public:
  const TargetInfo &Target;
  IdentifierTable &Idents;
  Builtin::Context &BuiltinInfo;
  mutable DefinitionNameTable DefinitionNames;
  llvm::OwningPtr<ExternalASTSource> ExternalSource;
  ASTMutationListener *Listener;
  mlang::PrintingPolicy PrintingPolicy;

  SourceManager& getSourceManager() { return SourceMgr; }
  const SourceManager& getSourceManager() const { return SourceMgr; }
  void *Allocate(unsigned Size, unsigned Align = 8) const {
    return BumpAlloc.Allocate(Size, Align);
  }
  void Deallocate(void *Ptr) const { }
  
  /// Return the total amount of physical memory allocated for representing
  /// AST nodes and type information.
  size_t getASTAllocatedMemory() const {
	  return BumpAlloc.getTotalMemory();
  }
  /// Return the total memory used for various side tables.
  size_t getSideTableAllocatedMemory() const;

  PartialDiagnostic::StorageAllocator &getDiagAllocator() {
    return DiagAllocator;
  }

  const LangOptions& getLangOptions() const { return LangOpts; }

  DiagnosticsEngine &getDiagnostics() const;

  FullSourceLoc getFullLoc(SourceLocation Loc) const {
    return FullSourceLoc(Loc, getSourceManager());
  }

  TranslationUnitDefn *getTranslationUnitDefn() const { return TUDefn; }

  // Fundamental Types.
  Type Int8Ty, Int16Ty, Int32Ty, Int64Ty;
  Type UInt8Ty, UInt16Ty, UInt32Ty, UInt64Ty;
  Type SingleTy, DoubleTy;
  Type LogicalTy;
  Type CharTy, UCharTy, WChar_UTy, Char16Ty, Char32Ty;
  Type FloatComplexTy, DoubleComplexTy;
//  Type VoidTy;
//  Type VoidPtrTy, NullPtrTy;
  // Type StructTy;
  // Type CellTy;
  // Type FunctionHandleTy;
  // Type MapTy;


  ASTContext(const LangOptions& LOpts, SourceManager &SM, const TargetInfo &t,
             IdentifierTable &idents, Builtin::Context &builtins,
             unsigned size_reserve);

  ~ASTContext();

  /// \brief Attach an external AST source to the AST context.
  ///
  /// The external AST source provides the ability to load parts of
  /// the abstract syntax tree as needed from some external storage,
  /// e.g., a precompiled header.
  void setExternalSource(llvm::OwningPtr<ExternalASTSource> &Source);

  /// \brief Retrieve a pointer to the external AST source associated
  /// with this AST context, if any.
  ExternalASTSource *getExternalSource() const { return ExternalSource.get(); }

  /// \brief Attach an AST mutation listener to the AST context.
    ///
  /// The AST mutation listener provides the ability to track modifications to
  /// the abstract syntax tree entities committed after they were initially
  /// created.
  void setASTMutationListener(ASTMutationListener *Listener) {
	  this->Listener = Listener;
  }
  /// \brief Retrieve a pointer to the AST mutation listener associated
  /// with this AST context, if any.
  ASTMutationListener *getASTMutationListener() const { return Listener; }

  const std::vector<RawType*>& getTypes() const { return Types; }

  void PrintStats() const;

  //===--------------------------------------------------------------------===//
  //    Type Constructors
  //===--------------------------------------------------------------------===//

private:
  /// getExtQualType - Return a type with extended TypeInfo.
  Type getExtTypeInfoType(const RawType *Base, TypeInfo Quals);

  Type getTypeDefnTypeSlow(const TypeDefn *Defn);

public:
  /// getCallConvType - Adds the specified calling convention attribute to
  /// the given type, which must be a FunctionType or a pointer to an
  /// allowable type.
  Type getCallConvType(Type T, CallingConv CallConv);

  /// getTypeDefnType - Return the unique reference to the type for
  /// the specified type declaration.
  Type getTypeDefnType(const TypeDefn *Defn,
		  const TypeDefn *PrevDefn = 0);

  Type getLValueReferenceType(Type T, bool SpelledAsLValue) const;
  Type getRValueReferenceType(Type T) const;
  Type getMatrixType(Type T, unsigned Rows, unsigned Cols,
		  MatrixType::MatrixKind K) const;
  Type getNDArrayType(Type T, DimVector &DV) const;
  Type getVectorType(Type T, unsigned NumElts, VectorType::VectorKind K,
		  bool isRowVec) const;
  Type getFunctionNoProtoType(Type T, FunctionType::ExtInfo E) const;
  Type getFunctionType(Type ResultTy,
                       const Type *Args, unsigned NumArgs,
                       const FunctionProtoType::ExtProtoInfo &EPI) const;
  Type getFunctionHandleType(Type T) const;

  enum GetBuiltinTypeError {
     GE_None,              //< No error
     GE_Missing_stdio,     //< Missing a type from <stdio.h>
     GE_Missing_setjmp     //< Missing a type from <setjmp.h>
   };

  /// GetBuiltinType - Return the type for the specified builtin.  If
  /// IntegerConstantArgs is non-null, it is filled in with a bitmask of
  /// arguments to the builtin that are required to be integer constant
  /// expressions.
  Type GetBuiltinType(unsigned ID, GetBuiltinTypeError &Error,
		      unsigned *IntegerConstantArgs = 0) const;

private:
  Type getFromTargetType(unsigned nType) const;

  //===--------------------------------------------------------------------===//
  //    Type Predicates.
  //===--------------------------------------------------------------------===//

public:
  /// areCompatibleVectorTypes - Return true if the given vector types
  /// are of the same unqualified type or if they are equivalent to the same
  /// GCC vector type, ignoring whether they are target-specific (AltiVec or
  /// Neon) types.
  bool areCompatibleVectorTypes(Type FirstVec, Type SecondVec);


  //===--------------------------------------------------------------------===//
  //    Type Sizing and Analysis
  //===--------------------------------------------------------------------===//

public:
  /// getFloatTypeSemantics - Return the APFloat 'semantics' for the specified
  /// scalar floating point type.
  const llvm::fltSemantics &getFloatTypeSemantics(Type T) const;

  /// getTypeSizeInfo - Get the size and alignment of the specified complete type in
  /// bits.
  std::pair<uint64_t, unsigned> getTypeSizeInfo(const RawType *T) const;
  std::pair<uint64_t, unsigned> getTypeSizeInfo(Type T) const {
	  return getTypeSizeInfo(T.getRawTypePtr());
  }

  /// getTypeSize - Return the size of the specified type, in bits.  This method
  /// does not work on incomplete types.
  uint64_t getTypeSize(Type T) const {
    return getTypeSizeInfo(T).first;
  }
  uint64_t getTypeSize(const RawType *T) const {
    return getTypeSizeInfo(T).first;
  }

  /// getCharWidth - Return the size of the character type, in bits
  uint64_t getCharWidth() const {
    return getTypeSize(CharTy);
  }

  /// toCharUnitsFromBits - Convert a size in bits to a size in characters.
  CharUnits toCharUnitsFromBits(int64_t BitSize) const;
  
  /// toBits - Convert a size in characters to a size in bits.
  int64_t toBits(CharUnits CharSize) const;

  /// getTypeSizeInChars - Return the size of the specified type, in characters.
  /// This method does not work on incomplete types.
  CharUnits getTypeSizeInChars(Type T);
  CharUnits getTypeSizeInChars(const RawType *T);

  /// getTypeAlign - Return the ABI-specified alignment of a type, in bits.
  /// This method does not work on incomplete types.
  unsigned getTypeAlign(Type T) {
    return getTypeSizeInfo(T).second;
  }
  unsigned getTypeAlign(const RawType *T) {
    return getTypeSizeInfo(T).second;
  }

  /// getTypeAlignInChars - Return the ABI-specified alignment of a type, in 
  /// characters. This method does not work on incomplete types.
  CharUnits getTypeAlignInChars(Type T);
  CharUnits getTypeAlignInChars(const RawType *T);

  std::pair<CharUnits, CharUnits> getTypeInfoInChars(const RawType *T);
  std::pair<CharUnits, CharUnits> getTypeInfoInChars(Type T);

  /// getPreferredTypeAlign - Return the "preferred" alignment of the specified
  /// type for the current target in bits.  This can be different than the ABI
  /// alignment in cases where it is beneficial for performance to overalign
  /// a data type.
  unsigned getPreferredTypeAlign(const RawType *T);

  /// getDefnAlign - Return a conservative estimate of the alignment of
  /// the specified Defn.  Note that bitfields do not have a valid alignment, so
  /// this method will assert on them.
  /// If @p RefAsPointee, references are treated like their underlying type
  /// (for alignof), else they're treated like pointers (for CodeGen).
  CharUnits getDefnAlign(const Defn *D, bool RefAsPointee = false);

  /// ASTRecordLayout - Get or compute information about the layout of the
  /// struct/cell/class, which indicates its size and field position information.
  const ASTRecordLayout &getASTRecordLayout(const Defn *D) const;
  void DumpRecordLayout(const Defn *D, llvm::raw_ostream &OS);

  /// getKeyFunction - Get the key function for the given record decl, or NULL
	/// if there isn't one.  The key function is, according to the Itanium C++ ABI
	/// section 5.2.3:
	///
	/// ...the first non-pure virtual function that is not inline at the point
	/// of class definition.
	const ClassMethodDefn *getKeyFunction(const UserClassDefn *RD);

  //===--------------------------------------------------------------------===//
  //    Type Operators
  //===--------------------------------------------------------------------===//

  /// \brief Retrieves the default calling convention to use for
  /// C++ instance methods.
  CallingConv getDefaultMethodCallConv();

  /// \brief Retrieves the canonical representation of the given
  /// calling convention.
  CallingConv getCanonicalCallConv(CallingConv CC) {
    if (CC == CC_C)
      return CC_Default;
    return CC;
  }

  /// \brief Determines whether two calling conventions name the same
  /// calling convention.
  bool isSameCallConv(CallingConv lcc, CallingConv rcc) {
    return (getCanonicalCallConv(lcc) == getCanonicalCallConv(rcc));
  }

  /// getPromotedIntegerType - Returns the type that Promotable will
  /// promote to: C99 6.3.1.1p2, assuming that Promotable is a promotable
  /// integer type.
  Type getPromotedIntegerType(Type PromotableType) const;

  /// getIntegerTypeOrder - Returns the highest ranked integer type:
  /// If LHS > RHS, return 1.  If LHS == RHS, return 0. If LHS < RHS,
  /// return -1.
  int getIntegerTypeOrder(Type LHS, Type RHS);

  /// getFloatingTypeOrder - Compare the rank of the two specified floating
  /// point types, ignoring the domain of the type (i.e. 'double' ==
  /// '_Complex double').  If LHS > RHS, return 1.  If LHS == RHS, return 0. If
  /// LHS < RHS, return -1.
  int getFloatingTypeOrder(Type LHS, Type RHS);

  /// getFloatingTypeOfSizeWithinDomain - Returns a real floating
  /// point or a complex type (based on typeDomain/typeSize).
  /// 'typeDomain' is a real floating point or complex type.
  /// 'typeSize' is a real floating point or complex type.
  Type getFloatingTypeOfSizeWithinDomain(Type typeSize,
                                         Type typeDomain) const;

  // AddrSpaceMap shall be defined in mlang/Basic/AddressSpace.h
  unsigned getTargetAddressSpace(unsigned AS) const {
	  return AS;
  }

  unsigned getTargetAddressSpace(Type T) const {
    return getTargetAddressSpace(T.getTypeInfo());
  }

  unsigned getTargetAddressSpace(TypeInfo Q) const {
    return getTargetAddressSpace(/*Q.getAddressSpace()*/0);
  }

private:
  // Helper for integer ordering
  unsigned getIntegerRank(Type* T);

public:
  //===--------------------------------------------------------------------===//
  //                    Type Compatibility Predicates
  //===--------------------------------------------------------------------===//

  /// Compatibility predicates used to check assignment expressions.
  bool typesAreCompatible(Type T1, Type T2,
                          bool CompareUnqualified = false); // C99 6.2.7p1

  // Functions for calculating composite types
  Type mergeTypes(Type , Type , bool OfBlockPointer=false,
                      bool Unqualified = false);
  Type mergeFunctionTypes(Type , Type , bool OfBlockPointer=false,
                              bool Unqualified = false);
  Type mergeFunctionArgumentTypes(Type , Type ,
                                      bool OfBlockPointer=false,
                                      bool Unqualified = false);
  //===--------------------------------------------------------------------===//
  //                    Integer Predicates
  //===--------------------------------------------------------------------===//

  // The width of an integer, as defined in C99 6.2.6.2. This is the number
  // of bits in an integer type excluding any padding bits.
  unsigned getIntWidth(Type T);

  // Per C99 6.2.5p6, for every signed integer type, there is a corresponding
  // unsigned integer type.  This method takes a signed type, and returns the
  // corresponding unsigned integer type.
  Type getCorrespondingUnsignedType(Type T);

  //===--------------------------------------------------------------------===//
  //                    Type Iterators.
  //===--------------------------------------------------------------------===//

  typedef std::vector<RawType*>::iterator       type_iterator;
  typedef std::vector<RawType*>::const_iterator const_type_iterator;

  type_iterator types_begin() { return Types.begin(); }
  type_iterator types_end() { return Types.end(); }
  const_type_iterator types_begin() const { return Types.begin(); }
  const_type_iterator types_end() const { return Types.end(); }

  //===--------------------------------------------------------------------===//
  //                    Integer Values
  //===--------------------------------------------------------------------===//

  /// MakeIntValue - Make an APSInt of the appropriate width and
  /// signedness for the given \arg Value and integer \arg Type.
  llvm::APSInt MakeIntValue(uint64_t Value, Type Type) {
    llvm::APSInt Res(getIntWidth(Type), !Type->isSignedIntegerType());
    Res = Value;
    return Res;
  }

  /// \brief Add a deallocation callback that will be invoked when the 
  /// ASTContext is destroyed.
  ///
  /// \brief Callback A callback function that will be invoked on destruction.
  ///
  /// \brief Data Pointer data that will be provided to the callback function
  /// when it is called.
  void AddDeallocation(void (*Callback)(void*), void *Data);

  GVALinkage GetGVALinkageForFunction(const FunctionDefn *FD);

  /// \brief Determines if the Defn can be CodeGen'ed or deserialized from PCH
  /// lazily, only when used; this is only relevant for function or file scoped
  /// var definitions.
  ///
  /// \returns true if the function/var must be CodeGen'ed/deserialized even if
  /// it is not used.
  bool DefnMustBeEmitted(const Defn *D);

  //===--------------------------------------------------------------------===//
  //                    Statistics
  //===--------------------------------------------------------------------===//
  /// \brief The number of implicitly-declared default constructors.
  static unsigned NumImplicitDefaultConstructors;

  /// \brief The number of implicitly-declared destructors.
  static unsigned NumImplicitDestructors;

private:
  ASTContext(const ASTContext&); // DO NOT IMPLEMENT
  void operator=(const ASTContext&); // DO NOT IMPLEMENT

  void InitBuiltinTypes(); //for Integer, FP, Char, Logical
  void InitSimpleNumericType(Type &R, SimpleNumericType::NumericKind K);

private:
  /// \brief A set of deallocations that should be performed when the 
  /// ASTContext is destroyed.
  llvm::SmallVector<std::pair<void (*)(void*), void *>, 16> Deallocations;

  // FIXME: This currently contains the set of StoredDefnMaps used
  // by DefnContext objects.  This probably should not be in ASTContext,
  // but we include it here so that ASTContext can quickly deallocate them.
  StoredDefnsMap* LastSDM;
                                       
  /// \brief A counter used to uniquely identify "blocks".
  unsigned int UniqueBlockByRefTypeID;
  unsigned int UniqueBlockParmTypeID;
  
  friend class DefnContext;
  friend class DefinitionNameTable;

  void ReleaseDefnContextMaps();
};

}  // end namespace mlang

// operator new and delete aren't allowed inside namespaces.
// The throw specifications are mandated by the standard.
/// @brief Placement new for using the ASTContext's allocator.
///
/// This placement form of operator new uses the ASTContext's allocator for
/// obtaining memory. It is a non-throwing new, which means that it returns
/// null on error. (If that is what the allocator does. The current does, so if
/// this ever changes, this operator will have to be changed, too.)
/// Usage looks like this (assuming there's an ASTContext 'Context' in scope):
/// @code
/// // Default alignment (8)
/// IntegerLiteral *Ex = new (Context) IntegerLiteral(arguments);
/// // Specific alignment
/// IntegerLiteral *Ex2 = new (Context, 4) IntegerLiteral(arguments);
/// @endcode
/// Please note that you cannot use delete on the pointer; it must be
/// deallocated using an explicit destructor call followed by
/// @c Context.Deallocate(Ptr).
///
/// @param Bytes The number of bytes to allocate. Calculated by the compiler.
/// @param C The ASTContext that provides the allocator.
/// @param Alignment The alignment of the allocated memory (if the underlying
///                  allocator supports it).
/// @return The allocated memory. Could be NULL.
inline void *operator new(size_t Bytes, const mlang::ASTContext &C,
                          size_t Alignment) throw () {
  return C.Allocate(Bytes, Alignment);
}
/// @brief Placement delete companion to the new above.
///
/// This operator is just a companion to the new above. There is no way of
/// invoking it directly; see the new operator for more details. This operator
/// is called implicitly by the compiler if a placement new expression using
/// the ASTContext throws in the object constructor.
inline void operator delete(void *Ptr, const mlang::ASTContext &C, size_t)
              throw () {
  C.Deallocate(Ptr);
}

/// This placement form of operator new[] uses the ASTContext's allocator for
/// obtaining memory. It is a non-throwing new[], which means that it returns
/// null on error.
/// Usage looks like this (assuming there's an ASTContext 'Context' in scope):
/// @code
/// // Default alignment (8)
/// char *data = new (Context) char[10];
/// // Specific alignment
/// char *data = new (Context, 4) char[10];
/// @endcode
/// Please note that you cannot use delete on the pointer; it must be
/// deallocated using an explicit destructor call followed by
/// @c Context.Deallocate(Ptr).
///
/// @param Bytes The number of bytes to allocate. Calculated by the compiler.
/// @param C The ASTContext that provides the allocator.
/// @param Alignment The alignment of the allocated memory (if the underlying
///                  allocator supports it).
/// @return The allocated memory. Could be NULL.
inline void *operator new[](size_t Bytes, const mlang::ASTContext& C,
                            size_t Alignment = 8) throw () {
  return C.Allocate(Bytes, Alignment);
}

/// @brief Placement delete[] companion to the new[] above.
///
/// This operator is just a companion to the new[] above. There is no way of
/// invoking it directly; see the new[] operator for more details. This operator
/// is called implicitly by the compiler if a placement new[] expression using
/// the ASTContext throws in the object constructor.
inline void operator delete[](void *Ptr, const mlang::ASTContext &C, size_t)
              throw () {
  C.Deallocate(Ptr);
}

#endif /* MLANG_AST_ASTCONTEXT_H_ */
