//===--- Preprocessor.h - M Language Preprocessor ---------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Preprocessor interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_LEX_PREPROCESSOR_H_
#define MLANG_LEX_PREPROCESSOR_H_

#include "mlang/Lex/Lexer.h"
#include "mlang/Lex/PPCallbacks.h"
#include "mlang/Lex/Token.h"
#include "mlang/Basic/Builtins.h"
#include "mlang/Basic/IdentifierTable.h"
#include "mlang/Basic/SourceLocation.h"
#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Allocator.h"
#include <vector>

namespace mlang {
class SourceManager;
class FileEntry;
class FileManager;
class ImportSearch;
class LangOptions;
class CommentHandler;
class ScratchBuffer;
class TargetInfo;
class PreprocessorLexer;
class PPCallbacks;
class DirectoryLookup;

/// Preprocessor - This object engages in a tight little dance with the lexer to
/// efficiently preprocess tokens.  Lexers know only about tokens within a
/// single source file, and don't know anything about preprocessor-level issues
/// like the #include stack, token expansion, etc.
///
/// Mainly deal with various comments in source files. And prepare related source
/// for lexer. Currently we don't support pragma like directives.
class Preprocessor : public llvm::RefCountedBase<Preprocessor> {
  DiagnosticsEngine *Diags;
  const LangOptions &Features;
  const TargetInfo &Target;
  FileManager &FileMgr;
  SourceManager &SourceMgr;
  ScratchBuffer *ScratchBuf;
  ImportSearch &ImportInfo;

  /// BP - A BumpPtrAllocator object used to quickly allocate and release
  ///  objects internal to the Preprocessor.
  llvm::BumpPtrAllocator BP;

  /// Identifiers for builtin macros and other builtins.
  IdentifierInfo *Ident__LINE__, *Ident__FILE__; // __LINE__, __FILE__
  IdentifierInfo *Ident__DATE__, *Ident__TIME__; // __DATE__, __TIME__
  IdentifierInfo *Ident__INCLUDE_LEVEL__; // __INCLUDE_LEVEL__
  IdentifierInfo *Ident__BASE_FILE__; // __BASE_FILE__
  IdentifierInfo *Ident__TIMESTAMP__; // __TIMESTAMP__
  IdentifierInfo *Ident__COUNTER__; // __COUNTER__
  IdentifierInfo *Ident_Pragma, *Ident__pragma; // _Pragma, __pragma
  IdentifierInfo *Ident__VA_ARGS__; // __VA_ARGS__
	IdentifierInfo *Ident__has_feature; // __has_feature
	IdentifierInfo *Ident__has_builtin; // __has_builtin
	IdentifierInfo *Ident__has_attribute; // __has_attribute

	SourceLocation DATELoc, TIMELoc;
	unsigned CounterValue; // Next __COUNTER__ value.

	// State that is set before the preprocessor begins.
	bool KeepComments :1;

	/// Identifiers - This is mapping/lookup information for all identifiers in
	/// the program, including program keywords.
	mutable IdentifierTable Identifiers;

	/// BuiltinInfo - Information about builtins.
	Builtin::Context BuiltinInfo;

	/// \brief Tracks all of the comment handlers that the client registered
	/// with this preprocessor.
	std::vector<CommentHandler *> CommentHandlers;

	/// \brief The number of bytes that we will initially skip when entering the
	/// main file, which is used when loading a precompiled preamble, along
	/// with a flag that indicates whether skipping this number of bytes will
	/// place the lexer at the start of a line.
	std::pair<unsigned, bool> SkipMainFilePreamble;

	/// CurLexer - This is the current top of the stack that we're lexing from if
	/// not expanding a macro and we are lexing directly from source code.
	///  Only one of CurLexer, CurPTHLexer, or CurTokenLexer will be non-null.
	llvm::OwningPtr<Lexer> CurLexer;

