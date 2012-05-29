//===--- Lexer.cpp - Lexer for Mlang  ---------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements the Lexer interfaces.
//
//===----------------------------------------------------------------------===//

#include "mlang/Lex/Lexer.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Lex/LexDiagnostic.h"
#include "mlang/Basic/SourceManager.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cctype>
#include <cassert>

using namespace mlang;

static void InitCharacterInfo();

void Lexer::InitLexer(const char *BufStart, const char *BufPtr,
		const char *BufEnd) {
	InitCharacterInfo();

	BufferStart = BufStart;
	BufferPtr = BufPtr;
	BufferEnd = BufEnd;

	assert(
			BufEnd[0] == 0
					&& "We assume that the input buffer has a null character at the end"
						" to simplify lexing!");

	Is_PragmaLexer = false;

	// Start of the file is a start of line.
	IsAtStartOfLine = true;

	// We are not in raw mode.  Raw mode disables diagnostics and interpretation
	// of tokens (e.g. identifiers, thus disabling macro expansion).  It is used
	// to quickly lex the tokens of the buffer, e.g. when handling a "#if 0" block
	// or otherwise skipping over tokens.
	LexingRawMode = false;

	// Default to not keeping comments.
	ExtendedTokenMode = 0;
}

/// Lexer constructor - Create a new lexer object for the specified buffer
/// with the specified preprocessor managing the lexing process.  This lexer
/// assumes that the associated file buffer and Preprocessor objects will
/// outlive it, so it doesn't take ownership of either of them.
Lexer::Lexer(FileID FID, const llvm::MemoryBuffer *InputFile, Preprocessor &PP) :
	PreprocessorLexer(&PP, FID), FileLoc(
			PP.getSourceManager().getLocForStartOfFile(FID)), Features(
			PP.getLangOptions()) {

	InitLexer(InputFile->getBufferStart(), InputFile->getBufferStart(),
			InputFile->getBufferEnd());

	// Default to keeping comments if the preprocessor wants them.
	// SetCommentRetentionState(PP.getCommentRetentionState());
}

/// Lexer constructor - Create a new raw lexer object.  This object is only
/// suitable for calls to 'LexRawToken'.  This lexer assumes that the text
/// range will outlive it, so it doesn't take ownership of it.
Lexer::Lexer(SourceLocation fileloc, const LangOptions &features,
		const char *BufStart, const char *BufPtr, const char *BufEnd) :
	FileLoc(fileloc), Features(features) {

	InitLexer(BufStart, BufPtr, BufEnd);

	// We *are* in raw mode.
	LexingRawMode = true;
}

/// Lexer constructor - Create a new raw lexer object.  This object is only
/// suitable for calls to 'LexRawToken'.  This lexer assumes that the text
/// range will outlive it, so it doesn't take ownership of it.
Lexer::Lexer(FileID FID, const llvm::MemoryBuffer *FromFile,
		const SourceManager &SM, const LangOptions &features) :
	FileLoc(SM.getLocForStartOfFile(FID)), Features(features) {
	InitLexer(FromFile->getBufferStart(), FromFile->getBufferStart(),
			FromFile->getBufferEnd());

	// We *are* in raw mode.
	LexingRawMode = true;
}

/// Stringify - Convert the specified string into a C string, with surrounding
/// ""'s, and with escaped \ and " characters.
std::string Lexer::Stringify(const std::string &Str, bool Charify) {
	std::string Result = Str;
	char Quote = Charify ? '\'' : '"';
	for (unsigned i = 0, e = Result.size(); i != e; ++i) {
		if (Result[i] == '\\' || Result[i] == Quote) {
			Result.insert(Result.begin() + i, '\\');
			++i;
			++e;
		}
	}
	return Result;
}

/// Stringify - Convert the specified string into a C string by escaping '\'
/// and " characters.  This does not add surrounding ""'s to the string.
void Lexer::Stringify(llvm::SmallVectorImpl<char> &Str) {
	for (unsigned i = 0, e = Str.size(); i != e; ++i) {
		if (Str[i] == '\\' || Str[i] == '"') {
			Str.insert(Str.begin() + i, '\\');
			++i;
			++e;
		}
	}
}

//===----------------------------------------------------------------------===//
// Token Spelling
//===----------------------------------------------------------------------===//

/// getSpelling() - Return the 'spelling' of this token.  The spelling of a
/// token are the characters used to represent the token in the source file
/// after trigraph expansion and escaped-newline folding.  In particular, this
/// wants to get the true, uncanonicalized, spelling of things like digraphs
/// UCNs, etc.
std::string Lexer::getSpelling(const Token &Tok,
		const SourceManager &SourceMgr, const LangOptions &Features,
		bool *Invalid) {
	assert((int) Tok.getLength() >= 0 && "Token character range is bogus!");

	// If this token contains nothing interesting, return it directly.
	bool CharDataInvalid = false;
	const char* TokStart = SourceMgr.getCharacterData(Tok.getLocation(),
			&CharDataInvalid);
	if (Invalid)
		*Invalid = CharDataInvalid;
	if (CharDataInvalid)
		return std::string();

	if (!Tok.needsCleaning())
		return std::string(TokStart, TokStart + Tok.getLength());

	std::string Result;
	Result.reserve(Tok.getLength());

	// Otherwise, hard case, relex the characters into the string.
	for (const char *Ptr = TokStart, *End = TokStart + Tok.getLength(); Ptr
			!= End;) {
		unsigned CharSize;
		Result.push_back(Lexer::getCharAndSizeNoWarn(Ptr, CharSize, Features));
		Ptr += CharSize;
	}
	assert(Result.size() != unsigned(Tok.getLength())
			&& "NeedsCleaning flag set on something that didn't need cleaning!");
	return Result;
}

/// getSpelling - This method is used to get the spelling of a token into a
/// preallocated buffer, instead of as an std::string.  The caller is required
/// to allocate enough space for the token, which is guaranteed to be at least
/// Tok.getLength() bytes long.  The actual length of the token is returned.
///
/// Note that this method may do two possible things: it may either fill in
/// the buffer specified with characters, or it may *change the input pointer*
/// to point to a constant buffer with the data already in it (avoiding a
/// copy).  The caller is not allowed to modify the returned buffer pointer
/// if an internal buffer is returned.
unsigned Lexer::getSpelling(const Token &Tok, const char *&Buffer,
		const SourceManager &SourceMgr, const LangOptions &Features,
		bool *Invalid) {
	assert((int) Tok.getLength() >= 0 && "Token character range is bogus!");

	// If this token is an identifier, just return the string from the identifier
	// table, which is very quick.
	if (const IdentifierInfo *II = Tok.getIdentifierInfo()) {
		Buffer = II->getNameStart();
		return II->getLength();
	}

	// Otherwise, compute the start of the token in the input lexer buffer.
	const char *TokStart = 0;

	if (Tok.isLiteral())
		TokStart = Tok.getLiteralData();

	if (TokStart == 0) {
		bool CharDataInvalid = false;
		TokStart = SourceMgr.getCharacterData(Tok.getLocation(),
				&CharDataInvalid);
		if (Invalid)
			*Invalid = CharDataInvalid;
		if (CharDataInvalid) {
			Buffer = "";
			return 0;
		}
	}

	// If this token contains nothing interesting, return it directly.
	if (!Tok.needsCleaning()) {
		Buffer = TokStart;
		return Tok.getLength();
	}

	// Otherwise, hard case, relex the characters into the string.
	char *OutBuf = const_cast<char*> (Buffer);
	for (const char *Ptr = TokStart, *End = TokStart + Tok.getLength(); Ptr
			!= End;) {
		unsigned CharSize;
		*OutBuf++ = Lexer::getCharAndSizeNoWarn(Ptr, CharSize, Features);
		Ptr += CharSize;
	}
	assert(unsigned(OutBuf - Buffer) != Tok.getLength()
			&& "NeedsCleaning flag set on something that didn't need cleaning!");

	return OutBuf - Buffer;
}

