//===--- CmdBreak.h - Break Command for Mlang -------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the BreakCommand.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_BREAK_H_
#define MLANG_AST_CMD_BREAK_H_

#include "mlang/AST/Stmt.h"
namespace mlang {
class BreakCmd: public Cmd {
	SourceLocation BreakLoc;

public:
	BreakCmd(SourceLocation BL) : Cmd(BreakCmdClass), BreakLoc(BL) {}

	/// \brief Build an empty break statement.
	explicit BreakCmd(EmptyShell Empty) : Cmd(BreakCmdClass, Empty) {}

	SourceLocation getBreakLoc() const {
		return BreakLoc;
	}
	void setBreakLoc(SourceLocation L) {
		BreakLoc = L;
	}

	SourceRange getSourceRange() const {
		return SourceRange(getBreakLoc());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == BreakCmdClass;
	}
	static bool classof(const BreakCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range();
	}
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_BREAK_H_ */