	/// CurPPLexer - This is the current top of the stack what we're lexing from
	///  if not expanding a macro.  This is an alias for either CurLexer or
	///  CurPTHLexer.
	PreprocessorLexer *CurPPLexer;

	/// CurLookup - The DirectoryLookup structure used to find the current
	/// FileEntry, if CurLexer is non-null and if applicable.  This allows us to
	/// implement #include_next and find directory-specific properties.
	const DirectoryLookup *CurDirLookup;

	/// Callbacks - These are actions invoked when some preprocessor activity is
	/// encountered (e.g. a file is #included, etc).
	PPCallbacks *Callbacks;

	// Various statistics we track for performance analysis.
	unsigned NumEnteredSourceFiles;
	unsigned NumSkipped;

	/// Predefines - This string is the predefined macros that preprocessor
	/// should use from the command line etc.
	std::string Predefines;

private:
	// Cached tokens state.
	typedef llvm::SmallVector<Token, 1> CachedTokensTy;

	/// CachedTokens - Cached tokens are stored here when we do backtracking or
	/// lookahead. They are "lexed" by the CachingLex() method.
	CachedTokensTy CachedTokens;

	/// CachedLexPos - The position of the cached token that CachingLex() should
	/// "lex" next. If it points beyond the CachedTokens vector, it means that
	/// a normal Lex() should be invoked.
	CachedTokensTy::size_type CachedLexPos;

	/// BacktrackPositions - Stack of backtrack positions, allowing nested
	/// backtracks. The EnableBacktrackAtThisPos() method pushes a position to
	/// indicate where  should be set when the BackTrack() method is
	/// invoked (at which point the last position is popped).
	std::vector<CachedTokensTy::size_type> BacktrackPositions;

public:
  Preprocessor(DiagnosticsEngine &diags, const LangOptions &opts,
               const TargetInfo &target, SourceManager &SM,
               ImportSearch &Imports, IdentifierInfoLookup *IILookup = 0);

  ~Preprocessor();

  DiagnosticsEngine &getDiagnostics() const {
    return *Diags;
  }

  void setDiagnostics(DiagnosticsEngine &D) {
    Diags = &D;
  }

  const LangOptions &getLangOptions() const {
    return Features;
  }
  const TargetInfo &getTargetInfo() const {
    return Target;
  }
  FileManager &getFileManager() const {
    return FileMgr;
  }
  SourceManager &getSourceManager() const {
    return SourceMgr;
  }
  ImportSearch &getImportSearchInfo() const {
    return ImportInfo;
  }
  IdentifierTable &getIdentifierTable() {
    return Identifiers;
  }
  Builtin::Context &getBuiltinInfo() {
    return BuiltinInfo;
  }
  llvm::BumpPtrAllocator &getPreprocessorAllocator() {
    return BP;
  }
  /// SetCommentRetentionState - Control whether or not the preprocessor retains
  /// comments in output.
  void SetCommentRetentionState(bool KeepComments) {
    this->KeepComments = KeepComments;
  }

  bool getCommentRetentionState() const {
    return KeepComments;
  }

  /// isCurrentLexer - Return true if we are lexing directly from the specified
  /// lexer.
  bool isCurrentLexer(const PreprocessorLexer *L) const {
		//return CurLexer == L;
		return true;
	}

	/// getCurrentLexer - Return the current lexer being lexed from.  Note
	/// that this ignores any potentially active macro expansions and _Pragma
	/// expansions going on at the time.
	PreprocessorLexer *getCurrentLexer() const {
		return CurPPLexer;
	}

	/// getCurrentFileLexer - Return the current file lexer being lexed from.
	/// Note that this ignores any potentially active macro expansions and _Pragma
	/// expansions going on at the time.
	PreprocessorLexer *getCurrentFileLexer() const;

	/// getPPCallbacks/addPPCallbacks - Accessors for preprocessor callbacks.
	/// Note that this class takes ownership of any PPCallbacks object given to
	/// it.
	PPCallbacks *getPPCallbacks() const {
		return Callbacks;
	}
	void addPPCallbacks(PPCallbacks *C) {
		if (Callbacks)
			C = new PPChainedCallbacks(C, Callbacks);
		Callbacks = C;
	}

