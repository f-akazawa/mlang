//===-- CGValue.h - LLVM CodeGen wrappers for llvm::Value* ------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// These classes implement wrappers around llvm::Value in order to
// fully represent the range of values for C L- and R- values.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CGVALUE_H_
#define MLANG_CODEGEN_CGVALUE_H_

#include "mlang/AST/ASTContext.h"
#include "mlang/AST/Type.h"

namespace llvm {
  class Constant;
  class Value;
}

namespace mlang {

namespace CodeGen {

/// RValue - This trivial value class is used to represent the result of an
/// expression that is evaluated.  It can be one of three things: either a
/// simple LLVM SSA value, a pair of SSA values for complex numbers, or the
/// address of an aggregate value in memory.
class RValue {
  enum Flavor { Scalar, Complex, Aggregate };

  // Stores first value and flavor.
  llvm::PointerIntPair<llvm::Value *, 2, Flavor> V1;
  // Stores second value and volatility.
  llvm::PointerIntPair<llvm::Value *, 1, bool> V2;

public:
  bool isScalar() const { return V1.getInt() == Scalar; }
  bool isComplex() const { return V1.getInt() == Complex; }
  bool isAggregate() const { return V1.getInt() == Aggregate; }

  bool isVolatileQualified() const { return V2.getInt(); }

  /// getScalarVal() - Return the Value* of this scalar value.
  llvm::Value *getScalarVal() const {
    assert(isScalar() && "Not a scalar!");
    return V1.getPointer();
  }

  /// getComplexVal - Return the real/imag components of this complex value.
  ///
  std::pair<llvm::Value *, llvm::Value *> getComplexVal() const {
    return std::make_pair(V1.getPointer(), V2.getPointer());
  }

  /// getAggregateAddr() - Return the Value* of the address of the aggregate.
  llvm::Value *getAggregateAddr() const {
    assert(isAggregate() && "Not an aggregate!");
    return V1.getPointer();
  }

  static RValue get(llvm::Value *V) {
    RValue ER;
    ER.V1.setPointer(V);
    ER.V1.setInt(Scalar);
    ER.V2.setInt(false);
    return ER;
  }
  static RValue getComplex(llvm::Value *V1, llvm::Value *V2) {
    RValue ER;
    ER.V1.setPointer(V1);
    ER.V2.setPointer(V2);
    ER.V1.setInt(Complex);
    ER.V2.setInt(false);
    return ER;
  }
  static RValue getComplex(const std::pair<llvm::Value *, llvm::Value *> &C) {
    return getComplex(C.first, C.second);
  }
  // FIXME: Aggregate rvalues need to retain information about whether they are
  // volatile or not.  Remove default to find all places that probably get this
  // wrong.
  static RValue getAggregate(llvm::Value *V, bool Volatile = false) {
    RValue ER;
    ER.V1.setPointer(V);
    ER.V1.setInt(Aggregate);
    ER.V2.setInt(Volatile);
    return ER;
  }
};


/// LValue - This represents an lvalue references.  Because C/C++ allow
/// bitfields, this is not a simple LLVM pointer, it may be a pointer plus a
/// bitrange.
class LValue {
  // FIXME: alignment?

  enum {
    Simple,       // This is a normal l-value, use getAddress().
    VectorElt,    // This is a vector element l-value (V[i]), use getVector*
    ExtVectorElt, // This is an extended vector subset, use getExtVectorComp
    PropertyRef   // This is an Objective-C property reference, use
                  // getPropertyRefExpr
  } LVType;

  llvm::Value *V;

  union {
    // Index into a vector subscript: V(i)
    llvm::Value *VectorIdx;

    // ExtVector element subset: V.xyx
    llvm::Constant *VectorElts;

    // Obj-C property reference expression
    // const ObjCPropertyRefExpr *PropertyRefExpr;
  };

  Type type;

  // 'const' is unused here
  TypeInfo Quals;

  /// The alignment to use when accessing this lvalue.
  unsigned short Alignment;

  // objective-c's ivar
  bool Ivar:1;
  
  // LValue is non-gc'able for any reason, including being a parameter or local
  // variable.
  bool NonGC: 1;

  // Lvalue is a thread local reference
  bool ThreadLocalRef : 1;

  Expr *BaseIvarExp;

  /// TBAAInfo - TBAA information to attach to dereferences of this LValue.
  llvm::MDNode *TBAAInfo;

private:
  void Initialize(Type ty, TypeInfo Quals, unsigned Alignment = 0,
                  llvm::MDNode *TBAAInfo = 0) {
	  this->type = ty;
    this->Quals = Quals;
    this->Alignment = Alignment;
    assert(this->Alignment == Alignment && "Alignment exceeds allowed max!");

    this->ThreadLocalRef = false;
    this->BaseIvarExp = 0;
    this->TBAAInfo = TBAAInfo;
  }

public:
  bool isSimple() const { return LVType == Simple; }
  bool isVectorElt() const { return LVType == VectorElt; }
  bool isExtVectorElt() const { return LVType == ExtVectorElt; }
  bool isPropertyRef() const { return LVType == PropertyRef; }

  bool isArrayType() const { return Quals.isArrayType(); }

  Type getType() const { return type; }

  unsigned getTypeInfo() const {
    return Quals.getFastTypeInfo() & ~TypeInfo::ArrayTy;
  }

  // FIXME yabin
  // for refactor purpose
  bool isVolatileQualified() {
	  return false;
  }

