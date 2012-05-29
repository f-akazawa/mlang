//===--- ParseInit.cpp - Initializer Parsing ------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements initializer parsing as specified by C99 6.7.8.
//
//===----------------------------------------------------------------------===//

#include "mlang/Parse/Parser.h"
#include "mlang/Parse/ParseDiagnostic.h"
#include "mlang/Sema/Scope.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"
using namespace mlang;


/// MayBeDesignationStart - Return true if this token might be the start of a
/// designator.  If we can tell it is impossible that it is a designator, return
/// false.
static bool MayBeDesignationStart(tok::TokenKind K, Preprocessor &PP) {
  switch (K) {
  default: return false;
  case tok::Dot:      // designator: '.' identifier
  case tok::LSquareBrackets:    // designator: array-designator
      return true;
  case tok::Identifier:  // designation: identifier ':'
    return PP.LookAhead(0).is(tok::Colon);
  }
}

ExprResult Parser::ParseBraceInitializer() {
  return ExprError();
}


ExprResult Parser::ParseBracketInitializer() {
	return ExprError();
}
