//===--- CmdReturn.h - Return Statement -------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines ReturnCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_RETURN_H_
#define MLANG_AST_CMD_RETURN_H_

#include "mlang/AST/Stmt.h"

namespace mlang {

class ReturnCmd: public Cmd {
	SourceLocation RetLoc;

public:
	ReturnCmd(/*ASTContext &C,*/ SourceLocation RL)
	: Cmd(ReturnCmdClass), RetLoc(RL) {}

	/// \brief Build an empty return expression.
	explicit ReturnCmd(EmptyShell Empty) : Cmd(ReturnCmdClass, Empty) {}

	SourceLocation getReturnLoc() const {
		return RetLoc;
	}
	void setReturnLoc(SourceLocation L) {
		RetLoc = L;
	}

	virtual SourceRange getSourceRange() const {
		return SourceRange(RetLoc);
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ReturnCmdClass;
	}
	static bool classof(const ReturnCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range();
	}
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_RETURN_H_ */
