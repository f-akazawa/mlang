//===--- ParseExpr.cpp - Expression Parsing -------------------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements the Expression parsing implementation.
//
//===----------------------------------------------------------------------===//

#include "mlang/Parse/Parser.h"
#include "mlang/Parse/ParseDiagnostic.h"
#include "mlang/AST/Type.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/Sema/Scope.h"
#include "mlang/Basic/PrettyStackTrace.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallString.h"

using namespace mlang;

/// getBinOpPrecedence - Return the precedence of the specified binary operator
/// token.
static prec::Level getBinOpPrecedence(tok::TokenKind Kind, bool OctaveOperators) {
	switch (Kind) {
	case tok::Comma:
		return prec::Comma;
	case tok::Assignment:
	case tok::MatPowerAssign:
	case tok::MatMulAssign:
	case tok::MatRDivAssign:
	case tok::MatLDivAssign:
	case tok::PowerAssign:
	case tok::MulAssign:
	case tok::RDivAssign:
	case tok::LDivAssign:
	case tok::AddAssign:
	case tok::SubAssign:
	case tok::ShlAssign:
	case tok::ShrAssign:
	case tok::AndAssign:
	case tok::OrAssign:
		return prec::Assignment;
	case tok::ShortCircuitOR:
		return prec::LogicalOr;
	case tok::ShortCircuitAND:
		return prec::LogicalAnd;
	case tok::EleWiseOR:
		return prec::InclusiveOr;
	case tok::EleWiseAND:
		return prec::And;
	case tok::NotEqualTo:
	case tok::EqualTo:
		return prec::Equality;
	case tok::LessThan:
	case tok::GreaterThan:
	case tok::LessThanOREqualTo:
	case tok::GreaterThanOREqualTo:
		return prec::Relational;
	case tok::Colon:
		return prec::Colon;
	case tok::ShiftLeft:
	case tok::ShiftRight:
		return prec::Shift;
	case tok::Addition:
	case tok::Subtraction:
		return prec::Additive;
	case tok::Asterisk:
	case tok::Slash:
	case tok::BackSlash:
	case tok::DotAsterisk:
	case tok::DotSlash:
	case tok::DotBackSlash:
		return prec::Multiplicative;
	//case tok::Addition:
	//case tok::Subtraction:
	case tok::Tilde:
	case tok::At:
	case tok::DoubleAdd:
	case tok::DoubleSub:
		return prec::UnaryStaff;
	case tok::DotQuote:
	case tok::DotCaret:
	case tok::Quote: // Complex conjugate transpose
	case tok::Caret:
		return prec::PowerTranspose;
	default:
		return prec::Unknown;
	}
}

