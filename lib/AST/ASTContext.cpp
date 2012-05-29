//===--- ASTContext.cpp - Context to hold long-lived AST nodes ------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements the ASTContext interface.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/ASTContext.h"
#include "mlang/AST/ASTMutationListener.h"
#include "mlang/AST/CharUnits.h"
#include "mlang/AST/DefnSub.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/ExternalASTSource.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/AST/Type.h"
#include "mlang/Basic/Builtins.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/TargetInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlang;

ASTMutationListener::~ASTMutationListener() { }

enum FloatingRank {
	FloatRank, DoubleRank, LongDoubleRank
};

ASTContext::ASTContext(const LangOptions& LOpts, SourceManager &SM,
		const TargetInfo &t, IdentifierTable &idents,
		Builtin::Context &builtins, unsigned size_reserve) :
  FunctionProtoTypes(this_()),
	SourceMgr(SM), LangOpts(LOpts), Target(t), Idents(idents),
	BuiltinInfo(builtins), DefinitionNames(this_()),
	ExternalSource(0), Listener(0), PrintingPolicy(LOpts), LastSDM(0),
	UniqueBlockByRefTypeID(0), UniqueBlockParmTypeID(0) {

	if (size_reserve > 0)
		Types.reserve(size_reserve);

	TUDefn = TranslationUnitDefn::Create(*this);
	InitBuiltinTypes();
}

ASTContext::~ASTContext() {
	// Release the DenseMaps associated with DefnContext objects.
	// FIXME: Is this the ideal solution?
	ReleaseDefnContextMaps();

	// Call all of the deallocation functions.
	for (unsigned I = 0, N = Deallocations.size(); I != N; ++I)
		Deallocations[I].first(Deallocations[I].second);

	for (llvm::DenseMap<const Defn*, const ASTRecordLayout*>::iterator
			I = ASTRecordLayouts.begin(), E = ASTRecordLayouts.end(); I != E;) {
		// Increment in loop to prevent using deallocated memory.
		if (ASTRecordLayout *R = const_cast<ASTRecordLayout*>((I++)->second))
			R->Destroy(*this);
	}
}

void ASTContext::AddDeallocation(void(*Callback)(void*), void *Data) {
	Deallocations.push_back(std::make_pair(Callback, Data));
}

void ASTContext::setExternalSource(llvm::OwningPtr<ExternalASTSource> &Source) {
	ExternalSource.reset(Source.take());
}

