//===--- CmdBlock.h - Block of commands -------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines BlockCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_BLOCK_H_
#define MLANG_AST_CMD_BLOCK_H_

#include "mlang/AST/Stmt.h"

namespace mlang {
class ScriptDefn;

/// BlockCmd - This represents a block of commands.
///
class BlockCmd: public Cmd {
	Stmt** Body;
	SourceLocation StartLoc;
	SourceLocation EndLoc;

public:
	BlockCmd(ASTContext& C, Stmt **StmtStart, unsigned NumStmts,
			     SourceLocation SL, SourceLocation EL)
	: Cmd(BlockCmdClass), StartLoc(SL), EndLoc(EL) {
		BlockCmdBits.NumStmts = NumStmts;

		if (NumStmts == 0) {
			Body = 0;
			return;
		}

		Body = new (C) Stmt*[NumStmts];
		memcpy(Body, StmtStart, NumStmts * sizeof(*Body));
	}

	// \brief Build an empty compound statement.
	explicit BlockCmd(EmptyShell Empty)
	: Cmd(BlockCmdClass, Empty), Body(0) {
		BlockCmdBits.NumStmts = 0;
	}

	void setStmts(ASTContext &C, Stmt **Stmts, unsigned NumStmts);

	bool body_empty() const {
		return BlockCmdBits.NumStmts == 0;
	}
	unsigned size() const {
		return BlockCmdBits.NumStmts;
	}

	const ScriptDefn *getContainerDefn() const {
		return NULL;
	}
	typedef Stmt** body_iterator;
	body_iterator body_begin() {
		return Body;
	}
	body_iterator body_end() {
		return Body + size();
	}
	Stmt *body_back() {
		return !body_empty() ? Body[size() - 1] : 0;
	}

	void setLastStmt(Stmt *S) {
		assert(!body_empty() && "setLastStmt");
		Body[size() - 1] = S;
	}

	typedef Stmt* const * const_body_iterator;
	const_body_iterator body_begin() const {
		return Body;
	}
	const_body_iterator body_end() const {
		return Body + size();
	}
	const Stmt *body_back() const {
		return !body_empty() ? Body[size() - 1] : 0;
	}

	typedef std::reverse_iterator<body_iterator> reverse_body_iterator;
	reverse_body_iterator body_rbegin() {
		return reverse_body_iterator(body_end());
	}
	reverse_body_iterator body_rend() {
		return reverse_body_iterator(body_begin());
	}

	typedef std::reverse_iterator<const_body_iterator>
			const_reverse_body_iterator;

	const_reverse_body_iterator body_rbegin() const {
		return const_reverse_body_iterator(body_end());
	}

	const_reverse_body_iterator body_rend() const {
		return const_reverse_body_iterator(body_begin());
	}

	SourceRange getSourceRange() const {
		return SourceRange(StartLoc, EndLoc);
	}

	 SourceLocation getStartLoc() const { return StartLoc; }
	 void setStartLoc(SourceLocation L) { StartLoc = L; }
	 SourceLocation getEndLoc() const { return EndLoc; }
	 void setEndLoc(SourceLocation L) { EndLoc = L; }

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == BlockCmdClass;
	}
	static bool classof(const BlockCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&Body[0], &Body[0] + BlockCmdBits.NumStmts);
	}
};
} // end namespace mlang

#endif /* CMDBLOCK_H_ */
