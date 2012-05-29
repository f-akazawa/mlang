//===--- CmdScopeDef.h - global or persistent declaration statement ----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines ScopeDefCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_SCOPE_DEF_H_
#define MLANG_AST_CMD_SCOPE_DEF_H_

namespace mlang {
/// ScopeDefCmd - global or persistent declaration statement.
class ScopeDefnCmd: public Cmd {
	Stmt **SubExprs;
	unsigned NumExprs;

	SourceLocation SpecifierLoc;
	bool pers, glob;

public:
	ScopeDefnCmd(ASTContext &C, Stmt **vars, unsigned nexpr, bool pers,
			bool glob, SourceLocation SL) :
		Cmd(ScopeDefnCmdClass), NumExprs(nexpr), pers(pers), glob(glob) {

		SubExprs = new (C) Stmt*[nexpr];

		for (unsigned i = 0; i < nexpr; ++i)
			SubExprs[i] = vars[i];

		SpecifierLoc = SL;
	}

	/// \brief Build an empty return expression.
	explicit ScopeDefnCmd(EmptyShell Empty) :
		Cmd(ScopeDefnCmdClass, Empty) {
	}

	bool isPersistent() {
		return pers;
	}
	bool isGlobal() {
		return glob;
	}

	unsigned getNumberofVars() {
			return NumExprs;
	}
	unsigned getNumberofVars() const {
		return NumExprs;
	}
	/// getVar - Return the specified argument.
	Expr *getVar(unsigned VarN) {
		assert(VarN < NumExprs && "Arg access out of range!");
		return cast<Expr> (SubExprs[VarN]);
	}
	const Expr *getVar(unsigned VarN) const {
		assert(VarN < NumExprs && "Arg access out of range!");
		return cast<Expr> (SubExprs[VarN]);
	}
	/// setVar - Set the specified argument.
	void setVar(unsigned VarN, Expr *VarExpr) {
		assert(VarN < NumExprs && "Arg access out of range!");
		SubExprs[VarN] = reinterpret_cast<Stmt*>(VarExpr);
	}

	SourceLocation getSpecifierLoc() {
		return SpecifierLoc;
	}
	const SourceLocation getSpecifierLoc() const {
		return SpecifierLoc;
	}
	void setSpecifierLoc(SourceLocation L) {
		SpecifierLoc = L;
	}

	typedef ExprIterator arg_iterator;
	typedef ConstExprIterator const_arg_iterator;

	arg_iterator arg_begin() { return SubExprs; }
	arg_iterator arg_end() { return SubExprs+getNumberofVars(); }
	const_arg_iterator arg_begin() const { return SubExprs; }
	const_arg_iterator arg_end() const { return SubExprs+getNumberofVars();}
	Stmt *arg_back() { return SubExprs[getNumberofVars() - 1]; }
	Stmt *arg_back() const { return SubExprs[getNumberofVars() - 1]; }

	SourceRange getSourceRange() const {
		return SourceRange(getSpecifierLoc(), arg_back()->getLocEnd());
	}

	static bool classof(const Stmt *T) {
		return T->getStmtClass() == ScopeDefnCmdClass;
	}
	static bool classof(const ScopeDefnCmd *) {
		return true;
	}

	// Iterators
	child_range children() {
		return child_range(&SubExprs[0], &SubExprs[0] + NumExprs);
	}
};
} // end namespace mlang

#endif /* MLANG_AST_DEFN_H_ */
