//===--- Parser.cpp - C Language Family Parser ----------------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Parser interfaces.
//
//===----------------------------------------------------------------------===//

#include "mlang/Parse/Parser.h"
#include "mlang/Parse/ParseDiagnostic.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/Sema/Scope.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

using namespace mlang;
using llvm::isa;
using llvm::cast;

Parser::Parser(Preprocessor &pp, Sema &actions)
  : CrashInfo(*this), PP(pp), Actions(actions),
    Diags(PP.getDiagnostics()) {
  Tok.setKind(tok::EoF);
  Actions.CurScope = 0;
  NumCachedScopes = 0;
  ParenCount = BracketCount = BraceCount = 0;
}

/// If a crash happens while the parser is active, print out a line indicating
/// what the current token is.
void PrettyStackTraceParserEntry::print(llvm::raw_ostream &OS) const {
  const Token &Tok = P.getCurToken();
  if (Tok.is(tok::EoF)) {
    OS << "<eof> parser at end of file\n";
    return;
  }

  if (Tok.getLocation().isInvalid()) {
    OS << "<unknown> parser at unknown location\n";
    return;
  }

  const Preprocessor &PP = P.getPreprocessor();
  Tok.getLocation().print(OS, PP.getSourceManager());
  if (Tok.isAnnotation())
    OS << ": at annotation token \n";
  else
    OS << ": current parser token '" << PP.getSpelling(Tok) << "'\n";
}


DiagnosticBuilder Parser::Diag(SourceLocation Loc, unsigned DiagID) {
  return Diags.Report(Loc, DiagID);
}

DiagnosticBuilder Parser::Diag(const Token &Tok, unsigned DiagID) {
  return Diag(Tok.getLocation(), DiagID);
}

/// \brief Emits a diagnostic suggesting parentheses surrounding a
/// given range.
///
/// \param Loc The location where we'll emit the diagnostic.
/// \param Loc The kind of diagnostic to emit.
/// \param ParenRange Source range enclosing code that should be parenthesized.
void Parser::SuggestParentheses(SourceLocation Loc, unsigned DK,
                                SourceRange ParenRange) {
  SourceLocation EndLoc = PP.getLocForEndOfToken(ParenRange.getEnd());
  if (!ParenRange.getEnd().isFileID() || EndLoc.isInvalid()) {
    // We can't display the parentheses, so just dig the
    // warning/error and return.
    Diag(Loc, DK);
    return;
  }

  Diag(Loc, DK)
    << FixItHint::CreateInsertion(ParenRange.getBegin(), "(")
    << FixItHint::CreateInsertion(EndLoc, ")");
}

/// MatchRHSPunctuation - For punctuation with a LHS and RHS (e.g. '['/']'),
/// this helper function matches and consumes the specified RHS token if
/// present.  If not present, it emits the specified diagnostic indicating
/// that the parser failed to match the RHS of the token at LHSLoc.  LHSName
/// should be the name of the unmatched LHS token.
SourceLocation Parser::MatchRHSPunctuation(tok::TokenKind RHSTok,
                                           SourceLocation LHSLoc) {

  if (Tok.is(RHSTok))
    return ConsumeAnyToken();

  SourceLocation R = Tok.getLocation();
  const char *LHSName = "unknown";
  diag::kind DID = diag::err_parse_error;
  switch (RHSTok) {
  default: break;
  case tok::RParentheses : LHSName = "("; DID = diag::err_expected_rparen; break;
  case tok::RCurlyBraces : LHSName = "{"; DID = diag::err_expected_rbrace; break;
  case tok::RSquareBrackets: LHSName = "["; DID = diag::err_expected_rsquare; break;
  case tok::GreaterThan:  LHSName = ">"; DID = diag::err_expected_greater; break;
  }
  Diag(Tok, DID);
  Diag(LHSLoc, diag::note_matching) << LHSName;
  SkipUntil(RHSTok);
  return R;
}

static bool IsCommonTypo(tok::TokenKind ExpectedTok, const Token &Tok) {
  switch (ExpectedTok) {
  case tok::Semicolon: return Tok.is(tok::Colon); // : for ;
  default: return false;
  }
}