	const std::string &getPredefines() const {
		return Predefines;
	}
	/// setPredefines - Set the predefines for this Preprocessor.  These
	/// predefines are automatically injected when parsing the main file.
	void setPredefines(const char *P) {
		Predefines = P;
	}
	void setPredefines(const std::string &P) {
		Predefines = P;
	}

	/// getIdentifierInfo - Return information about the specified preprocessor
	/// identifier token.  The version of this method that takes two character
	/// pointers is preferred unless the identifier is already available as a
	/// string (this avoids allocation and copying of memory to construct an
	/// std::string).
	IdentifierInfo *getIdentifierInfo(llvm::StringRef Name) const {
		return &Identifiers.get(Name);
	}

	/// \brief Add the specified comment handler to the preprocessor.
	void AddCommentHandler(CommentHandler *Handler);

	/// \brief Remove the specified comment handler.
	///
	/// It is an error to remove a handler that has not been registered.
	void RemoveCommentHandler(CommentHandler *Handler);

	/// EnterMainSourceFile - Enter the specified FileID as the main source file,
	/// which implicitly adds the builtin defines etc.
	void EnterMainSourceFile();

	/// EndSourceFile - Inform the preprocessor callbacks that processing is
	/// complete.
	void EndSourceFile();

	/// EnterSourceFile - Add a source file to the top of the include stack and
	/// start lexing tokens from it instead of the current buffer.  Emit an error
	/// and don't enter the file on error.
	void EnterSourceFile(FileID CurFileID, const DirectoryLookup *Dir,
			SourceLocation Loc);

	/// RemoveTopOfLexerStack - Pop the current lexer/macro exp off the top of the
	/// lexer stack.  This should only be used in situations where the current
	/// state of the top-of-stack lexer is known.
	void RemoveTopOfLexerStack();

	/// EnableBacktrackAtThisPos - From the point that this method is called, and
	/// until CommitBacktrackedTokens() or Backtrack() is called, the Preprocessor
	/// keeps track of the lexed tokens so that a subsequent Backtrack() call will
	/// make the Preprocessor re-lex the same tokens.
	///
	/// Nested backtracks are allowed, meaning that EnableBacktrackAtThisPos can
	/// be called multiple times and CommitBacktrackedTokens/Backtrack calls will
	/// be combined with the EnableBacktrackAtThisPos calls in reverse order.
	///
	/// NOTE: *DO NOT* forget to call either CommitBacktrackedTokens or Backtrack
	/// at some point after EnableBacktrackAtThisPos. If you don't, caching of
	/// tokens will continue indefinitely.
	///
	void EnableBacktrackAtThisPos();

	/// CommitBacktrackedTokens - Disable the last EnableBacktrackAtThisPos call.
	void CommitBacktrackedTokens();

	/// Backtrack - Make Preprocessor re-lex the tokens that were lexed since
	/// EnableBacktrackAtThisPos() was previously called.
	void Backtrack();

	/// isBacktrackEnabled - True if EnableBacktrackAtThisPos() was called and
	/// caching of tokens is on.
	bool isBacktrackEnabled() const {
		return !BacktrackPositions.empty();
	}

	/// Lex - To lex a token from the preprocessor, just pull a token from the
	/// current lexer or macro object.
	void Lex(Token &Result) {
		if (CurLexer)
			CurLexer->Lex(Result);
//		else
//			CachingLex(Result);
	}

	/// LexNonComment - Lex a token.  If it's a comment, keep lexing until we get
	/// something not a comment.  This is useful in -E -C mode where comments
	/// would foul up preprocessor directive handling.
	void LexNonComment(Token &Result) {
		do
			Lex(Result);
		while (Result.getKind() == tok::Comment);
	}

