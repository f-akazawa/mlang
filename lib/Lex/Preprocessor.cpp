//===--- Preprocessor.cpp - Mlang Preprocessor Implementation ---*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Preprocessor interface.
//
//===----------------------------------------------------------------------===//

#include "mlang/Lex/Preprocessor.h"
#include "mlang/Lex/ScratchBuffer.h"
#include "mlang/Lex/LexDiagnostic.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Basic/TargetInfo.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlang;

Preprocessor::Preprocessor(DiagnosticsEngine &diags, const LangOptions &opts,
                           const TargetInfo &target, SourceManager &SM,
                           ImportSearch &Imports,
                           IdentifierInfoLookup* IILookup) :
    Diags(&diags), Features(opts), Target(target),
    FileMgr(SM.getFileManager()),
    SourceMgr(SM),
    ImportInfo(Imports),
    Identifiers(opts, IILookup),
    BuiltinInfo(),
    SkipMainFilePreamble(0, true),
    CurPPLexer(0),
    CurDirLookup(0),
    Callbacks(0) {
  ScratchBuf = new ScratchBuffer(SourceMgr);
  CounterValue = 0; // __COUNTER__ starts at 0.
  // Clear stats.
  NumSkipped = 0;
  NumEnteredSourceFiles = 0;

  // Default to reserve comments.
  KeepComments = true;
  CachedLexPos = 0;

  Predefines = "";
}

Preprocessor::~Preprocessor() {
  assert(BacktrackPositions.empty() && "EnableBacktrack/Backtrack imbalance!");

  // Delete the scratch buffer info.
  delete ScratchBuf;

  delete Callbacks;
}

void Preprocessor::DumpToken(const Token &Tok, bool DumpFlags) const {
  llvm::errs() << tok::getTokenName(Tok.getKind()) << " '"
               << getSpelling(Tok) << "'";

  if (!DumpFlags)
    return;

  llvm::errs() << "\t";
  if (Tok.isAtStartOfLine())
    llvm::errs() << " [StartOfLine]";
  if (Tok.hasLeadingSpace())
    llvm::errs() << " [LeadingSpace]";
  if (Tok.isExpandDisabled())
    llvm::errs() << " [ExpandDisabled]";
  if (Tok.needsCleaning()) {
    const char *Start = SourceMgr.getCharacterData(Tok.getLocation());
    llvm::errs() << " [UnClean='"
                 << llvm::StringRef(Start, Tok.getLength()) << "']";
  }

  llvm::errs() << "\tLoc=<";
  DumpLocation(Tok.getLocation());
  llvm::errs() << ">";
}

void Preprocessor::DumpLocation(SourceLocation Loc) const {
  Loc.dump(SourceMgr);
}

void Preprocessor::PrintStats() {
  llvm::errs() << "\n*** Preprocessor Stats:\n";
  llvm::errs() << "    " << NumEnteredSourceFiles
               << " source files entered.\n";
  llvm::errs() << NumSkipped << " #if/#ifndef#ifdef regions skipped\n";
}

/// getSpelling - This method is used to get the spelling of a token into a
/// SmallVector. Note that the returned StringRef may not point to the
/// supplied buffer if a copy can be avoided.
llvm::StringRef Preprocessor::getSpelling(const Token &Tok,
		llvm::SmallVectorImpl<char> &Buffer, bool *Invalid) const {
  // Try the fast path.
  if (const IdentifierInfo *II = Tok.getIdentifierInfo())
    return II->getName();

  // Resize the buffer if we need to copy into it.
  if (Tok.needsCleaning())
    Buffer.resize(Tok.getLength());

  const char *Ptr = Buffer.data();
  unsigned Len = getSpelling(Tok, Ptr, Invalid);
  return llvm::StringRef(Ptr, Len);
}