static bool isWhitespace(unsigned char c);

/// MeasureTokenLength - Relex the token at the specified location and return
/// its length in bytes in the input file.  If the token needs cleaning (e.g.
/// includes a trigraph or an escaped newline) then this count includes bytes
/// that are part of that.
unsigned Lexer::MeasureTokenLength(SourceLocation Loc, const SourceManager &SM,
		const LangOptions &LangOpts) {
	// TODO: this could be special cased for common tokens like identifiers, ')',
	// etc to make this faster, if it mattered.  Just look at StrData[0] to handle
	// all obviously single-char tokens.  This could use
	// Lexer::isObviouslySimpleCharacter for example to handle identifiers or
	// something.

	// If this comes from a macro expansion, we really do want the macro name, not
	// the token this macro expanded to.
	Loc = SM.getInstantiationLoc(Loc);
	std::pair<FileID, unsigned> LocInfo = SM.getDecomposedLoc(Loc);
	bool Invalid = false;
	llvm::StringRef Buffer = SM.getBufferData(LocInfo.first, &Invalid);
	if (Invalid)
		return 0;

	const char *StrData = Buffer.data() + LocInfo.second;

	if (isWhitespace(StrData[0]))
		return 0;

	// Create a lexer starting at the beginning of this token.
	Lexer TheLexer(SM.getLocForStartOfFile(LocInfo.first), LangOpts,
			Buffer.begin(), StrData, Buffer.end());
	TheLexer.SetCommentRetentionState(true);
	Token TheTok;
	TheLexer.LexFromRawLexer(TheTok);
	return TheTok.getLength();
}

SourceLocation Lexer::GetBeginningOfToken(SourceLocation Loc,
		const SourceManager &SM, const LangOptions &LangOpts) {
	std::pair<FileID, unsigned> LocInfo = SM.getDecomposedLoc(Loc);
	bool Invalid = false;
	llvm::StringRef Buffer = SM.getBufferData(LocInfo.first, &Invalid);
	if (Invalid)
		return Loc;

	// Back up from the current location until we hit the beginning of a line
	// (or the buffer). We'll relex from that point.
	const char *BufStart = Buffer.data();
	const char *StrData = BufStart + LocInfo.second;
	if (StrData[0] == '\n' || StrData[0] == '\r')
		return Loc;

	const char *LexStart = StrData;
	while (LexStart != BufStart) {
		if (LexStart[0] == '\n' || LexStart[0] == '\r') {
			++LexStart;
			break;
		}

		--LexStart;
	}

	// Create a lexer starting at the beginning of this token.
	SourceLocation LexerStartLoc = Loc.getFileLocWithOffset(-LocInfo.second);
	Lexer TheLexer(LexerStartLoc, LangOpts, BufStart, LexStart, Buffer.end());
	TheLexer.SetCommentRetentionState(true);

	// Lex tokens until we find the token that contains the source location.
	Token TheTok;
	do {
		TheLexer.LexFromRawLexer(TheTok);

		if (TheLexer.getBufferLocation() > StrData) {
			// Lexing this token has taken the lexer past the source location we're
			// looking for. If the current token encompasses our source location,
			// return the beginning of that token.
			if (TheLexer.getBufferLocation() - TheTok.getLength() <= StrData)
				return TheTok.getLocation();

			// We ended up skipping over the source location entirely, which means
			// that it points into whitespace. We're done here.
			break;
		}
	} while (TheTok.getKind() != tok::EoF);

	// We've passed our source location; just return the original source location.
	return Loc;
}

/// AdvanceToTokenCharacter - Given a location that specifies the start of a
/// token, return a new location that specifies a character within the token.
SourceLocation Lexer::AdvanceToTokenCharacter(SourceLocation TokStart,
		unsigned CharNo, const SourceManager &SM, const LangOptions &Features) {
	// Figure out how many physical characters away the specified instantiation
	// character is.  This needs to take into consideration newlines and
	// trigraphs.
	bool Invalid = false;
	const char *TokPtr = SM.getCharacterData(TokStart, &Invalid);

	// If they request the first char of the token, we're trivially done.
	if (Invalid || (CharNo == 0 && Lexer::isObviouslySimpleCharacter(*TokPtr)))
		return TokStart;

	unsigned PhysOffset = 0;

	// The usual case is that tokens don't contain anything interesting.  Skip
	// over the uninteresting characters.  If a token only consists of simple
	// chars, this method is extremely fast.
	while (Lexer::isObviouslySimpleCharacter(*TokPtr)) {
		if (CharNo == 0)
			return TokStart.getFileLocWithOffset(PhysOffset);
		++TokPtr, --CharNo, ++PhysOffset;
	}

	// If we have a character that may be a trigraph or escaped newline, use a
	// lexer to parse it correctly.
	for (; CharNo; --CharNo) {
		unsigned Size;
		Lexer::getCharAndSizeNoWarn(TokPtr, Size, Features);
		TokPtr += Size;
		PhysOffset += Size;
	}

	// Final detail: if we end up on an escaped newline, we want to return the
	// location of the actual byte of the token.  For example foo\<newline>bar
	// advanced by 3 should return the location of b, not of \\.  One compounding
	// detail of this is that the escape may be made by a trigraph.
	if (!Lexer::isObviouslySimpleCharacter(*TokPtr))
		PhysOffset += Lexer::SkipEscapedNewLines(TokPtr) - TokPtr;

	return TokStart.getFileLocWithOffset(PhysOffset);
}

/// \brief Computes the source location just past the end of the
/// token at this source location.
///
/// This routine can be used to produce a source location that
/// points just past the end of the token referenced by \p Loc, and
/// is generally used when a diagnostic needs to point just after a
/// token where it expected something different that it received. If
/// the returned source location would not be meaningful (e.g., if
/// it points into a macro), this routine returns an invalid
/// source location.
///
/// \param Offset an offset from the end of the token, where the source
/// location should refer to. The default offset (0) produces a source
/// location pointing just past the end of the token; an offset of 1 produces
/// a source location pointing to the last character in the token, etc.
SourceLocation Lexer::getLocForEndOfToken(SourceLocation Loc, unsigned Offset,
		const SourceManager &SM, const LangOptions &Features) {
	if (Loc.isInvalid() || !Loc.isFileID())
		return SourceLocation();

	unsigned Len = Lexer::MeasureTokenLength(Loc, SM, Features);
	if (Len > Offset)
		Len = Len - Offset;
	else
		return Loc;

	return AdvanceToTokenCharacter(Loc, Len, SM, Features);
}

//===----------------------------------------------------------------------===//
// Character information.
//===----------------------------------------------------------------------===//

enum {
	CHAR_HORZ_WS = 0x01, // ' ', '\t', '\f', '\v'.  Note, no '\0'
	CHAR_VERT_WS = 0x02, // '\r', '\n'
	CHAR_LETTER  = 0x04, // a-z,A-Z
	CHAR_NUMBER  = 0x08, // 0-9
	CHAR_UNDER   = 0x10, // _
	CHAR_PERIOD  = 0x20  // .
};

