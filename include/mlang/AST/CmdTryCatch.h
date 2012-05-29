//===--- CmdCatch.h - Try - Catch Statement ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines TryCmd, CatchCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_TRY_CATCH_H_
#define MLANG_AST_CMD_TRY_CATCH_H_

#include "mlang/AST/Stmt.h"

namespace mlang {
class Vardefn;

class CatchCmd: public Cmd {
	SourceLocation CatchLoc;
	/// The exception-declaration of the type.
	VarDefn *ExceptionVar;
	/// The handler block.
	Stmt *HandlerBlock;

public:
	CatchCmd(SourceLocation catchLoc, VarDefn *exDecl, Stmt *handlerBlock) :
		Cmd(CatchCmdClass), CatchLoc(catchLoc), ExceptionVar(exDecl),
				HandlerBlock(handlerBlock) {
	}

	explicit CatchCmd(EmptyShell Empty) :
		Cmd(CatchCmdClass, Empty), ExceptionVar(0), HandlerBlock(0) {
	}

	VarDefn *getExceptionVariable() const {
		return reinterpret_cast<VarDefn*>(ExceptionVar);
	}
	void setExceptionVariable(VarDefn *ex) {
		ExceptionVar = reinterpret_cast<VarDefn*>(ex);
	}

	virtual SourceRange getSourceRange() const {
		return SourceRange(getCatchLoc(), getHandlerBlock()->getLocEnd());
	}

	SourceLocation getCatchLoc() const {
		return CatchLoc;
	}

	Type getCaughtType() const;
	Stmt *getHandlerBlock() const {
		return HandlerBlock;
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == CatchCmdClass;
	}
	static bool classof(const CatchCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&HandlerBlock, &HandlerBlock + 1);
	}

	friend class ASTStmtReader;
};

/// TryCmd - A try block, including catch handler.
///
class TryCmd: public Cmd {
	SourceLocation TryLoc;
	SourceLocation CatchLoc;
	SourceLocation EndLoc; // This is end of 'end' keyword.

	Stmt * TryBlk;
	Stmt * CatBlk;

public:
	TryCmd(SourceLocation TL, SourceLocation CL, SourceLocation EL,
			Stmt *tBlock, Stmt *cBlock) : Cmd(TryCmdClass),
			TryLoc(TL), CatchLoc(CL), EndLoc(EL), TryBlk(tBlock), CatBlk(cBlock) {}

	TryCmd(EmptyShell Empty) : Cmd(TryCmdClass, Empty) {	}


	SourceLocation getTryLoc() {
		return TryLoc;
	}
	SourceLocation getEndLoc() {
		return EndLoc;
	}
	SourceLocation getCatchLoc() {
		return CatchLoc;
	}
	const SourceLocation getTryLoc() const {
		return TryLoc;
	}
	const SourceLocation getEndLoc() const {
		return EndLoc;
	}
	const SourceLocation getCatchLoc() const {
		return CatchLoc;
	}
	void setTryLoc(SourceLocation L) {
		TryLoc = L;
	}
	void getEndLoc(SourceLocation L) {
		EndLoc = L;
	}
	void setCatchLoc(SourceLocation L) {
		CatchLoc = L;
	}

	BlockCmd *getTryBlock() {
		return llvm::cast<BlockCmd>(TryBlk);
	}
	const BlockCmd *getTryBlock() const {
		return llvm::cast<BlockCmd>(TryBlk);
	}

	CatchCmd *getCatchBlock() {
		return llvm::cast<CatchCmd>(CatBlk);
	}
	const CatchCmd *getCatchBlock() const {
		return llvm::cast<CatchCmd>(CatBlk);
	}

	SourceRange getSourceRange() const {
		return SourceRange(getTryLoc(), getEndLoc());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == TryCmdClass;
	}
	static bool classof(const TryCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(reinterpret_cast<Stmt **>(this + 1),
				reinterpret_cast<Stmt **>(this + 1) + 1 + 1);
	}

	friend class ASTStmtReader;
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_TRY_CATCH_H_ */
