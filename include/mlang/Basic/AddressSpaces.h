//===--- AddressSpaces.h - Language-specific address spaces -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file provides definitions for the various language-specific address
//  spaces.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_ADDRESSSPACES_H
#define MLANG_BASIC_ADDRESSSPACES_H

namespace mlang {

namespace LangAS {

/// This enum defines the set of possible language-specific address spaces.
/// It uses a high starting offset so as not to conflict with any address
/// space used by a target.
enum ID {
  Offset = 0xFFFF00,

  opencl_global = Offset,
  opencl_local,
  opencl_constant,

  cuda_device,
  cuda_constant,
  cuda_shared,

  Last,
  Count = Last-Offset
};

/// The type of a lookup table which maps from language-specific address spaces
/// to target-specific ones.
typedef unsigned Map[Count];

}

}

#endif