/// ParseExpression - Simple precedence-based parser for binary/ternary
/// operators.
///
/// Note: we diverge from the C99 grammar when parsing the assignment-expression
/// production.  C99 specifies that the LHS of an assignment operator should be
/// parsed as a unary-expression, but consistency dictates that it be a
/// conditional-expession.  In practice, the important thing here is that the
/// LHS of an assignment has to be an l-value, which productions between
/// unary-expression and conditional-expression don't produce.  Because we want
/// consistency, we parse the LHS as a conditional-expression, then check for
/// l-value-ness in semantic analysis stages.
///
///        primary-expression:
///        	 identifier
///          superclass_identifier
///        	 meta_identifier
///          string
///          constant
///           '[' ']'
///        	'[' expression ']'
/// ParsePostfixExpression - Parse a postfix expression
///        postfix-expression:
///          primary-expression
///          postfix-expression '(' ')'
///          postfix-expression '(' argument-expression-list ')'
///          postfix-expression '{' argument-expression-list '}'
///          postfix-expression '.' expression
///          postfix-expression '.(' expression ')'
///          postfix-expression QUOTE
///          postfix-expression TRANSPOSE
///          postfix-expression MATRIXPOWER postfix-expression
///          postfix-expression POWER postfix-expression
///          postfix-expression --
///          postfix-expression ++
///        argument-expression-list:
///          expression
///          magic_colon
///          magic_tilde
///          magic_end
///          argument-expression-list ',' magic_colon
///          argument-expression-list ',' magic_tilde
///          argument-expression-list ',' magic_end
///          argument-expression-list ',' expression
///        unary-expression:
///          postfix-expression
///          ++ unary-expression
///          -- unary-expression
///          unary-operator unary-expression
///
///        unary-operator: one of + - ~ @
///
///        multiplicative-expression:
///          unary-expression
///          multiplicative-expression * unary-expression
///          multiplicative-expression .* unary-expression
///          multiplicative-expression / unary-expression
///          multiplicative-expression ./ unary-expression
///          multiplicative-expression \ unary-expression
///          multiplicative-expression .\ unary-expression
///
///        additive-expression:
///          multiplicative-expression
///          additive-expression + multiplicative-expression
///          additive-expression - multiplicative-expression
///
///        colon-expression:
///          additive-expression
///          colon-expression ':' additive-expression
///
///        shift-expression:
///          colon-expression
///          shift-expression << colon-expression
///          shift-expression >> colon-expression
///
///        relational-expression:
///          shift-expression
///          relational-expression < shift-expression
///          relational-expression <= shift-expression
///          relational-expression == shift-expression
///          relational-expression >= shift-expression
///          relational-expression > shift-expression
///          relational-expression ~= shift-expression
///
///        elementwise-AND-expression:
///          relational-expression
///          AND-expression & relational-expression
///
///        elementwise-OR-expression
///          elementwise-AND-expression
///          elementwise-OR-expression | elementwise-AND-expression
///
///        short-circuit-AND-expression
///          elementwise-OR-expression
///          short-circuit-AND-expression && elementwise-OR-expression
///
///        short-circuit-OR-expression
///          short-circuit-AND-expression
///          short-circuit-AND-expression || short-circuit-OR-expression
///
///        assignment-expression:
///          short-circuit-OR-expression
///          assign-LHS-expression assignment-operator assignment-expression
///
///        assignment-operator: one of
///          = += -= *= /= \= ^= >>= <<= .*= ./= .\= .^= &= |=
///
///        assign-LHS-expression:
///          unary-expression
///          [ argument-expression-list ]
///
///        expression:
///          assignment-expression
///          expression ',' assignment-expression
///
ExprResult Parser::ParseExpression() {
	ExprResult Var;
	ExprResult LHS(ParseAssignmentExpression(Var));
	return ParseRHSOfBinaryExpression(move(LHS), prec::Comma);
}

/// ParseAssignmentExpression - Parse an expr that doesn't include commas.
///        expression:
///          assignment-expression
///          expression ',' assignment-expression
ExprResult Parser::ParseAssignmentExpression(ExprResult &Var) {
	ExprResult LHS(ParseAssignLHSExpression());
	if(LHS.isInvalid())
		return ExprError();

	Var = move(LHS);
	return ParseRHSOfBinaryExpression(move(LHS), prec::Assignment);
}

/// ParseAssignLHSExpression - Parse a lvalue expression
///        assign-LHS-expression:
///          unary-expression
///          [ argument-expression-list ]
ExprResult Parser::ParseAssignLHSExpression() {
	tok::TokenKind SavedKind = Tok.getKind();
	if(SavedKind ==  tok::LSquareBrackets) {
		SourceLocation LBracket = Tok.getLocation();
		SourceLocation RBracket;
		return ParseMultiOutputExpression(RBracket);
	}

	return ParseUnaryExpression();
}

/// ParseUnaryExpression - Parse a unary-expression.
///        unary-expression:
///          inc-dec-expression
///          ++ unary-expression
///          -- unary-expression
///          unary-operator unary-expression
ExprResult Parser::ParseUnaryExpression() {
	ExprResult Res;
	tok::TokenKind SavedKind = Tok.getKind();

	switch (SavedKind) {
	case tok::DoubleAdd:
	case tok::DoubleSub:
		// unary operators
	case tok::Addition:
	case tok::Subtraction:
	case tok::Tilde:
	case tok::At: {
		SourceLocation SavedLoc = ConsumeToken();
		Res = ParsePostfixExpression();
		if (!Res.isInvalid()) {
			Res = Actions.ActOnUnaryOp(getCurScope(), SavedLoc, SavedKind,false,
					                       Res.get());
		}
		return move(Res);
	}
	default:
		break;
	}

	return ParsePostfixExpression();
}

/// ParsePostfixExpression - Parse a postfix expression
///        postfix-expression:
///          primary_expr
///          postfix-expression '(' ')'
///          postfix-expression '(' argument-expression-list ')'
///          postfix-expression '{' argument-expression-list '}'
///          postfix-expression '.' expression
///          postfix-expression '.(' expression ')'
///          postfix-expression QUOTE
///          postfix-expression TRANSPOSE
///          postfix-expression MATRIXPOWER postfix-expression
///          postfix-expression MATRIXPOWER postfix-expression
///          postfix-expression --
///          postfix-expression ++
///
ExprResult Parser::ParsePostfixExpression() {
	ExprResult Res(ParsePrimaryExpression());
	return ParsePostfixExpressionSuffix(Res);
}

