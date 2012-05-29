//===--- Type.h - M Language Type Representation ----------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the Type interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_TYPE_H_
#define MLANG_AST_TYPE_H_

#include "mlang/Diag/Diagnostic.h"
#include "mlang/Diag/PartialDiagnostic.h"
#include "mlang/Basic/IdentifierTable.h"
#include "mlang/Basic/Linkage.h"
#include "mlang/Basic/Visibility.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/type_traits.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/PointerUnion.h"

using llvm::isa;
using llvm::cast;
using llvm::cast_or_null;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;

namespace mlang {
enum {
	TypeAlignmentInBits = 3, TypeAlignment = 1 << TypeAlignmentInBits
};
class RawType;
class ExtTypeInfo;
class Type;


/// Single variable can occupy a maximum 1G memory
enum {maxArrayDimensions = 30, defaultArrayDimensions = 2} ;
/// Dimension vector descriptor
typedef llvm::SmallVector<unsigned, defaultArrayDimensions> DimVector;

} //end namespace mlang

namespace llvm {
template<typename T>
class PointerLikeTypeTraits;

template<>
class PointerLikeTypeTraits< ::mlang::RawType*> {
public:
	static inline void *getAsVoidPointer(::mlang::RawType *P) {
		return P;
	}
	static inline ::mlang::RawType *getFromVoidPointer(void *P) {
		return static_cast< ::mlang::RawType*> (P);
	}
	enum {
		NumLowBitsAvailable = mlang::TypeAlignmentInBits
	};
};

template<>
class PointerLikeTypeTraits< ::mlang::ExtTypeInfo*> {
public:
	static inline void *getAsVoidPointer(::mlang::ExtTypeInfo *P) {
		return P;
	}
	static inline ::mlang::ExtTypeInfo *getFromVoidPointer(void *P) {
		return static_cast< ::mlang::ExtTypeInfo*> (P);
	}
	enum {
		NumLowBitsAvailable = mlang::TypeAlignmentInBits
	};
};

template<>
struct isPodLike<mlang::Type> {
	static const bool value = true;
};
} // end namespace llvm

namespace mlang {
class ASTContext;
class CellDefn;
class Expr;
class RawType;
class Stmt;
class StructDefn;
class SourceLocation;
class StmtIteratorBase;
class TypeDefn;
class UserClassDefn;
struct PrintingPolicy;

// Provide forward declarations for all of the *Type classes
#define TYPE(Class, Base) class Class##Type;
#include "mlang/AST/TypeNodes.def"

/// TypeInfo - Associated info of a type built-in types can be divided into
/// Simple type:
///  boolean (logical), numerical scalar, char, function handle
///
/// heterogeneous Container type:
///  struct, Cell Array, Containers.Map
///
/// homogeneous Array type:
///  boolean array, numeric array, text,  text array,
///  function handle array, container array
///
class TypeInfo {
public:
	enum Attr {
		ArrayTy = 0x1,
		AT_Complex = 0x2,
		AT_Global = 0x4,
		AT_Persistent = 0x8,
		AT_Sparse = 0x10,
		AT_Nesting = 0x20,
		AttrsMask = ArrayTy | AT_Complex | AT_Global | AT_Persistent |
		            AT_Sparse | AT_Nesting
	};

	enum {
		// width of fast bits for access to ArrayTy and complex attribute
		// type info
		FastWidth = 2,
		FastMask = (1 << FastWidth) - 1
	};

	TypeInfo() :
		Mask(0) {
	}

	static TypeInfo fromFastMask(unsigned Mask) {
		TypeInfo Qs;
		Qs.addFastTypeInfo(Mask);
		return Qs;
	}

	// Deserialize TypeInfo from an opaque representation.
	static TypeInfo fromOpaqueValue(unsigned opaque) {
		TypeInfo Qs;
		Qs.Mask = opaque;
		return Qs;
	}

	// Serialize these TypeInfo into an opaque representation.
	unsigned getAsOpaqueValue() const {
		return Mask;
	}

	bool isArrayType() const {
		return Mask & ArrayTy;
	}
	void setArrayType(bool flag) {
		Mask = (Mask & ~ArrayTy) | (flag ? ArrayTy : 0);
	}
	void removeArrayType() {
		Mask = Mask & ~ArrayTy;
	}
	void addArrayType() {
		Mask |= ArrayTy;
	}

	bool HasComplexAttr() const {
		return Mask & AT_Complex;
	}
	void setComplexAttr(bool flag) {
		Mask = (Mask & ~AT_Complex) | (flag ? AT_Complex : 0);
	}
	void removeComplexAttr() {
		Mask = Mask & ~AT_Complex;
	}
	void addComplexAttr() {
		Mask |= AT_Complex;
	}

	bool HasGlobalAttr() const {
		return Mask & AT_Global;
	}
	void setGlobalAttr(bool flag) {
		Mask = (Mask & ~AT_Global) | (flag ? AT_Global : 0);
	}
	void removeGlobalAttr() {
		Mask = Mask & ~AT_Global;
	}
	void addGlobalAttr() {
		Mask |= AT_Global;
	}

	bool HasPersistentAttr() const {
		return Mask & AT_Persistent;
	}
	void setPersistentAttr(bool flag) {
		Mask = (Mask & ~AT_Persistent) | (flag ? AT_Persistent : 0);
	}
	void removePersistentAttr() {
		Mask = Mask & ~AT_Persistent;
	}
	void addPersistentAttr() {
		Mask |= AT_Persistent;
	}

	bool HasSparseAttr() const {
		return Mask & AT_Sparse;
	}
	void setSparseAttr(bool flag) {
		Mask = (Mask & ~AT_Sparse) | (flag ? AT_Sparse : 0);
	}
	void removeSparseAttr() {
		Mask = Mask & ~AT_Sparse;
	}
	void addSparseAttr() {
		Mask |= AT_Sparse;
	}

	bool HasNestingAttr() const {
		return Mask & AT_Nesting;
	}
	void setNestingAttr(bool flag) {
		Mask = (Mask & ~AT_Nesting) | (flag ? AT_Nesting : 0);
	}
	void removeNestingAttr() {
		Mask = Mask & ~AT_Nesting;
	}
	void addNestingAttr() {
		Mask |= AT_Nesting;
	}

	bool HasAttrs() const {
		return getAttrs();
	}
	unsigned getAttrs() const {
		return Mask & AttrsMask;
	}
	void setAttrs(unsigned mask) {
		assert(!(mask & ~AttrsMask) && "bitmask contains non-Attrs bits");
		Mask = (Mask & ~AttrsMask) | mask;
	}
	void removeAttrs(unsigned mask) {
		assert(!(mask & ~AttrsMask) && "bitmask contains non-Attrs bits");
		Mask &=  ~mask;
	}
	void removeAttrs() {
		removeAttrs(AttrsMask);
	}
	void addAttrs(unsigned mask) {
		assert(!(mask & ~AttrsMask) && "bitmask contains non-Attrs bits");
		Mask |=  mask;
	}

