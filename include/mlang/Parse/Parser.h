//===--- Parser.h - C Language Parser ---------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Parser interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_PARSE_PARSER_H_
#define MLANG_PARSE_PARSER_H_

#include "mlang/AST/DefinitionName.h"
#include "mlang/Basic/Specifiers.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Sema/Sema.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/ADT/OwningPtr.h"
#include <stack>
#include <list>

namespace mlang {
class Defn;
class Expr;
class Stmt;
class Type;
class Token;
class Scope;
class DiagnosticBuilder;
class Parser;
class DefnGroupRef;
class ASTContext;

/// CachedTokens - A set of tokens that has been cached for later
/// parsing.
typedef llvm::SmallVector<Token, 4> CachedTokens;

/// PrettyStackTraceParserEntry - If a crash happens while the parser is active,
/// an entry is printed for it.
class PrettyStackTraceParserEntry : public llvm::PrettyStackTraceEntry {
  const Parser &P;
public:
  PrettyStackTraceParserEntry(const Parser &p) : P(p) {}
  virtual void print(llvm::raw_ostream &OS) const;
};

/// PrecedenceLevels - These are precedences for the binary/unary
/// operators in the grammar.
namespace prec {
  enum Level {
    Unknown         = 0,    // Not binary operator.
    Comma           = 1,    // ,
    Assignment      = 2,    // =, *=, /=, %=, +=, -=, <<=, >>=, &=, ^=, |=
    LogicalOr       = 3,    // ||
    LogicalAnd      = 4,    // &&
    InclusiveOr     = 5,    // |
    And             = 6,    // &
    Equality        = 7,    // ==, !=
    Relational      = 8,    // >=, <=, >, <
    Colon           = 9,    // :
    Shift           = 10,   // <<, >>
    Additive        = 11,   // -, +
    Multiplicative  = 12,   // *, /, \, .*, ./, ".\"
    UnaryStaff      = 13,   // ++, --, !, ~, -, +
    PowerTranspose  = 14    // .^, ^, ', .'
  };
}

/// Parser - This implements a parser for the M languages.  After
/// parsing units of the grammar, productions are invoked to handle whatever has
/// been read.
///
class Parser {
  friend class ParenBraceBracketBalancer;
  PrettyStackTraceParserEntry CrashInfo;

  Preprocessor &PP;

  /// Tok - The current token we are peeking ahead.  All parsing methods assume
  /// that this is valid.
  Token Tok;

  // PrevTokLocation - The location of the token we previously
  // consumed. This token is used for diagnostics where we expected to
  // see a token following another token (e.g., the ';' at the end of
  // a statement).
  SourceLocation PrevTokLocation;

  unsigned short ParenCount, BracketCount, BraceCount;

  /// Actions - These are the callbacks we invoke as we parse various constructs
  /// in the file. 
  Sema &Actions;

  Diagnostic &Diags;

  /// ScopeCache - Cache scopes to reduce malloc traffic.
  enum { ScopeCacheSize = 16 };
  unsigned NumCachedScopes;
  Scope *ScopeCache[ScopeCacheSize];

  /// Ident_super - IdentifierInfo for "super", to support fast
  /// comparison.
  IdentifierInfo *Ident_get;
  IdentifierInfo *Ident_set;

public:
  Parser(Preprocessor &PP, Sema &Actions);
  ~Parser();

  const LangOptions &getLang() const { return PP.getLangOptions(); }
  const TargetInfo &getTargetInfo() const { return PP.getTargetInfo(); }
  Preprocessor &getPreprocessor() const { return PP; }
  Sema &getActions() const { return Actions; }

  const Token &getCurToken() const { return Tok; }
  
  Scope *getCurScope() const { return Actions.getCurScope(); }

  // Type forwarding.  All of these are statically 'void*', but they may all be
  // different actual classes based on the actions in place.
  typedef Expr ExprTy;
  typedef Stmt StmtTy;
  typedef OpaquePtr<DefnGroupRef> DefnGroupPtrTy;