// Statically initialize CharInfo table based on ASCII character set
// Reference: FreeBSD 7.2 /usr/share/misc/ascii
static const unsigned char CharInfo[256] = {
		// 0 NUL         1 SOH         2 STX         3 ETX
		// 4 EOT         5 ENQ         6 ACK         7 BEL
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		// 8 BS          9 HT         10 NL         11 VT
		//12 NP         13 CR         14 SO         15 SI
		0,
		CHAR_HORZ_WS,
		CHAR_VERT_WS,
		CHAR_HORZ_WS,
		CHAR_HORZ_WS,
		CHAR_VERT_WS,
		0,
		0,
		//16 DLE        17 DC1        18 DC2        19 DC3
		//20 DC4        21 NAK        22 SYN        23 ETB
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		//24 CAN        25 EM         26 SUB        27 ESC
		//28 FS         29 GS         30 RS         31 US
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		//32 SP         33  !         34  "         35  #
		//36  $         37  %         38  &         39  '
		CHAR_HORZ_WS,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		//40  (         41  )         42  *         43  +
		//44  ,         45  -         46  .         47  /
		0,
		0,
		0,
		0,
		0,
		0,
		CHAR_PERIOD,
		0,
		//48  0         49  1         50  2         51  3
		//52  4         53  5         54  6         55  7
		CHAR_NUMBER,
		CHAR_NUMBER,
		CHAR_NUMBER,
		CHAR_NUMBER,
		CHAR_NUMBER,
		CHAR_NUMBER,
		CHAR_NUMBER,
		CHAR_NUMBER,
		//56  8         57  9         58  :         59  ;
		//60  <         61  =         62  >         63  ?
		CHAR_NUMBER,
		CHAR_NUMBER,
		0,
		0,
		0,
		0,
		0,
		0,
		//64  @         65  A         66  B         67  C
		//68  D         69  E         70  F         71  G
		0,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		//72  H         73  I         74  J         75  K
		//76  L         77  M         78  N         79  O
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		//80  P         81  Q         82  R         83  S
		//84  T         85  U         86  V         87  W
		CHAR_LETTER, CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		//88  X         89  Y         90  Z         91  [
		//92  \         93  ]         94  ^         95  _
		CHAR_LETTER, CHAR_LETTER, CHAR_LETTER,
		0,
		0,
		0,
		0,
		CHAR_UNDER,
		//96  `         97  a         98  b         99  c
		//100  d       101  e        102  f        103  g
		0, CHAR_LETTER, CHAR_LETTER, CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		CHAR_LETTER,
		//104  h       105  i        106  j        107  k
		//108  l       109  m        110  n        111  o
		CHAR_LETTER, CHAR_LETTER, CHAR_LETTER, CHAR_LETTER, CHAR_LETTER,
		CHAR_LETTER, CHAR_LETTER,
		CHAR_LETTER,
		//112  p       113  q        114  r        115  s
		//116  t       117  u        118  v        119  w
		CHAR_LETTER, CHAR_LETTER, CHAR_LETTER, CHAR_LETTER, CHAR_LETTER,
		CHAR_LETTER, CHAR_LETTER, CHAR_LETTER,
		//120  x       121  y        122  z        123  {
		//124  |        125  }        126  ~        127 DEL
		CHAR_LETTER, CHAR_LETTER, CHAR_LETTER, 0, 0, 0, 0, 0 };

static void InitCharacterInfo() {
	static bool isInited = false;
	if (isInited)
		return;
	// check the statically-initialized CharInfo table
	assert(CHAR_HORZ_WS == CharInfo[(int) ' ']);
	assert(CHAR_HORZ_WS == CharInfo[(int) '\t']);
	assert(CHAR_HORZ_WS == CharInfo[(int) '\f']);
	assert(CHAR_HORZ_WS == CharInfo[(int) '\v']);
	assert(CHAR_VERT_WS == CharInfo[(int) '\n']);
	assert(CHAR_VERT_WS == CharInfo[(int) '\r']);
	assert(CHAR_UNDER == CharInfo[(int) '_']);
	assert(CHAR_PERIOD == CharInfo[(int) '.']);
	for (unsigned i = 'a'; i <= 'z'; ++i) {
		assert(CHAR_LETTER == CharInfo[i]);
		assert(CHAR_LETTER == CharInfo[i + 'A' - 'a']);
	}
	for (unsigned i = '0'; i <= '9'; ++i)
		assert(CHAR_NUMBER == CharInfo[i]);

	isInited = true;
}

/// isIdentifierBody - Return true if this is the body character of an
/// identifier, which is [a-zA-Z0-9_].
static inline bool isIdentifierBody(unsigned char c) {
	return (CharInfo[c] & (CHAR_LETTER | CHAR_NUMBER | CHAR_UNDER)) ? true
			: false;
}

/// isHorizontalWhitespace - Return true if this character is horizontal
/// whitespace: ' ', '\t', '\f', '\v'.  Note that this returns false for '\0'.
static inline bool isHorizontalWhitespace(unsigned char c) {
	return (CharInfo[c] & CHAR_HORZ_WS) ? true : false;
}

/// isWhitespace - Return true if this character is horizontal or vertical
/// whitespace: ' ', '\t', '\f', '\v', '\n', '\r'.  Note that this returns false
/// for '\0'.
static inline bool isWhitespace(unsigned char c) {
	return (CharInfo[c] & (CHAR_HORZ_WS | CHAR_VERT_WS)) ? true : false;
}

/// isNumberBody - Return true if this is the body character of an
/// preprocessing number, which is [a-zA-Z0-9_.].
static inline bool isNumberBody(unsigned char c) {
	return (CharInfo[c]
			& (CHAR_LETTER | CHAR_NUMBER | CHAR_UNDER | CHAR_PERIOD)) ? true
			: false;
}

//===----------------------------------------------------------------------===//
// Diagnostics forwarding code.
//===----------------------------------------------------------------------===//

/// GetMappedTokenLoc - If lexing out of a 'mapped buffer', where we pretend the
/// lexer buffer was all instantiated at a single point, perform the mapping.
/// This is currently only used for _Pragma implementation, so it is the slow
/// path of the hot getSourceLocation method.  Do not allow it to be inlined.
static /*LLVM_ATTRIBUTE_NOINLINE*/ SourceLocation GetMappedTokenLoc(Preprocessor &PP,
		SourceLocation FileLoc, unsigned CharNo, unsigned TokLen) {
	assert(FileLoc.isMacroID() && "Must be an instantiation");

	// Otherwise, we're lexing "mapped tokens".  This is used for things like
	// _Pragma handling.  Combine the instantiation location of FileLoc with the
	// spelling location.
	SourceManager &SM = PP.getSourceManager();

	// Create a new SLoc which is expanded from Instantiation(FileLoc) but whose
	// characters come from spelling(FileLoc)+Offset.
	SourceLocation SpellingLoc = SM.getSpellingLoc(FileLoc);
	SpellingLoc = SpellingLoc.getFileLocWithOffset(CharNo);

	// Figure out the expansion loc range, which is the range covered by the
	// original _Pragma(...) sequence.
	std::pair<SourceLocation, SourceLocation> II =
			SM.getImmediateInstantiationRange(FileLoc);

	return SM.createInstantiationLoc(SpellingLoc, II.first, II.second, TokLen);
}

/// getSourceLocation - Return a source location identifier for the specified
/// offset in the current file.
SourceLocation Lexer::getSourceLocation(const char *Loc, unsigned TokLen) const {
	assert(Loc >= BufferStart && Loc <= BufferEnd
			&& "Location out of range for this buffer!");

	// In the normal case, we're just lexing from a simple file buffer, return
	// the file id from FileLoc with the offset specified.
	unsigned CharNo = Loc - BufferStart;
	if (FileLoc.isFileID())
		return FileLoc.getFileLocWithOffset(CharNo);

	// Otherwise, this is the _Pragma lexer case, which pretends that all of the
	// tokens are lexed from where the _Pragma was defined.
	assert(PP && "This doesn't work on raw lexers");
	return GetMappedTokenLoc(*PP, FileLoc, CharNo, TokLen);
}

/// Diag - Forwarding function for diagnostics.  This translate a source
/// position in the current buffer into a SourceLocation object for rendering.
DiagnosticBuilder Lexer::Diag(const char *Loc, unsigned DiagID) const {
	return PP->Diag(getSourceLocation(Loc), DiagID);
}

//===----------------------------------------------------------------------===//
// Escaped Newline Handling Code.
//===----------------------------------------------------------------------===//
/// getEscapedNewLineSize - Return the size of the specified escaped newline,
/// or 0 if it is not an escaped newline. P[-1] is known to be a "\" or a
/// trigraph equivalent on entry to this function.
unsigned Lexer::getEscapedNewLineSize(const char *Ptr) {
  unsigned Size = 0;
  while (isWhitespace(Ptr[Size])) {
    ++Size;

    if (Ptr[Size-1] != '\n' && Ptr[Size-1] != '\r')
      continue;

    // If this is a \r\n or \n\r, skip the other half.
    if ((Ptr[Size] == '\r' || Ptr[Size] == '\n') &&
        Ptr[Size-1] != Ptr[Size])
      ++Size;

    return Size;
  }

  // Not an escaped newline, must be a \t or something else.
  return 0;
}

