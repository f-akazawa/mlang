//===--- ParseStmt.cpp - Statement and Block Parser -----------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements the Statement and Block portions of the Parser
// interface.
//
//===----------------------------------------------------------------------===//

#include "mlang/Parse/Parser.h"
#include "mlang/Basic/PrettyStackTrace.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Diag/Diagnostic.h"
#include "mlang/Parse/ParseDiagnostic.h"
#include "mlang/Sema/Scope.h"
#include "mlang/AST/ExprBinaryOperator.h"

using namespace mlang;

//===----------------------------------------------------------------------===//
// C99 6.8: Statements and Blocks.
//===----------------------------------------------------------------------===//

Defn *Parser::ParseStatementsBody(Defn *D) {
	StmtResult Body(ParseBlockStatementBody(false));
	if(Body.isInvalid())
		return NULL;

	if(isInScriptDefinition())
		return Actions.ActOnFinishScriptBody(D, Body.take());

	if(isInFunctionDefinition())
		return Actions.ActOnFinishFunctionBody(D, Body.take());

	return NULL; //should not reach here
}

/// ParseStatementOrDeclaration - Read 'statement' or 'declaration'.
///       StatementOrDeclaration:
///         statement
///         declaration
///
///       statement:
///         labeled-statement
///         compound-statement
///         expression-statement
///         selection-statement
///         iteration-statement
///         jump-statement
///
///       labeled-statement:
///         identifier ':' statement
///         'case' constant-expression ':' statement
///         'default' ':' statement
///
///       selection-statement:
///         if-statement
///         switch-statement
///
///       iteration-statement:
///         while-statement
///         do-statement
///         for-statement
///
///       expression-statement:
///         expression[opt] ';'
///
///       jump-statement:
///         'goto' identifier ';'
///         'continue' ';'
///         'break' ';'
///         'return' expression[opt] ';'
///
StmtResult Parser::ParseStatementOrDeclaration(StmtVector &Stmts,
		bool OnlyStatement) {
	const char *SemiError = 0;
	StmtResult Res;
	tok::TokenKind Kind  = Tok.getKind();

	switch (Kind) {
	case tok::kw_global:
	case tok::kw_persistent: {
		Defn *SCSpecifier = ParseStorageClassDeclaration();
		return Actions.ActOnDefnStmt(SCSpecifier);
	}
	case tok::kw_case:
		return ParseCaseCmd(/*attrs*/);
	case tok::kw_otherwise:
		return ParseOtherwiseCmd(/*attrs*/);
	case tok::kw_if:                  // if-statement
		return ParseIfCmd(/*attrs*/);
	case tok::kw_switch:              // switch-statement
	  return ParseSwitchCmd(/*attrs*/);
  case tok::kw_while:               // while-statement
    return ParseWhileCmd(/*attrs*/);
	case tok::kw_for:                 // for-statement
	  return ParseForCmd(/*attrs*/);
  case tok::kw_continue:            // continue-statement
	    Res = ParseContinueCmd(/*attrs*/);
	    SemiError = "continue";
	    break;
  case tok::kw_break:               // break-statement
	    Res = ParseBreakCmd(/*attrs*/);
	    SemiError = "break";
	    break;
	case tok::kw_return:              // return-statement
	    Res = ParseReturnCmd(/*attrs*/);
	    SemiError = "return";
	    break;
  case tok::kw_try:                 // try-block
	    return ParseTryBlock(/*attrs*/);
	default: {
		ExprResult Expr(ParseExpression());
		if (Expr.isInvalid()) {
			return StmtError();
		}

		// Otherwise, expect semicolon to shut up the answer show.
		bool showflag = true;
		if(Tok.getKind() == tok::Semicolon) {
			ConsumeToken();
			showflag = false;
		}

		return Actions.ActOnExprStmt(Expr.get(), showflag);
	}
	}

	return move(Res);
}