  typedef mlang::ExprResult        ExprResult;
  typedef mlang::StmtResult        StmtResult;
  typedef mlang::TypeResult        TypeResult;
  typedef mlang::ParsedType        ParsedType;

  typedef Expr *ExprArg;
  typedef Stmt *MultiStmtArg;

  /// Adorns a ExprResult with Actions to make it an ExprResult
  ExprResult Owned(ExprResult res) {
    return ExprResult(res);
  }
  /// Adorns a StmtResult with Actions to make it an StmtResult
  StmtResult Owned(StmtResult res) {
    return StmtResult(res);
  }

  ExprResult ExprError() { return ExprResult(true); }
  StmtResult StmtError() { return StmtResult(true); }

  ExprResult ExprError(const DiagnosticBuilder &) { return ExprError(); }
  StmtResult StmtError(const DiagnosticBuilder &) { return StmtError(); }

  ExprResult ExprEmpty() { return ExprResult(false); }

 /// Initialize - Warm up the parser.
  ///
  void Initialize();

  //===--------------------------------------------------------------------===//
  // Top-level
  bool ParseTopLevelDefn(DefnGroupPtrTy &Result);
  void ParseTranslationUnit();
  DefnGroupPtrTy ParseExternalDefinition();
  DefnGroupPtrTy ParseScriptDefinition();
  DefnGroupPtrTy ParseFunctionDefinition();
  DefnGroupPtrTy ParseClassDefinition();

private:
  //===--------------------------------------------------------------------===//
  // Low-Level token peeking and consumption methods.
  //
  /// isTokenParen - Return true if the cur token is '(' or ')'.
  bool isTokenParen() const {
    return Tok.getKind() == tok::LParentheses || Tok.getKind() == tok::RParentheses;
  }
  /// isTokenBracket - Return true if the cur token is '[' or ']'.
  bool isTokenBracket() const {
    return Tok.getKind() == tok::LSquareBrackets ||
    		Tok.getKind() == tok::RSquareBrackets;
  }
  /// isTokenBrace - Return true if the cur token is '{' or '}'.
  bool isTokenBrace() const {
    return Tok.getKind() == tok::LCurlyBraces || Tok.getKind() == tok::RCurlyBraces;
  }

  /// isTokenStringLiteral - True if this token is a string-literal.
  ///
  bool isTokenStringLiteral() const {
    return Tok.getKind() == tok::StringLiteral;
  }

  /// \brief Returns true if the current token is a '=' or '==' and
  /// false otherwise. If it's '==', we assume that it's a typo and we emit
  /// DiagID and a fixit hint to turn '==' -> '='.
  bool isTokenEqualOrMistypedEqualEqual(unsigned DiagID);

public: // for unit test purpose
  /// ConsumeToken - Consume the current 'peek token' and lex the next one.
  /// This does not work with all kinds of tokens: strings and specific other
  /// tokens must be consumed with custom methods below.  This returns the
  /// location of the consumed token.
  SourceLocation ConsumeToken() {
    assert(!isTokenStringLiteral() && !isTokenParen() && !isTokenBracket() &&
           !isTokenBrace() &&
           "Should consume special tokens with Consume*Token");
    
    PrevTokLocation = Tok.getLocation();
    PP.Lex(Tok);
    return PrevTokLocation;
  }

  /// ConsumeAnyToken - Dispatch to the right Consume* method based on the
  /// current token type.  This should only be used in cases where the type of
  /// the token really isn't known, e.g. in error recovery.
  SourceLocation ConsumeAnyToken() {
    if (isTokenParen())
      return ConsumeParen();
    else if (isTokenBracket())
      return ConsumeBracket();
    else if (isTokenBrace())
      return ConsumeBrace();
    else if (isTokenStringLiteral())
      return ConsumeStringToken();
    else
      return ConsumeToken();
  }

  /// ConsumeParen - This consume method keeps the paren count up-to-date.
  ///
  SourceLocation ConsumeParen() {
    assert(isTokenParen() && "wrong consume method");
    if (Tok.getKind() == tok::LParentheses)
      ++ParenCount;
    else if (ParenCount)
      --ParenCount;       // Don't let unbalanced )'s drive the count negative.
    PrevTokLocation = Tok.getLocation();
    PP.Lex(Tok);
    return PrevTokLocation;
  }