/// ParseExpressionList - Used for argument-expression-list.
///
///       argument-expression-list:
///         assignment-expression
///         argument-expression-list , assignment-expression
///
bool Parser::ParseExpressionList(llvm::SmallVectorImpl<Expr*> &Exprs,
		llvm::SmallVectorImpl<SourceLocation> &CommaLocs) {
	while (1) {
		ExprResult Var;
		ExprResult Expr(ParseAssignmentExpression(Var));

		if (Expr.isInvalid())
		  return true;

		Exprs.push_back(Expr.release());

		if (Tok.isNot(tok::Comma))
			return false;
		// Move to the next argument, remember where the comma was.
		CommaLocs.push_back(ConsumeToken());
	}
}

/// ParsePostfixExpressionSuffix - Once the leading part of a postfix-expression
/// is parsed, this method parses any suffixes that apply.
///
///        postfix-expression:
///          primary_expr
///          postfix-expression '(' ')'
///          postfix-expression '(' argument-expression-list ')'
///          postfix-expression '{' argument-expression-list '}'
///          postfix-expression '.' expression
///          postfix-expression '.(' expression ')'
///          postfix-expression QUOTE
///          postfix-expression TRANSPOSE
///          postfix-expression MATRIXPOWER postfix-expression
///          postfix-expression POWER postfix-expression
///          postfix-expression --
///          postfix-expression ++
///
///        argument-expression-list:
///          expression
///          argument-expression-list ',' expression
///
ExprResult Parser::ParsePostfixExpressionSuffix(ExprResult LHS) {
	// Now that the primary-expression piece of the postfix-expression has been
	// parsed, see if there are any postfix-expression pieces here.
	SourceLocation Loc;
	while (1) {
		switch (Tok.getKind()) {
		default: // Not a postfix-expression suffix.
			return move(LHS);

		case tok::LParentheses: { // p-e: p-e '(' argument-expression-list ')'
			ExprVector ArgExprs(Actions);
			CommaLocsTy CommaLocs;

			Loc = ConsumeParen();

			if (Tok.isNot(tok::RParentheses)) {
				if (ParseExpressionList(ArgExprs, CommaLocs)) {
					SkipUntil(tok::RParentheses);
					LHS = ExprError();
				}
			}

			// Match the ')'.
			if (LHS.isInvalid()) {
				SkipUntil(tok::RParentheses);
			} else if (Tok.isNot(tok::RParentheses)) {
				MatchRHSPunctuation(tok::RParentheses, Loc);
				LHS = ExprError();
			} else {
				assert((ArgExprs.size() == 0 ||
								ArgExprs.size()-1 == CommaLocs.size())&&
						"Unexpected number of commas!");

				LHS = Actions.ActOnArrayIndexingOrFunctionCallExpr(getCurScope(), LHS.get(),
						Loc, move_arg(ArgExprs), Tok.getLocation());
				ConsumeParen();
			}

			break;
		}

		case tok::LCurlyBraces: {
			ExprVector ArgExprs(Actions);
			CommaLocsTy CommaLocs;

			Loc = ConsumeBrace();

			if (Tok.isNot(tok::RCurlyBraces)) {
				if (ParseExpressionList(ArgExprs, CommaLocs)) {
					SkipUntil(tok::RCurlyBraces);
					LHS = ExprError();
				}
			}

			// Match the '}'.
			if (Tok.isNot(tok::RCurlyBraces)) {
				MatchRHSPunctuation(tok::RCurlyBraces, Loc);
				LHS = ExprError();
			} else {
				assert((ArgExprs.size() == 0 ||
								ArgExprs.size()-1 == CommaLocs.size())&&
						"Unexpected number of commas!");
				LHS = Actions.ActOnArrayIndexExpr(getCurScope(), NULL, LHS.get(),
						Loc, move_arg(ArgExprs), Tok.getLocation(), true);
				ConsumeBrace();
			}
			break;
		}

		case tok::Dot: {
			// postfix-expression: p-e '.' id-expression
			tok::TokenKind OpKind = Tok.getKind();
			SourceLocation OpLoc = ConsumeToken(); // Eat the "." token.

			ExprResult MemberName;

			LHS = Actions.ActOnMemberAccessExpr(getCurScope(), LHS, OpLoc,
					OpKind, MemberName, Tok.is(tok::LParentheses));
			break;
		}

		case tok::DotLParentheses:
			break;

		case tok::DoubleAdd:
		case tok::DoubleSub:
		case tok::DotQuote:
		case tok::Quote: {
			tok::TokenKind OpKind = Tok.getKind();
			SourceLocation SavedLoc = ConsumeToken();
			if (!LHS.isInvalid()) {
				LHS = Actions.ActOnUnaryOp(getCurScope(), SavedLoc, OpKind,true,
						LHS.get());
			}
			break;
		}

		case tok::DotCaret:
		case tok::Caret: {
			LHS = ParseRHSOfBinaryExpression(LHS, prec::PowerTranspose);
			if (LHS.isInvalid())
				LHS = ExprError();
			break;
		}
		}
	}
}