/// ExpectAndConsume - The parser expects that 'ExpectedTok' is next in the
/// input.  If so, it is consumed and false is returned.
///
/// If the input is malformed, this emits the specified diagnostic.  Next, if
/// SkipToTok is specified, it calls SkipUntil(SkipToTok).  Finally, true is
/// returned.
bool Parser::ExpectAndConsume(tok::TokenKind ExpectedTok, unsigned DiagID,
                              const char *Msg, tok::TokenKind SkipToTok) {
  if (Tok.is(ExpectedTok)) {
    ConsumeAnyToken();
    return false;
  }

  // Detect common single-character typos and resume.
  if (IsCommonTypo(ExpectedTok, Tok)) {
    SourceLocation Loc = Tok.getLocation();
    Diag(Loc, DiagID)
      << Msg
      << FixItHint::CreateReplacement(SourceRange(Loc),
                                      getTokenSimpleSpelling(ExpectedTok));
    ConsumeAnyToken();

    // Pretend there wasn't a problem.
    return false;
  }

  const char *Spelling = 0;
  SourceLocation EndLoc = PP.getLocForEndOfToken(PrevTokLocation);
  if (EndLoc.isValid() &&
      (Spelling = tok::getTokenSimpleSpelling(ExpectedTok))) {
    // Show what code to insert to fix this problem.
    Diag(EndLoc, DiagID)
      << Msg
      << FixItHint::CreateInsertion(EndLoc, Spelling);
  } else
    Diag(Tok, DiagID) << Msg;

  if (SkipToTok != tok::Unknown)
    SkipUntil(SkipToTok);
  return true;
}

bool Parser::ExpectAndConsumeSemi(unsigned DiagID) {
  if (Tok.is(tok::Semicolon)) {
    ConsumeAnyToken();
    return false;
  }
  
  if ((Tok.is(tok::RParentheses) || Tok.is(tok::RSquareBrackets)) &&
      NextToken().is(tok::Semicolon)) {
    Diag(Tok, diag::err_extraneous_token_before_semi)
      << PP.getSpelling(Tok)
      << FixItHint::CreateRemoval(Tok.getLocation());
    ConsumeAnyToken(); // The ')' or ']'.
    ConsumeToken(); // The ';'.
    return false;
  }
  
  return ExpectAndConsume(tok::Semicolon, DiagID);
}

//===----------------------------------------------------------------------===//
// Error recovery.
//===----------------------------------------------------------------------===//

/// SkipUntil - Read tokens until we get to the specified token, then consume
/// it (unless DontConsume is true).  Because we cannot guarantee that the
/// token will ever occur, this skips to the next token, or to some likely
/// good stopping point.  If StopAtSemi is true, skipping will stop at a ';'
/// character.
///
/// If SkipUntil finds the specified token, it returns true, otherwise it
/// returns false.
bool Parser::SkipUntil(const tok::TokenKind *Toks, unsigned NumToks,
		bool StopAtEndOfLine, bool StopAtSemi, bool DontConsume) {
  // We always want this function to skip at least one token if the first token
  // isn't T and if not at EOF.
  bool isFirstTokenSkipped = true;
  while (1) {
    // If we found one of the tokens, stop and return true.
    for (unsigned i = 0; i != NumToks; ++i) {
      if (Tok.is(Toks[i])) {
        if (DontConsume) {
          // Noop, don't consume the token.
        } else {
          ConsumeAnyToken();
        }
        return true;
      }
    }
    //FIXME yabin to stop at a new line
    if(Tok.isAtStartOfLine() && StopAtEndOfLine)
    	return false;

    switch (Tok.getKind()) {
    case tok::EoF:
      // Ran out of tokens.
      return false;
        
    case tok::LParentheses:
      // Recursively skip properly-nested parens.
      ConsumeParen();
      SkipUntil(tok::RParentheses, true, false);
      break;
    case tok::LSquareBrackets:
      // Recursively skip properly-nested square brackets.
      ConsumeBracket();
      SkipUntil(tok::RSquareBrackets, true, false);
      break;
    case tok::LCurlyBraces:
      // Recursively skip properly-nested braces.
      ConsumeBrace();
      SkipUntil(tok::LCurlyBraces, true, false);
      break;

    // Okay, we found a ']' or '}' or ')', which we think should be balanced.
    // Since the user wasn't looking for this token (if they were, it would
    // already be handled), this isn't balanced.  If there is a LHS token at a
    // higher level, we will assume that this matches the unbalanced token
    // and return it.  Otherwise, this is a spurious RHS token, which we skip.
    case tok::RParentheses:
      if (ParenCount && !isFirstTokenSkipped)
        return false;  // Matches something.
      ConsumeParen();
      break;
    case tok::RSquareBrackets:
      if (BracketCount && !isFirstTokenSkipped)
        return false;  // Matches something.
      ConsumeBracket();
      break;
    case tok::RCurlyBraces:
      if (BraceCount && !isFirstTokenSkipped)
        return false;  // Matches something.
      ConsumeBrace();
      break;

    case tok::StringLiteral:
      ConsumeStringToken();
      break;
    case tok::Semicolon:
      if (StopAtSemi)
        return false;
      // FALL THROUGH.
    default:
      // Skip this token.
      ConsumeToken();
      break;
    }
    isFirstTokenSkipped = false;
  }
}

//===----------------------------------------------------------------------===//
// Scope manipulation
//===----------------------------------------------------------------------===//

