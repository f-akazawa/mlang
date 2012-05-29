//===--- ParseDefn.cpp - Definition Parsing ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements ParseDefn* interfaces.
//
//===----------------------------------------------------------------------===//

#include "mlang/Parse/Parser.h"
#include "mlang/AST/DefinitionName.h"

using namespace mlang;

//===----------------------------------------------------------------------===//
// Definitions.
//===----------------------------------------------------------------------===//
Parser::DefnGroupPtrTy Parser::ParseDefinition(StmtVector &Stmts,
        unsigned Context, SourceLocation &DefnEnd) {
		return ParseSimpleDefinition(Stmts, Context, DefnEnd);
}

Parser::DefnGroupPtrTy Parser::ParseSimpleDefinition(StmtVector &Stmts,
    unsigned Context, SourceLocation &DefnEnd) {
	return DefnGroupPtrTy();
}

Parser::DefnGroupPtrTy Parser::ParseDefnGroup(unsigned Context,
                                              bool AllowFunctionDefinitions,
                                              SourceLocation *DefnEnd) {
  return DefnGroupPtrTy();
}

/// ParseInitDeclartor -
///
///       init-declarator:
///         declarator initializer
///       declarator:
///         Id-Expression
///       Id-Expression:
///         unqualified-id
///         qualified-id
///       qualified-id:
///         nestednamespecifier '.' unqualified-id
///       initializer:
///         '=' initializer-clause
///       initializer-clause:
///         assignment-expression
///         [ initializer-list ]
///         [ NULL ]
///       initializer-list:
///         initializer-clause
///         initializer-list ',' initializer-clause
Parser::DefnGroupPtrTy Parser::ParseInitDeclarator() {
	Defn *SingleVarDefn = NULL;

	// if it is a struct memberdefn, it will create two defns
	// one is a struct typedefn, the other is a memberdefn
	if(Tok.is(tok::Identifier)) {
		Token Next = NextToken();
		switch(Next.getKind()) {
		case tok::Dot: {
			// create a typedefn

			// create a memberdefn

			// return the defn group

		}
		break;
		case tok::Identifier: {
			//maybe a word-list command
			//e.g. disp hello_string
		}
		break;
		case tok::LParentheses: {
			// may be array indexing defn or function call
		}
		break;
		case tok::LCurlyBraces: {
			// cell array indexing
		}
		break;
		case tok::Assignment: {
			// may be this is a variable initialization
			IdentifierInfo *II = Tok.getIdentifierInfo();
			DefinitionName Name(II);
			SourceLocation NameLoc = ConsumeToken(); // eat the identifier
			DefinitionNameInfo NameInfo(Name,NameLoc);
			ExprResult Var = Actions.ActOnIdExpression(getCurScope(), NameInfo, false);
			ConsumeToken(); // eat the '='
			ExprResult Init(ParseInitializer());

		}
		break;
		default:
			return DefnGroupPtrTy();
		}
	}

	return Actions.ConvertDefnToDefnGroup(SingleVarDefn);
}

Defn *Parser::ParseStorageClassDeclaration() {
	return NULL;
}