/// ParseBlockStatement - Parse a code block.
///
///       block-command:
///         { block-item-list[opt] }
///
///       block-item-list:
///         block-item
///         block-item-list block-item
///
///       block-item:
///         declaration
///         statement
///
StmtResult Parser::ParseBlockStatement(bool isStmtExpr) {
	return StmtError();
}

/// ParseBlockStatementBody - Parse a sequence of statements and invoke the
/// ActOnBlockCmd action. This expects the 'end' kwyword or 'EoF' at the
/// end of the block. It does not manipulate the scope stack.
StmtResult Parser::ParseBlockStatementBody(bool isStmtExpr) {
	SourceLocation StartLoc = Tok.getLocation();
	StmtVector Stmts(Actions);
	while (Tok.isNot(tok::kw_end) && Tok.isNot(tok::EoF)) {
		StmtResult R;
		R = ParseStatementOrDeclaration(Stmts, false);
		if (R.isUsable())
			Stmts.push_back(R.release());
	}

	SourceLocation EndLoc = Tok.getLocation();

	return Actions.ActOnBlockCmd(StartLoc, EndLoc, move_arg(Stmts),
	                             isStmtExpr);
}

/// ParseParenExprOrCondition:
///      '(' expression ')'
///      '(' condition ')'       [not allowed if OnlyAllowCondition=true]
///
/// This function parses and performs error recovery on the specified condition
/// or expression (depending on whether we're in C++ or C mode).  This function
/// goes out of its way to recover well.  It returns true if there was a parser
/// error (the right paren couldn't be found), which indicates that the caller
/// should try to recover harder.  It returns false if the condition is
/// successfully parsed.  Note that a successful parse can still have semantic
/// errors in the condition.
bool Parser::ParseParenExprOrCondition(ExprResult &ExprResult,
                                       Defn *&DefnResult,
                                       SourceLocation Loc,
                                       bool ConvertToBoolean) {
  return false;
}

StmtResult Parser::ParseElseIfBlock() {
	assert(Tok.is(tok::kw_elseif) && "shoule be elseif keyword");
	SourceLocation ELoc = ConsumeToken();
	ExprResult ElseIfCondition(ParseExpression());
	while(Tok.is(tok::Comma))
		ConsumeToken();

	StmtVector Stmts(Actions);

	while(Tok.isNot(tok::kw_elseif) && Tok.isNot(tok::kw_else)
			&& Tok.isNot(tok::kw_end) && Tok.isNot(tok::EoF)) {
		StmtResult ElseIfStmt = ParseStatementOrDeclaration(Stmts, false);
		if(ElseIfStmt.isUsable()) {
			Stmts.push_back(ElseIfStmt.release());
		}
	}

	return Actions.ActOnElseIfCmd(ELoc, ElseIfCondition.get(),
			move_arg(Stmts));
}

StmtResult Parser::ParseThenBlockBody() {
	SourceLocation ThenLoc = Tok.getLocation();
	while(Tok.is(tok::Comma)) {
		ConsumeToken();
		ThenLoc = Tok.getLocation();
	}

	StmtVector Stmts(Actions);

	while(Tok.isNot(tok::kw_elseif) && Tok.isNot(tok::kw_else)
			&& Tok.isNot(tok::kw_end) && Tok.isNot(tok::EoF)) {
		StmtResult ThenStmt = ParseStatementOrDeclaration(Stmts, false);
		if(ThenStmt.isUsable()) {
			Stmts.push_back(ThenStmt.release());
		}
	}

	return Actions.ActOnThenCmd(ThenLoc, move_arg(Stmts));
}