  bool isNonGC () const { return NonGC; }
  void setNonGC(bool Value) { NonGC = Value; }

  bool isThreadLocalRef() const { return ThreadLocalRef; }
  void setThreadLocalRef(bool Value) { ThreadLocalRef = Value;}

  Expr *getBaseIvarExp() const { return BaseIvarExp; }
  void setBaseIvarExp(Expr *V) { BaseIvarExp = V; }

  llvm::MDNode *getTBAAInfo() const { return TBAAInfo; }
  void setTBAAInfo(llvm::MDNode *N) { TBAAInfo = N; }

  const TypeInfo &getQuals() const { return Quals; }
  TypeInfo &getQuals() { return Quals; }

  unsigned getArraySize() const { return Quals.getArraySize(); }

  unsigned getAlignment() const { return Alignment; }

  // simple lvalue
  llvm::Value *getAddress() const { assert(isSimple()); return V; }

  // vector elt lvalue
  llvm::Value *getVectorAddr() const { assert(isVectorElt()); return V; }
  llvm::Value *getVectorIdx() const { assert(isVectorElt()); return VectorIdx; }

  // extended vector elements.
  llvm::Value *getExtVectorAddr() const { assert(isExtVectorElt()); return V; }
  llvm::Constant *getExtVectorElts() const {
    assert(isExtVectorElt());
    return VectorElts;
  }

  // property ref lvalue
  llvm::Value *getPropertyRefBaseAddr() const {
    assert(isPropertyRef());
    return V;
  }

  static LValue MakeAddr(llvm::Value *V, Type T, unsigned Alignment,
                         ASTContext &Context,
                         llvm::MDNode *TBAAInfo = 0) {
    TypeInfo Quals = T.getTypeInfo();

    LValue R;
    R.LVType = Simple;
    R.V = V;
    R.Initialize(T, Quals, Alignment, TBAAInfo);
    return R;
  }

  static LValue MakeVectorElt(llvm::Value *Vec, llvm::Value *Idx,
                              Type ty) {
    LValue R;
    R.LVType = VectorElt;
    R.V = Vec;
    R.VectorIdx = Idx;
    R.Initialize(ty, ty.getTypeInfo());
    return R;
  }

  static LValue MakeExtVectorElt(llvm::Value *Vec, llvm::Constant *Elts,
		                             Type ty) {
    LValue R;
    R.LVType = ExtVectorElt;
    R.V = Vec;
    R.VectorElts = Elts;
    R.Initialize(ty, ty.getTypeInfo());
    return R;
  }

};

/// An aggregate value slot.
class AggValueSlot {
  /// The address.
  llvm::Value *Addr;
  
  // Associated flags.
  bool VolatileFlag : 1;
  bool LifetimeFlag : 1;
  bool RequiresGCollection : 1;
  
  /// IsZeroed - This is set to true if the destination is known to be zero
  /// before the assignment into it.  This means that zero fields don't need to
  /// be set.
  bool IsZeroed : 1;

public:
  /// ignored - Returns an aggregate value slot indicating that the
  /// aggregate value is being ignored.
  static AggValueSlot ignored() {
    AggValueSlot AV;
    AV.Addr = 0;
    AV.VolatileFlag = AV.LifetimeFlag = AV.RequiresGCollection = AV.IsZeroed =0;
    return AV;
  }

  /// forAddr - Make a slot for an aggregate value.
  ///
  /// \param Volatile - true if the slot should be volatile-initialized
  /// \param LifetimeExternallyManaged - true if the slot's lifetime
  ///   is being externally managed; false if a destructor should be
  ///   registered for any temporaries evaluated into the slot
  /// \param RequiresGCollection - true if the slot is located
  ///   somewhere that ObjC GC calls should be emitted for
  static AggValueSlot forAddr(llvm::Value *Addr, bool Volatile,
                              bool LifetimeExternallyManaged,
                              bool RequiresGCollection = false,
                              bool IsZeroed = false) {
    AggValueSlot AV;
    AV.Addr = Addr;
    AV.VolatileFlag = Volatile;
    AV.LifetimeFlag = LifetimeExternallyManaged;
    AV.RequiresGCollection = RequiresGCollection;
    AV.IsZeroed = IsZeroed;
    return AV;
  }

  static AggValueSlot forLValue(LValue LV, bool LifetimeExternallyManaged,
                                bool RequiresGCollection = false) {
    return forAddr(LV.getAddress(), LV.isArrayType(),
                   LifetimeExternallyManaged, RequiresGCollection);
  }

  bool isLifetimeExternallyManaged() const {
    return LifetimeFlag;
  }
  void setLifetimeExternallyManaged(bool Managed = true) {
    LifetimeFlag = Managed;
  }

  bool isVolatile() const {
    return VolatileFlag;
  }

  bool requiresGCollection() const {
    return RequiresGCollection;
  }
  
  llvm::Value *getAddr() const {
    return Addr;
  }

  bool isIgnored() const {
    return Addr == 0;
  }

  RValue asRValue() const {
    return RValue::getAggregate(getAddr(), isVolatile());
  }
  
  void setZeroed(bool V = true) { IsZeroed = V; }
  bool isZeroed() const {
    return IsZeroed;
  }
};

}  // end namespace CodeGen
}  // end namespace mlang

#endif /* MLANG_CODEGEN_CGVALUE_H_ */
