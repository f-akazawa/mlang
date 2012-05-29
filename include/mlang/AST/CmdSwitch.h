//===--- CmdSwitch.h - Switch Command for Mlang -----------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines SwitchCase, CaseCmd, OtherwiseCmd and SwitchCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_SWITCH_H_
#define MLANG_AST_CMD_SWITCH_H_

#include "mlang/AST/Stmt.h"

namespace mlang {
// class VarDefn;

/// In its basic syntax, switch executes the statements associated with the
/// first case where switch_expr == case_expr. When the case expression is a
/// cell array (as in the second case above), the case_expr matches if any of
/// the elements of the cell array matches the switch expression. If no case
/// expression matches the switch expression, then control passes to the
/// otherwise case (if it exists). After the case is executed, program execution
/// resumes with the statement after the end.
///
/// The switch_expr can be a scalar or a string. A scalar switch_expr matches
/// a case_expr if switch_expr==case_expr. A string switch_expr matches a
/// case_expr if strcmp(switch_expr,case_expr) returns logical 1 (true).
///
class CaseCmd: public Cmd {
	enum {
		PATTERN = 0, STMT_START
	};
	Stmt** SubStmts;
	SourceLocation CaseLoc;
	unsigned NumStmts;

public:
	CaseCmd(ASTContext &C, SourceLocation caseLoc, Expr *pat, Stmt **subStmts,
			unsigned numStmts);

	/// \brief Build an empty switch case statement.
	explicit CaseCmd(EmptyShell Empty) :
		Cmd(CaseCmdClass) {
	}

	SourceLocation getCaseLoc() {
		return CaseLoc;
	}
	const SourceLocation getCaseLoc() const {
		return CaseLoc;
	}
	void setCaseLoc(SourceLocation L) {
		CaseLoc = L;
	}

	Expr *getPattern() {
		return reinterpret_cast<Expr*> (SubStmts[PATTERN]);
	}
	const Expr *getPattern() const {
		return reinterpret_cast<const Expr*> (SubStmts[PATTERN]);
	}
	void setPattern(Expr *Val) {
		SubStmts[PATTERN] = reinterpret_cast<Stmt*> (Val);
	}

	Stmt *getSubStmt(unsigned Arg) {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg + STMT_START];
	}

	const Stmt *getSubStmt(unsigned Arg) const {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg + STMT_START];
	}

	void setSubStmt(unsigned Arg, Stmt *S) {
		assert(Arg < NumStmts && "Arg access out of range!");
		SubStmts[Arg + STMT_START] = S;
	}


	SourceRange getSourceRange() const {
		// Handle deeply nested case statements with iteration instead of recursion.
		const CaseCmd *CS = this;
		return SourceRange(getCaseLoc(),
				CS->getSubStmt(NumStmts+STMT_START-1)->getLocEnd());
	}
	static bool classof(const Stmt *T) {
		return T->getStmtClass() == CaseCmdClass;
	}
	static bool classof(const CaseCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubStmts[0], &SubStmts[0] + NumStmts + PATTERN);
	}
};

/// Unlike the C language switch construct, the MATLAB switch does not
/// "fall through." That is, switch executes only the first matching case;
/// subsequent matching cases do not execute. Therefore, break statements
/// are not used.
class OtherwiseCmd: public Cmd {
	Stmt** SubStmts;
	SourceLocation OtherwiseLoc;
	unsigned NumStmts;

public:
	OtherwiseCmd(ASTContext &C, SourceLocation DL, Stmt **substmts,
			unsigned numStmts);

	/// \brief Build an empty default statement.
	explicit OtherwiseCmd(EmptyShell) : Cmd(OtherwiseCmdClass) {}

	Stmt *getSubStmt(unsigned Arg) {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg];
	}
	const Stmt *getSubStmt(unsigned Arg) const {
		assert(Arg < NumStmts && "Arg access out of range!");
		return SubStmts[Arg];
	}
	void setSubStmt(unsigned Arg, Stmt *S) {
		assert(Arg < NumStmts && "Arg access out of range!");
		SubStmts[Arg] = S;
	}

	SourceLocation getOtherwiseLoc() {
		return OtherwiseLoc;
	}
	const SourceLocation getOtherwiseLoc() const {
		return OtherwiseLoc;
	}
	void setOtherwiseLoc(SourceLocation L) {
		OtherwiseLoc = L;
	}

	SourceRange getSourceRange() const {
		return SourceRange(getOtherwiseLoc(), getSubStmt(NumStmts-1)->getLocEnd());
	}
	static bool classof(const Stmt *T) {
		return T->getStmtClass() == OtherwiseCmdClass;
	}
	static bool classof(const OtherwiseCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubStmts[0], &SubStmts[0] + NumStmts);
	}
};

