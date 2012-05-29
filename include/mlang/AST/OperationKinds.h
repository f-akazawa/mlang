//===--- OperationKinds.h - Operation enums  --------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file enumerates the different kinds of operations that can be
//  performed by various expressions.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_OPERATION_KINDS_H_
#define MLANG_AST_OPERATION_KINDS_H_

namespace mlang {
/// CastKind - the kind of cast this represents.
enum CastKind {
  /// CK_Unknown - Unknown cast kind.
  /// FIXME: The goal is to get rid of this and make all casts have a
  /// kind so that the AST client doesn't have to try to figure out what's
  /// going on.
  CK_Unknown,

  /// CK_BitCast - Used for reinterpret_cast.
  CK_BitCast,

  /// CK_LValueBitCast - Used for reinterpret_cast of expressions to
  /// a reference type.
  CK_LValueBitCast,

  /// CK_NoOp - Used for const_cast.
  CK_NoOp,

  /// CK_BaseToDerived - Base to derived class casts.
  CK_BaseToDerived,

  /// CK_DerivedToBase - Derived to base class casts.
  CK_DerivedToBase,

  /// CK_UncheckedDerivedToBase - Derived to base class casts that
  /// assume that the derived pointer is not null.
  CK_UncheckedDerivedToBase,

  /// CK_Dynamic - Dynamic cast.
  CK_Dynamic,

  /// CK_ToUnion - Cast to union (GCC extension).
  CK_ToUnion,

  /// CK_ArrayToPointerDecay - Array to pointer decay.
  CK_ArrayToPointerDecay,

  /// CK_FunctionToPointerDecay - Function to pointer decay.
  CK_FunctionToPointerDecay,

  /// CK_NullToMemberPointer - Null pointer to member pointer.
  CK_NullToMemberPointer,

  /// CK_BaseToDerivedMemberPointer - Member pointer in base class to
  /// member pointer in derived class.
  CK_BaseToDerivedMemberPointer,

  /// CK_DerivedToBaseMemberPointer - Member pointer in derived class to
  /// member pointer in base class.
  CK_DerivedToBaseMemberPointer,

  /// CK_UserDefinedConversion - Conversion using a user defined type
  /// conversion function.
  CK_UserDefinedConversion,

  /// CK_ConstructorConversion - Conversion by constructor
  CK_ConstructorConversion,

  /// CK_IntegralToPointer - Integral to pointer
  CK_IntegralToPointer,

  /// CK_PointerToIntegral - Pointer to integral
  CK_PointerToIntegral,

  /// CK_ToVoid - Cast to void.
  CK_ToVoid,

  /// CK_VectorSplat - Casting from an integer/floating type to an extended
  /// vector type with the same element type as the src type. Splats the
  /// src expression into the destination expression.
  CK_VectorSplat,

  /// CK_IntegralCast - Casting between integral types of different size.
  CK_IntegralCast,

  /// CK_IntegralToFloating - Integral to floating point.
  CK_IntegralToFloating,

  /// CK_FloatingToIntegral - Floating point to integral.
  CK_FloatingToIntegral,

  /// CK_FloatingCast - Casting between floating types of different size.
  CK_FloatingCast,

  /// CK_MemberPointerToBoolean - Member pointer to boolean
  CK_MemberPointerToBoolean,

  /// CK_AnyPointerToObjCPointerCast - Casting any pointer to objective-c
  /// pointer
  CK_AnyPointerToObjCPointerCast,

  /// CK_AnyPointerToBlockPointerCast - Casting any pointer to block
  /// pointer
  CK_AnyPointerToBlockPointerCast,

  /// \brief Converting between two Objective-C object types, which
  /// can occur when performing reference binding to an Objective-C
  /// object.
  CK_ObjCObjectLValueCast
};

enum UnaryOperatorKind {
  // Note that additions to this should also update the StmtVisitor class.
	UO_Transpose, UO_Quote, // complex conjugate transpose and matrix transpose
	UO_PostInc, UO_PostDec, // Postfix increment and decrement
  UO_PreInc, UO_PreDec,   // Prefix increment and decrement
  UO_Plus, UO_Minus,      // Unary arithmetic
  UO_LNot, /*UO_Not,*/
  UO_Handle,              // @ to get function handle
  UO_UnaryOpNums
};

enum BinaryOperatorKind {
  // Operators listed in order of precedence.
  // Note that additions to this should also update the StmtVisitor class.
	// Note that we *DONT* include colon operator here
  BO_Power, BO_MatrixPower,     // ".^" "^"
  BO_Mul = UO_UnaryOpNums, BO_RDiv, BO_LDiv,
  BO_MatrixMul, BO_MatrixRDiv, BO_MatrixLDiv,    // ".*" "./" ".\" "*" "/" "\"
  BO_Add, BO_Sub,               // Additive operators. "+" "-"
  BO_Shl, BO_Shr,               // Bitwise shift operators.
  BO_LT, BO_GT, BO_LE, BO_GE,   // Relational operators.
  BO_EQ, BO_NE,                 // Equality operators.
  BO_And,                       // Element-wise AND operator.
  BO_Or,                        // Element-wise OR operator.
  BO_LAnd,                      // Short-circuit logical AND operator.
  BO_LOr,                       // Short-circuit logical OR operator.
  BO_Assign,       // Assignment operators.
  BO_MatPowerAssign, BO_MatMulAssign,
  BO_MatRDivAssign, BO_MatLDivAssign,
  BO_PowerAssign,BO_MulAssign,
  BO_RDivAssign, BO_LDivAssign,
  BO_AddAssign, BO_SubAssign,
  BO_ShlAssign, BO_ShrAssign,
  BO_AndAssign, BO_OrAssign,
  BO_Colon,                     // Colon operator
  BO_Comma                      // Comma operator.
};
} // end namespace mlang

#endif /* MLANG_AST_OPERATION_KINDS_H_ */