/// ParsePrimaryExpression() - Parse a primary expression
///        primary-expression:
///        	 identifier
///          superclass_identifier
///        	 meta_identifier
///          string
///          constant
///           '[' ']'
///        	'[' expression ']'
///           '{' '}'
///        	'{' expression '}'
///         '(' expression ')'
ExprResult Parser::ParsePrimaryExpression() {
	tok::TokenKind SavedKind = Tok.getKind();
	ExprResult Res;
	SourceLocation RLoc;

	switch(SavedKind) {
	case tok::Identifier:
		return ParseIdentifier();
	case tok::CharConstant:
	case tok::NumericConstant:
		return ParseNumericConstant();
	case tok::StringLiteral:
		return ParseStringLiteralExpression();
	case tok::LParentheses:
		return ParseParenExpression(RLoc);
	case tok::LSquareBrackets:
		return ParseArrayExpression(RLoc, false);
	case tok::LCurlyBraces:
		return ParseArrayExpression(RLoc, true);
	default:
		return ExprError();
	}

	return ParsePostfixExpressionSuffix(Res);
}

/// ParseIdentifier -  handle an Identifier
ExprResult Parser::ParseIdentifier() {
	ExprResult Res;
	if(Tok.isNot(tok::Identifier)) return ExprError();
	IdentifierInfo *II = Tok.getIdentifierInfo();
	SourceLocation ILoc = ConsumeToken();
	Type ty;
	DefinitionName Name(II);
	DefinitionNameInfo NameInfo(Name,ILoc);

	Res = Actions.ActOnIdExpression(Actions.getCurScope(), NameInfo, Tok.is(tok::LParentheses));
	return move(Res);
}

ExprResult Parser::ParseNumericConstant() {
	assert(Tok.is(tok::NumericConstant) && "should be a numeric constant");
	ExprResult Res = Actions.ActOnNumericConstant(Tok);
	ConsumeToken();
	return move(Res);
}
/// ParseBuiltinPrimaryExpression
///
///       primary-expression:
///
ExprResult  Parser::ParseBuiltinPrimaryExpression() {
//	ExprResult  Res;
//	const IdentifierInfo *BuiltinII = Tok.getIdentifierInfo();
//
//	tok::TokenKind T = Tok.getKind();
//	SourceLocation StartLoc = ConsumeToken(); // Eat the builtin identifier.
//
//	// These can be followed by postfix-expr pieces because they are
//	// primary-expressions.
//	return ParsePostfixExpressionSuffix(Res);
	return ExprError();
}

/// ParseMultiOutputExpression - this parses the units that start with a '['
/// token, which is usually as multi-output of assignment expression.
///
///        assign-LHS-expression:
///          unary-expression
///          [ argument-expression-list ]
///
/// This is also integrated into ParseUnaryExpression.
///
ExprResult Parser::ParseMultiOutputExpression(SourceLocation &RBracketLoc) {
	assert(Tok.is(tok::LSquareBrackets) && "Not a multi-outputs expr!");
	return ExprError();
}

/// ParseParenExpression - This parses the unit that starts with a '(' token,
/// based on what is allowed by ExprType.  The actual thing parsed is returned
/// in ExprType. If stopIfCastExpr is true, it will only return the parsed type,
/// not the parsed cast-expression.
///
///       primary-expression: [C99 6.5.1]
///         '(' expression ')'
///
ExprResult Parser::ParseParenExpression(SourceLocation &RParenLoc) {
	assert(Tok.is(tok::LParentheses) && "Not a paren expr!");

	SourceLocation OpenLoc = ConsumeParen();
	ExprResult  Result(ParseExpression());

	if (Result.isInvalid()) {
		SkipUntil(tok::RParentheses, true, true, true);
		return ExprError();
	}

	if (!Tok.is(tok::RParentheses))
		MatchRHSPunctuation(tok::RParentheses, OpenLoc);

	RParenLoc = ConsumeParen();

	// FIXME yabin we should do some clean step here
	// Result = Actions.ActOnParenExpr(OpenLoc, RParenLoc, Result.take());

	return Result;
}