StmtResult Parser::ParseElseBlockBody(SourceLocation ELoc) {
	StmtVector Stmts(Actions);

	while(Tok.isNot(tok::kw_end) && Tok.isNot(tok::EoF)) {
		StmtResult ElseStmt = ParseStatementOrDeclaration(Stmts, false);
		if(ElseStmt.isUsable()) {
			Stmts.push_back(ElseStmt.release());
		}
	}

	return Actions.ActOnElseCmd(ELoc, move_arg(Stmts));
}
/// ParseIfStatement
///       if-statement:
///         if expression, statements, end
///         if expression1
///           statements1
///         elseif expression2
///           statements2
///         else
///           statements3
///         end
///
StmtResult Parser::ParseIfCmd(/*AttributeList *Attr*/) {
	assert(Tok.is(tok::kw_if) && "Not an if stmt!");
	SourceLocation IfLoc = ConsumeToken();  // eat the 'if'.

	// Parse the condition.
	ExprResult IfCondition(ParseExpression());
	if (IfCondition.isInvalid())
		return StmtError();

	// Read the 'then' stmt.
	SourceLocation ThenStmtLoc = Tok.getLocation();
	StmtResult ThenStmt = ParseThenBlockBody();

	StmtVector ElseIfClauses(Actions);
	StmtResult ElseIfStmt;

	while(Tok.is(tok::kw_elseif)) {
		ElseIfStmt = ParseElseIfBlock();
		if(ElseIfStmt.isUsable())
			ElseIfClauses.push_back(ElseIfStmt.release());
	}

	// If it has an else, parse it.
	SourceLocation ElseLoc;
	SourceLocation ElseStmtLoc;
	StmtResult ElseStmt;

	if (Tok.is(tok::kw_else)) {
		ElseLoc = ConsumeToken();
		ElseStmtLoc = Tok.getLocation();
		ElseStmt = ParseElseBlockBody(ElseLoc);
	}

	// If the then or else stmt is invalid and the other is valid (and present),
	// make turn the invalid one into a null stmt to avoid dropping the other
	// part.  If both are invalid, return error.
	if(ElseIfClauses.size() == 0) {
		if ((ThenStmt.isInvalid() && ElseStmt.isInvalid())
				|| (ThenStmt.isInvalid() && ElseStmt.get() == 0)
				|| (ThenStmt.get() == 0 && ElseStmt.isInvalid())) {
			// Both invalid, or one is invalid and other is non-present: return error.
			return StmtError();
		}
	}

	// Now if either are invalid, replace with a ';'.
	if (ThenStmt.isInvalid())
		ThenStmt = Actions.ActOnNullCmd(ThenStmtLoc);
	if (ElseStmt.isInvalid())
		ElseStmt = Actions.ActOnNullCmd(ElseStmtLoc);

	SourceLocation EndLoc;
	if(Tok.is(tok::kw_end))
		EndLoc = ConsumeToken();
	else {
		// show Diag info here "expecting end kw"
		assert(Tok.is(tok::EoF) && "should get to the end of file");
		EndLoc = Tok.getLocation();
	}


	if(ElseIfClauses.size() == 0)
		return Actions.ActOnIfCmd(IfLoc, EndLoc, IfCondition.get(), ThenStmt.get(),
				ElseLoc, ElseStmt.get());
	else
		return Actions.ActOnIfCmd(IfLoc, EndLoc, IfCondition.get(), ThenStmt.get(),
				move_arg(ElseIfClauses), ElseLoc, ElseStmt.get());
}
/// ParseCaseCmd -
///         'case' constant-expression ':' statement
StmtResult Parser::ParseCaseCmd() {
	assert(Tok.is(tok::kw_case) && "should at start of case clause");
	SourceLocation CLoc = ConsumeToken();
	ExprResult CasePattern(ParseExpression());

	while(Tok.is(tok::Comma))
		ConsumeToken();

	StmtVector Stmts(Actions);

	while(Tok.isNot(tok::kw_case) && Tok.isNot(tok::kw_otherwise)
				&& Tok.isNot(tok::kw_end) && Tok.isNot(tok::EoF)) {
		StmtResult SubStmt = ParseStatementOrDeclaration(Stmts, false);
		if(SubStmt.isUsable()) {
			Stmts.push_back(SubStmt.release());
		}
	}

	return Actions.ActOnCaseCmd(CLoc, CasePattern.get(),
			move_arg(Stmts));
}