	bool hasFastTypeInfo() const {
		return getFastTypeInfo();
	}
	unsigned getFastTypeInfo() const {
		return Mask & FastMask;
	}
	void setFastTypeInfo(unsigned mask) {
		assert(!(mask & ~FastMask) && "bitmask contains non-fast TypeInfo bits");
		Mask = (Mask & ~FastMask) | mask;
	}
	void removeFastTypeInfo(unsigned mask) {
		assert(!(mask & ~FastMask) && "bitmask contains non-fast TypeInfo bits");
		Mask &= ~mask;
	}
	void removeFastTypeInfo() {
		removeFastTypeInfo(FastMask);
	}
	void addFastTypeInfo(unsigned mask) {
		assert(!(mask & ~FastMask) && "bitmask contains non-fast TypeInfo bits");
		Mask |= mask;
	}

	bool hasNonFastTypeInfo() const {
		return Mask & ~FastMask & ArraySizeMask;
	}
	TypeInfo getNonFastTypeInfo() const {
		TypeInfo TI = *this;
		TI.setFastTypeInfo(0);
		return TI;
	}

	bool isArrayEmpty() const {
		return Mask & ArraySizeMask;
	}
	unsigned getArraySize() const {
		return Mask >> ArraySizeShift;
	}
	void setArraySize(unsigned space) {
		assert(space <= MaxArraySize);
		Mask = (Mask & ~ArraySizeMask) | (((uint32_t) space) << ArraySizeShift);
	}
	void removeArraySize() {
		setArraySize(0);
	}
	void addArraySize(unsigned space) {
		assert(space);
		setArraySize(space);
	}

	/// hasTypeInfo - Return true if the set contains any TypeInfo.
	bool hasTypeInfo() const {
		return Mask;
	}
	bool empty() const {
		return !Mask;
	}

	/// \brief Add the TypeInfo from the given set to this set.
	void addTypeInfo(TypeInfo Q) {
		// If the other set doesn't have any non-boolean TypeInfo, just
		// bit-or it in.
		if (!(Q.Mask & ~AttrsMask))
			Mask |= Q.Mask;
		else {
			Mask |= (Q.Mask & AttrsMask);
			if (Q.isArrayEmpty())
				addArraySize(Q.getArraySize());
		}
	}

	bool isSupersetOf(TypeInfo Other) const;

	bool operator==(TypeInfo Other) const {
		return Mask == Other.Mask;
	}
	bool operator!=(TypeInfo Other) const {
		return Mask != Other.Mask;
	}

	operator bool() const {
		return hasTypeInfo();
	}

	TypeInfo &operator+=(TypeInfo R) {
		addTypeInfo(R);
		return *this;
	}

	// Union two TypeInfo sets.  If an enumerated TypeInfo appears
	// in both sets, use the one from the right.
	friend TypeInfo operator+(TypeInfo L, TypeInfo R) {
		L += R;
		return L;
	}

	TypeInfo &operator-=(TypeInfo R) {
		Mask = Mask & ~(R.Mask);
		return *this;
	}

	/// \brief Compute the difference between two TypeInfo sets.
	friend TypeInfo operator-(TypeInfo L, TypeInfo R) {
		L -= R;
		return L;
	}

	std::string getAsString() const;
	std::string getAsString(const PrintingPolicy &Policy) const {
		std::string Buffer;
		getAsStringInternal(Buffer, Policy);
		return Buffer;
	}

	void getAsStringInternal(std::string &S, const PrintingPolicy &Policy) const;

	void Profile(llvm::FoldingSetNodeID &ID) const {
		ID.AddInteger(Mask);
	}

private:
	// bits:  |   0   |   1   |   2  |   3      |   4  |   5   |6  ...  31|
	//        |ArrayTy|Complex|Global|Persistent|Sparse|Nesting|Array Size|
	uint32_t Mask;

	static const uint32_t ArraySizeMask = ~ AttrsMask;
	static const uint32_t ArraySizeShift = 6;

public:
	// Max array size
	static const uint32_t MaxArraySize = (1 << (32 - ArraySizeShift)) - 1;
};

/// ExtTypeInfo - Packed RawType* and its associative TypeInfo
class ExtTypeInfo: public llvm::FoldingSetNode {
	/// BaseType - the underlying type that this qualifies
	const RawType *BaseType;

	/// Quals - the immutable set of TypeInfos applied by this
	/// node;  always contains extended qualifiers.
	TypeInfo Quals;

public:
	ExtTypeInfo(const RawType *Base, TypeInfo Quals) :
		BaseType(Base), Quals(Quals) {
		assert(Quals.hasNonFastTypeInfo()
				&& "ExtQuals created with no fast TypeInfos");
		assert(!Quals.hasFastTypeInfo()
				&& "ExtQuals created with fast TypeInfos");
	}

	TypeInfo getTypeInfo() const {
		return Quals;
	}

	bool hasComplexAttr() const {
		return Quals.HasAttrs();
	}

	bool hasArraySize() const {
		return !Quals.isArrayEmpty();
	}
	unsigned getArraySize() const {
		return Quals.getArraySize();
	}

	const RawType *getBaseType() const {
		return BaseType;
	}

public:
	void Profile(llvm::FoldingSetNodeID &ID) const {
		Profile(ID, getBaseType(), Quals);
	}
	static void Profile(llvm::FoldingSetNodeID &ID, const RawType *BaseType,
			TypeInfo Quals) {
		assert(!Quals.hasFastTypeInfo() && "fast TypeInfo in ExtTypeInfo hash!");
		ID.AddPointer(BaseType);
		Quals.Profile(ID);
	}
};

/// CallingConv - Specifies the calling convention that a function uses.
enum CallingConv {
	CC_Default, CC_C, // __attribute__((cdecl))
	CC_X86StdCall, // __attribute__((stdcall))
	CC_X86FastCall, // __attribute__((fastcall))
	CC_X86ThisCall, // __attribute__((thiscall))
	CC_X86Pascal
// __attribute__((pascal))
};

typedef std::pair<const RawType*, TypeInfo> SplitQualType;

/// Type - We pack a RawType pointer with its fast TypeInfo
class Type {
	// Thankfully, these are efficiently composable.
	llvm::PointerIntPair<
			llvm::PointerUnion<const RawType*, const ExtTypeInfo*>,
			TypeInfo::FastWidth> Value;

	const ExtTypeInfo *getExtTypeInfoUnsafe() const {
		return Value.getPointer().get<const ExtTypeInfo*> ();
	}
	const RawType *getRawTypePtrUnsafe() const {
		return Value.getPointer().get<const RawType*> ();
	}

	// Type getUnpackedTypeSlow() const;

public:
	Type() {
	}
	Type(const RawType *Ptr, unsigned TI) :
		Value(Ptr, TI) {
	}
	Type(const ExtTypeInfo *Ptr, unsigned TI) :
		Value(Ptr, TI) {
	}

	unsigned getFastTypeInfo() const {
		return Value.getInt();
	}
	void setFastTypeInfo(unsigned TI) {
		Value.setInt(TI);
	}

	/// Retrieves a pointer to the underlying (unqualified) type.
	/// This should really return a const Type, but it's not worth
	/// changing all the users right now.
	RawType *getRawTypePtr() const {
		if (hasNonFastTypeInfo())
			return const_cast<RawType*> (getExtTypeInfoUnsafe()->getBaseType());
		return const_cast<RawType*> (getRawTypePtrUnsafe());
	}

