//===--- CmdIF.h - IF Command for Mlang  -------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the IfCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_IF_H_
#define MLANG_AST_CMD_IF_H_

#include "mlang/AST/Stmt.h"

namespace mlang {
class ThenCmd: public Cmd {
	Stmt** SubStmts;
	SourceLocation StartLoc;
	unsigned NumStmts;

public:
	ThenCmd(ASTContext &C, SourceLocation SL, Stmt **thenblock,	unsigned numStmts);

	/// \brief Build an empty if/then/else statement
	explicit ThenCmd(EmptyShell Empty) :
		Cmd(ThenCmdClass, Empty) {
	}

	/// getElseIfClause - Return the specified argument.
	Stmt *getSubStmts(unsigned Arg) {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg];
	}
	const Stmt *getSubStmts(unsigned Arg) const {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg];
	}
	/// setElseIfClause - Set the specified argument.
	void setSubStmts(unsigned Arg, Stmt *ArgStmt) {
		assert(Arg < NumStmts && "Arg access out of range!");
		SubStmts[Arg] = ArgStmt;
	}

	SourceLocation getThenStartLoc() const {
		return StartLoc;
	}
	void setThenStartLoc(SourceLocation L) {
		StartLoc = L;
	}

	virtual SourceRange getSourceRange() const {
			return SourceRange(getThenStartLoc(), SubStmts[NumStmts]->getLocEnd());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ThenCmdClass;
	}
	static bool classof(const ThenCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubStmts[0], &SubStmts[0] + NumStmts);
	}
};

class ElseCmd: public Cmd {
	Stmt** SubStmts;
	SourceLocation ElseLoc;
	unsigned NumStmts;

public:
	ElseCmd(ASTContext &C, SourceLocation EL, Stmt **elseblock,
			unsigned numStmts);

	/// \brief Build an empty if/then/else statement
	explicit ElseCmd(EmptyShell Empty) :
		Cmd(ElseIfCmdClass, Empty) {
	}

	/// getElseIfClause - Return the specified argument.
	Stmt *getSubStmts(unsigned Arg) {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg];
	}
	const Stmt *getSubStmts(unsigned Arg) const {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg];
	}
	/// setElseIfClause - Set the specified argument.
	void setSubStmts(unsigned Arg, Stmt *ArgStmt) {
		assert(Arg < NumStmts && "Arg access out of range!");
		SubStmts[Arg] = ArgStmt;
	}

	SourceLocation getElseLoc() const {
		return ElseLoc;
	}
	void setElseLoc(SourceLocation L) {
		ElseLoc = L;
	}

	SourceRange getSourceRange() const {
			return SourceRange(getElseLoc(), SubStmts[NumStmts]->getLocEnd());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ElseCmdClass;
	}
	static bool classof(const ElseCmd *) {
		return true;
	}

	// Iterators over subexpressions.  The iterators will include iterating
	// over the initialization expression referenced by the condition variable.
	// Iterators
	child_range children() {
		return child_range(&SubStmts[0], &SubStmts[0] + NumStmts);
	}
};

class ElseIfCmd: public Cmd {
	enum {
		COND=0, STMT_START
	};
	Stmt** SubStmts;
	SourceLocation ElseIfLoc;
	unsigned NumStmts;

public:
	ElseIfCmd(ASTContext &C, SourceLocation EL, Expr *cond,	Stmt **then,
			unsigned numStmts);

	/// \brief Build an empty if/then/else statement
	explicit ElseIfCmd(EmptyShell Empty) :
		Cmd(ElseIfCmdClass, Empty) {
	}

	const Expr *getCond() const {
		return reinterpret_cast<Expr*> (SubStmts[COND]);
	}
	Expr *getCond() {
		return reinterpret_cast<Expr*> (SubStmts[COND]);
	}
	void setCond(Expr *E) {
		SubStmts[COND] = reinterpret_cast<Stmt *> (E);
	}

	/// getElseIfClause - Return the specified argument.
	Stmt *getSubStmts(unsigned Arg) {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg + STMT_START];
	}
	const Stmt *getSubStmts(unsigned Arg) const {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg + STMT_START];
	}
	/// setElseIfClause - Set the specified argument.
	void setSubStmts(unsigned Arg, Stmt *ArgStmt) {
		assert(Arg < NumStmts && "Arg access out of range!");
		SubStmts[Arg + STMT_START] = ArgStmt;
	}

	SourceLocation getElseIfLoc() const {
		return ElseIfLoc;
	}
	void setElseIfLoc(SourceLocation L) {
		ElseIfLoc = L;
	}

	SourceRange getSourceRange() const {
			return SourceRange(getElseIfLoc(), SubStmts[NumStmts]->getLocEnd());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ElseIfCmdClass;
	}
	static bool classof(const ElseIfCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubStmts[0], &SubStmts[0] + STMT_START + NumStmts);
	}
};