/// ParseOtherwiseCmd -
///         'otherwise'  statement
StmtResult Parser::ParseOtherwiseCmd() {
	assert(Tok.is(tok::kw_otherwise) && "should at start of case clause");
  SourceLocation OLoc = ConsumeToken();

  while(Tok.is(tok::Comma))
	  ConsumeToken();

	StmtVector Stmts(Actions);

	while(Tok.isNot(tok::kw_end) && Tok.isNot(tok::EoF)) {
		StmtResult SubStmt = ParseStatementOrDeclaration(Stmts, false);
		if(SubStmt.isUsable()) {
			Stmts.push_back(SubStmt.release());
		}
	}

	return Actions.ActOnOtherwiseCmd(OLoc, move_arg(Stmts));
}
/// ParseSwitchCmd
/// switch switch_expr
///   case case_expr
///     statement, ..., statement
///   case {case_expr1, case_expr2, case_expr3, ...}
///     statement, ..., statement
///   otherwise
///     statement, ..., statement
/// end
StmtResult Parser::ParseSwitchCmd(/*AttributeList *Attr*/) {
	assert(Tok.is(tok::kw_switch) && "should at start of switch-case structure");
	SourceLocation SwitchLoc = ConsumeToken();

	ExprResult CondVar;
	ExprResult SwExpr(ParseAssignmentExpression(CondVar));
	if(SwExpr.isInvalid()) {
		SkipUntil(tok::kw_end, false, false, false);
		return StmtError();
	}

	StmtVector CaseClauses(Actions);
	while(Tok.is(tok::kw_case)) {
	  StmtResult R = ParseCaseCmd();
	  if(R.isUsable())
		  CaseClauses.push_back(R.release());
	}

	StmtResult OtherwiseCmd(true);
	if(Tok.is(tok::kw_otherwise)) {
		OtherwiseCmd = ParseOtherwiseCmd();
	}

	SourceLocation EndLoc;
	if(Tok.is(tok::kw_end)) {
		EndLoc = ConsumeToken();
	} else {
		// show diags info for expected end keyword
		SkipUntil(tok::kw_end, false, false, false);
		return StmtError(); //expect a end keyword of while structure
	}

	return Actions.ActOnSwitchCmd(SwitchLoc, EndLoc, CondVar.get(),
			SwExpr.get(),	move_arg(CaseClauses), OtherwiseCmd.get());
}

/// ParseWhileCmd
///       while expression, statements, end
StmtResult Parser::ParseWhileCmd(/*AttributeList *Attr*/) {
	assert(Tok.is(tok::kw_while) && "should at start of for structure");
	SourceLocation WhileLoc = ConsumeToken();
	ExprResult LoopCondition(ParseExpression());
	if(LoopCondition.isInvalid())
		return StmtError();

	SourceLocation EndLoc;
	StmtResult WhileBlock = ParseBlockStatementBody(false);

	if(Tok.is(tok::kw_end))
		SourceLocation EndLoc = ConsumeToken();
	else {
		SkipUntil(tok::kw_end, false, false);
		return StmtError(); //expect a end keyword of while structure
	}

	return Actions.ActOnWhileCmd(WhileLoc, EndLoc, LoopCondition.take(),
			WhileBlock.take());
}

/// ParseDoCmd
///       do-statement: [C99 6.8.5.2]
///         'do' statement 'while' '(' expression ')' ';'
/// Note: this lets the caller parse the end ';'.
StmtResult Parser::ParseDoCmd(/*AttributeList *Attr*/) {
	return StmtError();
}

