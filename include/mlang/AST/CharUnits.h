//===--- CharUnits.h - Character units for sizes and offsets ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the CharUnits class
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CHARUNITS_H_
#define MLANG_AST_CHARUNITS_H_

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/MathExtras.h"

namespace mlang {
  
  /// CharUnits - This is an opaque type for sizes expressed in character units.
  /// Instances of this type represent a quantity as a multiple of the size 
  /// of the standard C type, char, on the target architecture. As an opaque
  /// type, CharUnits protects you from accidentally combining operations on
  /// quantities in bit units and character units. 
  ///
  /// It should be noted that characters and bytes are distinct concepts. Bytes
  /// refer to addressable units of data storage on the target machine, and
  /// characters are members of a set of elements used for the organization,
  /// control, or representation of data. According to C99, bytes are allowed
  /// to exceed characters in size, although currently, mlang only supports
  /// architectures where the two are the same size.
  /// 
  /// For portability, never assume that a target character is 8 bits wide. Use 
  /// CharUnit values whereever you calculate sizes, offsets, or alignments
  /// in character units.
  class CharUnits {
    public:
      typedef int64_t QuantityType;

    private:
      QuantityType Quantity;

      explicit CharUnits(QuantityType C) : Quantity(C) {}

    public:

      /// CharUnits - A default constructor.
      CharUnits() : Quantity(0) {}

      /// Zero - Construct a CharUnits quantity of zero.
      static CharUnits Zero() {
        return CharUnits(0);
      }

      /// One - Construct a CharUnits quantity of one.
      static CharUnits One() {
        return CharUnits(1);
      }

      /// fromQuantity - Construct a CharUnits quantity from a raw integer type.
      static CharUnits fromQuantity(QuantityType Quantity) {
        return CharUnits(Quantity); 
      }

      // Compound assignment.
      CharUnits& operator+= (const CharUnits &Other) {
        Quantity += Other.Quantity;
        return *this;
      }
      CharUnits& operator-= (const CharUnits &Other) {
        Quantity -= Other.Quantity;
        return *this;
      }
       
      // Comparison operators.
      bool operator== (const CharUnits &Other) const {
        return Quantity == Other.Quantity;
      }
      bool operator!= (const CharUnits &Other) const {
        return Quantity != Other.Quantity;
      }

      // Relational operators.
      bool operator<  (const CharUnits &Other) const { 
        return Quantity <  Other.Quantity; 
      }
      bool operator<= (const CharUnits &Other) const { 
        return Quantity <= Other.Quantity;
      }
      bool operator>  (const CharUnits &Other) const { 
        return Quantity >  Other.Quantity; 
      }
      bool operator>= (const CharUnits &Other) const { 
        return Quantity >= Other.Quantity; 
      }

      // Other predicates.
      
      /// isZero - Test whether the quantity equals zero.
      bool isZero() const     { return Quantity == 0; }

      /// isOne - Test whether the quantity equals one.
      bool isOne() const      { return Quantity == 1; }

      /// isPositive - Test whether the quantity is greater than zero.
      bool isPositive() const { return Quantity  > 0; }

      /// isNegative - Test whether the quantity is less than zero.
      bool isNegative() const { return Quantity  < 0; }

      // Arithmetic operators.
      CharUnits operator* (QuantityType N) const {
        return CharUnits(Quantity * N);
      }
      CharUnits operator/ (QuantityType N) const {
        return CharUnits(Quantity / N);
      }
      QuantityType operator/ (const CharUnits &Other) const {
        return Quantity / Other.Quantity;
      }
      CharUnits operator% (QuantityType N) const {
        return CharUnits(Quantity % N);
      }
      QuantityType operator% (const CharUnits &Other) const {
        return Quantity % Other.Quantity;
      }
      CharUnits operator+ (const CharUnits &Other) const {
        return CharUnits(Quantity + Other.Quantity);
      }
      CharUnits operator- (const CharUnits &Other) const {
        return CharUnits(Quantity - Other.Quantity);
      }
      
      // Conversions.

      /// getQuantity - Get the raw integer representation of this quantity.
      QuantityType getQuantity() const { return Quantity; }

      /// RoundUpToAlignment - Returns the next integer (mod 2**64) that is
      /// greater than or equal to this quantity and is a multiple of \arg
      /// Align. Align must be non-zero.
      CharUnits RoundUpToAlignment(const CharUnits &Align) {
    	  return CharUnits(llvm::RoundUpToAlignment(Quantity,
    			  Align.Quantity));
      }

  }; // class CharUnit
} // namespace mlang

inline mlang::CharUnits operator* (mlang::CharUnits::QuantityType Scale,
                                   const mlang::CharUnits &CU) {
  return CU * Scale;
}

namespace llvm {

template<> struct DenseMapInfo<mlang::CharUnits> {
  static mlang::CharUnits getEmptyKey() {
    mlang::CharUnits::QuantityType Quantity =
      DenseMapInfo<mlang::CharUnits::QuantityType>::getEmptyKey();

    return mlang::CharUnits::fromQuantity(Quantity);
  }

  static mlang::CharUnits getTombstoneKey() {
    mlang::CharUnits::QuantityType Quantity =
      DenseMapInfo<mlang::CharUnits::QuantityType>::getTombstoneKey();
    
    return mlang::CharUnits::fromQuantity(Quantity);
  }

  static unsigned getHashValue(const mlang::CharUnits &CU) {
    mlang::CharUnits::QuantityType Quantity = CU.getQuantity();
    return DenseMapInfo<mlang::CharUnits::QuantityType>::getHashValue(Quantity);
  }

  static bool isEqual(const mlang::CharUnits &LHS,
                      const mlang::CharUnits &RHS) {
    return LHS == RHS;
  }
};

template <> struct isPodLike<mlang::CharUnits> {
  static const bool value = true;
};
  
} // end namespace llvm

#endif // MLANG_AST_CHARUNITS_H_