/// EnterScope - Start a new scope.
void Parser::EnterScope(unsigned ScopeFlags) {
  if (NumCachedScopes) {
    Scope *N = ScopeCache[--NumCachedScopes];
    N->Init(getCurScope(), ScopeFlags);
    Actions.CurScope = N;
  } else {
	  Actions.CurScope = new Scope(getCurScope(), ScopeFlags, Diags);
  }
}

/// ExitScope - Pop a scope off the scope stack.
void Parser::ExitScope() {
  assert(getCurScope() && "Scope imbalance!");

  // Inform the actions module that this scope is going away if there are any
  // decls in it.
  if (!getCurScope()->defn_empty()) {
	  Actions.ActOnPopScope(Tok.getLocation(), getCurScope());
  }

  Scope *OldScope = getCurScope();
  Actions.CurScope = OldScope->getParent();


  if (NumCachedScopes == ScopeCacheSize)
    delete OldScope;
  else
    ScopeCache[NumCachedScopes++] = OldScope;
}




//===----------------------------------------------------------------------===//
// C99 6.9: External Definitions.
//===----------------------------------------------------------------------===//

Parser::~Parser() {
  // If we still have scopes active, delete the scope tree.
  delete getCurScope();
  Actions.CurScope = 0;
  
  // Free the scope cache.
  for (unsigned i = 0, e = NumCachedScopes; i != e; ++i)
    delete ScopeCache[i];

}

/// Initialize - Warm up the parser.
///
void Parser::Initialize() {
  // Create the translation unit scope.  Install it as the current scope.
  assert(getCurScope() == 0 && "A scope is already active?");
  EnterScope(Scope::TopScope);

  Actions.ActOnTranslationUnitScope(getCurScope());

  // Prime the lexer look-ahead.
  ConsumeToken();

  // Ident_super = &PP.getIdentifierTable().get("super");
  PP.getBuiltinInfo().InitializeBuiltins(PP.getIdentifierTable(),
                                         PP.getLangOptions());
}

/// ParseTopLevelDefn - Parse one top-level definition, return whatever the
/// action tells us to.  This returns true if the EOF was encountered.
bool Parser::ParseTopLevelDefn(DefnGroupPtrTy &Result) {
  Result = DefnGroupPtrTy();
  if (Tok.is(tok::EoF)) {
    Actions.ActOnEndOfTranslationUnit();
    return true;
  }

  Result = ParseExternalDefinition();
  return false;
}

/// ParseTranslationUnit:
///       translation-unit:
///         external-definition
///         translation-unit external-definition
void Parser::ParseTranslationUnit() {
  Initialize();

  DefnGroupPtrTy Res;
  while (!ParseTopLevelDefn(Res))
    /*parse them all*/;

  ExitScope();
  assert(getCurScope() == 0 && "Scope imbalance!");
}

/// ParseExternalDefinition:
///
///       external-definition:
///         script-definition
///         function-definition
///         class-definition
Parser::DefnGroupPtrTy Parser::ParseExternalDefinition() {
  if(isStartOfFunctionDefinition()) {
	  return ParseFunctionDefinition();
  }

  if(isStartOfClassDefinition()) {
	  return ParseClassDefinition();
  }

  return ParseScriptDefinition();
}

/// \brief Determine whether the current token is a 'function' keyword.
bool Parser::isStartOfFunctionDefinition() const {
  return false;
}

/// \brief Determine whether the current tokenis a 'classdef' keyword.
bool Parser::isStartOfClassDefinition() const {
  return false;
}

/// \brief Determine whether we are parsing a script.
bool Parser::isInScriptDefinition() const {
  return true;
}

/// \brief Determine whether we are parsing a script.
bool Parser::isInClassDefinition() const {
  return false;
}

/// \brief Determine whether we are parsing a script.
bool Parser::isInFunctionDefinition() const {
  return false;
}

/// ParseScriptDefinition :
///
///       script-definition:
///         definition-seq
///       definition-seq:
///         definition
///         definition-seq definition
///       definition:
///         simple-definition
Parser::DefnGroupPtrTy Parser::ParseScriptDefinition()
{
	if (Tok.getKind() == tok::EoF) {
		Diag(Tok, diag::err_expected_external_declaration);
		return DefnGroupPtrTy();
	}

	// return a ScriptDefn object
	Defn *Script = Actions.ActOnStartOfScriptDef(
			getCurScope());
	Script = ParseStatementsBody(Script);

	return Actions.ConvertDefnToDefnGroup(Script);
}

/// ParseFunctionDefinition - We parsed and verified that the specified
/// Declarator is well formed.
///
Parser::DefnGroupPtrTy Parser::ParseFunctionDefinition() {
  return DefnGroupPtrTy();
}

/// ParseClassDefinition -
Parser::DefnGroupPtrTy Parser::ParseClassDefinition()
{
	return DefnGroupPtrTy();
}

bool Parser::isTokenEqualOrMistypedEqualEqual(unsigned DiagID) {
  return false;
}