  /// ConsumeBracket - This consume method keeps the bracket count up-to-date.
  ///
  SourceLocation ConsumeBracket() {
    assert(isTokenBracket() && "wrong consume method");
    if (Tok.getKind() == tok::LSquareBrackets)
      ++BracketCount;
    else if (BracketCount)
      --BracketCount;     // Don't let unbalanced ]'s drive the count negative.

    PrevTokLocation = Tok.getLocation();
    PP.Lex(Tok);
    return PrevTokLocation;
  }

  /// ConsumeBrace - This consume method keeps the brace count up-to-date.
  ///
  SourceLocation ConsumeBrace() {
    assert(isTokenBrace() && "wrong consume method");
    if (Tok.getKind() == tok::LCurlyBraces)
      ++BraceCount;
    else if (BraceCount)
      --BraceCount;     // Don't let unbalanced }'s drive the count negative.

    PrevTokLocation = Tok.getLocation();
    PP.Lex(Tok);
    return PrevTokLocation;
  }

  /// ConsumeStringToken - Consume the current 'peek token', lexing a new one
  /// and returning the token kind.  This method is specific to strings, as it
  /// handles string literal concatenation, as per C99 5.1.1.2, translation
  /// phase #6.
  SourceLocation ConsumeStringToken() {
    assert(isTokenStringLiteral() &&
           "Should only consume string literals with this method");
    PrevTokLocation = Tok.getLocation();
    PP.Lex(Tok);
    return PrevTokLocation;
  }

  /// GetLookAheadToken - This peeks ahead N tokens and returns that token
  /// without consuming any tokens.  LookAhead(0) returns 'Tok', LookAhead(1)
  /// returns the token after Tok, etc.
  ///
  /// Note that this differs from the Preprocessor's LookAhead method, because
  /// the Parser always has one token lexed that the preprocessor doesn't.
  ///
  const Token &GetLookAheadToken(unsigned N) {
    if (N == 0 || Tok.is(tok::EoF)) return Tok;
    return PP.LookAhead(N-1);
  }

  /// NextToken - This peeks ahead one token and returns it without
  /// consuming it.
  const Token &NextToken() {
    return PP.LookAhead(0);
  }

private: // for unit test purpose

  /// TentativeParsingAction - An object that is used as a kind of "tentative
  /// parsing transaction". It gets instantiated to mark the token position and
  /// after the token consumption is done, Commit() or Revert() is called to
  /// either "commit the consumed tokens" or revert to the previously marked
  /// token position. Example:
  ///
  ///   TentativeParsingAction TPA(*this);
  ///   ConsumeToken();
  ///   ....
  ///   TPA.Revert();
  ///
  class TentativeParsingAction {
    Parser &P;
    Token PrevTok;
    bool isActive;

  public:
    explicit TentativeParsingAction(Parser& p) : P(p) {
      PrevTok = P.Tok;
      P.PP.EnableBacktrackAtThisPos();
      isActive = true;
    }
    void Commit() {
      assert(isActive && "Parsing action was finished!");
      P.PP.CommitBacktrackedTokens();
      isActive = false;
    }
    void Revert() {
      assert(isActive && "Parsing action was finished!");
      P.PP.Backtrack();
      P.Tok = PrevTok;
      isActive = false;
    }
    ~TentativeParsingAction() {
      assert(!isActive && "Forgot to call Commit or Revert!");
    }
  };

  /// MatchRHSPunctuation - For punctuation with a LHS and RHS (e.g. '['/']'),
  /// this helper function matches and consumes the specified RHS token if
  /// present.  If not present, it emits the specified diagnostic indicating
  /// that the parser failed to match the RHS of the token at LHSLoc.  LHSName
  /// should be the name of the unmatched LHS token.  This returns the location
  /// of the consumed token.
  SourceLocation MatchRHSPunctuation(tok::TokenKind RHSTok,
                                     SourceLocation LHSLoc);