class IfCmd: public Cmd {
	enum {
		COND=0, THEN, ELSE, ELSEIF_START
	};
	Stmt** SubStmts;
	SourceLocation IfLoc;
	SourceLocation EndLoc; // This is end of 'end' keyword.
	SourceLocation ElseLoc;
	unsigned NumElseIfClauses;
	// elseif list
	ElseIfCmd *FirstElseIf;

public:
	IfCmd(ASTContext &C, SourceLocation IL, SourceLocation EndL,
			Expr *cond,	Stmt *then, Stmt** ElseIfClauses, unsigned nElseIfClauses,
			SourceLocation ElseLoc = SourceLocation(),
			Stmt *elsev =	0);

	/// \brief Build an empty if/then/else statement
	explicit IfCmd(EmptyShell Empty) :
		Cmd(IfCmdClass, Empty) {
	}

	const Expr *getCond() const {
		return reinterpret_cast<Expr*> (SubStmts[COND]);
	}
	void setCond(Expr *E) {
		SubStmts[COND] = reinterpret_cast<Stmt *> (E);
	}
	const Stmt *getThen() const {
		return SubStmts[THEN];
	}
	void setThen(Stmt *S) {
		SubStmts[THEN] = S;
	}
	const Stmt *getElse() const {
		return SubStmts[ELSE];
	}
	void setElse(Stmt *S) {
		SubStmts[ELSE] = S;
	}

	Expr *getCond() {
		return reinterpret_cast<Expr*> (SubStmts[COND]);
	}
	Stmt *getThen() {
		return SubStmts[THEN];
	}
	Stmt *getElse() {
		return SubStmts[ELSE];
	}

	/// getNumArgs - Return the number of actual arguments to this call.
	///
	unsigned getNumElseIfClauses() const {
		return NumElseIfClauses;
	}

	/// getElseIfClause - Return the specified argument.
	Stmt *getElseIfClause(unsigned Arg) {
		assert(Arg < NumElseIfClauses && "Arg access out of range!");
		return SubStmts[Arg + ELSEIF_START];
	}
	const Stmt *getElseIfClause(unsigned Arg) const {
		assert(Arg < NumElseIfClauses && "Arg access out of range!");
		return SubStmts[Arg + ELSEIF_START];
	}

	/// setElseIfClause - Set the specified argument.
	void setElseIfClause(unsigned Arg, Stmt *ArgStmt) {
		assert(Arg < NumElseIfClauses && "Arg access out of range!");
		SubStmts[Arg + ELSEIF_START] = ArgStmt;
	}

	/// setNumElseIfClauses - This changes the number of arguments present in
	/// this call. Any orphaned expressions are deleted by this, and any new
	/// operands are set to null.
	void setNumElseIfClauses(ASTContext& C, unsigned NumArgs);

	typedef StmtIterator elseif_iterator;
	typedef ConstStmtIterator const_elseif_iterator;

	elseif_iterator elseif_begin() {
		return SubStmts + ELSEIF_START;
	}
	elseif_iterator elseif_end() {
		return SubStmts + ELSEIF_START + getNumElseIfClauses();
	}
	const_elseif_iterator elseif_begin() const {
		return const_elseif_iterator(this->elseif_begin());
	}
	const_elseif_iterator elseif_end() const {
		return const_elseif_iterator(this->elseif_end());
	}

	SourceLocation getIfLoc() const {
		return IfLoc;
	}
	void setIfLoc(SourceLocation L) {
		IfLoc = L;
	}
	SourceLocation getEndLoc() const {
		return EndLoc;
	}
	void setEndLoc(SourceLocation L) {
		EndLoc = L;
	}
	SourceLocation getElseLoc() const {
		return ElseLoc;
	}
	void setElseLoc(SourceLocation L) {
		ElseLoc = L;
	}

	virtual SourceRange getSourceRange() const {
		return SourceRange(IfLoc, EndLoc);
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == IfCmdClass;
	}
	static bool classof(const IfCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubStmts[0], &SubStmts[0] + ELSEIF_START + NumElseIfClauses);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_IF_H_ */