	/// LookAhead - This peeks ahead N tokens and returns that token without
	/// consuming any tokens.  LookAhead(0) returns the next token that would be
	/// returned by Lex(), LookAhead(1) returns the token after it, etc.  This
	/// returns normal tokens after phase 5.  As such, it is equivalent to using
	/// 'Lex', not 'LexUnexpandedToken'.
	const Token &LookAhead(unsigned N) {
		if (CachedLexPos + N < CachedTokens.size())
			return CachedTokens[CachedLexPos + N];
		else
			return PeekAhead(N + 1);
	}

	/// RevertCachedTokens - When backtracking is enabled and tokens are cached,
	/// this allows to revert a specific number of tokens.
	/// Note that the number of tokens being reverted should be up to the last
	/// backtrack position, not more.
	void RevertCachedTokens(unsigned N) {
		assert(isBacktrackEnabled() &&
				"Should only be called when tokens are cached for backtracking");
		assert(signed(CachedLexPos) - signed(N) >= signed(BacktrackPositions.back())
				&& "Should revert tokens up to the last backtrack position, not more");
		assert(signed(CachedLexPos) - signed(N) >= 0 &&
				"Corrupted backtrack positions ?");
		CachedLexPos -= N;
	}

	/// EnterToken - Enters a token in the token stream to be lexed next. If
	/// BackTrack() is called afterwards, the token will remain at the insertion
	/// point.
	void EnterToken(const Token &Tok) {
		//EnterCachingLexMode();
		CachedTokens.insert(CachedTokens.begin() + CachedLexPos, Tok);
	}

	/// AnnotateCachedTokens - We notify the Preprocessor that if it is caching
	/// tokens (because backtrack is enabled) it should replace the most recent
	/// cached tokens with the given annotation token. This function has no effect
	/// if backtracking is not enabled.
	///
	/// Note that the use of this function is just for optimization; so that the
	/// cached tokens doesn't get re-parsed and re-resolved after a backtrack is
	/// invoked.
	void AnnotateCachedTokens(const Token &Tok) {
		assert(Tok.isAnnotation() && "Expected annotation token");
		if (CachedLexPos != 0 && isBacktrackEnabled())
			AnnotatePreviousCachedTokens(Tok);
	}

	/// \brief Replace the last token with an annotation token.
	///
	/// Like AnnotateCachedTokens(), this routine replaces an
	/// already-parsed (and resolved) token with an annotation
	/// token. However, this routine only replaces the last token with
	/// the annotation token; it does not affect any other cached
	/// tokens. This function has no effect if backtracking is not
	/// enabled.
	void ReplaceLastTokenWithAnnotation(const Token &Tok) {
		assert(Tok.isAnnotation() && "Expected annotation token");
		if (CachedLexPos != 0 && isBacktrackEnabled())
			CachedTokens[CachedLexPos - 1] = Tok;
	}

	/// \brief Instruct the preprocessor to skip part of the main
	/// the main source file.
	///
	/// \brief Bytes The number of bytes in the preamble to skip.
	///
	/// \brief StartOfLine Whether skipping these bytes puts the lexer at the
	/// start of a line.
	void setSkipMainFilePreamble(unsigned Bytes, bool StartOfLine) {
		SkipMainFilePreamble.first = Bytes;
		SkipMainFilePreamble.second = StartOfLine;
	}

	/// Diag - Forwarding function for diagnostics.  This emits a diagnostic at
	/// the specified Token's location, translating the token's start
	/// position in the current buffer into a SourcePosition object for rendering.
	DiagnosticBuilder Diag(SourceLocation Loc, unsigned DiagID) {
		return Diags->Report(Loc, DiagID);
	}

	DiagnosticBuilder Diag(const Token &Tok, unsigned DiagID) {
		return Diags->Report(Tok.getLocation(), DiagID);
	}

