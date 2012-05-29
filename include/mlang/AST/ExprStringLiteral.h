//===--- ExprStringLiteral.h - String constant for Mlang --------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the StringLiteral class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_STRING_LITERAL_H_
#define MLANG_AST_EXPR_STRING_LITERAL_H_

#include "mlang/AST/Expr.h"

namespace mlang {
class LangOptions;
class SourceManager;
class TargetInfo;

/// A character in the MATLAB software is actually an integer value
/// converted to its Unicode® character equivalent. A character string
/// is a vector with components that are the numeric codes for the
/// characters. The actual characters displayed depend on the character
/// set encoding for a given font.
///
/// The elements of a character or string belong to the char class.
/// Arrays of class char can hold multiple strings, as long as each string
/// in the array has the same length. (This is because MATLAB arrays must
/// be rectangular.) To store an array of strings of unequal length, use
/// a cell array.
class StringLiteral : public Expr {
  const char *StrData;
  unsigned ByteLength;
  unsigned NumConcatenated;
  SourceLocation TokLocs[1];

  StringLiteral(Type Ty) : Expr(StringLiteralClass, Ty, VK_RValue) {}

public:
  /// This is the "fully general" constructor that allows representation of
  /// strings formed from multiple concatenated tokens.
  static StringLiteral *Create(ASTContext &C, const char *StrData,
          unsigned ByteLength, Type Ty,
          const SourceLocation *Loc,
          unsigned NumStrs);

  /// Simple constructor for string literals made from one token.
  static StringLiteral *Create(ASTContext &C, const char *StrData,
                               unsigned ByteLength,
                               Type Ty, SourceLocation Loc) {
    return Create(C, StrData, ByteLength, Ty, &Loc, 1);
  }

  /// \brief Construct an empty string literal.
  static StringLiteral *CreateEmpty(ASTContext &C, unsigned NumStrs);

  llvm::StringRef getString() const {
    return llvm::StringRef(StrData, ByteLength);
  }

  unsigned getByteLength() const { return ByteLength; }

  /// \brief Sets the string data to the given string data.
  void setString(ASTContext &C, llvm::StringRef Str);

  bool containsNonAsciiOrNull() const {
    llvm::StringRef Str = getString();
    for (unsigned i = 0, e = Str.size(); i != e; ++i)
      if (!isascii(Str[i]) || !Str[i])
        return true;
    return false;
  }
  /// getNumConcatenated - Get the number of string literal tokens that were
  /// concatenated in translation phase #6 to form this string literal.
  unsigned getNumConcatenated() const { return NumConcatenated; }

  SourceLocation getStrTokenLoc(unsigned TokNum) const {
    assert(TokNum < NumConcatenated && "Invalid tok number");
    return TokLocs[TokNum];
  }
  void setStrTokenLoc(unsigned TokNum, SourceLocation L) {
    assert(TokNum < NumConcatenated && "Invalid tok number");
    TokLocs[TokNum] = L;
  }

  bool isWide() const {
	  return false;
  }

  SourceLocation getLocationOfByte(unsigned ByteNo, const SourceManager &SM,
                    const LangOptions &Features, const TargetInfo &Target) const;

  typedef const SourceLocation *tokloc_iterator;
  tokloc_iterator tokloc_begin() const { return TokLocs; }
  tokloc_iterator tokloc_end() const { return TokLocs+NumConcatenated; }

  SourceRange getSourceRange() const {
    return SourceRange(TokLocs[0], TokLocs[NumConcatenated-1]);
  }
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == StringLiteralClass;
  }
  static bool classof(const StringLiteral *) { return true; }

  // Iterators
  child_range children() {
	  return child_range();
  }
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_STRING_LITERAL_H_ */
