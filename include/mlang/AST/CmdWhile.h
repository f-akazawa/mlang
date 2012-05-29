//===--- CmdWhile.h - While Command for Mlang -------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the WhileCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_WHILE_H_
#define MLANG_AST_CMD_WHILE_H_

namespace mlang {
class VarDefn;

class WhileCmd: public Cmd {
	enum {
		COND, BODY, END_EXPR
	};
	Stmt* SubStmts[END_EXPR];
	SourceLocation WhileLoc;
	SourceLocation EndLoc; // This is end location of 'end' keyword.
public:
	WhileCmd(ASTContext &C, Expr *cond, Stmt *body,
			SourceLocation WL, SourceLocation EL);

	/// \brief Build an empty while statement.
	explicit WhileCmd(EmptyShell Empty) : Cmd(WhileCmdClass, Empty) {}

	Expr *getCond() {
		return reinterpret_cast<Expr*> (SubStmts[COND]);
	}
	const Expr *getCond() const {
		return reinterpret_cast<Expr*> (SubStmts[COND]);
	}
	void setCond(Expr *E) {
		SubStmts[COND] = reinterpret_cast<Stmt*> (E);
	}
	Stmt *getBody() {
		return SubStmts[BODY];
	}
	const Stmt *getBody() const {
		return SubStmts[BODY];
	}
	void setBody(Stmt *S) {
		SubStmts[BODY] = S;
	}

	SourceLocation getWhileLoc() {
		return WhileLoc;
	}
	const SourceLocation getWhileLoc() const {
		return WhileLoc;
	}
	void setWhileLoc(SourceLocation L) {
		WhileLoc = L;
	}

	SourceLocation getEndLoc() {
		return EndLoc;
	}
	const SourceLocation getEndLoc() const {
		return EndLoc;
	}
	void setEndLoc(SourceLocation L) {
		EndLoc = L;
	}

	SourceRange getSourceRange() const {
		return SourceRange(getWhileLoc(), getEndLoc());
	}
	static bool classof(const Stmt *T) {
		return T->getStmtClass() == WhileCmdClass;
	}
	static bool classof(const WhileCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubStmts[0], &SubStmts[0] + END_EXPR);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_WHILE_H_ */