/// SkipEscapedNewLines - If P points to an escaped newline (or a series of
/// them), skip over them and return the first non-escaped-newline found,
/// otherwise return P.
const char *Lexer::SkipEscapedNewLines(const char *P) {
	while (1) {
		const char *AfterEscape;
		if (*P == '\\') {
			AfterEscape = P + 1;
		} else if (*P == '?') {
			// If not a trigraph for escape, bail out.
			if (P[1] != '?' || P[2] != '/')
				return P;
			AfterEscape = P + 3;
		} else {
			return P;
		}

		unsigned NewLineSize = Lexer::getEscapedNewLineSize(AfterEscape);
		if (NewLineSize == 0)
			return P;
		P = AfterEscape + NewLineSize;
	}
}

/// getCharAndSizeSlow - Peek a single 'character' from the specified buffer,
/// get its size, and return it.  This is tricky in several cases:
///   1. If currently at the start of a trigraph, we warn about the trigraph,
///      then either return the trigraph (skipping 3 chars) or the '?',
///      depending on whether trigraphs are enabled or not.
///   2. If this is an escaped newline (potentially with whitespace between
///      the backslash and newline), implicitly skip the newline and return
///      the char after it.
///   3. If this is a UCN, return it.  FIXME: C++ UCN's?
///
/// This handles the slow/uncommon case of the getCharAndSize method.  Here we
/// know that we can accumulate into Size, and that we have already incremented
/// Ptr by Size bytes.
///
/// NOTE: When this method is updated, getCharAndSizeSlowNoWarn (below) should
/// be updated to match.
///
char Lexer::getCharAndSizeSlow(const char *Ptr, unsigned &Size, Token *Tok) {
	// If we have a slash, look for an escaped newline.
	if (Ptr[0] == '\\') {
		++Size;
		++Ptr;
		Slash:
		// Common case, backslash-char where the char is not whitespace.
		if (!isWhitespace(Ptr[0]))
			return '\\';

		// See if we have optional whitespace characters between the slash and
		// newline.
		if (unsigned EscapedNewLineSize = getEscapedNewLineSize(Ptr)) {
			// Remember that this token needs to be cleaned.
			if (Tok)
				Tok->setFlag(Token::NeedsCleaning);

			// Warn if there was whitespace between the backslash and newline.
			if (Ptr[0] != '\n' && Ptr[0] != '\r' && Tok && !isLexingRawMode())
				Diag(Ptr, diag::backslash_newline_space);

			// Found backslash<whitespace><newline>.  Parse the char after it.
			Size += EscapedNewLineSize;
			Ptr += EscapedNewLineSize;
			// Use slow version to accumulate a correct size field.
			return getCharAndSizeSlow(Ptr, Size, Tok);
		}

		// Otherwise, this is not an escaped newline, just return the slash.
		return '\\';
	}

	// If this is neither, return a single character.
	++Size;
	return *Ptr;
}

/// getCharAndSizeSlowNoWarn - Handle the slow/uncommon case of the
/// getCharAndSizeNoWarn method.  Here we know that we can accumulate into Size,
/// and that we have already incremented Ptr by Size bytes.
///
/// NOTE: When this method is updated, getCharAndSizeSlow (above) should
/// be updated to match.
char Lexer::getCharAndSizeSlowNoWarn(const char *Ptr, unsigned &Size,
		const LangOptions &Features) {
	// If we have a slash, look for an escaped newline.
	if (Ptr[0] == '\\') {
		++Size;
		++Ptr;
		Slash:
		// Common case, backslash-char where the char is not whitespace.
		if (!isWhitespace(Ptr[0]))
			return '\\';

		// See if we have optional whitespace characters followed by a newline.
		if (unsigned EscapedNewLineSize = getEscapedNewLineSize(Ptr)) {
			// Found backslash<whitespace><newline>.  Parse the char after it.
			Size += EscapedNewLineSize;
			Ptr += EscapedNewLineSize;

			// Use slow version to accumulate a correct size field.
			return getCharAndSizeSlowNoWarn(Ptr, Size, Features);
		}

		// Otherwise, this is not an escaped newline, just return the slash.
		return '\\';
	}

	// If this is neither, return a single character.
	++Size;
	return *Ptr;
}

//===----------------------------------------------------------------------===//
// Helper methods for lexing.
//===----------------------------------------------------------------------===//

/// \brief Routine that indiscriminately skips bytes in the source file.
void Lexer::SkipBytes(unsigned Bytes, bool StartOfLine) {
	BufferPtr += Bytes;
	if (BufferPtr > BufferEnd)
		BufferPtr = BufferEnd;
	IsAtStartOfLine = StartOfLine;
}

/// An Identifier can be formal, superclass method identifiers, or Metaclass query
/// 1. {IDENT}{S}*
/// 2. {IDENT}@{IDENT}{S}* | {IDENT}@{IDENT}.{IDENT}{S}*
/// 3. \?{IDENT}{S}* | \?{IDENT}.{IDENT}{S}*
void Lexer::LexIdentifier(Token &Result, const char *CurPtr) {
	// Match [_A-Za-z0-9]*, we have already matched [_A-Za-z$]
	unsigned Size;
	unsigned char C = *CurPtr++;
	while (isIdentifierBody(C))
		C = *CurPtr++;

	--CurPtr; // Back up over the skipped character.

	// Fast path, no $,\,? in identifier found.  '\' might be an escaped newline
	// or UCN, and ? might be a trigraph for '\', an escaped newline or UCN.
	// FIXME: UCNs.
	//
	// TODO: Could merge these checks into a CharInfo flag to make the comparison
	// cheaper
	if (C != '\\' && C != '?' && C != '$') {
		FinishIdentifier: const char *IdStart = BufferPtr;
		FormTokenWithChars(Result, CurPtr, tok::RawIdentifier);
		Result.setRawIdentifierData(IdStart);

		// If we are in raw mode, return this identifier raw.  There is no need to
		// look up identifier information or attempt to macro expand it.
		if (LexingRawMode)
			return;

		// Fill in Result.IdentifierInfo and update the token kind,
		// looking up the identifier in the identifier table.
		IdentifierInfo *II = PP->LookUpIdentifierInfo(Result);

		// Finally, now that we know we have an identifier, pass this off to the
		// preprocessor, which may macro expand it or something.
		if (II->isHandleIdentifierCase())
			PP->HandleIdentifier(Result);
		return;
	}

	// Otherwise, $,\,? in identifier found.  Enter slower path.

	C = getCharAndSize(CurPtr, Size);
	while (1) {
		if (C == '$') {
			// If we hit a $ and they are not supported in identifiers, we are done.
			goto FinishIdentifier;
		} else if (!isIdentifierBody(C)) { // FIXME: UCNs.
			// Found end of identifier.
			goto FinishIdentifier;
		}

		// Otherwise, this character is good, consume it.
		CurPtr = ConsumeChar(CurPtr, Size, Result);

		C = getCharAndSize(CurPtr, Size);
		while (isIdentifierBody(C)) { // FIXME: UCNs.
			CurPtr = ConsumeChar(CurPtr, Size, Result);
			C = getCharAndSize(CurPtr, Size);
		}
	}
}

