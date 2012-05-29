//===--- MacroBuilder.h - CPP Macro building utility ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the MacroBuilder utility class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_MACROBUILDER_H_
#define MLANG_BASIC_MACROBUILDER_H_

#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"

namespace mlang {

class MacroBuilder {
  llvm::raw_ostream &Out;
public:
  MacroBuilder(llvm::raw_ostream &Output) : Out(Output) {}

  /// Append a #define line for macro of the form "#define Name Value\n".
  void defineMacro(const llvm::Twine &Name, const llvm::Twine &Value = "1") {
    Out << "#define " << Name << ' ' << Value << '\n';
  }

  /// Append a #undef line for Name.  Name should be of the form XXX
  /// and we emit "#undef XXX".
  void undefineMacro(const llvm::Twine &Name) {
    Out << "#undef " << Name << '\n';
  }

  /// Directly append Str and a newline to the underlying buffer.
  void append(const llvm::Twine &Str) {
    Out << Str << '\n';
  }
};

}  // end namespace mlang

#endif /* MLANG_BASIC_MACROBUILDER_H_ */