	/// Divides a QualType into its unqualified type and a set of local
	/// qualifiers.
	SplitQualType split() const;

	void *getAsOpaquePtr() const {
		return Value.getOpaqueValue();
	}
	static Type getFromOpaquePtr(void *Ptr) {
		Type T;
		T.Value.setFromOpaqueValue(Ptr);
		return T;
	}

	RawType &operator*() const {
		return *getRawTypePtr();
	}

	RawType *operator->() const {
		return getRawTypePtr();
	}

	/// isNull - Return true if this Type doesn't point to a type yet.
	bool isNull() const {
		return Value.getPointer().isNull();
	}

	/// \brief Determine whether this particular Type instance has the
	/// "restrict" qualifier set, without looking through typedefs that may have
	/// added "restrict" at a different level.
	bool isArrayType() const {
		return getFastTypeInfo() & TypeInfo::ArrayTy;
	}

	bool hasTypeInfo() const;

	bool hasNonFastTypeInfo() const {
		return Value.getPointer().is<const ExtTypeInfo*> ();
	}

	/// \brief Retrieve the set of typeinfos applied to this type.
	TypeInfo getTypeInfo() const;

	void addFastTypeInfo(unsigned TQs) {
		assert(!(TQs & ~TypeInfo::FastMask)
				&& "non-fast TypeInfo bits set in mask!");
		Value.setInt(Value.getInt() | TQs);
	}

	void removeFastTypeInfo() {
		Value.setInt(0);
	}
	void removeFastTypeInfo(unsigned Mask) {
		assert(!(Mask & ~TypeInfo::FastMask) && "mask has non-fast TypeInfo");
		Value.setInt(Value.getInt() & ~Mask);
	}

	// Creates a type with the given typeinfos in addition to any
	// typeinfos already on this type.
	Type withFastTypeInfo(unsigned TQs) const {
		Type T = *this;
		T.addFastTypeInfo(TQs);
		return T;
	}

	// Creates a type with exactly the given fast qualifiers, removing
	// any existing fast qualifiers.
	Type withExactFastTypeInfo(unsigned TQs) const {
		return withoutFastTypeInfo().withFastTypeInfo(TQs);
	}

	// Removes fast qualifiers, but leaves any extended typeinfo in place.
	Type withoutFastTypeInfo() const {
		Type T = *this;
		T.removeFastTypeInfo();
		return T;
	}

	/// operator==/!= - Indicate whether the specified types and typeinfo are
	/// identical.
	friend bool operator==(const Type &LHS, const Type &RHS) {
		return LHS.Value == RHS.Value;
	}
	friend bool operator!=(const Type &LHS, const Type &RHS) {
		return LHS.Value != RHS.Value;
	}

	std::string getAsString() const;

	static std::string getAsString(SplitQualType T);

	static std::string getAsString(const RawType *ty, TypeInfo qs);

	std::string getAsString(const PrintingPolicy &Policy) const {
		std::string S;
		getAsStringInternal(S, Policy);
		return S;
	}

	void getAsStringInternal(std::string &Str,
			                     const PrintingPolicy &Policy) const {
		return getAsStringInternal(split(), Str, Policy);
	}

	static void getAsStringInternal(SplitQualType split, std::string &out,
	                                const PrintingPolicy &policy) {
		return getAsStringInternal(split.first, split.second, out, policy);
	}

	static void getAsStringInternal(const RawType *ty, TypeInfo qs,
			                            std::string &buffer,
			                            const PrintingPolicy &policy);

	void dump(const char *s) const;
	void dump() const;

	void Profile(llvm::FoldingSetNodeID &ID) const {
		ID.AddPointer(getAsOpaquePtr());
	}

	/// getArraySize - Return the array size of this type.
	unsigned getArraySize() const;

	/// isComplex true when Type is complex type.
	bool hasComplexAttr() const {
		if (hasNonFastTypeInfo()) {
			const ExtTypeInfo *EQ = getExtTypeInfoUnsafe();
			return EQ->hasComplexAttr();
		}
		return false;
	}
};

// Inline function definitions.
inline bool Type::hasTypeInfo() const {
  return hasNonFastTypeInfo();
}

inline TypeInfo Type::getTypeInfo() const {
	TypeInfo Quals;
	if (hasNonFastTypeInfo())
		Quals = getExtTypeInfoUnsafe()->getTypeInfo();
	Quals.addFastTypeInfo(getFastTypeInfo());
	return Quals;
}

inline unsigned Type::getArraySize() const {
	if (hasNonFastTypeInfo()) {
		const ExtTypeInfo *EQ = getExtTypeInfoUnsafe();
		return EQ->getArraySize();
	}
	return 0;
}

inline SplitQualType Type::split() const {
  if (!hasNonFastTypeInfo())
    return SplitQualType(getRawTypePtrUnsafe(),
                         TypeInfo::fromFastMask(getFastTypeInfo()));

  const ExtTypeInfo *eq = getExtTypeInfoUnsafe();
  TypeInfo qs = eq->getTypeInfo();
  return SplitQualType(eq->getBaseType(), qs);
}
} // end namespace mlang

namespace llvm {
/// Implement simplify_type for QualType, so that we can dyn_cast from QualType
/// to a specific Type class.
template<> struct simplify_type<const ::mlang::Type> {
  typedef ::mlang::RawType* SimpleType;
  static SimpleType getSimplifiedValue(const ::mlang::Type &Val) {
    return Val.getRawTypePtr();
  }
};
template<> struct simplify_type< ::mlang::Type>
  : public simplify_type<const ::mlang::Type> {};

// Teach SmallPtrSet that QualType is "basically a pointer".
template<>
class PointerLikeTypeTraits<mlang::Type> {
public:
  static inline void *getAsVoidPointer(mlang::Type P) {
    return P.getAsOpaquePtr();
  }
  static inline mlang::Type getFromVoidPointer(void *P) {
    return mlang::Type::getFromOpaquePtr(P);
  }
  // Various qualifiers go in low bits.
  enum { NumLowBitsAvailable = 0 };
};

} // end namespace llvm

