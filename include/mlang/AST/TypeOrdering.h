//===--- TypeOrdering.h - Total ordering for types --------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file provides a function objects and specializations that
//  allow Type values to be sorted, used in std::maps, std::sets,
//  llvm::DenseMaps, and llvm::DenseSets.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_TYPEORDERING_H_
#define MLANG_AST_TYPEORDERING_H_

#include "mlang/AST/Type.h"
#include <functional>

namespace mlang {
/// TypeOrdering - Function object that provides a total ordering
/// on Type values.
struct TypeOrdering : std::binary_function<Type, Type, bool> {
  bool operator()(Type T1, Type T2) const {
    return std::less<void*>()(T1.getAsOpaquePtr(), T2.getAsOpaquePtr());
  }
};
} // end namespace mlang

namespace llvm {
  template<class> struct DenseMapInfo;

  template<> struct DenseMapInfo<mlang::Type> {
    static inline mlang::Type getEmptyKey() { return mlang::Type(); }

    static inline mlang::Type getTombstoneKey() {
      using mlang::Type;
      return Type::getFromOpaquePtr(reinterpret_cast<mlang::Type *>(-1));
    }

    static unsigned getHashValue(mlang::Type Val) {
      return (unsigned)((uintptr_t)Val.getAsOpaquePtr()) ^
            ((unsigned)((uintptr_t)Val.getAsOpaquePtr() >> 9));
    }

    static bool isEqual(mlang::Type LHS, mlang::Type RHS) {
      return LHS == RHS;
    }
  };

} // end namespace llvm

#endif /* MLANG_AST_TYPEORDERING_H_ */