/// CreateString - Plop the specified string into a scratch buffer and return a
/// location for it.  If specified, the source location provides a source
/// location for the token.
void Preprocessor::CreateString(const char *Buf, unsigned Len, Token &Tok,
                                SourceLocation InstantiationLoc) {
  Tok.setLength(Len);

  const char *DestPtr;
  SourceLocation Loc = ScratchBuf->getToken(Buf, Len, DestPtr);

  if (InstantiationLoc.isValid())
    Loc = SourceMgr.createExpansionLoc(Loc, InstantiationLoc,
                                       InstantiationLoc, Len);
  Tok.setLocation(Loc);

  // If this is a literal token, set the pointer data.
  if (Tok.isLiteral())
    Tok.setLiteralData(DestPtr);
}

//===----------------------------------------------------------------------===//
// Preprocessor Initialization Methods
//===----------------------------------------------------------------------===//


/// EnterMainSourceFile - Enter the specified FileID as the main source file,
/// which implicitly adds the builtin defines etc.
void Preprocessor::EnterMainSourceFile() {
  // We do not allow the preprocessor to reenter the main file.  Doing so will
  // cause FileID's to accumulate information from both runs (e.g. #line
  // information) and predefined macros aren't guaranteed to be set properly.
  assert(NumEnteredSourceFiles == 0 && "Cannot reenter the main file!");
  FileID MainFileID = SourceMgr.getMainFileID();

  // Enter the main file source buffer.
  EnterSourceFile(MainFileID, 0, SourceLocation());

  // If we've been asked to skip bytes in the main file (e.g., as part of a
  // precompiled preamble), do so now.
  if (SkipMainFilePreamble.first > 0)
    CurLexer->SkipBytes(SkipMainFilePreamble.first,
                        SkipMainFilePreamble.second);

  // Preprocess Predefines to populate the initial preprocessor state.
  //llvm::MemoryBuffer *SB = llvm::MemoryBuffer::getMemBufferCopy(Predefines,
  //			"<built-in>");
//	assert(SB && "Cannot create predefined source buffer");
//	FileID FID = SourceMgr.createFileIDForMemBuffer(SB);
//	assert(!FID.isInvalid() && "Could not create FileID for predefines?");
//
//	// Start parsing the predefines.
//	EnterSourceFile(FID, 0, SourceLocation());
}

void Preprocessor::EndSourceFile() {
  // Notify the client that we reached the end of the source file.
  if (Callbacks)
    Callbacks->EndOfMainFile();
}

//===----------------------------------------------------------------------===//
// Lexer Event Handling.
//===----------------------------------------------------------------------===//

/// LookUpIdentifierInfo - Given a tok::identifier token, look up the
/// identifier information for the token and install it into the token.
IdentifierInfo *Preprocessor::LookUpIdentifierInfo(Token &Identifier) const {
  assert(Identifier.getRawIdentifierData() != 0 && "No raw identifier data!");

  // Look up this token, see if it is a macro, or if it is a language keyword.
  IdentifierInfo *II;
  if (!Identifier.needsCleaning()) {
    // No cleaning needed, just use the characters from the lexed buffer.
    II = getIdentifierInfo(llvm::StringRef(
        Identifier.getRawIdentifierData(), Identifier.getLength()));
  } else {
    // Cleaning needed, alloca a buffer, clean into it, then use the buffer.
    llvm::SmallString<64> IdentifierBuffer;
    llvm::StringRef CleanedStr = getSpelling(Identifier, IdentifierBuffer);
    II = getIdentifierInfo(CleanedStr);
  }

  // Update the token info (identifier info and appropriate token kind).
  Identifier.setIdentifierInfo(II);
  Identifier.setKind(II->getTokenID());

  return II;
}

/// HandleIdentifier - This callback is invoked when the lexer reads an
/// identifier.  This callback looks up the identifier in the map and/or
/// potentially macro expands it or turns it into a named token (like 'for').
///
/// Note that callers of this method are guarded by checking the
/// IdentifierInfo's 'isHandleIdentifierCase' bit.  If this method changes, the
/// IdentifierInfo methods that compute these properties will need to change to
/// match.
void Preprocessor::HandleIdentifier(Token &Identifier) {
	assert(Identifier.getIdentifierInfo()
			&& "Can't handle identifiers without identifier info!");

	IdentifierInfo &II = *Identifier.getIdentifierInfo();

	// If this is an alternative representation of a Mlang operator,
	// then we act as if it is the actual operator and not the textual
	// representation of it.
	if (II.isMlangOperatorKeyword())
		Identifier.setIdentifierInfo(0);
}

