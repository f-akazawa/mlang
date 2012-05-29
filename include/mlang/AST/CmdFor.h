//===--- CmdFor.h - For Command for Mlang  ----------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the ForCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_FOR_H_
#define MLANG_AST_CMD_FOR_H_

namespace mlang {

class ForCmd: public Cmd {
	enum {
		LOOPVAR, RANGE, BODY, END_EXPR
	};
	Stmt* SubStmts[END_EXPR]; // SubStmts[INIT] is an expression or declstmt.
	SourceLocation ForLoc;
	SourceLocation EndLoc; // This is end location of 'end' Keyword.

public:
	ForCmd(ASTContext &C, Expr *condVar, Expr *range,
			Stmt *Body, SourceLocation FL, SourceLocation EL);

	/// \brief Build an empty for statement.
	explicit ForCmd(EmptyShell Empty) : Cmd(ForCmdClass, Empty) {}

	/// \brief Retrieve the variable declared in this "for" statement, if any.
	///
	/// In the following example, "y" is the condition variable.
	/// \code
	/// for (int x = random(); int y = mangle(x); ++x) {
	///   // ...
	/// }
	/// \endcode
	VarDefn *getLoopVariable() const;
	void setLoopVariable(ASTContext &C, VarDefn *V);

	Expr *getRange() {
		return reinterpret_cast<Expr*> (SubStmts[RANGE]);
	}
	Stmt *getBody() {
		return SubStmts[BODY];
	}
	const Expr *getRange() const {
		return reinterpret_cast<Expr*> (SubStmts[RANGE]);
	}
	const Stmt *getBody() const {
		return SubStmts[BODY];
	}

	void setRange(Expr *E) {
		SubStmts[RANGE] = reinterpret_cast<Stmt*> (E);
	}

	void setBody(Stmt *S) {
		SubStmts[BODY] = S;
	}

	SourceLocation getForLoc() {
		return ForLoc;
	}
	const SourceLocation getForLoc() const {
		return ForLoc;
	}
	void setForLoc(SourceLocation L) {
		ForLoc = L;
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
		return SourceRange(getForLoc(), getEndLoc());
	}
	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ForCmdClass;
	}
	static bool classof(const ForCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubStmts[0], &SubStmts[0] + END_EXPR);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_FOR_H_ */