namespace mlang {

/// There are many different data types, or classes, that you can work with
/// in the MATLAB software. You can build matrices and arrays of floating-point
/// and integer data, characters and strings, and logical true and false states.
/// Function handles connect your code with any MATLAB function regardless of
/// the current scope. Structures and cell arrays, provide a way to store
/// dissimilar types of data in the same array.
///
/// There are 15 fundamental classes in MATLAB. Each of these classes is in the
/// form of a matrix or array. This matrix or array is a minimum of 0-by-0 in
/// size and can grow to an n-dimensional array of any size.
///
/// Numeric classes in the MATLAB software include signed and unsigned integers,
/// and single- and double-precision floating-point numbers. By default, MATLAB
/// stores all numeric values as double-precision floating point. (You cannot
/// change the default type and precision.) You can choose to store any number,
/// or array of numbers, as integers or as single-precision. Integer and
/// single-precision arrays offer more memory-efficient storage than
/// double-precision.
///
/// All numeric types support basic array operations, such as subscripting and
/// reshaping. All numeric types except for int64 and uint64 can be used in
/// mathematical operations.
///
/// You can create two-dimensional double and logical matrices using one of two
/// storage formats: full or sparse. For matrices with mostly zero-valued
/// elements, a sparse matrix requires a fraction of the storage space required
/// for an equivalent full matrix. Sparse matrices invoke methods especially
/// tailored to solve sparse problems
///
/// These classes require different amounts of storage, the smallest being a
/// logical value or 8–bit integer which requires only 1 byte. It is important
/// to keep this minimum size in mind if you work on data in files that were
/// written using a precision smaller than 8 bits.
///
class RawType {
public:
	enum TypeClass {
#define TYPE(Class, Base) Class,
#define LAST_TYPE(Class) TypeLast = Class
#define ABSTRACT_TYPE(Class, Base)
#include "mlang/AST/TypeNodes.def"
	};

private:
	RawType(const RawType&);           // DO NOT IMPLEMENT.
	void operator=(const RawType&);    // DO NOT IMPLEMENT.

	class TypeBitfields {
		friend class RawType;

		/// TypeClass bitfield - Enum that specifies what subclass this belongs to.
		unsigned TC :5;
		/// \brief Nonzero if the cache (i.e. the bitfields here starting
		/// with 'Cache') is valid.  If so, then this is a
		/// LangOptions::VisibilityMode+1.
		mutable unsigned CacheValidAndVisibility : 2;
		/// \brief Linkage of this type.
		mutable unsigned CachedLinkage :2;
		/// \brief FromAST - Whether this type comes from an AST file.
		mutable bool FromAST :1;

		bool isCacheValid() const {
			return (CacheValidAndVisibility != 0);
		}
		Visibility getVisibility() const {
			assert(isCacheValid() && "getting linkage from invalid cache");
			return static_cast<Visibility>(CacheValidAndVisibility-1);
		}
		Linkage getLinkage() const {
		  assert(isCacheValid() && "getting linkage from invalid cache");
		  return static_cast<Linkage>(CachedLinkage);
	    }
	};

	enum { NumTypeBits = 10 };

protected:
	class NumericTypeBitfields {
		friend class SimpleNumericType;

		unsigned : NumTypeBits;

		unsigned NumKind : 4;
	};

	class FunctionTypeBitfields {
		friend class FunctionType;

		unsigned : NumTypeBits;

		/// Extra information which affects how the function is called, like
		/// regparm and the calling convention.
		unsigned ExtInfo : 8;

		/// MultiOutput - whether this function return multiple results.
		unsigned MultiOutput : 1;

		/// isVariadic - whether this function can accept variable inputs.
		unsigned isVariadic : 1;
	};

	class MatrixTypeBitfields {
		friend class MatrixType;

		unsigned : NumTypeBits;

		/// VecKind - The kind of vector, either a generic vector type or some
		/// target-specific vector type such as for AltiVec or Neon.
		unsigned MatKind : 4;

		/// NumElements - The number of rows and columns in the matrix.
		unsigned NumColumns : 14;
		unsigned NumRows : 14 - NumTypeBits;
	};

	class VectorTypeBitfields {
		friend class VectorType;

		unsigned : NumTypeBits;

		/// VecKind - The kind of vector, either a generic vector type or some
		/// target-specific vector type such as for AltiVec or Neon.
		unsigned VecKind : 3;

		/// RowVector - indicate whether this is a RowVector or ColumnVector.
		unsigned RowVector : 1;

		/// NumElements - The number of elements in the vector.
		unsigned NumElements : 29 - NumTypeBits;
	};

	class ReferenceTypeBitfields {
		friend class ReferenceType;

		unsigned : NumTypeBits;

		unsigned SpelledAsLValue : 1;
		unsigned InnerRef : 1;
	};

	union {
		TypeBitfields TypeBits;
		NumericTypeBitfields NumericTypeBits;
		FunctionTypeBitfields FunctionTypeBits;
		MatrixTypeBitfields MatrixTypeBits;
		VectorTypeBitfields VectorTypeBits;
		ReferenceTypeBitfields ReferenceTypeBits;
	};

private:
  /// \brief Set whether this type comes from an AST file.
  void setFromAST(bool V = true) const {
    TypeBits.FromAST = V;
  }
	/// \brief Compute the linkage of this type.
  // virtual Linkage getLinkageImpl() const;

protected:
	// silence VC++ warning C4355: 'this' : used in base member initializer list
	RawType *this_() {
		return this;
	}

	explicit RawType(TypeClass tc) {
		TypeBits.TC = tc;
		TypeBits.CacheValidAndVisibility = 0;
		TypeBits.CachedLinkage = NoLinkage;
		TypeBits.FromAST = false;
	}

	friend class ASTContext;

public:
	TypeClass getTypeClass() const {
		return static_cast<TypeClass> (TypeBits.TC);
	}

	/// \brief Whether this type comes from an AST file.
	bool isFromAST() const {
		return TypeBits.FromAST;
	}

	// integer + floating + char + logical + function handle
	bool isFundamentalType() const;

	// integer + floating + char + logical
	bool isSimpleNumericType() const;
	bool isLogicalType() const;
	bool isCharType() const;
	// integer + floating
	bool isNumericType() const;
	bool isIntegerType() const;
	bool isSignedIntegerType() const;
	bool isUnsignedIntegerType() const;
	bool isFloatingPointType() const;

	// struct + cell + map + classdef
	bool isHeteroContainerType() const;
	bool isStructType() const;
	bool isCellType() const;
	bool isMapType() const;
	bool isClassdefType() const;

	// various function types
	bool isFunctionType() const;
	bool isFunctionProtoType() const;
	bool isFunctionNoProtoType() const;
	bool isFunctionHandleType() const;

	// NDArray, Matrix, Vector, RowVector and ColumnVector in Matrix
	bool isArrayType() const;
	bool isMatrixType() const;
	bool isNDArrayType() const;
	bool isVectorType() const;

	//Extending Types
	bool isReferenceType() const;
	bool isLValueReferenceType() const;
	bool isRValueReferenceType() const;

	/// More type predicates useful for type checking/promotion
	bool isPromotableIntegerType() const;

	// Member-template getAs<specific type>'.  Look through sugar for
	// an instance of <specific type>.   This scheme will eventually
	// replace the specific getAsXXXX methods above.
	//
	// There are some specializations of this member template listed
	// immediately following this class.
	template<typename T> const T *getAs() const;

	// Type Checking Functions: Check to see if this type is structurally the
	// specified type, ignoring typedefs and qualifiers, and return a pointer to
	// the best type we can.
	const StructType *getAsStructureType() const;

	/// isSpecifierType - Returns true if this type can be represented by some
	/// set of type specifiers.
	bool isSpecifierType() const;

	/// \brief Determine whether this type has an integer representation
	/// of some sort, e.g., it is an integer type or a vector.
	bool hasIntegerRepresentation() const;

	/// \brief Determine whether this type has an signed integer representation
	/// of some sort, e.g., it is an signed integer type or a vector.
	bool hasSignedIntegerRepresentation() const;