  /// ExpectAndConsume - The parser expects that 'ExpectedTok' is next in the
  /// input.  If so, it is consumed and false is returned.
  ///
  /// If the input is malformed, this emits the specified diagnostic.  Next, if
  /// SkipToTok is specified, it calls SkipUntil(SkipToTok).  Finally, true is
  /// returned.
  bool ExpectAndConsume(tok::TokenKind ExpectedTok, unsigned Diag,
                        const char *DiagMsg = "",
                        tok::TokenKind SkipToTok = tok::Unknown);

  /// \brief The parser expects a semicolon and, if present, will consume it.
  ///
  /// If the next token is not a semicolon, this emits the specified diagnostic,
  /// or, if there's just some closing-delimiter noise (e.g., ')' or ']') prior
  /// to the semicolon, consumes that extra token.
  bool ExpectAndConsumeSemi(unsigned DiagID);
  
  //===--------------------------------------------------------------------===//
  // Scope manipulation

  /// ParseScope - Introduces a new scope for parsing. The kind of
  /// scope is determined by ScopeKind. Objects of this type should
  /// be created on the stack to coincide with the position where the
  /// parser enters the new scope, and this object's constructor will
  /// create that new scope. Similarly, once the object is destroyed
  /// the parser will exit the scope.
  class ParseScope {
    Parser *Self;
    ParseScope(const ParseScope&); // do not implement
    ParseScope& operator=(const ParseScope&); // do not implement

  public:
    // ParseScope - Construct a new object to manage a scope in the
    // parser Self where the new Scope is created with the flags
    // ScopeKind, but only when ManageScope is true (the default). If
    // ManageScope is false, this object does nothing.
    ParseScope(Parser *Self, unsigned ScopeFlags, bool ManageScope = true)
      : Self(Self) {
      if (ManageScope)
        Self->EnterScope(ScopeFlags);
      else
        this->Self = 0;
    }

    // Exit - Exit the scope associated with this object now, rather
    // than waiting until the object is destroyed.
    void Exit() {
      if (Self) {
        Self->ExitScope();
        Self = 0;
      }
    }

    ~ParseScope() {
      Exit();
    }
  };

  /// EnterScope - Start a new scope.
  void EnterScope(unsigned ScopeFlags);

  /// ExitScope - Pop a scope off the scope stack.
  void ExitScope();

  //===--------------------------------------------------------------------===//
  // Diagnostic Emission and Error recovery.

public:
  DiagnosticBuilder Diag(SourceLocation Loc, unsigned DiagID);
  DiagnosticBuilder Diag(const Token &Tok, unsigned DiagID);

private:
  void SuggestParentheses(SourceLocation Loc, unsigned DK,
                          SourceRange ParenRange);

  /// SkipUntil - Read tokens until we get to the specified token, then consume
  /// it (unless DontConsume is true).  Because we cannot guarantee that the
  /// token will ever occur, this skips to the next token, or to some likely
  /// good stopping point.  If StopAtSemi is true, skipping will stop at a ';'
  /// character.
  ///
  /// If SkipUntil finds the specified token, it returns true, otherwise it
  /// returns false.
  bool SkipUntil(tok::TokenKind T, bool StopAtEndOfLine = true,
		  bool StopAtSemi = true, bool DontConsume = false) {
    return SkipUntil(&T, 1, StopAtEndOfLine, StopAtSemi, DontConsume);
  }
  bool SkipUntil(tok::TokenKind T1, tok::TokenKind T2,
		  bool StopAtEndOfLine = true,
		  bool StopAtSemi = true, bool DontConsume = false) {
    tok::TokenKind TokArray[] = {T1, T2};
    return SkipUntil(TokArray, 2, StopAtEndOfLine, StopAtSemi, DontConsume);
  }
  bool SkipUntil(const tok::TokenKind *Toks, unsigned NumToks,
		  bool StopAtEndOfLine = true, bool StopAtSemi = true,
		  bool DontConsume = false);

  //===--------------------------------------------------------------------===//
  // Lexing and parsing of embeded class.