/// ParseArrayExpression - This parses the unit that starts with a '(' token,
/// based on what is allowed by ExprType.  The actual thing parsed is returned
/// in ExprType. If stopIfCastExpr is true, it will only return the parsed type,
/// not the parsed cast-expression.
///
///       primary-expression:  (array-expression)
///         '[' NULL ']'
///         '[' rows ']'
///         '{' NULL '}'
///         '{' rows '}'
///
///       rows:
///         expression-list
///         rows ; expression-list
ExprResult Parser::ParseArrayExpression(SourceLocation &ROpLoc, bool isCell) {
	assert((Tok.is(tok::LSquareBrackets)|| Tok.is(tok::LCurlyBraces))
			&& "Not a bracket expr!");
	SourceLocation OpenLoc;
	if(isCell)
		OpenLoc = ConsumeBrace();
	else
		OpenLoc = ConsumeBracket();

	tok::TokenKind OPC = isCell ? tok::LCurlyBraces : tok::LSquareBrackets;

	ExprResult  Result(ParseArrayRows(OpenLoc, isCell));
	if(Result.isInvalid()) {
		SkipUntil(OPC, true, false, true);
		return ExprError();
	}

	// Match the ']'.
	if(isCell) {
		if (Tok.is(tok::RCurlyBraces))
			ROpLoc = ConsumeBrace();
		else
			MatchRHSPunctuation(tok::RCurlyBraces, OpenLoc);
	} else {
		if (Tok.is(tok::RSquareBrackets))
			ROpLoc = ConsumeBracket();
		else
			MatchRHSPunctuation(tok::RSquareBrackets, OpenLoc);
	}

	// FIXME yabin we should do some clean step here
	// if(isCell)
	//   Result = Actions.ActOnBracketExpr(OpenLoc, RParenLoc, Result.take());
	// else
	//   Result = Actions.ActOnBraceExpr(OpenLoc, RParenLoc, Result.take());
	return move(Result);
}

ExprResult Parser::ParseArrayRows(SourceLocation OpenLoc, bool isCell) {
	ExprVector ArgExprs(Actions);
	ExprVector Value(Actions);
	CommaLocsTy CommaLocs;
	ExprResult RowRes;
	tok::TokenKind Close = isCell ?	tok::RCurlyBraces :	tok::RSquareBrackets;

	while(1) {
		switch(Tok.getKind()) {
		case tok::Semicolon: {
			Value.push_back(RowRes.get());
			ArgExprs.clear(); // new row begin
			ConsumeToken(); // eat the semicolon
			break;
		}
		case tok::RCurlyBraces: {
			if(!RowRes.isInvalid())
				Value.push_back(RowRes.get());
			return Actions.ActOnMatrixOrCellExpr(move_arg(Value),true,
					OpenLoc,Tok.getLocation());
		}
		case tok::RSquareBrackets: {
			Value.push_back(RowRes.get());
			return Actions.ActOnMatrixOrCellExpr(move_arg(Value),false,
					OpenLoc,Tok.getLocation());
		}
		case tok::EoF:
			return ExprError();
		default: {
			if(ParseExpressionList(ArgExprs, CommaLocs)) {
				SkipUntil(Close, true, false, true);
				RowRes = ExprError();
			} else {
				RowRes = Actions.ActOnRowVectorExpr(move_arg(ArgExprs));
				if(RowRes.isInvalid()) {
					SkipUntil(Close, true, false, true);
					RowRes = ExprError();
				}
			}
		}
	} // switch
	} // while
}

/// ParseStringLiteralExpression - This handles the various token types that
/// form string literals, and also handles string concatenation [C99 5.1.1.2,
/// translation phase #6].
///
///       primary-expression: [C99 6.5.1]
///         string-literal
ExprResult Parser::ParseStringLiteralExpression() {
	assert(isTokenStringLiteral() && "Not a string literal!");

	// String concat.  Note that keywords like __func__ and __FUNCTION__ are not
	// considered to be strings for concatenation purposes.
	llvm::SmallVector<Token, 4> StringToks;

	do {
		StringToks.push_back(Tok);
		ConsumeStringToken();
	} while (isTokenStringLiteral());

	// Pass the set of string tokens, ready for concatenation, to the actions.
	return Actions.ActOnStringLiteral(&StringToks[0], StringToks.size());
}