	/// \brief Determine whether this type has an unsigned integer representation
	/// of some sort, e.g., it is an unsigned integer type or a vector.
	bool hasUnsignedIntegerRepresentation() const;

	/// \brief Determine whether this type has a floating-point representation
	/// of some sort, e.g., it is a floating-point type or a vector thereof.
	bool hasFloatingRepresentation() const;

	/// \brief Determine the linkage of this type.
	Linkage getLinkage() const;

	/// \brief Note that the linkage is no longer known.
	void ClearLinkageCache();

	const char *getTypeClassName() const;

	void dump() const;
	static bool classof(const RawType *) {
		return true;
	}

	friend class ASTReader;
	friend class ASTWriter;
};

// We can do leaf types faster, because we don't have to worry about
// preserving child type decoration.
#define TYPE(Class, Base)
#define LEAF_TYPE(Class) \
template <> inline const Class##Type *RawType::getAs() const { \
  return dyn_cast<Class##Type>(this); \
}
#include "mlang/AST/TypeNodes.def"

//==========================================
// class definition of various data classes
//==========================================
/// SimpleNumericType - Represents a base type for numeric types.
class SimpleNumericType: public RawType {
	friend class ASTContext;

public:
	enum NumericKind {
		Logical, //1 byte
		Char, //2 bytes
		Int8, Int16, Int32, Int64, UInt8, UInt16, UInt32, UInt64,
		Single, Double
	};

	explicit SimpleNumericType(NumericKind NK) :
		RawType(SimpleNumeric) {
		NumericTypeBits.NumKind = NK;
	}

	const char * getName() const {
		unsigned K = getKind();
		switch (K) {
		case Logical:
			return "logical";
		case Char:
			return "char";
		case Int8:
			return "int8";
		case Int16:
			return "int16";
		case Int32:
			return "int32";
		case Int64:
			return "int64";
		case UInt8:
			return "uint8";
		case UInt16:
			return "uint16";
		case UInt32:
			return "uint32";
		case UInt64:
			return "uint64";
		case Single:
			return "single";
		case Double:
			return "double";
		default:
			return "";
		}
	}

	NumericKind getKind() const {
		return static_cast<NumericKind>(NumericTypeBits.NumKind);
	}

	bool isSugared() const { return false; }
	Type desugar() const { return Type(this, 0); }

	bool isChar() const {
		return getKind() == Char;
	}

	bool isLogical() const {
		return getKind() == Logical;
	}

	bool isInteger() const {
		return getKind() >= Int8 || getKind() < Single;
	}

	bool isUnsignedInteger() const {
		return getKind() >= UInt8 && getKind() <= UInt64;
	}

	bool isFloatingPoint() const {
		return getKind() == Single || getKind() == Double;
	}

	static bool classof(const RawType *T) {
		return T->getTypeClass() == SimpleNumeric;
	}
	static bool classof(const SimpleNumericType *) { return true; }
};

/// ArrayType - Represents an Array of elements of basic types
class ArrayType : public RawType {
protected:
	DimVector dims;
	unsigned Size;

  ArrayType(TypeClass tc, Type EL) :
    RawType(tc), ElementType(EL) {
  }

  friend class ASTContext;  // ASTContext creates these.

public:

  ArrayType(TypeClass tc, Type EL, unsigned S) :
      RawType(tc), Size(S), ElementType(EL) {
    }

  Type getElementType() const { return ElementType; }

  unsigned getSize() const { return Size; }

  DimVector getDimensions() const { return dims; }

  static bool classof(const RawType *T) {
    return T->getTypeClass() == Matrix ||
    		   T->getTypeClass() == NDArray ||
           T->getTypeClass() == Vector ;
  }
  static bool classof(const ArrayType *) { return true; }

private:
	/// ElementType - The element type of the array.
  Type ElementType;
};

/// MatrixType - Represents a Matrix
class MatrixType : public ArrayType, public llvm::FoldingSetNode {

	friend class ASTContext; // ASTContext creates these.

public:
	enum MatrixKind {
		Unknown = 0,
		Full,
		Diagonal,
		Permuted_Diagonal,
		Upper,
		Lower,
		Permuted_Upper,
		Permuted_Lower,
		Banded,
		Hermitian,
		Banded_Hermitian,
		Tridiagonal,
		Tridiagonal_Hermitian,
		Rectangular
	};

	MatrixType(Type EL, unsigned R, unsigned C, MatrixKind MK) :
		ArrayType(Matrix, EL) {
		MatrixTypeBits.MatKind = MK;
		MatrixTypeBits.NumColumns = C;
		MatrixTypeBits.NumRows = R;
		dims.push_back(R);
		dims.push_back(C);
		Size = R * C;
	}

	MatrixKind getMatrixKind() const {
    return MatrixKind(MatrixTypeBits.MatKind);
  }
  unsigned getNumRows() const {
    return MatrixTypeBits.NumRows;
  }
  unsigned getNumColumns() const {
      return MatrixTypeBits.NumColumns;
    }
  void Profile(llvm::FoldingSetNodeID &ID) {
	  Profile(ID, getElementType(), getNumRows(), getNumColumns(),
			      getMatrixKind());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, Type ElementType,
		                  unsigned NumRows, unsigned NumColumns,
		                  MatrixKind MatKind) {
	  ID.AddPointer(ElementType.getAsOpaquePtr());
	  ID.AddInteger(NumRows);
	  ID.AddInteger(NumColumns);
	  ID.AddInteger((TypeClass)Matrix);
	  ID.AddInteger(MatKind);
  }
  static bool classof(const RawType *T) {
	  return T->getTypeClass() == Matrix;
  }
  static bool classof(const MatrixType *) { return true; }
};

/// NDArrayType - Represents a N-Dimension Array
class NDArrayType : public ArrayType, public llvm::FoldingSetNode {

	friend class ASTContext; // ASTContext creates these.

public:
	NDArrayType(Type EL, DimVector dv) :
		ArrayType(NDArray, EL) {
		dims = dv;
		Size = 1;
		for(DimVector::iterator I = dims.begin(); I < dims.end(); I++) {
			Size *= dims[*I];
		}
	}

  void Profile(llvm::FoldingSetNodeID &ID) {
	  Profile(ID, getElementType(), getDimensions());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, Type ElementType,
		                  DimVector dims) {
	  ID.AddPointer(ElementType.getAsOpaquePtr());
	  ID.AddPointer(&dims);
	  ID.AddInteger(TypeClass(NDArray));
  }
  static bool classof(const RawType *T) {
	  return T->getTypeClass() == NDArray;
  }
  static bool classof(const NDArrayType *) { return true; }
};

/// VectorType - Represents a vector
class VectorType : public ArrayType, public llvm::FoldingSetNode {

	friend class ASTContext;  // ASTContext creates these.

public:
	enum VectorKind {
		GenericVector,  // not a target-specific vector type
		AltiVecVector,  // is AltiVec vector
		AltiVecPixel,   // is AltiVec 'vector Pixel'
		AltiVecBool,    // is AltiVec 'vector bool ...'
		NeonVector,     // is ARM Neon vector
		NeonPolyVector  // is ARM Neon polynomial vector
	};