	/// getSpelling() - Return the 'spelling' of the Tok token.  The spelling of a
	/// token is the characters used to represent the token in the source file
	/// after trigraph expansion and escaped-newline folding.  In particular, this
	/// wants to get the true, uncanonicalized, spelling of things like digraphs
	/// UCNs, etc.
	///
	/// \param Invalid If non-NULL, will be set \c true if an error occurs.
	std::string getSpelling(const Token &Tok, bool *Invalid = 0) const {
		return Lexer::getSpelling(Tok, SourceMgr, Features, Invalid);
	}

	/// getSpelling - This method is used to get the spelling of a token into a
	/// preallocated buffer, instead of as an std::string.  The caller is required
	/// to allocate enough space for the token, which is guaranteed to be at least
	/// Tok.getLength() bytes long.  The length of the actual result is returned.
	///
	/// Note that this method may do two possible things: it may either fill in
	/// the buffer specified with characters, or it may *change the input pointer*
	/// to point to a constant buffer with the data already in it (avoiding a
	/// copy).  The caller is not allowed to modify the returned buffer pointer
	/// if an internal buffer is returned.
	unsigned getSpelling(const Token &Tok, const char *&Buffer, bool *Invalid =
			0) const {
		return Lexer::getSpelling(Tok, Buffer, SourceMgr, Features, Invalid);
	}

	/// getSpelling - This method is used to get the spelling of a token into a
	/// SmallVector. Note that the returned StringRef may not point to the
	/// supplied buffer if a copy can be avoided.
	llvm::StringRef getSpelling(const Token &Tok,
			llvm::SmallVectorImpl<char> &Buffer, bool *Invalid = 0) const;

	/// getSpellingOfSingleCharacterNumericConstant - Tok is a numeric constant
	/// with length 1, return the character.
	char getSpellingOfSingleCharacterNumericConstant(const Token &Tok,
			bool *Invalid = 0) const {
		assert(Tok.is(tok::NumericConstant) &&
				Tok.getLength() == 1 && "Called on unsupported token");
		assert(!Tok.needsCleaning() && "Token can't need cleaning with length 1");

		// If the token is carrying a literal data pointer, just use it.
		if (const char *D = Tok.getLiteralData())
			return *D;

		// Otherwise, fall back on getCharacterData, which is slower, but always
		// works.
		return *SourceMgr.getCharacterData(Tok.getLocation(), Invalid);
	}

	/// CreateString - Plop the specified string into a scratch buffer and set the
	/// specified token's location and length to it.  If specified, the source
	/// location provides a location of the instantiation point of the token.
	void CreateString(const char *Buf, unsigned Len, Token &Tok,
			SourceLocation SourceLoc = SourceLocation());

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
	SourceLocation getLocForEndOfToken(SourceLocation Loc, unsigned Offset = 0) {
		return Lexer::getLocForEndOfToken(Loc, Offset, SourceMgr, Features);
	}

	/// DumpToken - Print the token to stderr, used for debugging.
	///
	void DumpToken(const Token &Tok, bool DumpFlags = false) const;
	void DumpLocation(SourceLocation Loc) const;

	/// AdvanceToTokenCharacter - Given a location that specifies the start of a
	/// token, return a new location that specifies a character within the token.
	SourceLocation AdvanceToTokenCharacter(SourceLocation TokStart,
			unsigned Char) const {
		return Lexer::AdvanceToTokenCharacter(TokStart, Char, SourceMgr,
				Features);
	}

	void PrintStats();

	//===--------------------------------------------------------------------===//
	// Preprocessor callback methods.  These are invoked by a lexer as various
	// directives and events are found.

	/// LookUpIdentifierInfo - Given a tok::identifier token, look up the
	/// identifier information for the token and install it into the token.
	IdentifierInfo *LookUpIdentifierInfo(Token &Identifier) const;

	/// HandleIdentifier - This callback is invoked when the lexer reads an
	/// identifier and has filled in the tokens IdentifierInfo member.  This
	/// callback potentially macro expands it or turns it into a named token (like
	/// 'for').
	void HandleIdentifier(Token &Identifier);

