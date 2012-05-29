//===--- ExprNumericLiteral.h - Numeric Literal for Mlang AST ----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the FloatingLiteral class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_NUMERIC_LITERAL_H_
#define MLANG_AST_EXPR_NUMERIC_LITERAL_H_

#include "mlang/AST/Expr.h"
#include "llvm/ADT/APFloat.h"

namespace mlang {
/// \brief Used by IntegerLiteral/FloatingLiteral to store the numeric without
/// leaking memory.
///
/// For large floats/integers, APFloat/APInt will allocate memory from the heap
/// to represent these numbers.  Unfortunately, when we use a BumpPtrAllocator
/// to allocate IntegerLiteral/FloatingLiteral nodes the memory associated with
/// the APFloat/APInt values will never get freed. APNumericStorage uses
/// ASTContext's allocator for memory allocation.
class APNumericStorage {
  unsigned BitWidth;
  union {
    uint64_t VAL;    ///< Used to store the <= 64 bits integer value.
    uint64_t *pVal;  ///< Used to store the >64 bits integer value.
  };

  bool hasAllocation() const { return llvm::APInt::getNumWords(BitWidth) > 1; }

  APNumericStorage(const APNumericStorage&); // do not implement
  APNumericStorage& operator=(const APNumericStorage&); // do not implement

protected:
  APNumericStorage() : BitWidth(0), VAL(0) { }

  llvm::APInt getIntValue() const {
    unsigned NumWords = llvm::APInt::getNumWords(BitWidth);
    if (NumWords > 1)
      return llvm::APInt(BitWidth, NumWords, pVal);
    else
      return llvm::APInt(BitWidth, VAL);
  }
  void setIntValue(ASTContext &C, const llvm::APInt &Val);
};

class APIntStorage : public APNumericStorage {
public:
  llvm::APInt getValue() const { return getIntValue(); }
  void setValue(ASTContext &C, const llvm::APInt &Val) { setIntValue(C, Val); }
};

class APFloatStorage : public APNumericStorage {
public:
  llvm::APFloat getValue() const { return llvm::APFloat(getIntValue()); }
  void setValue(ASTContext &C, const llvm::APFloat &Val) {
    setIntValue(C, Val.bitcastToAPInt());
  }
};

class IntegerLiteral : public Expr {
  APIntStorage Num;
  SourceLocation Loc;

  /// \brief Construct an empty integer literal.
  explicit IntegerLiteral(EmptyShell Empty)
    : Expr(IntegerLiteralClass, Empty) { }

public:
  // type should be IntTy, LongTy, LongLongTy, UnsignedIntTy, UnsignedLongTy,
  // or UnsignedLongLongTy
  IntegerLiteral(ASTContext &C, const llvm::APInt &V,
                 Type type, SourceLocation l)
    : Expr(IntegerLiteralClass, type, VK_RValue),
      Loc(l) {
    assert(type->isIntegerType() && "Illegal type in IntegerLiteral");
    setValue(C, V);
  }

  // type should be IntTy, LongTy, LongLongTy, UnsignedIntTy, UnsignedLongTy,
  // or UnsignedLongLongTy
  static IntegerLiteral *Create(ASTContext &C, const llvm::APInt &V,
                                Type type, SourceLocation l);
  static IntegerLiteral *Create(ASTContext &C, EmptyShell Empty);

  llvm::APInt getValue() const { return Num.getValue(); }

  /// \brief Retrieve the location of the literal.
  SourceLocation getLocation() const { return Loc; }

  virtual SourceRange getSourceRange() const {
	  return SourceRange(getLocation());
  }

  void setValue(ASTContext &C, const llvm::APInt &Val) { Num.setValue(C, Val); }
  void setLocation(SourceLocation Location) { Loc = Location; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == IntegerLiteralClass;
  }
  static bool classof(const IntegerLiteral *) { return true; }

  // Iterators
  child_range children() {
	  return child_range();
  }
};

class FloatingLiteral : public Expr {
  APFloatStorage Num;
  bool IsExact : 1;
  SourceLocation Loc;

  FloatingLiteral(ASTContext &C, const llvm::APFloat &V, bool isexact,
                  Type Type, SourceLocation L)
    : Expr(FloatingLiteralClass, Type, VK_RValue),
      IsExact(isexact), Loc(L) {
    setValue(C, V);
  }

  /// \brief Construct an empty floating-point literal.
  explicit FloatingLiteral(EmptyShell Empty)
    : Expr(FloatingLiteralClass, Empty), IsExact(false) { }

public:
  static FloatingLiteral *Create(ASTContext &C, const llvm::APFloat &V,
                                 bool isexact, Type Type, SourceLocation L);
  static FloatingLiteral *Create(ASTContext &C, EmptyShell Empty);

  llvm::APFloat getValue() const { return Num.getValue(); }
  void setValue(ASTContext &C, const llvm::APFloat &Val) {
    Num.setValue(C, Val);
  }

  bool isExact() const { return IsExact; }
  void setExact(bool E) { IsExact = E; }

  /// getValueAsApproximateDouble - This returns the value as an inaccurate
  /// double.  Note that this may cause loss of precision, but is useful for
  /// debugging dumps, etc.
  double getValueAsApproximateDouble() const;

  SourceLocation getLocation() const { return Loc; }
  void setLocation(SourceLocation L) { Loc = L; }

  virtual SourceRange getSourceRange() const { return SourceRange(getLocation()); }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == FloatingLiteralClass;
  }
  static bool classof(const FloatingLiteral *) { return true; }

  // Iterators
  child_range children() {
	  return child_range();
  }
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_NUMERIC_LITERAL_H_ */