/// ParseForCmd
///       for-command:
///         for x=initval:endval, statements, end
///         for x=initval:stepval:endval, statements, end
///
StmtResult Parser::ParseForCmd(/*AttributeList *Attr*/) {
	assert(Tok.is(tok::kw_for) && "should at start of for structure");
	SourceLocation ForLoc = ConsumeToken();
	ExprResult LoopVar;
	ExprResult LoopCondition(ParseAssignmentExpression(LoopVar));

	if(LoopCondition.isInvalid()) {
		return StmtError();
	}

	Expr *E = LoopCondition.get();
	Expr *LoopRange = NULL;
	if(isa<BinaryOperator>(*E)) {
		LoopRange = cast<BinaryOperator>(E)->getRHS();
	}	else
		return StmtError();


	SourceLocation EndLoc;
	StmtResult ForBlock = ParseBlockStatementBody(false);
	if(Tok.is(tok::kw_end))
		SourceLocation EndLoc = ConsumeToken();
	else {
		SkipUntil(tok::kw_end, false, false);
		return StmtError(); //expect a end keyword of for structure
	}

	return Actions.ActOnForCmd(ForLoc, EndLoc, LoopVar.take(),
			LoopRange, ForBlock.take());
}

/// ParseGotoStatement
///       jump-statement:
///         'goto' identifier ';'
/// [GNU]   'goto' '*' expression ';'
///
/// Note: this lets the caller parse the end ';'.
///
StmtResult Parser::ParseGotoStatement(/*AttributeList *Attr*/) {
	return StmtError();
}

/// ParseContinueCmd
///       jump-statement:
///         'continue' ';'
///
/// Note: this lets the caller parse the end ';'.
///
StmtResult Parser::ParseContinueCmd(/*AttributeList *Attr*/) {
	assert(Tok.is(tok::kw_continue) && "should be a continue statement");
	SourceLocation CLoc = ConsumeToken();
	return Actions.ActOnContinueCmd(CLoc, getCurScope());
}

/// ParseBreakCmd
///       jump-statement:
///         'break' ';'
///
/// Note: this lets the caller parse the end ';'.
///
StmtResult Parser::ParseBreakCmd(/*AttributeList *Attr*/) {
	assert(Tok.is(tok::kw_break) && "should be a break statement");
	SourceLocation BLoc = ConsumeToken();
	return Actions.ActOnBreakCmd(BLoc, getCurScope());
}

/// ParseReturnCmd
///       jump-statement:
///         'return' expression[opt] ';'
StmtResult Parser::ParseReturnCmd(/*AttributeList *Attr*/) {
	assert(Tok.is(tok::kw_return) && "should be a return statement");
	SourceLocation RLoc = ConsumeToken();
	ExprResult RetVal(ParseExpression());
	if(RetVal.isInvalid())
		RetVal = ExprError();

	return Actions.ActOnReturnCmd(RLoc, RetVal.get());
}

/// ParseTryBlock - Parse a try-block.
///
///       try-block:
///         'try' compound-statement handler-seq
///
StmtResult Parser::ParseTryBlock(/*AttributeList *Attr*/) {
	return StmtError();
}

/// ParseTryBlockCommon - Parse the common part of try-block and
/// function-try-block.
///
///       try-block:
///         'try' compound-statement handler-seq
///
///       function-try-block:
///         'try' ctor-initializer[opt] compound-statement handler-seq
///
///       handler-seq:
///         handler handler-seq[opt]
///
StmtResult Parser::ParseTryBlockCommon(SourceLocation TryLoc) {
	return StmtError();
}

/// ParseCatchBlock - Parse a catch block, called handler in the standard
///
///       handler:
///         'catch' '(' exception-declaration ')' compound-statement
///
///       exception-declaration:
///         type-specifier-seq declarator
///         type-specifier-seq abstract-declarator
///         type-specifier-seq
///         '...'
///
StmtResult Parser::ParseCatchBlock() {
	return StmtError();
}