  /// \brief Representation of a class that has been parsed, including
  /// any member function declarations or definitions that need to be
  /// parsed after the corresponding top-level class is complete.
  struct ParsingClass {
    ParsingClass(Defn *Tag, bool TopLevelClass)
      : TopLevelClass(TopLevelClass), Tag(Tag) { }

    /// \brief Whether this is a "top-level" class, meaning that it is
    /// not nested within another class.
    bool TopLevelClass : 1;

    /// \brief The class or class template whose definition we are parsing.
    Defn *Tag;

  };

  /// \brief The stack of classes that is currently being
  /// parsed. Nested and local classes will be pushed onto this stack
  /// when they are parsed, and removed afterward.
//  std::stack<ParsingClass *> ClassStack;
//
//  ParsingClass &getCurrentClass() {
//    assert(!ClassStack.empty() && "No lexed method stacks!");
//    return *ClassStack.top();
//  }

  /// \brief RAII object used to
  class ParsingClassDefinition {
    Parser &P;
    bool Popped;

  public:
    ParsingClassDefinition(Parser &P, Defn *TagOrTemplate, bool TopLevelClass)
      : P(P), Popped(false) {
      P.PushParsingClass(TagOrTemplate, TopLevelClass);
    }

    /// \brief Pop this class of the stack.
    void Pop() {
      assert(!Popped && "Nested class has already been popped");
      Popped = true;
      P.PopParsingClass();
    }

    ~ParsingClassDefinition() {
      if (!Popped)
        P.PopParsingClass();
    }
  };

  bool isInScriptDefinition() const;
  bool isInFunctionDefinition() const;
  bool isInClassDefinition() const;
  bool isStartOfFunctionDefinition() const;
  bool isStartOfClassDefinition() const;
  void PushParsingClass(Defn *TagOrTemplate, bool TopLevelClass);
  void DeallocateParsedClasses(ParsingClass *Class);
  void PopParsingClass();

  bool ConsumeAndStoreUntil(tok::TokenKind T1,
                            CachedTokens &Toks,
                            bool StopAtSemi = true,
                            bool ConsumeFinalToken = true) {
    return ConsumeAndStoreUntil(T1, T1, Toks, StopAtSemi, ConsumeFinalToken);
  }
  bool ConsumeAndStoreUntil(tok::TokenKind T1, tok::TokenKind T2,
                            CachedTokens &Toks,
                            bool StopAtSemi = true,
                            bool ConsumeFinalToken = true);

  //===--------------------------------------------------------------------===//
  // Expressions.
  // FIXME yabin this should be private
  // we modify to public for convenient unit tests purposes
public:
  ExprResult ParseExpression();
  // Expr that doesn't include commas.
  ExprResult ParseAssignmentExpression(ExprResult &Var);

  ExprResult ParseAssignLHSExpression();

  ExprResult ParseExpressionWithLeadingAt(SourceLocation AtLoc);

  ExprResult ParseRHSOfBinaryExpression(ExprResult LHS,
                                              prec::Level MinPrec);
  ExprResult ParseUnaryExpression();

  // ExprResult ParseIncDecExpression();

  ExprResult ParseTransposePowerExpression();

  ExprResult ParsePostfixExpression();

  ExprResult ParsePrimaryExpression();

  ExprResult ParseConstantExpression();

  ExprResult ParseIdentifier();

  /// Returns true if the next token would start a postfix-expression
  /// suffix.
  bool isPostfixExpressionSuffixStart() {
    tok::TokenKind K = Tok.getKind();
    return (K == tok::LSquareBrackets || K == tok::LParentheses ||
            K == tok::Dot || K == tok::DoubleAdd || K == tok::DoubleSub);
  }

  ExprResult ParsePostfixExpressionSuffix(ExprResult LHS);
  ExprResult ParseNumericConstant();
  ExprResult ParseBuiltinPrimaryExpression();

  typedef llvm::SmallVector<Expr*, 20> ExprListTy;
  typedef llvm::SmallVector<SourceLocation, 20> CommaLocsTy;
  typedef llvm::SmallVector<SourceLocation, 4> ElseIfLocsTy;