void Preprocessor::AddCommentHandler(CommentHandler *Handler) {
	assert(Handler && "NULL comment handler");
	assert(std::find(CommentHandlers.begin(), CommentHandlers.end(), Handler)
			== CommentHandlers.end() && "Comment handler already registered");
	CommentHandlers.push_back(Handler);
}

void Preprocessor::RemoveCommentHandler(CommentHandler *Handler) {
	std::vector<CommentHandler *>::iterator Pos = std::find(
			CommentHandlers.begin(), CommentHandlers.end(), Handler);
	assert(Pos != CommentHandlers.end() && "Comment handler not registered");
	CommentHandlers.erase(Pos);
}

bool Preprocessor::HandleComment(Token &result, SourceRange Comment) {
	bool AnyPendingTokens = false;
	for (std::vector<CommentHandler *>::iterator H = CommentHandlers.begin(),
			HEnd = CommentHandlers.end(); H != HEnd; ++H) {
		if ((*H)->HandleComment(*this, Comment))
			AnyPendingTokens = true;
	}
	if (!AnyPendingTokens || getCommentRetentionState())
		return false;
	Lex(result);
	return true;
}

//===----------------------------------------------------------------------===//
// Methods for Entering and Callbacks for leaving various contexts
//===----------------------------------------------------------------------===//

/// EnterSourceFile - Add a source file to the top of the include stack and
/// start lexing tokens from it instead of the current buffer.
void Preprocessor::EnterSourceFile(FileID FID, const DirectoryLookup *CurDir,
                                   SourceLocation Loc) {
  ++NumEnteredSourceFiles;

  // Get the MemoryBuffer for this FID, if it fails, we fail.
  bool Invalid = false;
  const llvm::MemoryBuffer *InputFile =
    getSourceManager().getBuffer(FID, Loc, &Invalid);
  if (Invalid) {
    SourceLocation FileStart = SourceMgr.getLocForStartOfFile(FID);
    Diag(Loc, diag::err_pp_error_opening_file)
      << std::string(SourceMgr.getBufferName(FileStart)) << "";
    return;
  }

  EnterSourceFileWithLexer(new Lexer(FID, InputFile, *this), CurDir);
  return;
}

/// EnterSourceFileWithLexer - Add a source file to the top of the include stack
///  and start lexing tokens from it instead of the current buffer.
void Preprocessor::EnterSourceFileWithLexer(Lexer *TheLexer,
                                            const DirectoryLookup *CurDir) {
  CurLexer.reset(TheLexer);
  CurPPLexer = TheLexer;
  CurDirLookup = CurDir;

  // Notify the client, if desired, that we are in a new source file.
  if (Callbacks) {
    SrcMgr::CharacteristicKind FileType =
       SourceMgr.getFileCharacteristic(CurLexer->getFileLoc());

    Callbacks->FileChanged(CurLexer->getFileLoc(),
                           PPCallbacks::EnterFile, FileType);
  }
}