	/// HandleEndOfFile - This callback is invoked when the lexer hits the end of
	/// the current file.  This either returns the EOF token and returns true, or
	/// pops a level off the include stack and returns false, at which point the
	/// client should call lex again.
	bool HandleEndOfFile(Token &Result, bool isEndOfMacro = false);

	/// HandleEndOfTokenLexer - This callback is invoked when the current
	/// TokenLexer hits the end of its token stream.
	bool HandleEndOfTokenLexer(Token &Result);

	/// SawDateOrTime - This returns true if the preprocessor has seen a use of
	/// __DATE__ or __TIME__ in the file so far.
	bool SawDateOrTime() const {
		return DATELoc != SourceLocation() || TIMELoc != SourceLocation();
	}
	unsigned getCounterValue() const {
		return CounterValue;
	}
	void setCounterValue(unsigned V) {
		CounterValue = V;
	}

	/// LookupFile - Given a "foo" or <foo> reference, look up the indicated file,
	/// return null on failure.  isAngled indicates whether the file reference is
	/// for system #include's or not (i.e. using <> instead of "").
	const FileEntry *LookupFile(llvm::StringRef Filename, bool isAngled,
			const DirectoryLookup *FromDir, const DirectoryLookup *&CurDir);

	/// GetCurLookup - The DirectoryLookup structure used to find the current
	/// FileEntry, if CurLexer is non-null and if applicable.  This allows us to
	/// implement #include_next and find directory-specific properties.
	const DirectoryLookup *GetCurDirLookup() {
		return CurDirLookup;
	}

	/// isInPrimaryFile - Return true if we're in the top-level file, not in a
	/// #include.
	bool isInPrimaryFile() const;
private:

	/// isNextPPTokenLParen - Determine whether the next preprocessor token to be
	/// lexed is a '('.  If so, consume the token and return true, if not, this
	/// method should have no observable side-effect on the lexed tokens.
	bool isNextPPTokenLParen();

	/// EnterSourceFileWithLexer - Add a lexer to the top of the include stack and
	/// start lexing tokens from it instead of the current buffer.
	void EnterSourceFileWithLexer(Lexer *TheLexer, const DirectoryLookup *Dir);

	/// IsFileLexer - Returns true if we are lexing from a file and not a
	///  pragma or a macro.
	static bool IsFileLexer(const Lexer* L, const PreprocessorLexer* P) {
		// FIXME HYB   return L ? !L->isCommandLineLexer() : P != 0;
		return false;
	}

	bool IsFileLexer() const {
		return IsFileLexer(CurLexer.get(), CurPPLexer);
	}

	//===--------------------------------------------------------------------===//
	// Caching stuff.
	void CachingLex(Token &Result);
	bool InCachingLexMode() const {
		// If the Lexer pointers are 0 and IncludeMacroStack is empty, it means
		// that we are past EOF, not that we are in CachingLex mode.
		return CurPPLexer == 0;
	}
	void EnterCachingLexMode();
	void ExitCachingLexMode() {
		if (InCachingLexMode())
			RemoveTopOfLexerStack();
	}
	const Token &PeekAhead(unsigned N);
	void AnnotatePreviousCachedTokens(const Token &Tok);

public:
	// Return true and store the first token only if any CommentHandler
	// has inserted some tokens and getCommentRetentionState() is false.
	bool HandleComment(Token &Token, SourceRange Comment);
};

/// \brief Abstract base class that describes a handler that will receive
/// source ranges for each of the comments encountered in the source file.
class CommentHandler {
public:
	virtual ~CommentHandler();

	// The handler shall return true if it has pushed any tokens
	// to be read using e.g. EnterToken or EnterTokenStream.
	virtual bool HandleComment(Preprocessor &PP, SourceRange Comment) = 0;
};
} // end namespace mlang

#endif /* MLANG_LEX_PREPROCESSOR_H_ */