  /// ParseExpressionList - Used for C/C++ (argument-)expression-list.
  bool ParseExpressionList(llvm::SmallVectorImpl<Expr*> &Exprs,
                           llvm::SmallVectorImpl<SourceLocation> &CommaLocs);

  ExprResult ParseMultiOutputExpression(SourceLocation &RBraketLoc);

  ExprResult ParseParenExpression(SourceLocation &RParenLoc);

  ExprResult ParseArrayExpression(SourceLocation &ROpLoc, bool isCell);

  ExprResult ParseArrayRows(SourceLocation OpenLoc, bool isCell);

  ExprResult ParseStringLiteralExpression();

  //===--------------------------------------------------------------------===//
  // Statements and Blocks.
  Defn *ParseStatementsBody(Defn *D);
  StmtResult ParseStatementOrDeclaration(StmtVector &Stmts,
  		bool OnlyStatement);
  StmtResult ParseBlockStatement(/*AttributeList *Attr,*/
                                            bool isStmtExpr = false);
  StmtResult ParseBlockStatementBody(bool isStmtExpr = false);
  StmtResult ParseLabeledStatement(/*AttributeList *Attr*/);
  StmtResult ParseCaseStatement(/*AttributeList *Attr*/);
  StmtResult ParseOtherwiseStatement(/*AttributeList *Attr*/);
  bool ParseParenExprOrCondition(ExprResult &ExprResult,
                                 Defn *&DeclResult,
                                 SourceLocation Loc,
                                 bool ConvertToBoolean);
  StmtResult ParseThenBlockBody();
  StmtResult ParseElseBlockBody(SourceLocation ELoc);
  StmtResult ParseElseIfBlock();
  StmtResult ParseIfCmd(/*AttributeList *Attr*/);
  StmtResult ParseCaseCmd();
  StmtResult ParseOtherwiseCmd();
  StmtResult ParseSwitchCmd(/*AttributeList *Attr*/);
  StmtResult ParseWhileCmd(/*AttributeList *Attr*/);
  StmtResult ParseDoCmd(/*AttributeList *Attr*/);
  StmtResult ParseForCmd(/*AttributeList *Attr*/);
  StmtResult ParseGotoStatement(/*AttributeList *Attr*/);
  StmtResult ParseContinueCmd(/*AttributeList *Attr*/);
  StmtResult ParseBreakCmd(/*AttributeList *Attr*/);
  StmtResult ParseReturnCmd(/*AttributeList *Attr*/);
  StmtResult ParseTryBlock(/*AttributeList *Attr*/);
  StmtResult ParseTryBlockCommon(SourceLocation TryLoc);
  StmtResult ParseCatchBlock();

  //===--------------------------------------------------------------------===//
  // Initializations
  /// ParseInitializer -
  ///       initializer:
  ///         assignment-expression
  ///        '[' ...
  ///         '{' ...
  ExprResult ParseInitializer() {
	  ExprResult Var;
	  if (Tok.is(tok::LCurlyBraces))
		  return ParseBraceInitializer();
	  else if (Tok.is(tok::LSquareBrackets))
		  return ParseBracketInitializer();
	  else
		  return ParseAssignmentExpression(Var);
  }
  ExprResult ParseBraceInitializer(); // for cell array initializer {...}
  ExprResult ParseBracketInitializer(); // for array initializer [...]
  ExprResult ParseInitializerWithPotentialDesignator();

  //===--------------------------------------------------------------------===//
  // Declarations and Definitions
  DefnGroupPtrTy ParseDefinition(StmtVector &Stmts,
	    unsigned Context, SourceLocation &DefnEnd);
  DefnGroupPtrTy ParseSimpleDefinition(StmtVector &Stmts,
      unsigned Context, SourceLocation &DefnEnd);
  DefnGroupPtrTy ParseDefnGroup(unsigned Context,
      bool AllowFunctionDefinitions, SourceLocation *DefnEnd = 0) ;
  DefnGroupPtrTy ParseInitDeclarator();
  Defn * ParseStorageClassDeclaration();
};

}  // end namespace mlang

#endif