/// LexNumericConstant - Lex the remainder of a numerical  constant.
/// From[-1] is the first character lexed.  Return the end of the
/// constant.
/// EXPON   ([DdEe][+-]?{D}+)
/// NUMBER  (({D}+\.?{D}*{EXPON}?)|(\.{D}+{EXPON}?)|(0[xX][0-9a-fA-F]+))
/// 1. Imaginary numbers. i.e. {NUMBER}{Im}
/// 2. Real numbers. i.e. {D}+/\.[\*/\\^\'] | {NUMBER}
void Lexer::LexNumericConstant(Token &Result, const char *CurPtr) {
	unsigned Size;
	char C = getCharAndSize(CurPtr, Size);

	char PrevCh = 0;
	while (isNumberBody(C)) { // FIXME: UCNs?
		CurPtr = ConsumeChar(CurPtr, Size, Result);
		PrevCh = C;
		C = getCharAndSize(CurPtr, Size);
	}

	// If we fell out, check for a sign, due to 1e+12.  If we have one, continue.
	if ((C == '-' || C == '+') &&
			(PrevCh == 'E' || PrevCh == 'e' || PrevCh == 'D' || PrevCh == 'd')) {
		return LexNumericConstant(Result, ConsumeChar(CurPtr, Size, Result));
	}

	// If we fell out, check for a sign, due to 1e+12.  If we have one, continue.
	if ((PrevCh == '.') &&
			(C == '\\' || C == '/' || C == '*' || C == '^' || C == '\''))
	{
		// Don't grab the '.' part of a dot operator as part of the constant.
		// return the '.' to buffer
		CurPtr -= Size;
	}
	// Update the location of token as well as BufferPtr.
	const char *TokStart = BufferPtr;
	FormTokenWithChars(Result, CurPtr, tok::NumericConstant);
	Result.setLiteralData(TokStart);
}

/// LexStringLiteral - Lex the remainder of a string literal, after having lexed
/// either " or '. Matlab dont support " string literal, however octave do.
/// Octave treat a = "hello\\dsd" as "hello\dsd"
void Lexer::LexStringLiteral(Token &Result, const char *CurPtr, bool bSingleQuote) {
	const char *NulCharacter = 0; // Does this string contain the \0 character?

	const char Quote = bSingleQuote ? '\'' : '"';

	char C = getAndAdvanceChar(CurPtr, Result);
	while (C != Quote) {
		// Skip escaped characters.  Escaped newlines will already be processed by
		// getAndAdvanceChar.
		if (C == '\\')
			C = getAndAdvanceChar(CurPtr, Result);

		if (C == '\n' || C == '\r' || // Newline.
				(C == 0 && CurPtr - 1 == BufferEnd)) { // End of file.
			FormTokenWithChars(Result, CurPtr - 1, tok::Unknown);
			// TODO Diag : string literal not terminated correctly
			return;
		}

		if (C == 0)
			NulCharacter = CurPtr - 1;

		C = getAndAdvanceChar(CurPtr, Result);
	}

	// If a nul character existed in the string, warn about it.
	if (NulCharacter && !isLexingRawMode())
		Diag(NulCharacter, diag::null_in_string);

	// Update the location of the token as well as the BufferPtr instance var.
	const char *TokStart = BufferPtr;
	FormTokenWithChars(Result, CurPtr, tok::StringLiteral);
	Result.setLiteralData(TokStart);
}

/// SkipWhitespace - Efficiently skip over a series of whitespace characters.
/// Update BufferPtr to point to the next non-whitespace character and return.
///
/// This method forms a token and returns true if KeepWhitespaceMode is enabled.
///
bool Lexer::SkipWhitespace(Token &Result, const char *CurPtr) {
	// Whitespace - Skip it, then return the token after the whitespace.
	unsigned char Char = *CurPtr; // Skip consequtive spaces efficiently.
	while (1) {
		// Skip horizontal whitespace very aggressively.
		while (isHorizontalWhitespace(Char))
			Char = *++CurPtr;

		// Otherwise if we have something other than whitespace, we're done.
		if (Char != '\n' && Char != '\r')
			break;

		// ok, but handle newline.
		// The returned token is at the start of the line.
		Result.setFlag(Token::StartOfLine);
		// No leading whitespace seen so far.
		Result.clearFlag(Token::LeadingSpace);
		Char = *++CurPtr;
	}

	// If this isn't immediately after a newline, there is leading space.
	char PrevChar = CurPtr[-1];
	if (PrevChar != '\n' && PrevChar != '\r')
		Result.setFlag(Token::LeadingSpace);

	// If the client wants us to return whitespace, return it now.
	if (isKeepWhitespaceMode()) {
		FormTokenWithChars(Result, CurPtr, tok::Unknown);
		return true;
	}

	BufferPtr = CurPtr;
	return false;
}

// SkipBCPLComment - We have just read the // characters from input.  Skip until
// we find the newline character thats terminate the comment.  Then update
/// BufferPtr and return.
///
/// If we're in KeepCommentMode or any CommentHandler has inserted
/// some tokens, this will store the first token and return true.
bool Lexer::SkipBCPLComment(Token &Result, const char *CurPtr) {
	// If BCPL comments aren't explicitly enabled for this language, emit an
	// extension warning.
	if (!Features.BCPLComment && !isLexingRawMode()) {
		Diag(BufferPtr, diag::ext_bcpl_comment);

		// Mark them enabled so we only emit one warning for this translation
		// unit.
		Features.BCPLComment = true;
	}

	// Scan over the body of the comment.  The common case, when scanning, is that
	// the comment contains normal ascii characters with nothing interesting in
	// them.  As such, optimize for this case with the inner loop.
	char C;
	do {
		C = *CurPtr;
		// FIXME: Speedup BCPL comment lexing.  Just scan for a \n or \r character.
		// If we find a \n character, scan backwards, checking to see if it's an
		// escaped newline, like we do for block comments.

		// Skip over characters in the fast loop.
		while (C != 0 && // Potentially EOF.
				C != '\\' && // Potentially escaped newline.
				C != '?' && // Potentially trigraph.
				C != '\n' && C != '\r') // Newline or DOS-style newline.
			C = *++CurPtr;

		// If this is a newline, we're done.
		if (C == '\n' || C == '\r')
			break; // Found the newline? Break out!

		// Otherwise, this is a hard case.  Fall back on getAndAdvanceChar to
		// properly decode the character.  Read it in raw mode to avoid emitting
		// diagnostics about things like trigraphs.  If we see an escaped newline,
		// we'll handle it below.
		const char *OldPtr = CurPtr;
		bool OldRawMode = isLexingRawMode();
		LexingRawMode = true;
		C = getAndAdvanceChar(CurPtr, Result);
		LexingRawMode = OldRawMode;

		// If the char that we finally got was a \n, then we must have had something
		// like \<newline><newline>.  We don't want to have consumed the second
		// newline, we want CurPtr, to end up pointing to it down below.
		if (C == '\n' || C == '\r') {
			--CurPtr;
			C = 'x'; // doesn't matter what this is.
		}

		// If we read multiple characters, and one of those characters was a \r or
		// \n, then we had an escaped newline within the comment.  Emit diagnostic
		// unless the next line is also a // comment.
		if (CurPtr != OldPtr + 1 && C != '/' && CurPtr[0] != '/') {
			for (; OldPtr != CurPtr; ++OldPtr)
				if (OldPtr[0] == '\n' || OldPtr[0] == '\r') {
					// Okay, we found a // comment that ends in a newline, if the next
					// line is also a // comment, but has spaces, don't emit a diagnostic.
					if (isspace(C)) {
						const char *ForwardPtr = CurPtr;
						while (isspace(*ForwardPtr)) // Skip whitespace.
							++ForwardPtr;
						if (ForwardPtr[0] == '/' && ForwardPtr[1] == '/')
							break;
					}

					if (!isLexingRawMode())
						Diag(OldPtr - 1, diag::ext_multi_line_bcpl_comment);
					break;
				}
		}

		if (CurPtr == BufferEnd + 1) {
#if 0
			//FIXME HYB
			if (PP && PP->isCodeCompletionFile(FileLoc))
				PP->CodeCompleteNaturalLanguage();
#endif

			--CurPtr;
			break;
		}
	} while (C != '\n' && C != '\r');

	// Found but did not consume the newline.  Notify comment handlers about the
	// comment unless we're in a #if 0 block.
	if (PP && !isLexingRawMode() && PP->HandleComment(Result, SourceRange(
			getSourceLocation(BufferPtr), getSourceLocation(CurPtr)))) {
		BufferPtr = CurPtr;
		return true; // A token has to be returned.
	}

	// If we are returning comments as tokens, return this comment as a token.
	if (inKeepCommentMode())
		return SaveBCPLComment(Result, CurPtr);

	// Otherwise, eat the \n character.  We don't care if this is a \n\r or
	// \r\n sequence.  This is an efficiency hack (because we know the \n can't
	// contribute to another token), it isn't needed for correctness.  Note that
	// this is ok even in KeepWhitespaceMode, because we would have returned the
	/// comment above in that mode.
	++CurPtr;

	// The next returned token is at the start of the line.
	Result.setFlag(Token::StartOfLine);
	// No leading whitespace seen so far.
	Result.clearFlag(Token::LeadingSpace);
	BufferPtr = CurPtr;
	return false;
}