	VectorType(Type EL, unsigned nElements, VectorKind VK, bool isRowVec) :
	  ArrayType(Vector, EL) {
		VectorTypeBits.NumElements = nElements;
		VectorTypeBits.VecKind = VK;
		VectorTypeBits.RowVector = isRowVec;
		if(isRowVec) {
			dims.push_back(1);
			dims.push_back(nElements);
			Size = nElements;
		} else {
			dims.push_back(nElements);
			dims.push_back(1);
			Size = nElements;
		}
	}

	VectorKind getVectorKind() const { return VectorKind(VectorTypeBits.VecKind);}
	unsigned getNumElements() const { return VectorTypeBits.NumElements; }
	bool isRowVector() const { return VectorTypeBits.RowVector;}
	bool isColumnVector() const { return !isRowVector();}

  void Profile(llvm::FoldingSetNodeID &ID) {
	  Profile(ID, getElementType(), getNumElements(),
			  getTypeClass(), isRowVector(), getVectorKind());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, Type ElementType,
		                  unsigned NumElements, TypeClass TypeClass,
		                  bool isRowVec, VectorKind VecKind) {
	  ID.AddPointer(ElementType.getAsOpaquePtr());
	  ID.AddInteger(NumElements);
	  ID.AddInteger(TypeClass);
	  ID.AddInteger(isRowVec);
	  ID.AddInteger(VecKind);
  }

  static bool classof(const RawType *T) {
	  return T->getTypeClass() == Vector;
  }
  static bool classof(const VectorType *) { return true; }
};

/// HeteroContainerType - Represents a container prototype.
class HeteroContainerType: public RawType {
	TypeDefn* defn;

protected:
	HeteroContainerType(TypeClass tc, const TypeDefn *defn) :
		RawType(tc), defn(const_cast<TypeDefn*>(defn)) {
	}

	friend class ASTContext;

public:
	TypeDefn * getDefn() const {
		return defn;
	}

	/// @brief Determines whether this type is in the process of being
	/// defined.
	bool isBeingDefined() const;

	bool isSugared() const { return false; }
	Type desugar() const { return Type(this, 0); }

	static bool classof(const RawType *T) {
		return T->getTypeClass() == Struct ||
				   T->getTypeClass() == Cell ||
				   T->getTypeClass() == Map ||
				   T->getTypeClass() == Classdef;
	}
	static bool classof(const HeteroContainerType *) { return true; }
	static bool classof(const StructType *) { return true; }
	static bool classof(const CellType *) { return true; }
	static bool classof(const MapType *) { return true; }
	static bool classof(const ClassdefType *) { return true; }
};

/// StructType - Represents a struct type
class StructType: public HeteroContainerType {
	explicit StructType(const StructDefn *D) :
	  HeteroContainerType(Struct, reinterpret_cast<const TypeDefn *>(D)) {
	}

  friend class ASTContext;   // ASTContext creates these.

public:
  const char * getName() const;

  StructDefn *getDefn() const {
	  return reinterpret_cast<StructDefn*>(HeteroContainerType::getDefn());
  }

  bool isSugared() const { return false; }
  Type desugar() const { return Type(this, 0); }

  unsigned getSize() const { return 0;}

	static bool classof(const HeteroContainerType *T);
	static bool classof(const RawType *T) {
		return isa<HeteroContainerType>(T) &&
				classof(cast<HeteroContainerType>(T));
	}
	static bool classof(const StructType *) { return true; }
};

/// CellType - Represents a cell array type
class CellType: public HeteroContainerType {
	explicit CellType(const CellDefn *D) :
		HeteroContainerType(Cell, reinterpret_cast<const TypeDefn *>(D)) {
	}

	friend class ASTContext;   // ASTContext creates these.

public:
	const char * getName() const;

	CellDefn *getDefn() const {
		return reinterpret_cast<CellDefn*>(HeteroContainerType::getDefn());
	}

	bool isSugared() const { return false; }
	Type desugar() const { return Type(this, 0); }

	static bool classof(const HeteroContainerType *T);
	static bool classof(const RawType *T) {
		return isa<HeteroContainerType>(T) &&
				classof(cast<HeteroContainerType>(T));
	}
	static bool classof(const CellType *) { return true; }
};

/// MapType - Represents a container.map type
class MapType: public HeteroContainerType {

};

/// ClassdefType - Represents a user-defined class
class ClassdefType: public HeteroContainerType {
  explicit ClassdefType(const UserClassDefn *D) :
		HeteroContainerType(Classdef, reinterpret_cast<const TypeDefn *>(D)) {
  	}

	friend class ASTContext;   // ASTContext creates these.

public:
	const char * getName() const;

	UserClassDefn *getDefn() const {
		return reinterpret_cast<UserClassDefn*>(HeteroContainerType::getDefn());
	}

	bool isSugared() const { return false; }
	Type desugar() const { return Type(this, 0); }

	static bool classof(const HeteroContainerType *T);
	static bool classof(const RawType *T) {
		return isa<HeteroContainerType>(T) &&
				classof(cast<HeteroContainerType>(T));
	}
	static bool classof(const ClassdefType *) { return true; }
};

/// FunctionType - Represents a function
class FunctionType: public RawType {
  Type ResTy;

public:
	/// ExtInfo - A class which abstracts out some details necessary for
	/// making a call.
	///
	/// It is not actually used directly for storing this information in
	/// a FunctionType, although FunctionType does currently use the
	/// same bit-pattern.
	///
	// If you add a field (say Foo), other than the obvious places (both,
	// constructors, compile failures), what you need to update is
	// * Operator==
	// * getFoo
	// * withFoo
	// * functionType. Add Foo, getFoo.
	// * ASTContext::getFooType
	// * ASTContext::mergeFunctionTypes
	// * FunctionNoProtoType::Profile
	// * FunctionProtoType::Profile
	// * TypePrinter::PrintFunctionProto
	// * AST read and write
	// * Codegen
	class ExtInfo {
		// Feel free to rearrange or add bits, but if you go over 8,
		// you'll need to adjust both the Bits field below and
		// Type::FunctionTypeBitfields.
	    //   |  CC  |noreturn|produces|regparm|
	    //   |0 .. 2|   3    |    4   | 5 .. 7|
		enum {
			CallConvMask = 0x7
		};
		enum {
			NoReturnMask = 0x8
		};
		enum { ProducesResultMask = 0x10 };
		enum { RegParmMask = ~(CallConvMask | NoReturnMask | ProducesResultMask),
	         RegParmOffset = 5 }; // Assumed to be the last field

		uint16_t Bits;
		ExtInfo(unsigned Bits) :
			Bits(static_cast<uint16_t> (Bits)) {
		}

		friend class FunctionType;
	public:
		// Constructor with no defaults. Use this when you know that you
		// have all the elements (when reading an AST file for example).
		ExtInfo(bool noReturn, bool hasRegParm, unsigned regParm,
				CallingConv cc, bool producesResult) {
			assert((!hasRegParm || regParm < 7) && "Invalid regparm value");
			Bits = ((unsigned) cc) |
					    (noReturn ? NoReturnMask : 0) |
					    (producesResult ? ProducesResultMask : 0) |
					    (hasRegParm ? ((regParm + 1) << RegParmOffset) : 0);
		}

