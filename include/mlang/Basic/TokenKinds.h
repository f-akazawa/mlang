//===--- TokenKinds.h - Enum values for Mlang Token Kinds -------*- C++ -*-===//
//
// Copyright (C) 2010 Yabin Hu @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the TokenKind enum and support functions.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_TOKEN_KINDS_H_
#define MLANG_BASIC_TOKEN_KINDS_H_

namespace mlang {

namespace tok {

/// TokenKind - This provides a simple uniform namespace for tokens from all C
/// languages.
enum TokenKind {
#define TOK(X) X,
#include "mlang/Basic/TokenKinds.def"
  NUM_TOKENS
};

/// PPKeywordKind - This provides a namespace for preprocessor keywords which
/// start with a '#' at the beginning of the line.
enum PPKeywordKind {
#define PPKEYWORD(X) pp_##X,
#include "mlang/Basic/TokenKinds.def"
  NUM_PP_KEYWORDS
};

/// \brief Determines the name of a token as used within the front end.
///
/// The name of a token will be an internal name (such as "l_square")
/// and should not be used as part of diagnostic messages.
const char *getTokenName(enum TokenKind Kind);

/// \brief Determines the spelling of simple punctuation tokens like
/// '!' or '%', and returns NULL for literal and annotation tokens.
///
/// This routine only retrieves the "simple" spelling of the token,
/// and will not produce any alternative spellings (e.g., a
/// digraph). For the actual spelling of a given Token, use
/// Preprocessor::getSpelling().
const char *getTokenSimpleSpelling(enum TokenKind Kind);

}  // end namespace tok
}  // end namespace mlang

#endif /* MLANG_BASIC_TOKEN_KINDS_H_ */