ExprResult  Parser::ParseConstantExpression() {
	// C++ [basic.def.odr]p2:
	//   An expression is potentially evaluated unless it appears where an
	//   integral constant expression is required (see 5.19) [...].
	// FIXME yabin
	EnterExpressionEvaluationContext Unevaluated(Actions, Sema::Unevaluated);

	ExprResult  LHS(ParseUnaryExpression());
	return ParseRHSOfBinaryExpression(LHS, prec::Assignment);
}

/// ParseRHSOfBinaryExpression - Parse a binary expression that starts with
/// LHS and has a precedence of at least MinPrec.
ExprResult
Parser::ParseRHSOfBinaryExpression(ExprResult LHS, prec::Level MinPrec) {
	prec::Level NextTokPrec = getBinOpPrecedence(Tok.getKind(),
			getLang().OCTAVEKeywords);

	while(1) {
		// If this token has a lower precedence than we are allowed to parse (e.g.
		// because we are called recursively, or because the token is not a binop),
		// then we are done!
		if (NextTokPrec < MinPrec || Tok.isAtStartOfLine())
			return move(LHS);

		// Consume the operator, saving the operator token for error reporting.
		Token OpToken = Tok;
		SourceLocation OpLoc = ConsumeToken();

		ExprResult RHS(ParseUnaryExpression());
		if (RHS.isInvalid())
			LHS = ExprError();

		// Remember the precedence of this operator and get the precedence of the
		// operator immediately to the right of the RHS.
		prec::Level ThisPrec = NextTokPrec;
		NextTokPrec = getBinOpPrecedence(Tok.getKind(),
				getLang().OCTAVEKeywords);

		// if we meet a tenary colon operator
		if ((ThisPrec == NextTokPrec) && (NextTokPrec == prec::Colon)) {
			assert(Tok.getKind()==tok::Colon);
			SourceLocation SecondColonLoc = ConsumeToken();
			ExprResult Limit(ParseUnaryExpression());
			if (Limit.isInvalid())
				LHS = ExprError();
			else
				LHS = Actions.ActOnTernaryColonOp(getCurScope(), OpLoc, SecondColonLoc,
					LHS.take(), RHS.take(), Limit.take());
			return move(LHS);
		}

		// Assignment and conditional expressions are right-associative.
		bool isRightAssoc = (ThisPrec == prec::Assignment);

		// Get the precedence of the operator to the right of the RHS.  If it binds
		// more tightly with RHS than we do, evaluate it completely first.
		if (ThisPrec < NextTokPrec ||
				(ThisPrec == NextTokPrec && isRightAssoc)) {
			// If this is left-associative, only parse things on the RHS that bind
			// more tightly than the current operator.  If it is left-associative, it
			// is okay, to bind exactly as tightly.  For example, compile A=B=C=D as
			// A=(B=(C=D)), where each paren is a level of recursion here.
			// The function takes ownership of the RHS.
			RHS = ParseRHSOfBinaryExpression(RHS,
					static_cast<prec::Level>(ThisPrec + !isRightAssoc));

			if (RHS.isInvalid())
				LHS = ExprError();

			NextTokPrec = getBinOpPrecedence(Tok.getKind(),	getLang().OCTAVEKeywords);
		}
		assert((NextTokPrec <= ThisPrec || Tok.isAtStartOfLine())
				&& "Recursion didn't work!");
		if (!LHS.isInvalid()) {
			LHS = Actions.ActOnBinOp(getCurScope(), OpToken.getLocation(),
			                         OpToken.getKind(), LHS.take(), RHS.take());
		}
	}
	assert(Tok.isAtStartOfLine() && "should at start of line");
	return move(LHS);
}

/// This routine is called when the '@' is seen and consumed.
/// Current token is an Identifier and is not a 'try'. This
/// routine is necessary to disambiguate @try-statement from,
/// for example, @encode-expression.
///
ExprResult
Parser::ParseExpressionWithLeadingAt(SourceLocation AtLoc) {
	ExprResult LHS(ParseUnaryExpression());
	return ParseRHSOfBinaryExpression(LHS, prec::Comma);
}