/// HandleEndOfFile - This callback is invoked when the lexer hits the end of
/// the current file.  This either returns the EOF token or pops a level off
/// the include stack and keeps going.
bool Preprocessor::HandleEndOfFile(Token &Result, bool isEndOfMacro) {
 // assert(!CurTokenLexer &&
 //        "Ending a file when currently in a macro!");

  // See if this file had a controlling macro.
  if (CurPPLexer) {  // Not ending a macro, ignore it.
#if 0
    if (const IdentifierInfo *ControllingMacro =
          CurPPLexer->MIOpt.GetControllingMacroAtEndOfFile()) {
      // Okay, this has a controlling macro, remember in ImportedFileInfo.
      if (const FileEntry *FE =
            SourceMgr.getFileEntryForID(CurPPLexer->getFileID()))
        HeaderInfo.SetFileControllingMacro(FE, ControllingMacro);
    }
#endif
    //FIXME return true;
  }

#if 0
  // If this is a #include'd file, pop it off the include stack and continue
  // lexing the #includer file.
  if (!IncludeMacroStack.empty()) {
    // We're done with the #included file.
    RemoveTopOfLexerStack();

    // Notify the client, if desired, that we are in a new source file.
    if (Callbacks && !isEndOfMacro && CurPPLexer) {
      SrcMgr::CharacteristicKind FileType =
        SourceMgr.getFileCharacteristic(CurPPLexer->getSourceLocation());
      Callbacks->FileChanged(CurPPLexer->getSourceLocation(),
                             PPCallbacks::ExitFile, FileType);
    }

    // Client should lex another token.
    return false;
  }
#endif

  // If the file ends with a newline, form the EOF token on the newline itself,
  // rather than "on the line following it", which doesn't exist.  This makes
  // diagnostics relating to the end of file include the last file that the user
  // actually typed, which is goodness.
  if (CurLexer) {
    const char *EndPos = CurLexer->BufferEnd;
    if (EndPos != CurLexer->BufferStart &&
        (EndPos[-1] == '\n' || EndPos[-1] == '\r')) {
      --EndPos;

      // Handle \n\r and \r\n:
      if (EndPos != CurLexer->BufferStart &&
          (EndPos[-1] == '\n' || EndPos[-1] == '\r') &&
          EndPos[-1] != EndPos[0])
        --EndPos;
    }

    Result.startToken();
    CurLexer->BufferPtr = EndPos;
    CurLexer->FormTokenWithChars(Result, EndPos, tok::EoF);

    // We're done with the #included file.
    CurLexer.reset();
  } else {
#if 0
    assert(CurPTHLexer && "Got EOF but no current lexer set!");
    CurPTHLexer->getEOF(Result);
    CurPTHLexer.reset();
#endif
  }

  CurPPLexer = 0;

#if 0
  // This is the end of the top-level file.  If the diag::pp_macro_not_used
  // diagnostic is enabled, look for macros that have not been used.
  if (getDiagnostics().getDiagnosticLevel(diag::pp_macro_not_used) !=
        Diagnostic::Ignored) {

    for (macro_iterator I = macro_begin(false), E = macro_end(false);
         I != E; ++I)
      if (!I->second->isUsed())
        Diag(I->second->getDefinitionLoc(), diag::pp_macro_not_used);

  }
#endif

  return true;
}

/// HandleEndOfTokenLexer - This callback is invoked when the current TokenLexer
/// hits the end of its token stream.
bool Preprocessor::HandleEndOfTokenLexer(Token &Result) {
   // Handle this like a #include file being popped off the stack.
  return HandleEndOfFile(Result, true);
}

void Preprocessor::EnterCachingLexMode() {
  if (InCachingLexMode())
    return;

  CurPPLexer = 0;
}

/// RemoveTopOfLexerStack - Pop the current lexer/macro exp off the top of the
/// lexer stack.  This should only be used in situations where the current
/// state of the top-of-stack lexer is unknown.
void Preprocessor::RemoveTopOfLexerStack() {
//  assert(!IncludeMacroStack.empty() && "Ran out of stack entries to load");
//
//  if (CurTokenLexer) {
//    // Delete or cache the now-dead macro expander.
//    if (NumCachedTokenLexers == TokenLexerCacheSize)
//      CurTokenLexer.reset();
//    else
//      TokenLexerCache[NumCachedTokenLexers++] = CurTokenLexer.take();
//  }
//
//  PopIncludeMacroStack();
}

const Token &Preprocessor::PeekAhead(unsigned N) {
  assert(CachedLexPos + N > CachedTokens.size() && "Confused caching.");
  ExitCachingLexMode();
  for (unsigned C = CachedLexPos + N - CachedTokens.size(); C > 0; --C) {
    CachedTokens.push_back(Token());
    Lex(CachedTokens.back());
  }
  EnterCachingLexMode();
  return CachedTokens.back();
}

CommentHandler::~CommentHandler() {
}