		// Constructor with all defaults. Use when for example creating a
		// function know to use defaults.
		ExtInfo() :
			Bits(0) {
		}

		bool getNoReturn() const {
			return Bits & NoReturnMask;
		}
		bool getProducesResult() const { return Bits & ProducesResultMask; }
		bool getHasRegParm() const { return (Bits >> RegParmOffset) != 0; }
		unsigned getRegParm() const {
			return Bits >> RegParmOffset;
		}
		CallingConv getCC() const {
			return CallingConv(Bits & CallConvMask);
		}

		bool operator==(ExtInfo Other) const {
			return Bits == Other.Bits;
		}
		bool operator!=(ExtInfo Other) const {
			return Bits != Other.Bits;
		}

		// Note that we don't have setters. That is by design, use
		// the following with methods instead of mutating these objects.
		ExtInfo withNoReturn(bool noReturn) const {
			if (noReturn)
				return ExtInfo(Bits | NoReturnMask);
			else
				return ExtInfo(Bits & ~NoReturnMask);
		}

		ExtInfo withProducesResult(bool producesResult) const {
			if (producesResult)
				return ExtInfo(Bits | ProducesResultMask);
			else
				return ExtInfo(Bits & ~ProducesResultMask);
		}

		ExtInfo withRegParm(unsigned RegParm) const {
			assert(RegParm < 7 && "Invalid regparm value");
			return ExtInfo((Bits & ~RegParmMask) |
					           ((RegParm + 1) << RegParmOffset));
		}

		ExtInfo withCallingConv(CallingConv cc) const {
			return ExtInfo((Bits & ~CallConvMask) | (unsigned) cc);
		}

		void Profile(llvm::FoldingSetNodeID &ID) const {
			ID.AddInteger(Bits);
		}
	};

protected:
	FunctionType(TypeClass tc, Type res, bool isMultiOutput,
			         bool variadic, ExtInfo Info) :
		RawType(tc), ResTy(res) {
		FunctionTypeBits.ExtInfo = Info.Bits;
		FunctionTypeBits.MultiOutput = isMultiOutput;
		FunctionTypeBits.isVariadic = variadic;
	}

	bool isMultiOutput() const {
		return FunctionTypeBits.MultiOutput;
	}

	bool isVariadic() const {
		return FunctionTypeBits.isVariadic;
	}

public:
	Type getResultType() const {
		return ResTy;
	}

	bool getHasRegParm() const { return getExtInfo().getHasRegParm(); }
	unsigned getRegParmType() const { return getExtInfo().getRegParm(); }
	bool getNoReturnAttr() const { return getExtInfo().getNoReturn(); }
	CallingConv getCallConv() const { return getExtInfo().getCC(); }
	ExtInfo getExtInfo() const { return ExtInfo(FunctionTypeBits.ExtInfo); }

	/// \brief Determine the type of an expression that calls a function of
	/// this type.
	// FIXME yabin
	//	Type getCallResultType(ASTContext &Context) const {
	//		return getResultType().getNonLValueExprType(Context);
	//	}

	static llvm::StringRef getNameForCallConv(CallingConv CC);

	static bool classof(const RawType *T) {
		return T->getTypeClass() == FunctionNoProto || T->getTypeClass()
				== FunctionProto;
	}
	static bool classof(const FunctionType *) {
		return true;
	}
};

/// FunctionProtoType - Represent a function with input and output param
class FunctionProtoType: public FunctionType, public llvm::FoldingSetNode {
public:
	struct ExtProtoInfo {
		ExtProtoInfo() :
			Variadic(false), HasExceptionSpec(false),
					HasAnyExceptionSpec(false), MultiOutput(0),
					NumExceptions(0), Exceptions(0) {
		}

		FunctionType::ExtInfo ExtInfo;
		bool Variadic;
		bool HasExceptionSpec;
		bool HasAnyExceptionSpec;
		unsigned char MultiOutput;
		unsigned NumExceptions;
		const Type *Exceptions;
	};

private:
	FunctionProtoType(Type result, const Type *args, unsigned numArgs,
			const ExtProtoInfo & epi);

	void InitFuncArguments(unsigned numArgs, const Type *args,
			const ExtProtoInfo & epi);
	unsigned NumArgs :20;
	unsigned NumExceptions :10;
	unsigned HasExceptionSpec :1;
	unsigned HasAnyExceptionSpec :1;

	friend class ASTContext; // created by ASTContext

public:
	unsigned getNumArgs() const {
		return NumArgs;
	}

	Type getArgType(unsigned i) const {
		assert(i < NumArgs && "Invalid argument number!");
		return arg_type_begin()[i];
	}

	ExtProtoInfo getExtProtoInfo() const {
		ExtProtoInfo EPI;
		EPI.ExtInfo = getExtInfo();
		EPI.MultiOutput = isMultiOutput();
		EPI.Variadic = isVariadic();
		EPI.HasExceptionSpec = hasExceptionSpec();
		EPI.HasAnyExceptionSpec = hasAnyExceptionSpec();
		EPI.NumExceptions = NumExceptions;
		EPI.Exceptions = exception_begin();
		return EPI;
	}

	bool hasExceptionSpec() const {
		return HasExceptionSpec;
	}

	bool hasAnyExceptionSpec() const {
		return HasAnyExceptionSpec;
	}

	unsigned getNumExceptions() const {
		return NumExceptions;
	}

	Type getExceptionType(unsigned i) const {
		assert(i < NumExceptions && "Invalid exception number!");
		return exception_begin()[i];
	}

	bool hasEmptyExceptionSpec() const {
		return hasExceptionSpec() && !hasAnyExceptionSpec()
				&& getNumExceptions() == 0;
	}

	using FunctionType::isMultiOutput;
	using FunctionType::isVariadic;
	typedef const Type *arg_type_iterator;
	arg_type_iterator arg_type_begin() const {
		return reinterpret_cast<const Type*> (this + 1);
	}
	arg_type_iterator arg_type_end() const {
		return arg_type_begin() + NumArgs;
	}

	typedef const Type *exception_iterator;
	exception_iterator exception_begin() const {
		return arg_type_end();
	}
	exception_iterator exception_end() const {
		return exception_begin() + NumExceptions;
	}

	bool isSugared() const {
		return false;
	}
	Type desugar() const {
		return Type(this, 0);
	}

	static bool classof(const RawType *T) {
		return T->getTypeClass() == FunctionProto;
	}
	static bool classof(const FunctionProtoType *) {
		return true;
	}

	void Profile(llvm::FoldingSetNodeID &ID, const ASTContext &Ctx);
	static void	Profile(llvm::FoldingSetNodeID &ID, Type Result,
					            arg_type_iterator ArgTys, unsigned NumArgs,
					            const ExtProtoInfo &EPI, const ASTContext &Ctx);
};

/// FunctionNoProtoType - - Represent a function without input param
class FunctionNoProtoType: public FunctionType, public llvm::FoldingSetNode {
	FunctionNoProtoType(Type Result, ExtInfo Info) :
		FunctionType(FunctionNoProto, Result, false, false, Info) {
	}