void ASTContext::PrintStats() const {
	fprintf(stderr, "*** AST Context Stats:\n");
	fprintf(stderr, "  %d types total.\n", (int) Types.size());

	unsigned counts[] = {
#define TYPE(Name, Parent) 0,
#define ABSTRACT_TYPE(Name, Parent)
#include "mlang/AST/TypeNodes.def"
			0 // Extra
			};

	for (unsigned i = 0, e = Types.size(); i != e; ++i) {
		RawType *T = Types[i];
		counts[(unsigned) T->getTypeClass()]++;
	}

	unsigned Idx = 0;
	unsigned TotalBytes = 0;
#define TYPE(Name, Parent)                                              \
  if (counts[Idx])                                                      \
    fprintf(stderr, "    %d %s types\n", (int)counts[Idx], #Name);      \
  TotalBytes += counts[Idx] * sizeof(Name##Type);                       \
  ++Idx;
#define ABSTRACT_TYPE(Name, Parent)
#include "mlang/AST/TypeNodes.def"

	fprintf(stderr, "Total bytes = %d\n", int(TotalBytes));

	if (ExternalSource.get()) {
		fprintf(stderr, "\n");
		ExternalSource->PrintStats();
	}

	BumpAlloc.PrintStats();
}

void ASTContext::InitSimpleNumericType(Type &R,
		                                   SimpleNumericType::NumericKind K) {
	SimpleNumericType *Ty = new (*this, TypeAlignment) SimpleNumericType(K);
	R = Type(Ty, 0);
	Types.push_back(Ty);
}

void ASTContext::InitBuiltinTypes() {
	// FIXME yabin  we do *NOT* support void type currently
#if 0
	assert(VoidTy.isNull() && "Context reinitialized?");
	InitBuiltinType(VoidTy, BuiltinType::Void);
	// void * type
	VoidPtrTy = getPointerType(VoidTy);
#endif

	// Integer types.
	InitSimpleNumericType(Int8Ty,   SimpleNumericType::Int8);
	InitSimpleNumericType(Int16Ty,  SimpleNumericType::Int16);
	InitSimpleNumericType(Int32Ty,  SimpleNumericType::Int32);
	InitSimpleNumericType(Int64Ty,  SimpleNumericType::Int64);
	InitSimpleNumericType(UInt8Ty,  SimpleNumericType::UInt8);
	InitSimpleNumericType(UInt16Ty, SimpleNumericType::UInt16);
	InitSimpleNumericType(UInt32Ty, SimpleNumericType::UInt32);
	InitSimpleNumericType(UInt64Ty, SimpleNumericType::UInt64);

	// FloatingPoint types.
	InitSimpleNumericType(SingleTy, SimpleNumericType::Single);
	InitSimpleNumericType(DoubleTy, SimpleNumericType::Double);

	FloatComplexTy  = SingleTy.withFastTypeInfo(0x2); // add complex attr to typeinfo
	DoubleComplexTy = DoubleTy.withFastTypeInfo(0x2);

	InitSimpleNumericType(CharTy,  SimpleNumericType::Char);

	// Logical type.
	InitSimpleNumericType(LogicalTy, SimpleNumericType::Logical);
}

Diagnostic &ASTContext::getDiagnostics() const {
	return SourceMgr.getDiagnostics();
}

//===----------------------------------------------------------------------===//
//                         Type Sizing and Analysis
//===----------------------------------------------------------------------===//

/// getFloatTypeSemantics - Return the APFloat 'semantics' for the specified
/// scalar floating point type.
const llvm::fltSemantics &ASTContext::getFloatTypeSemantics(Type T) const {
	const SimpleNumericType *FP = T->getAs<SimpleNumericType>();
	assert(FP->isFloatingPointType() && "Not a floating point type!");
	switch (FP->getKind()) {
		default: assert(0 && "Not a floating point type!");
		case SimpleNumericType::Single: return Target.getFloatFormat();
		case SimpleNumericType::Double: return Target.getDoubleFormat();
		// case FloatingPointType::LongDouble: return Target.getLongDoubleFormat();
	}
}

/// getDefnAlign - Return a conservative estimate of the alignment of the
/// specified defn.  Note that bitfields do not have a valid alignment, so
/// this method will assert on them.
/// If @p RefAsPointee, references are treated like their underlying type
/// (for alignof), else they're treated like pointers (for CodeGen).
CharUnits ASTContext::getDefnAlign(const Defn *D, bool RefAsPointee) {
  unsigned Align = Target.getCharWidth();

  // FIXME : we should put her the rules we calculate alignment.
  // Align = ...

  return CharUnits::fromQuantity(Align / Target.getCharWidth());
}

std::pair<CharUnits, CharUnits> ASTContext::getTypeInfoInChars(const RawType *T) {
	std::pair<uint64_t, unsigned> Info = getTypeSizeInfo(T);
	return std::make_pair(CharUnits::fromQuantity(Info.first / getCharWidth()),
			CharUnits::fromQuantity(Info.second / getCharWidth()));
}

std::pair<CharUnits, CharUnits> ASTContext::getTypeInfoInChars(Type T) {
	return getTypeInfoInChars(T.getRawTypePtr());
}

/// getTypeSizeInfo - Return the size of the specified type, in bits.  This method
/// does not work on incomplete types.
std::pair<uint64_t, unsigned> ASTContext::getTypeSizeInfo(const RawType *T) const {
	uint64_t Width = 0;
	unsigned Align = 8;
	switch (T->getTypeClass()) {
	case RawType::SimpleNumeric:
		switch(cast<SimpleNumericType>(T)->getKind()) {
	  case SimpleNumericType::Logical:
			Width = 8;
			Align = 8;
			break;
		case SimpleNumericType::Char:
			Width = 8;
			Align = 8;
			break;
		case SimpleNumericType::Int8:
		case SimpleNumericType::UInt8:
			Width = 8;
			Align = 8;
			break;
		case SimpleNumericType::Int16:
		case SimpleNumericType::UInt16:
			Width = 16;
			Align = 16;
			break;
		case SimpleNumericType::Int32:
		case SimpleNumericType::UInt32:
			Width = 32;
			Align = 32;
			break;
		case SimpleNumericType::Int64:
		case SimpleNumericType::UInt64:
			Width = 64;
			Align = 64;
			break;
		case SimpleNumericType::Single :
			Width = Target.getFloatWidth();
			Align = Target.getFloatAlign();
			break;
		case SimpleNumericType::Double :
			Width = Target.getDoubleWidth();
			Align = Target.getDoubleAlign();
			break;
	  }
	  break;
	case RawType::FunctionHandle:
		Width = 32;
		Align = 32;
		break;
	case RawType::Struct:
		Width = 32; // FIXME
		Align = 32; // FIXME
		break;
	case RawType::Cell:
		Width = 32; // FIXME
		Align = 32; // FIXME
		break;
	case RawType::Map:
		Width = 32; // FIXME
		Align = 32; // FIXME
		break;
	case RawType::Classdef: {
		const ClassdefType *CT = cast<ClassdefType>(T);
		const ASTRecordLayout &Layout = getASTRecordLayout(CT->getDefn());
		Width = toBits(Layout.getSize());
		Align = toBits(Layout.getAlignment());
		break;
	}
	default:
		break;
	}

	assert(Align && (Align & (Align-1)) == 0 && "Alignment must be power of 2");
	return std::make_pair(Width, Align);
}

/// toCharUnitsFromBits - Convert a size in bits to a size in characters.
CharUnits ASTContext::toCharUnitsFromBits(int64_t BitSize) const {
  return CharUnits::fromQuantity(BitSize / getCharWidth());
}

/// toBits - Convert a size in characters to a size in characters.
int64_t ASTContext::toBits(CharUnits CharSize) const {
  return CharSize.getQuantity() * getCharWidth();
}

/// getTypeSizeInChars - Return the size of the specified type, in characters.
/// This method does not work on incomplete types.
CharUnits ASTContext::getTypeSizeInChars(Type T) {
	return CharUnits::fromQuantity(getTypeSize(T) / getCharWidth());
}
CharUnits ASTContext::getTypeSizeInChars(const RawType *T) {
	return CharUnits::fromQuantity(getTypeSize(T) / getCharWidth());
}

/// getTypeAlignInChars - Return the ABI-specified alignment of a type, in 
/// characters. This method does not work on incomplete types.
CharUnits ASTContext::getTypeAlignInChars(Type T) {
	return CharUnits::fromQuantity(getTypeAlign(T) / getCharWidth());
}
CharUnits ASTContext::getTypeAlignInChars(const RawType *T) {
	return CharUnits::fromQuantity(getTypeAlign(T) / getCharWidth());
}

/// getPreferredTypeAlign - Return the "preferred" alignment of the specified
/// type for the current target in bits.  This can be different than the ABI
/// alignment in cases where it is beneficial for performance to overalign
/// a data type.
unsigned ASTContext::getPreferredTypeAlign(const RawType *T) {
	unsigned ABIAlign = getTypeAlign(T);
#if 0
	// Double and long long should be naturally aligned if possible.
	if (const ComplexType* CT = T->getAs<ComplexType>())
	T = CT->getElementType().getRawTypePtr();
	if (T->isSpecificBuiltinType(BuiltinType::Double) ||
			T->isSpecificBuiltinType(BuiltinType::LongLong))
	return std::max(ABIAlign, (unsigned)getTypeSize(T));
#endif
	return ABIAlign;
}

//===----------------------------------------------------------------------===//
//                   Type creation/memorization methods
//===----------------------------------------------------------------------===//
Type ASTContext::getExtTypeInfoType(const RawType *Base, TypeInfo quals) {
	unsigned fastQuals = quals.getFastTypeInfo();
	quals.removeFastTypeInfo();

	// Check if we've already instantiated this type.
	llvm::FoldingSetNodeID ID;
	ExtTypeInfo::Profile(ID, Base, quals);
	void *insertPos = 0;
	if (ExtTypeInfo *eq = ExtTypeInfoNodes.FindNodeOrInsertPos(ID, insertPos)) {
		assert(eq->getTypeInfo() == quals);
		return Type(eq, fastQuals);
	}

	ExtTypeInfo *eq = new (*this, TypeAlignment) ExtTypeInfo(Base, quals);
	ExtTypeInfoNodes.InsertNode(eq, insertPos);
	return Type(eq, fastQuals);
}

Type ASTContext::getTypeDefnTypeSlow(const TypeDefn *Defn) {
	assert(Defn && "Passed null for Decl param");
	assert(!Defn->TypeForDefn && "TypeForDefn present in slow case");

//	if (const TypeDefn *Record = dyn_cast<TypeDefn>(Defn)) {
//		return getRecordType(Record);
//	} else
//		llvm_unreachable("TypeDefn without a type?");

	return Type(Defn->TypeForDefn, 0);
}

Type ASTContext::getTypeDefnType(const TypeDefn *Defn,
		const TypeDefn *PrevDefn) {
	assert(Defn && "Passed null for Defn param");
	if (Defn->TypeForDefn) return Type(Defn->TypeForDefn, 0);

	if (PrevDefn) {
		assert(PrevDefn->TypeForDefn && "previous Defn has no TypeForDefn");
		Defn->TypeForDefn = PrevDefn->TypeForDefn;
		return Type(PrevDefn->TypeForDefn, 0);
	}
	return getTypeDefnTypeSlow(Defn);
}

/// getFromTargetType - Given one of the integer types provided by
/// TargetInfo, produce the corresponding type. The unsigned @p Type
/// is actually a value of type @c TargetInfo::IntType.
Type ASTContext::getFromTargetType(unsigned nType) const {
	switch (nType) {
	case TargetInfo::NoInt:  return Type();
	case TargetInfo::Int8:   return Int8Ty;
	case TargetInfo::UInt8:  return UInt8Ty;
	case TargetInfo::Int16:  return Int16Ty;
	case TargetInfo::UInt16: return UInt16Ty;
	case TargetInfo::Int32:  return Int32Ty;
	case TargetInfo::UInt32: return UInt32Ty;
	case TargetInfo::Int64:  return UInt64Ty;
	case TargetInfo::UInt64: return UInt64Ty;
	}

	assert(false && "Unhandled TargetInfo::IntType value");
	return Type();
}

/// getLValueReferenceType - Return the uniqued reference to the type for an
/// lvalue reference to the specified type.
Type ASTContext::getLValueReferenceType(Type T, bool SpelledAsLValue) const{
	  // Unique pointers, to guarantee there is only one pointer of a particular
	  // structure.
	  llvm::FoldingSetNodeID ID;
	  ReferenceType::Profile(ID, T, SpelledAsLValue);

	  void *InsertPos = 0;
	  if (LValueReferenceType *RT =
	        LValueReferenceTypes.FindNodeOrInsertPos(ID, InsertPos))
	    return Type(RT, 0);

	  const ReferenceType *InnerRef = T->getAs<ReferenceType>();

	  // If the referencee type isn't canonical, this won't be a canonical type
	  // either, so fill in the canonical type field.
	  if (!SpelledAsLValue || InnerRef) {
	    Type PointeeType = (InnerRef ? InnerRef->getPointeeType() : T);

	    // Get the new insert position for the node we care about.
	    LValueReferenceType *NewIP =
	      LValueReferenceTypes.FindNodeOrInsertPos(ID, InsertPos);
	    assert(NewIP == 0 && "Shouldn't be in the map!"); (void)NewIP;
	  }

	  LValueReferenceType *New
	    = new (*this, TypeAlignment) LValueReferenceType(T, SpelledAsLValue);
	  Types.push_back(New);
	  LValueReferenceTypes.InsertNode(New, InsertPos);

	  return Type(New, 0);
}

/// getRValueReferenceType - Return the uniqued reference to the type for an
/// rvalue reference to the specified type.
Type ASTContext::getRValueReferenceType(Type T) const {
	  // Unique pointers, to guarantee there is only one pointer of a particular
	  // structure.
	  llvm::FoldingSetNodeID ID;
	  ReferenceType::Profile(ID, T, false);

	  void *InsertPos = 0;
	  if (RValueReferenceType *RT =
	        RValueReferenceTypes.FindNodeOrInsertPos(ID, InsertPos))
	    return Type(RT, 0);

	  const ReferenceType *InnerRef = T->getAs<ReferenceType>();

	  // If the referencee type isn't canonical, this won't be a canonical type
	  // either, so fill in the canonical type field.
	  Type Canonical;
	  if (InnerRef) {
	    Type PointeeType = (InnerRef ? InnerRef->getPointeeType() : T);
	    Canonical = getRValueReferenceType(PointeeType);

	    // Get the new insert position for the node we care about.
	    RValueReferenceType *NewIP =
	      RValueReferenceTypes.FindNodeOrInsertPos(ID, InsertPos);
	    assert(NewIP == 0 && "Shouldn't be in the map!"); (void)NewIP;
	  }

	  RValueReferenceType *New
	    = new (*this, TypeAlignment) RValueReferenceType(T);
	  Types.push_back(New);
	  RValueReferenceTypes.InsertNode(New, InsertPos);
	  return Type(New, 0);
}

Type ASTContext::getMatrixType(Type T, unsigned  Rows, unsigned  Cols,
		MatrixType::MatrixKind K) const {
	assert(T->isFundamentalType());

	// Check if we've already instantiated a vector of this type.
	llvm::FoldingSetNodeID ID;
	MatrixType::Profile(ID, T, Rows, Cols, K);

	void *InsertPos = 0;
	if (MatrixType *MTP = MatrixTypes.FindNodeOrInsertPos(ID, InsertPos))
		return Type(MTP, 0);

	MatrixType *New = new (*this, TypeAlignment)
			MatrixType(T, Rows, Cols, K);
	MatrixTypes.InsertNode(New, InsertPos);
	Types.push_back(New);
	return Type(New, 0);
}

Type ASTContext::getNDArrayType(Type T, DimVector & DV) const {
	assert(T->isFundamentalType());

	// Check if we've already instantiated a vector of this type.
	llvm::FoldingSetNodeID ID;
	NDArrayType::Profile(ID, T, DV);

	void *InsertPos = 0;
	if (NDArrayType *MTP = NDArrayTypes.FindNodeOrInsertPos(ID, InsertPos))
		return Type(MTP, 0);

	NDArrayType *New = new (*this, TypeAlignment)
			NDArrayType(T, DV);

	NDArrayTypes.InsertNode(New, InsertPos);
	Types.push_back(New);
	return Type(New, 0);
}

bool ASTContext::areCompatibleVectorTypes(Type FirstVec, Type SecondVec) {
	return false;
}

Type ASTContext::getVectorType(Type T, unsigned NumElts,
		VectorType::VectorKind VecKind, bool isRowVec) const {
	assert(T->isFundamentalType());

	// Check if we've already instantiated a vector of this type.
	llvm::FoldingSetNodeID ID;
	VectorType::Profile(ID, T, NumElts, RawType::Vector, isRowVec, VecKind);

	void *InsertPos = 0;
	if (VectorType *VTP = VectorTypes.FindNodeOrInsertPos(ID, InsertPos))
		return Type(VTP, 0);

	VectorType *New = new (*this, (unsigned)TypeAlignment)
			VectorType(T, NumElts, VecKind, isRowVec);
	VectorTypes.InsertNode(New, InsertPos);
	Types.push_back(New);
	return Type(New, 0);
}

Type ASTContext::getFunctionNoProtoType(Type ResultTy,
		FunctionType::ExtInfo Info) {
	const CallingConv DefaultCC = Info.getCC();
	const CallingConv CallConv = (DefaultCC == CC_Default) ?
	                              CC_X86StdCall : DefaultCC;

	// Unique functions, to guarantee there is only one function of a particular
	// structure.
	llvm::FoldingSetNodeID ID;
	FunctionNoProtoType::Profile(ID, ResultTy, Info);

	void *InsertPos = 0;
	if (FunctionNoProtoType *FT =
			FunctionNoProtoTypes.FindNodeOrInsertPos(ID, InsertPos))
		return Type(FT, 0);

	FunctionProtoType::ExtInfo newInfo = Info.withCallingConv(CallConv);
	FunctionNoProtoType *New;
	New = new (*this, TypeAlignment) FunctionNoProtoType(
						   ResultTy, newInfo);

	Types.push_back(New);
	FunctionNoProtoTypes.InsertNode(New, InsertPos);
	return Type(New, 0);
}

Type ASTContext::getFunctionType(Type ResultTy,
                                 const Type *ArgArray, unsigned NumArgs,
                                 const FunctionProtoType::ExtProtoInfo &EPI) {
	// Unique functions, to guarantee there is only one function of a particular
	// structure.
	llvm::FoldingSetNodeID ID;
	FunctionProtoType::Profile(ID, ResultTy, ArgArray, NumArgs, EPI, *this);

	void *InsertPos = 0;
	if (FunctionProtoType *FTP =
	        FunctionProtoTypes.FindNodeOrInsertPos(ID, InsertPos))
		return Type(FTP, 0);

	const CallingConv DefaultCC = EPI.ExtInfo.getCC();
	const CallingConv CallConv =
			(DefaultCC == CC_Default) ? CC_X86StdCall
					: DefaultCC;

	// FunctionProtoType objects are allocated with extra bytes after them
	// for two variable size arrays (for parameter and exception types) at the
	// end of them. Instead of the exception types, there could be a noexcept
	// expression and a context pointer.
	size_t Size = sizeof(FunctionProtoType) + NumArgs * sizeof(Type);
//	if (EPI.ExceptionSpecType == EST_Dynamic)
//		Size += EPI.NumExceptions * sizeof(Type);
//	else if (EPI.ExceptionSpecType == EST_ComputedNoexcept) {
//		Size += sizeof(Expr*);
//	}
	FunctionProtoType *FTP = (FunctionProtoType*) Allocate(Size, TypeAlignment);
	FunctionProtoType::ExtProtoInfo newEPI = EPI;
	newEPI.ExtInfo = EPI.ExtInfo.withCallingConv(CallConv);
	new (FTP) FunctionProtoType(
				ResultTy,	ArgArray, NumArgs, newEPI);
	Types.push_back(FTP);
	FunctionProtoTypes.InsertNode(FTP, InsertPos);
	return Type(FTP, 0);
}

//===----------------------------------------------------------------------===//
//                        Type Operators
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
//                        Type Predicates.
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
//                        Type Compatibility Testing
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
//                        Integer Predicates
//===----------------------------------------------------------------------===//

unsigned ASTContext::getIntWidth(Type T) {
  if (T->isLogicalType())
    return 1;
  // For builtin types, just use the standard type sizing method
  return (unsigned)getTypeSize(T);
}

//===----------------------------------------------------------------------===//
//                        Builtin Type Computation
//===----------------------------------------------------------------------===//
/// DecodeTypeFromStr - This decodes one type descriptor from Str, advancing the
/// pointer over the consumed characters.  This returns the resultant type.  If
/// AllowTypeModifiers is false then modifier like * are not parsed, just basic
/// types.  This allows "v2i*" to be parsed as a pointer to a v2i instead of
/// a vector of "i*".
///
/// RequiresICE is filled in on return to indicate whether the value is required
/// to be an Integer Constant Expression.
static Type DecodeTypeFromStr(const char *&Str,
		const ASTContext &Context, ASTContext::GetBuiltinTypeError &Error,
		bool &RequiresICE, bool AllowTypeModifiers) {
  // Modifiers.
  int HowLong = 0;
  bool Signed = false, Unsigned = false;
  RequiresICE = false;

  // Read the prefixed modifiers first.
  bool Done = false;
  while (!Done) {
    switch (*Str++) {
    default: Done = true; --Str; break;
    case 'I':
      RequiresICE = true;
      break;
    case 'S':
      assert(!Unsigned && "Can't use both 'S' and 'U' modifiers!");
      assert(!Signed && "Can't use 'S' modifier multiple times!");
      Signed = true;
      break;
    case 'U':
      assert(!Signed && "Can't use both 'S' and 'U' modifiers!");
      assert(!Unsigned && "Can't use 'S' modifier multiple times!");
      Unsigned = true;
      break;
    case 'L':
      assert(HowLong < 4 && "Can't have LLLL modifier");
      ++HowLong;
      break;
    }
  }

  Type Ty;

  // Read the base type.
  switch (*Str++) {
  default: assert(0 && "Unknown builtin type letter!");
  case 'v':
    assert(HowLong == 0 && !Signed && !Unsigned &&
           "Bad modifiers used with 'v'!");
    // Ty = Context.VoidTy;
    break;
  case 'f':
    assert(HowLong == 0 && !Signed && !Unsigned &&
           "Bad modifiers used with 'f'!");
    Ty = Context.SingleTy;
    break;
  case 'd':
    assert(HowLong < 2 && !Signed && !Unsigned &&
           "Bad modifiers used with 'd'!");
    Ty = Context.DoubleTy;
    break;
  case 's':
    assert(HowLong == 0 && "Bad modifiers used with 's'!");
//    if (Unsigned)
//      Ty = Context.UnsignedShortTy;
//    else
//      Ty = Context.ShortTy;
    break;
  case 'i':
    if (HowLong == 3)
      Ty = Unsigned ? Context.UInt64Ty : Context.Int64Ty;
    else if (HowLong == 2)
      Ty = Unsigned ? Context.UInt32Ty : Context.Int32Ty;
    else if (HowLong == 1)
      Ty = Unsigned ? Context.UInt16Ty : Context.Int16Ty;
    else
      Ty = Unsigned ? Context.UInt8Ty : Context.Int8Ty;
    break;
  case 'c':
    assert(HowLong == 0 && "Bad modifiers used with 'c'!");
    Ty = Context.CharTy;
    break;
  case 'b': // boolean
    assert(HowLong == 0 && !Signed && !Unsigned && "Bad modifiers for 'b'!");
    Ty = Context.LogicalTy;
    break;
  case 'z':  // size_t.
    assert(HowLong == 0 && !Signed && !Unsigned && "Bad modifiers for 'z'!");
    // Ty = Context.getSizeType();
    break;
  case 'F':
    //Ty = Context.getCFConstantStringType();
    break;
  case 'G':
    //Ty = Context.getObjCIdType();
    break;
  case 'H':
    //Ty = Context.getObjCSelType();
    break;
  case 'a':
    //Ty = Context.getBuiltinVaListType();
    //assert(!Ty.isNull() && "builtin va list type not initialized!");
    break;
  case 'A':
    // This is a "reference" to a va_list; however, what exactly
    // this means depends on how va_list is defined. There are two
    // different kinds of va_list: ones passed by value, and ones
    // passed by reference.  An example of a by-value va_list is
    // x86, where va_list is a char*. An example of by-ref va_list
    // is x86-64, where va_list is a __va_list_tag[1]. For x86,
    // we want this argument to be a char*&; for x86-64, we want
    // it to be a __va_list_tag*.
//    Ty = Context.getBuiltinVaListType();
//    assert(!Ty.isNull() && "builtin va list type not initialized!");
//    if (Ty->isArrayType())
//      Ty = Context.getArrayDecayedType(Ty);
//    else
//      Ty = Context.getLValueReferenceType(Ty);
    break;
  case 'V': {
    char *End;
    unsigned NumElements = strtoul(Str, &End, 10);
    assert(End != Str && "Missing vector size");
    Str = End;

    Type ElementType = DecodeTypeFromStr(Str, Context, Error,
                                             RequiresICE, false);
    assert(!RequiresICE && "Can't require vector ICE");

    // TODO: No way to make AltiVec vectors in builtins yet.
    Ty = Context.getVectorType(ElementType, NumElements,
    		VectorType::GenericVector, true);
    break;
  }
  case 'X': {
    Type ElementType = DecodeTypeFromStr(Str, Context, Error, RequiresICE,
                                             false);
    assert(!RequiresICE && "Can't require complex ICE");
//    Ty = Context.getComplexType(ElementType);
    break;
  }
  case 'P':
 //   Ty = Context.getFILEType();
//    if (Ty.isNull()) {
//      Error = ASTContext::GE_Missing_stdio;
      return Type();
//    }
//    break;
  case 'J':
//    if (Signed)
//      Ty = Context.getsigjmp_bufType();
//    else
//      Ty = Context.getjmp_bufType();

    if (Ty.isNull()) {
      Error = ASTContext::GE_Missing_setjmp;
      return Type();
    }
    break;
  }

  // If there are modifiers and if we're allowed to parse them, go for it.
  Done = !AllowTypeModifiers;
  while (!Done) {
    switch (/*char c =*/ *Str++) {
    default: Done = true; --Str; break;
    case '*':
    case '&': {
      // Both pointers and references can have their pointee types
      // qualified with an address space.
      char *End;
      unsigned AddrSpace = strtoul(Str, &End, 10);
      if (End != Str && AddrSpace != 0) {
//        Ty = Context.getAddrSpaceQualType(Ty, AddrSpace);
        Str = End;
      }
//      if (c == '*')
//        Ty = Context.getPointerType(Ty);
//      else
//        Ty = Context.getLValueReferenceType(Ty);
//      break;
    }
    // FIXME: There's no way to have a built-in with an rvalue ref arg.
    case 'C':
//      Ty = Ty.withConst();
      break;
    case 'D':
//      Ty = Context.getVolatileType(Ty);
      break;
    }
  }

  assert((!RequiresICE || Ty->isIntegerType()) &&
         "Integer constant 'I' type must be an integer");

  return Ty;
}

static Type DecodeRetsTypeFromStr(const char *Str,
		const ASTContext &Context, ASTContext::GetBuiltinTypeError &Error,
		bool &RequiresICE, bool AllowTypeModifiers) {
	llvm::SmallVector<Type,4> Outs;
	while (Str[0] && Str[0] != '.') {
		Type Ty =	DecodeTypeFromStr(Str, Context, Error, RequiresICE,	true);
		if (Error != ASTContext::GE_None)
			return Type();

			// Do array -> pointer decay.  The builtin should use the decayed type.
	//		if (Ty->isArrayType())
	//			Ty = getArrayDecayedType(Ty);

		Outs.push_back(Ty);
	}

	// if it is a multiple outputs function
	if(Outs.size() > 1) {
		//FIXME we should handle the multiple outputs case
		return Type();
	} else {
		return *Outs.begin();
	}
}

Type ASTContext::GetBuiltinFunctionType(unsigned Id, GetBuiltinTypeError &Error,
		unsigned *IntegerConstantArgs) {
	const char *TypeStr = BuiltinInfo.GetRetsTypeString(Id);
	bool RequiresICE = false;
	Error = GE_None;
	Type ResType =
			DecodeRetsTypeFromStr(TypeStr, *this, Error, RequiresICE, true);
	if (Error != GE_None)
		return Type();

	assert(!RequiresICE && "Result of intrinsic cannot be required to be an ICE");

	TypeStr = BuiltinInfo.GetArgsTypeString(Id);
	llvm::SmallVector<Type, 8> ArgTypes;
	while (TypeStr[0] && TypeStr[0] != '.') {
		Type Ty =	DecodeTypeFromStr(TypeStr, *this, Error, RequiresICE,	true);
		if (Error != GE_None)
			return Type();

		// If this argument is required to be an IntegerConstantExpression and the
		// caller cares, fill in the bitmask we return.
		if (RequiresICE && IntegerConstantArgs)
			*IntegerConstantArgs |= 1 << ArgTypes.size();

		// Do array -> pointer decay.  The builtin should use the decayed type.
//		if (Ty->isArrayType())
//			Ty = getArrayDecayedType(Ty);

		ArgTypes.push_back(Ty);
	}

	assert((TypeStr[0] != '.' || TypeStr[1] == 0) &&
			"'.' should only occur at end of builtin type list!");

	FunctionType::ExtInfo EI;
	if (BuiltinInfo.isNoReturn(Id))
		EI = EI.withNoReturn(true);

	bool Variadic = (TypeStr[0] == '.');

	// We really shouldn't be making a no-proto type here, especially in C++.
	if (ArgTypes.empty() && Variadic) {
		return getFunctionNoProtoType(ResType, EI);
	}

	FunctionProtoType::ExtProtoInfo EPI;
	EPI.ExtInfo = EI;
	EPI.Variadic = Variadic;

	return getFunctionType(ResType, ArgTypes.data(), ArgTypes.size(), EPI);
}

/// getPointerType - Return the uniqued reference to the type for a pointer to
/// the specified type.
Type ASTContext::getFunctionHandleType(Type T) const {
  // Unique pointers, to guarantee there is only one pointer of a particular
  // structure.
  llvm::FoldingSetNodeID ID;
  FunctionHandleType::Profile(ID, T);

  void *InsertPos = 0;
  if (FunctionHandleType *PT = FunctionHandleTypes.FindNodeOrInsertPos(
		  ID, InsertPos))
    return Type(PT, 0);

  FunctionHandleType *New = new (*this, TypeAlignment) FunctionHandleType(T);
  Types.push_back(New);
  FunctionHandleTypes.InsertNode(New, InsertPos);
  return Type(New, 0);
}

//===--------------------------------------------------------------------===//
//                    Integer Values
//===--------------------------------------------------------------------===//
GVALinkage ASTContext::GetGVALinkageForFunction(const FunctionDefn *FD) {
  return GVA_StrongExternal;
}

bool ASTContext::DefnMustBeEmitted(const Defn *D) {
	if(const ScriptDefn *SD = cast<ScriptDefn>(D))
		return true;

	return false;
}
