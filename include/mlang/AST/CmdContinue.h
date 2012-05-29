//===--- CmdContinue.h - Continue Command for Mlang -------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines ContinueCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_CONTINUE_H_
#define MLANG_AST_CMD_CONTINUE_H_

namespace mlang {
class ContinueCmd: public Cmd {
	SourceLocation ContinueLoc;

public:
	ContinueCmd(SourceLocation CL) : Cmd(ContinueCmdClass), ContinueLoc(CL) {}

	/// \brief Build an empty continue statement.
	explicit ContinueCmd(EmptyShell Empty) : Cmd(ContinueCmdClass, Empty) {}

	const SourceLocation getContinueLoc() const {
		return ContinueLoc;
	}
	SourceLocation getContinueLoc() {
		return ContinueLoc;
	}
	void setContinueLoc(SourceLocation L) {
		ContinueLoc = L;
	}

	virtual SourceRange getSourceRange() const {
		return SourceRange(getContinueLoc());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ContinueCmdClass;
	}
	static bool classof(const ContinueCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range();
	}
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_CONTINUE_H_ */