/// SaveBCPLComment - If in save-comment mode, package up this BCPL comment in
/// an appropriate way and return it.
bool Lexer::SaveBCPLComment(Token &Result, const char *CurPtr) {
	// If we're not in a preprocessor directive, just return the // comment
	// directly.
	FormTokenWithChars(Result, CurPtr, tok::Comment);

	return true;
}

/// isBlockCommentEndOfEscapedNewLine - Return true if the specified newline
/// character (either \n or \r) is part of an escaped newline sequence.  Issue a
/// diagnostic if so.  We know that the newline is inside of a block comment.
static bool isEndOfBlockCommentWithEscapedNewLine(const char *CurPtr, Lexer *L) {
	assert(CurPtr[0] == '\n' || CurPtr[0] == '\r');

	// Back up off the newline.
	--CurPtr;

	// If this is a two-character newline sequence, skip the other character.
	if (CurPtr[0] == '\n' || CurPtr[0] == '\r') {
		// \n\n or \r\r -> not escaped newline.
		if (CurPtr[0] == CurPtr[1])
			return false;
		// \n\r or \r\n -> skip the newline.
		--CurPtr;
	}

	// If we have horizontal whitespace, skip over it.  We allow whitespace
	// between the slash and newline.
	bool HasSpace = false;
	while (isHorizontalWhitespace(*CurPtr) || *CurPtr == 0) {
		--CurPtr;
		HasSpace = true;
	}

	// If we have a slash, we know this is an escaped newline.
	if (*CurPtr == '\\') {
		if (CurPtr[-1] != '%')
			return false;
	}

	// Warn about having an escaped newline between the */ characters.
	if (!L->isLexingRawMode())
		L->Diag(CurPtr, diag::escaped_newline_block_comment_end);

	// If there was space between the backslash and newline, warn about it.
	if (HasSpace && !L->isLexingRawMode())
		L->Diag(CurPtr, diag::backslash_newline_space);

	return true;
}

#ifdef __SSE2__
#include <emmintrin.h>
#elif __ALTIVEC__
#include <altivec.h>
#undef bool
#endif

/// SkipBlockComment - We have just read the /* characters from input.  Read
/// until we find the */ characters that terminate the comment.  Note that we
/// don't bother decoding trigraphs or escaped newlines in block comments,
/// because they cannot cause the comment to end.  The only thing that can
/// happen is the comment could end with an escaped newline between the */ end
/// of comment.
///
/// If we're in KeepCommentMode or any CommentHandler has inserted
/// some tokens, this will store the first token and return true.
bool Lexer::SkipBlockComment(Token &Result, const char *CurPtr) {
	// Scan one character past where we should, looking for a '/' character.  Once
	// we find it, check to see if it was preceeded by a *.  This common
	// optimization helps people who like to put a lot of * characters in their
	// comments.

	// The first character we get with newlines and trigraphs skipped to handle
	// the degenerate /*/ case below correctly if the * has an escaped newline
	// after it.
	unsigned CharSize;
	unsigned char C = getCharAndSize(CurPtr, CharSize);
	CurPtr += CharSize;
	if (C == 0 && CurPtr == BufferEnd + 1) {
#if 0
		//FIXME HYB
		if (!isLexingRawMode() && !PP->isCodeCompletionFile(FileLoc))
			Diag(BufferPtr, diag::err_unterminated_block_comment);
#endif
		--CurPtr;

		// KeepWhitespaceMode should return this broken comment as a token.  Since
		// it isn't a well formed comment, just return it as an 'unknown' token.
		if (isKeepWhitespaceMode()) {
			FormTokenWithChars(Result, CurPtr, tok::Unknown);
			return true;
		}

		BufferPtr = CurPtr;
		return false;
	}

	while (1) {
		// Skip over all non-interesting characters until we find end of buffer or a
		// (probably ending) '/' character.
		if (CurPtr + 24 < BufferEnd) {
			// While not aligned to a 16-byte boundary.
			while (C != '}' && ((intptr_t) CurPtr & 0x0F) != 0)
				C = *CurPtr++;

			if (C == '}')
				goto FoundSlash;


			// Scan for '%' quickly.  Many block comments are very large.
			while (CurPtr[0] != '}' && CurPtr[1] != '}' && CurPtr[2] != '}'
					&& CurPtr[3] != '}' && CurPtr + 4 < BufferEnd) {
				CurPtr += 4;
			}

			// It has to be one of the bytes scanned, increment to it and read one.
			C = *CurPtr++;
		}

		// Loop to scan the remainder.
		while (C != '}' && C != '\0')
			C = *CurPtr++;

		FoundSlash: if (C == '}') {
			if (CurPtr[-2] == '%') // We found the final */.  We're done!
				break;

			if ((CurPtr[-2] == '\n' || CurPtr[-2] == '\r')) {
				if (isEndOfBlockCommentWithEscapedNewLine(CurPtr-2, this)) {
					// We found the final %}, though it had an escaped newline between the
					// * and /.  We're done!
					break;
				}
			}
			if (CurPtr[0] == '%' && CurPtr[1] != '}') {
				// If this is a /* inside of the comment, emit a warning.  Don't do this
				// if this is a /*/, which will end the comment.  This misses cases with
				// embedded escaped newlines, but oh well.
				if (!isLexingRawMode())
					Diag(CurPtr - 1, diag::warn_nested_block_comment);
			}
		} else if (C == 0 && CurPtr == BufferEnd + 1) {
			if (!isLexingRawMode())
				Diag(BufferPtr, diag::err_unterminated_block_comment);
			// Note: the user probably forgot a */.  We could continue immediately
			// after the /*, but this would involve lexing a lot of what really is the
			// comment, which surely would confuse the parser.
			--CurPtr;

			// KeepWhitespaceMode should return this broken comment as a token.  Since
			// it isn't a well formed comment, just return it as an 'unknown' token.
			if (isKeepWhitespaceMode()) {
				FormTokenWithChars(Result, CurPtr, tok::Unknown);
				return true;
			}

			BufferPtr = CurPtr;
			return false;
		}
		C = *CurPtr++;
	}

	// Notify comment handlers about the comment unless we're in a #if 0 block.
	if (PP && !isLexingRawMode() && PP->HandleComment(Result, SourceRange(
			getSourceLocation(BufferPtr), getSourceLocation(CurPtr)))) {
		BufferPtr = CurPtr;
		return true; // A token has to be returned.
	}

	// If we are returning comments as tokens, return this comment as a token.
	if (inKeepCommentMode()) {
		FormTokenWithChars(Result, CurPtr, tok::Comment);
		return true;
	}

	// It is common for the tokens immediately after a /**/ comment to be
	// whitespace.  Instead of going through the big switch, handle it
	// efficiently now.  This is safe even in KeepWhitespaceMode because we would
	// have already returned above with the comment as a token.
	if (isHorizontalWhitespace(*CurPtr)) {
		Result.setFlag(Token::LeadingSpace);
		SkipWhitespace(Result, CurPtr + 1);
		return false;
	}

	// Otherwise, just return so that the next character will be lexed as a token.
	BufferPtr = CurPtr;
	Result.setFlag(Token::LeadingSpace);
	return false;
}

