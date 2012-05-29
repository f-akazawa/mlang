//===--- Util.h - Common Driver Utilities -----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_DRIVER_UTIL_H_
#define MLANG_DRIVER_UTIL_H_

namespace llvm {
  template<typename T, unsigned N> class SmallVector;
}

namespace mlang {
namespace driver {
  class Action;

  /// ArgStringList - Type used for constructing argv lists for subprocesses.
  typedef llvm::SmallVector<const char*, 16> ArgStringList;

  /// ActionList - Type used for lists of actions.
  typedef llvm::SmallVector<Action*, 3> ActionList;

} // end namespace driver
} // end namespace mlang

#endif
