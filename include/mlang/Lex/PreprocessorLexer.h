//===--- PreprocessorLexer.h - Lexer for Mlang pre-processing ---*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the PreprocessorLexer interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_LEX_PREPROCESSOR_LEXER_H_
#define MLANG_LEX_PREPROCESSOR_LEXER_H_

#include "mlang/Lex/Token.h"
#include "llvm/ADT/SmallVector.h"
#include <string>

namespace mlang {
class FileEntry;
class FileID;
class Preprocessor;

class PreprocessorLexer {
protected:
	Preprocessor *PP; // Preprocessor object controlling lexing.

	/// The SourceManager FileID corresponding to the file being lexed.
	const FileID FID;

	//===--------------------------------------------------------------------===//
	// Context-specific lexing flags set by the preprocessor.
	//===--------------------------------------------------------------------===//
	/// ParsingPreprocessorDirective - This is true when parsing import command or
	/// pragma command. This turns '\n' into a tok::EoD token.
	bool ParsingPreprocessorDirective;

	/// ParsingFilename - True after import keyword
	bool ParsingFilename;

	/// LexingRawMode - True if in raw mode:  This flag disables interpretation of
	/// tokens and is a far faster mode to lex in than non-raw-mode.  This flag:
	///  1. If EOF of the current lexer is found, the include stack isn't popped.
	///  2. Identifier information is not looked up for identifier tokens.  As an
	///     effect of this, implicit macro expansion is naturally disabled.
	///  3. "#" tokens at the start of a line are treated as normal tokens, not
	///     implicitly transformed by the lexer.
	///  4. All diagnostic messages are disabled.
	///  5. No callbacks are made into the preprocessor.
	///
	/// Note that in raw mode that the PP pointer may be null.
	/// In mlang, we use raw mode by default.
	bool LexingRawMode;

	PreprocessorLexer(const PreprocessorLexer&); // DO NOT IMPLEMENT
	void operator=(const PreprocessorLexer&); // DO NOT IMPLEMENT
	friend class Preprocessor;

	/// when we need to use preprocessor, we
	/// set raw mode to false.
	PreprocessorLexer(Preprocessor *pp, FileID fid) :
		PP(pp), FID(fid), ParsingPreprocessorDirective(false),
	      ParsingFilename(false), LexingRawMode(false) {
	}

	/// we use raw mode by default
	PreprocessorLexer() :
		PP(0), ParsingPreprocessorDirective(false),
	      ParsingFilename(false), LexingRawMode(true) {
	}

	virtual ~PreprocessorLexer() {
	}

	virtual void IndirectLex(Token& Result) = 0;

	/// getSourceLocation - Return the source location for the next observable
	///  location.
	virtual SourceLocation getSourceLocation() = 0;

public:
	//===--------------------------------------------------------------------===//
	// Misc. lexing methods.

	/// LexImportFilename - After the preprocessor has parsed an import, lex
	/// the filename.  If the sequence parsed is not lexically legal, emit
	/// a diagnostic and return a result tok::EoD token.
	void LexImportFilename(Token &Result);

	/// setParsingPreprocessorDirective - Inform the lexer whether or not
	///  we are currently lexing a preprocessor directive.
	void setParsingPreprocessorDirective(bool f) {
		ParsingPreprocessorDirective = f;
	}

	/// isLexingRawMode - Return true if this lexer is in raw mode or not.
	bool isLexingRawMode() const {
		return LexingRawMode;
	}

	/// getPP - Return the preprocessor object for this lexer.
	Preprocessor *getPP() const {
		return PP;
	}

	FileID getFileID() const {
		assert(PP
				&& "PreprocessorLexer::getFileID() should only be used with a Preprocessor");
		return FID;
	}

	/// getFileEntry - Return the FileEntry corresponding to this FileID.  Like
	/// getFileID(), this only works for lexers with attached preprocessors.
	const FileEntry *getFileEntry() const;
};
} // end namespace mlang

#endif /* MLANG_LEX_PREPROCESSOR_LEXER_H_ */