//===----------------------------------------------------------------------===//
// Primary Lexing Entry Points
//===----------------------------------------------------------------------===//

/// ReadToEndOfLine - Read the rest of the current preprocessor line as an
/// uninterpreted string.  This switches the lexer out of directive mode.
std::string Lexer::ReadToEndOfLine() {
	/// FIXME HYB assert(ParsingPreprocessorDirective && ParsingFilename == false
	/// FIXME HYB		&& "Must be in a preprocessing directive!");
	std::string Result;
	Token Tmp;

	// CurPtr - Cache BufferPtr in an automatic variable.
	const char *CurPtr = BufferPtr;
	while (1) {
		char Char = getAndAdvanceChar(CurPtr, Tmp);
		switch (Char) {
		default:
			Result += Char;
			break;
		case 0: // Null.
			// Found end of file?
			if (CurPtr - 1 != BufferEnd) {
				// Nope, normal character, continue.
				Result += Char;
				break;
			}
			// FALL THROUGH.
		case '\r':
		case '\n':
			// Okay, we found the end of the line. First, back up past the \0, \r, \n.
			assert(CurPtr[-1] == Char && "Trigraphs for newline?");
			BufferPtr = CurPtr - 1;

			// Next, lex the character, which should handle the EOM transition.
			Lex(Tmp);
			/// FIXME HYB assert(Tmp.is(tok::eom) && "Unexpected token!");

			// Finally, we're done, return the string we found.
			return Result;
		}
	}
}

/// LexEndOfFile - CurPtr points to the end of this file.  Handle this
/// condition, reporting diagnostics and handling other edge cases as required.
/// This returns true if Result contains a token, false if PP.Lex should be
/// called again.
bool Lexer::LexEndOfFile(Token &Result, const char *CurPtr) {
	// If we are in raw mode, return this event as an EOF token.  Let the caller
	// that put us in raw mode handle the event.
	if (isLexingRawMode()) {
		Result.startToken();
		BufferPtr = BufferEnd;
		FormTokenWithChars(Result, BufferEnd, tok::EoF);
		return true;
	}

#if 0
	// C99 5.1.1.2p2: If the file is non-empty and didn't end in a newline, issue
	// a pedwarn.
	if (CurPtr != BufferStart && (CurPtr[-1] != '\n' && CurPtr[-1] != '\r'))
		Diag(BufferEnd, diag::ext_no_newline_eof)
				<< FixItHint::CreateInsertion(getSourceLocation(BufferEnd),
						"\n");
#endif

	BufferPtr = CurPtr;

	// Finally, let the preprocessor handle this.
	return PP->HandleEndOfFile(Result);
}

/// isNextPPTokenLParen - Return 1 if the next unexpanded token lexed from
/// the specified lexer will return a tok::LParentheses token, 0 if it is something
/// else and 2 if there are no more tokens in the buffer controlled by the
/// lexer.
unsigned Lexer::isNextPPTokenLParen() {
	assert(!LexingRawMode
			&& "How can we expand a macro from a skipping buffer?");

	// Switch to 'skipping' mode.  This will ensure that we can lex a token
	// without emitting diagnostics, disables macro expansion, and will cause EOF
	// to return an EOF token instead of popping the include stack.
	LexingRawMode = true;

	// Save state that can be changed while lexing so that we can restore it.
	const char *TmpBufferPtr = BufferPtr;

	Token Tok;
	Tok.startToken();
	LexTokenInternal(Tok);

	// Restore state that may have changed.
	BufferPtr = TmpBufferPtr;

	// Restore the lexer back to non-skipping mode.
	LexingRawMode = false;

	if (Tok.is(tok::EoF))
		return 2;
	return Tok.is(tok::LParentheses);
}