/// SwitchCmd - This represents a 'switch' command.
///
class SwitchCmd: public Cmd {
	enum {
		VAR = 0, COND, OTHERWISE, CASE_START
	};
	Stmt** SubStmts;

	SourceLocation SwitchLoc;
	SourceLocation EndLoc;
	unsigned NumCases;

	/// If the SwitchStmt is a switch on an enum value, this records whether
	/// all the enum values were covered by CaseStmts.  This value is meant to
	/// be a hint for possible clients.
	unsigned AllEnumCasesCovered :1;

public:
	SwitchCmd(ASTContext &C, SourceLocation SLoc, SourceLocation ELoc, Expr *Var,
			Expr *Cond, Stmt** Cases,	unsigned numCases, Stmt *OtherwiseCmd);

	/// \brief Build a empty switch statement.
	explicit SwitchCmd(EmptyShell Empty) :
		Cmd(SwitchCmdClass, Empty) {
	}

	const Expr *getConditionVariable() const {
		return reinterpret_cast<Expr*> (SubStmts[VAR]);
	}
	Expr *getConditionVariable() {
		return reinterpret_cast<Expr*> (SubStmts[VAR]);
	}
	void setConditionVariable(Expr *E) {
		SubStmts[VAR] = reinterpret_cast<Stmt *> (E);
	}

	const Expr *getCondition() const {
		return reinterpret_cast<Expr*> (SubStmts[COND]);
	}
	Expr *getCondition() {
		return reinterpret_cast<Expr*> (SubStmts[COND]);
	}
	void setCondition(Expr *E) {
		SubStmts[COND] = reinterpret_cast<Stmt *> (E);
	}

	const Stmt *getOtherwise() const{
		return SubStmts[OTHERWISE];
	}
	Stmt *getOtherwise() {
		return SubStmts[OTHERWISE];
	}
	void setOtherwise(Stmt *S) {
		SubStmts[OTHERWISE] = S;
	}

	SourceLocation getSwitchLoc() const {
		return SwitchLoc;
	}
	void setSwitchLoc(SourceLocation L) {
		SwitchLoc = L;
	}

	SourceLocation getEndLoc() const {
		return EndLoc;
	}
	void setEndLoc(SourceLocation L) {
		EndLoc = L;
	}

	/// getNumCases - Return the number of actual arguments to this call.
	///
	unsigned getNumCases() const {
		return NumCases;
	}

	/// getCaseClause - Return the specified argument.
	Stmt *getCaseClause(unsigned Arg) {
		assert(Arg < NumCases && "Arg access out of range!");
		return SubStmts[Arg + CASE_START];
	}
	const Stmt *getCaseClause(unsigned Arg) const {
		assert(Arg < NumCases && "Arg access out of range!");
		return SubStmts[Arg + CASE_START];
	}
	/// setCaseClause - Set the specified argument.
	void setCaseClause(unsigned Arg, Stmt *ArgStmt) {
		assert(Arg < NumCases && "Arg access out of range!");
		SubStmts[Arg + CASE_START] = ArgStmt;
	}

	/// Set a flag in the SwitchCmd indicating that if the 'switch (X)' is a
	/// switch over an enum value then all cases have been explicitly covered.
	void setAllEnumCasesCovered() {
		AllEnumCasesCovered = 1;
	}

	/// Returns true if the SwitchCmd is a switch of an enum value and all cases
	/// have been explicitly covered.
	bool isAllEnumCasesCovered() const {
		return (bool) AllEnumCasesCovered;
	}

	SourceRange getSourceRange() const {
		return SourceRange(SwitchLoc, EndLoc);
	}
	static bool classof(const Stmt *T) {
		return T->getStmtClass() == SwitchCmdClass;
	}
	static bool classof(const SwitchCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubStmts[0], &SubStmts[0] + CASE_START + NumCases);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_SWITCH_H_ */