	friend class ASTContext; // ASTContext creates these.

public:
	bool isSugared() const {
		return false;
	}
	Type desugar() const {
		return Type(this, 0);
	}

	void Profile(llvm::FoldingSetNodeID &ID) {
		Profile(ID, getResultType(), getExtInfo());
	}
	static void Profile(llvm::FoldingSetNodeID &ID, Type ResultType,
			ExtInfo Info) {
		Info.Profile(ID);
		ID.AddPointer(ResultType.getAsOpaquePtr());
	}

	static bool classof(const RawType *T) {
		return T->getTypeClass() == FunctionNoProto;
	}
	static bool classof(const FunctionNoProtoType *) { return true; }
};

/// FunctionHandleType - Represents a prototype with argument type info, e.g.
/// 'int foo(int)' or 'int foo(void)'.  'void' is represented as having no
class FunctionHandleType : public RawType, public llvm::FoldingSetNode {
	Type PointeeFCNType;

	explicit FunctionHandleType(Type Pointee) :
	    RawType(FunctionHandle),
	    PointeeFCNType(Pointee) {
	}

	friend class ASTContext;  // ASTContext creates these.

public:
	const char * getName() const;

	Type getPointeeType() const {
		return PointeeFCNType;
	}

	bool isSugared() const { return false; }
	Type desugar() const { return Type(this, 0); }

  void Profile(llvm::FoldingSetNodeID &ID) {
	  Profile(ID, getPointeeType());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, Type Pointee) {
	  ID.AddPointer(Pointee.getAsOpaquePtr());
  }

  static bool classof(const RawType *T) {
		return T->getTypeClass() == FunctionHandle;
	}
	static bool classof(const FunctionHandleType *) { return true; }
};

/// ReferenceType - Represents a referrence to a real value type
class ReferenceType : public RawType, public llvm::FoldingSetNode{
	Type PointeeType;

protected:
	ReferenceType(TypeClass tc, Type Referencee, bool SpelledAsLValue) :
		RawType(tc), PointeeType(Referencee) {
		ReferenceTypeBits.SpelledAsLValue = SpelledAsLValue;
		ReferenceTypeBits.InnerRef = Referencee->isReferenceType();
	}

public:
	bool isSpelledAsLValue() const {
		return ReferenceTypeBits.SpelledAsLValue;
	}
	bool isInnerRef() const {
		return ReferenceTypeBits.InnerRef;
	}

	Type getPointeeTypeAsWritten() const {
		return PointeeType;
	}
	Type getPointeeType() const {
		// FIXME: this might strip inner qualifiers; okay?
		const ReferenceType *T = this;
		while (T->isInnerRef())
			T = T->PointeeType->getAs<ReferenceType> ();
		return T->PointeeType;
	}

	void Profile(llvm::FoldingSetNodeID &ID) {
		Profile(ID, PointeeType, isSpelledAsLValue());
	}
	static void Profile(llvm::FoldingSetNodeID &ID, Type Referencee,
			bool SpelledAsLValue) {
		ID.AddPointer(Referencee.getAsOpaquePtr());
		ID.AddBoolean(SpelledAsLValue);
	}

	static bool classof(const RawType *T) {
		return T->getTypeClass() == LValueReference || T->getTypeClass()
				== RValueReference;
	}
	static bool classof(const ReferenceType *) {
		return true;
	}
};

class LValueReferenceType : public ReferenceType {
	LValueReferenceType(Type Referencee, bool SpelledAsLValue) :
	    ReferenceType(LValueReference, Referencee, SpelledAsLValue)
	  {}
	friend class ASTContext; // ASTContext creates these

public:
	  bool isSugared() const { return false; }
	  Type desugar() const { return Type(this, 0); }

	  static bool classof(const RawType *T) {
	    return T->getTypeClass() == LValueReference;
	  }
	  static bool classof(const LValueReferenceType *) { return true; }
};

class RValueReferenceType : public ReferenceType {
	explicit RValueReferenceType(Type Referencee) :
		ReferenceType(RValueReference, Referencee, false) {}

	friend class ASTContext; // ASTContext creates these

public:
	bool isSugared() const { return false; }
	Type desugar() const { return Type(this, 0); }

	static bool classof(const RawType *T) {
	  return T->getTypeClass() == RValueReference;
	}
	static bool classof(const RValueReferenceType *) { return true; }
};

//==========================================
// is* wrappers for various data classes
//==========================================
inline bool RawType::isSimpleNumericType() const {
	return isa<SimpleNumericType> (this);
}
inline bool RawType::isFunctionNoProtoType() const {
	return isa<FunctionNoProtoType> (this);
}
inline bool RawType::isFunctionProtoType() const {
	return isa<FunctionProtoType> (this);
}
inline bool RawType::isFunctionType() const {
	return isa<FunctionProtoType> (this)
			|| isa<FunctionNoProtoType> (this);
}
inline bool RawType::isFunctionHandleType() const {
	return isa<FunctionHandleType> (this);
}
inline bool RawType::isFundamentalType() const {
	return isSimpleNumericType() || isFunctionHandleType();
}
inline bool RawType::isStructType() const {
	return isa<StructType> (this);
}
inline bool RawType::isCellType() const {
	return isa<CellType> (this);
}
inline bool RawType::isMapType() const {
	return isa<MapType> (this);
}
inline bool RawType::isClassdefType() const {
	return isa<ClassdefType> (this);
}
inline bool RawType::isHeteroContainerType() const {
	return isCellType() || isStructType() || isClassdefType() ||
			   isMapType();
}
inline bool RawType::isReferenceType() const {
  return isa<ReferenceType>(this);
}
inline bool RawType::isLValueReferenceType() const {
  return isa<LValueReferenceType>(this);
}
inline bool RawType::isRValueReferenceType() const {
  return isa<RValueReferenceType>(this);
}
inline bool RawType::isArrayType() const {
  return isa<ArrayType>(this);
}
inline bool RawType::isMatrixType() const {
  return isa<MatrixType>(this);
}
inline bool RawType::isNDArrayType() const {
  return isa<NDArrayType>(this);
}
inline bool RawType::isVectorType() const {
  return isa<VectorType>(this);
}

/// Insertion operator for diagnostics.  This allows sending Type's into a
/// diagnostic with <<.
inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB, Type T) {
	// DB.AddTaggedVal(reinterpret_cast<intptr_t>(T.getAsOpaquePtr()),
	//                Diagnostic::ak_qualtype);
	return DB;
}

/// Insertion operator for partial diagnostics.  This allows sending Type's
/// into a diagnostic with <<.
inline const PartialDiagnostic &operator<<(const PartialDiagnostic &PD, Type T) {
	// PD.AddTaggedVal(reinterpret_cast<intptr_t>(T.getAsOpaquePtr()),
	//                Diagnostic::ak_qualtype);
	return PD;
}

/// Member-template getAs<specific type>'.
template <typename T> const T *RawType::getAs() const {
  // If this is directly a T type, return it.
  if (const T *Ty = dyn_cast<T>(this))
    return Ty;

  return 0;
}

} // end namespace mlang

#endif /* MLANG_AST_TYPE_H_ */