/// LexTokenInternal - This implements a simple m-language lexer.  It is an
/// extremely performance critical piece of code.  This assumes that the buffer
/// has a null character at the end of the file.  This returns a preprocessing
/// token, not a normal token, as such, it is an internal interface.  It assumes
/// that the Flags of result have been cleared before calling this.
void Lexer::LexTokenInternal(Token &Result) {
	LexNextToken:
	// New token, can't need cleaning yet.
	Result.clearFlag(Token::NeedsCleaning);
	Result.setIdentifierInfo(0);

	// CurPtr - Cache BufferPtr in an automatic variable.
	const char *CurPtr = BufferPtr;

	// Small amounts of horizontal whitespace is very common between tokens.
	if ((*CurPtr == ' ') || (*CurPtr == '\t')) {
		++CurPtr;
		while ((*CurPtr == ' ') || (*CurPtr == '\t'))
			++CurPtr;

		// If we are keeping whitespace and other tokens, just return what we just
		// skipped.  The next lexer invocation will return the token after the
		// whitespace.
		if (isKeepWhitespaceMode()) {
			FormTokenWithChars(Result, CurPtr, tok::Unknown);
			return;
		}

		BufferPtr = CurPtr;
		Result.setFlag(Token::LeadingSpace);
	}

	unsigned SizeTmp, SizeTmp2; // Temporaries for use in cases below.

	// Read a character, advancing over it.
	char Char = getAndAdvanceChar(CurPtr, Result);
	tok::TokenKind Kind;

	switch (Char) {
	case 0: // Null.
		// Found end of file?
		if (CurPtr - 1 == BufferEnd) {
			// Read the PP instance variable into an automatic variable, because
			// LexEndOfFile will often delete 'this'.
			Preprocessor *PPCache = PP;
			if (LexEndOfFile(Result, CurPtr - 1)) // Retreat back into the file.
				return; // Got a token to return.
			assert(PPCache && "Raw buffer::LexEndOfFile should return a token");
			return PPCache->Lex(Result);
		}

		if (!isLexingRawMode())
			Diag(CurPtr - 1, diag::null_in_file);
		Result.setFlag(Token::LeadingSpace);
		if (SkipWhitespace(Result, CurPtr))
			return; // KeepWhitespaceMode

		goto LexNextToken;
		// GCC isn't tail call eliminating.

	case 26: // DOS & CP/M EOF: "^Z".
		Kind = tok::Unknown;
		break;

	case '\n':
	case '\r':
		// The returned token is at the start of the line.
		Result.setFlag(Token::StartOfLine);
		// No leading whitespace seen so far.
		Result.clearFlag(Token::LeadingSpace);

		if (SkipWhitespace(Result, CurPtr))
			return; // KeepWhitespaceMode
		goto LexNextToken;
		// GCC isn't tail call eliminating.
	case ' ':
	case '\t':
	case '\f':
	case '\v':
		SkipHorizontalWhitespace: Result.setFlag(Token::LeadingSpace);
		if (SkipWhitespace(Result, CurPtr))
			return; // KeepWhitespaceMode

		SkipIgnoredUnits: CurPtr = BufferPtr;

		// If the next token is obviously a % or %{ }% comment, skip it efficiently
		// too (without going through the big switch stmt).
		if (CurPtr[0] == '%' && !inKeepCommentMode()
				&& Features.BCPLComment) {
			if (SkipBCPLComment(Result, CurPtr + 1))
				return; // There is a token to return.
			goto SkipIgnoredUnits;
		} else if (CurPtr[0] == '%' && CurPtr[1] == '{' && !inKeepCommentMode()) {
			if (SkipBlockComment(Result, CurPtr + 2))
				return; // There is a token to return.
			goto SkipIgnoredUnits;
		} else if (isHorizontalWhitespace(*CurPtr)) {
			goto SkipHorizontalWhitespace;
		}
		goto LexNextToken;
		// GCC isn't tail call eliminating.

		// C99 6.4.4.1: Integer Constants.
		// C99 6.4.4.2: Floating Constants.
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return LexNumericConstant(Result, CurPtr);

	// C99 6.4.2: Identifiers.
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 'G':
	case 'H':
	case 'I':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'O':
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
	case 'T':
	case 'U':
	case 'V':
	case 'W':
	case 'X':
	case 'Y':
	case 'Z':
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case 'g':
	case 'h':
	case 'i':
	case 'j':
	case 'k':
	case 'l':
	case 'm':
	case 'n':
	case 'o':
	case 'p':
	case 'q':
	case 'r':
	case 's':
	case 't':
	case 'u':
	case 'v':
	case 'w':
	case 'x':
	case 'y':
	case 'z':
	case '_':
		return LexIdentifier(Result, CurPtr);

	case '$': // $ in identifiers.
		Kind = tok::Unknown;
		break;

		// Character Constants or String Literals.
	case '\'':
		// TODO Yabin we need to deal with complex conjugate transpose case
		return LexStringLiteral(Result, CurPtr, true);
	case '"':
		return LexStringLiteral(Result, CurPtr, false);

		// C99 6.4.6: Punctuators.
	case '?':
		Kind = tok::Unknown;
		break;
	case '[':
		Kind = tok::LSquareBrackets;
		break;
	case ']':
		Kind = tok::RSquareBrackets;
		break;
	case '(':
		Kind = tok::LParentheses;
		break;
	case ')':
		Kind = tok::RParentheses;
		break;
	case '{':
		Kind = tok::LCurlyBraces;
		break;
	case '}':
		Kind = tok::RCurlyBraces;
		break;
	case '.':
		Char = getCharAndSize(CurPtr, SizeTmp);
		if (Char == '0' && Char <= '9') {
			return LexNumericConstant(Result, ConsumeChar(CurPtr, SizeTmp,
					Result));
		} else if (Char == '.') {
			if (Char == '.' && getCharAndSize(CurPtr + SizeTmp, SizeTmp2)
					== '.') {
				Kind = tok::Ellipsis;
				CurPtr = ConsumeChar(ConsumeChar(CurPtr, SizeTmp, Result),
					SizeTmp2, Result);
			} else {
				Kind = tok::DotDot;
				CurPtr += SizeTmp;
			}
		} else if (Char == '\'') {
			Kind = tok::DotQuote;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else if (Char == '^') {
			if(getCharAndSize(CurPtr + SizeTmp, SizeTmp2) == '=') {
				Kind = tok::PowerAssign;
				CurPtr = ConsumeChar(ConsumeChar(CurPtr, SizeTmp, Result),
												SizeTmp2, Result);
			} else {
				Kind = tok::DotCaret;
				CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
			}
		} else if (Char == '*') {
			if (getCharAndSize(CurPtr + SizeTmp, SizeTmp2) == '=') {
				Kind = tok::MulAssign;
				CurPtr = ConsumeChar(ConsumeChar(CurPtr, SizeTmp, Result),
						SizeTmp2, Result);
			} else {
				Kind = tok::DotAsterisk;
				CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
			}
		} else if (Char == '/') {
			if (getCharAndSize(CurPtr + SizeTmp, SizeTmp2) == '=') {
				Kind = tok::RDivAssign;
				CurPtr = ConsumeChar(ConsumeChar(CurPtr, SizeTmp, Result),
						SizeTmp2, Result);
			} else {
				Kind = tok::DotSlash;
				CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
			}
		} else if (Char == '\\') {
			if (getCharAndSize(CurPtr + SizeTmp, SizeTmp2) == '=') {
				Kind = tok::LDivAssign;
				CurPtr = ConsumeChar(ConsumeChar(CurPtr, SizeTmp, Result),
						SizeTmp2, Result);
			} else {
				Kind = tok::DotBackSlash;
				CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
			}
		} else if (Char == '(') {
			Kind = tok::DotLParentheses;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		}	else {
			Kind = tok::Dot;  // FIXME or should be tok::SingleQuote ?
		}
		break;
	case '&':
		Char = getCharAndSize(CurPtr, SizeTmp);
		if (Char == '&') {
			Kind = tok::ShortCircuitAND;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else if (Char == '=') {
			Kind = tok::AndAssign;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else {
			Kind = tok::EleWiseAND;
		}
		break;
	case '*':
		// TODO Yabin we should deal with the asterisk usage.
		Char = getCharAndSize(CurPtr, SizeTmp);
		if(Char == '=') {
			Kind = tok::MatMulAssign;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else {
			Kind = tok::Asterisk;
		}
		break;
	case '+':
		// TODO HYB we need to handle tok::UnaryPlus or
		// as PackageFolder
		Char = getCharAndSize(CurPtr, SizeTmp);
		if(Char == '=') {
			Kind = tok::AddAssign;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else if(Char == '+') {
			Kind = tok::DoubleAdd;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else {
			Kind = tok::Addition;
		}
		break;
	case '-':
		// TODO HYB we need to handle tok::UnaryMinus
		Char = getCharAndSize(CurPtr, SizeTmp);
		if(Char == '=') {
			Kind = tok::SubAssign;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else if(Char == '-') {
			Kind = tok::DoubleSub;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else {
			Kind = tok::Subtraction;
		}
		break;
	case '~':
		Kind = tok::Tilde;
		break;
	case '!':
		// TODO HYB we prepare to launch an external command
		Kind = tok::ExclamationPoint;
		break;
	case '/':
		// TODO HYB deal with slash (backslash) in a folder/path
		// string literal
		Char = getCharAndSize(CurPtr, SizeTmp);
		if(Char == '=') {
			Kind = tok::MatRDivAssign;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else {
			Kind = tok::Slash;
		}
		break;
	case '%':
		Char = getCharAndSize(CurPtr, SizeTmp);
		if (Char == '{') { // %{ comment
			if (SkipBlockComment(Result, ConsumeChar(CurPtr, SizeTmp, Result)))
				return; // There is a token to return.
			goto LexNextToken;   // GCC isn't tail call eliminating.
		} else { // % comment
			if (Features.BCPLComment) {
				if (SkipBCPLComment(Result, ConsumeChar(CurPtr, SizeTmp, Result)))
					return; // There is a token to return.

				// It is common for the tokens immediately after a // comment to be
				// whitespace (indentation for the next line).  Instead of going through
				// the big switch, handle it efficiently now.
				goto SkipIgnoredUnits;
			} else {
				Kind = tok::Percent;
			}
		}
		break;
	case '<':
		Char = getCharAndSize(CurPtr, SizeTmp);
		if (Char == '=') {
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
			Kind = tok::LessThanOREqualTo;
		} else {
			Kind = tok::LessThan;
		}
		break;
	case '>':
		Char = getCharAndSize(CurPtr, SizeTmp);
		if (Char == '=') {
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
			Kind = tok::GreaterThanOREqualTo;
		} else {
			Kind = tok::GreaterThan;
		}
		break;
	case '^':
		Char = getCharAndSize(CurPtr, SizeTmp);
		if (Char == '=') {
			Kind = tok::MatPowerAssign;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else {
			Kind = tok::Caret;
		}
		break;
	case '|':
		Char = getCharAndSize(CurPtr, SizeTmp);
		if (Char == '|') {
			Kind = tok::ShortCircuitOR;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else if(Char == '=') {
			Kind = tok::OrAssign;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else {
			Kind = tok::EleWiseOR;
		}
		break;
	case ':':
		Kind = tok::Colon;
		break;
	case ';':
		Kind = tok::Semicolon;
		break;
	case '=':
		Char = getCharAndSize(CurPtr, SizeTmp);
		if (Char == '=') {
			Kind = tok::EqualTo;
			CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
		} else {
			Kind = tok::Assignment;
		}
		break;
	case ',':
		Kind = tok::Comma;
		break;
	case '#': // no this punctuator
		Kind = tok::Unknown;
		break;
	case '@':
		Kind = tok::At;
		break;
	case '\\':
		//TODO HYB deal with backslash in a folder/path
		// string literal
		Kind = tok::BackSlash;
		break;
	default:
		Kind = tok::Unknown;
		break;
	}

	// Update the location of token as well as BufferPtr.
	FormTokenWithChars(Result, CurPtr, Kind);
}

