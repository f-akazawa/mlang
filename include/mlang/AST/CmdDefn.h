//===--- CmdDefn.h - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines DefnCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_DEFN_H_
#define MLANG_AST_CMD_DEFN_H_

#include "mlang/AST/Stmt.h"
#include "mlang/AST/DefnGroup.h"

namespace mlang {
class Defn;

class DefnCmd : public Cmd {
	  DefnGroupRef DG;
	  SourceLocation StartLoc, EndLoc;

	public:
	  DefnCmd(DefnGroupRef dg, SourceLocation startLoc,
	           SourceLocation endLoc) : Cmd(DefnCmdClass), DG(dg),
	                                    StartLoc(startLoc), EndLoc(endLoc) {}

	  /// \brief Build an empty declaration statement.
	  explicit DefnCmd(EmptyShell Empty) : Cmd(DefnCmdClass, Empty) { }

	  /// isSingleDecl - This method returns true if this DeclStmt refers
	  /// to a single Decl.
	  bool isSingleDefn() const {
	    return DG.isSingleDefn();
	  }

	  const Defn *getSingleDefn() const { return DG.getSingleDefn(); }
	  Defn *getSingleDefn() { return DG.getSingleDefn(); }

	  const DefnGroupRef getDefnGroup() const { return DG; }
	  DefnGroupRef getDefnGroup() { return DG; }
	  void setDefnGroup(DefnGroupRef DGR) { DG = DGR; }

	  SourceLocation getStartLoc() const { return StartLoc; }
	  void setStartLoc(SourceLocation L) { StartLoc = L; }
	  SourceLocation getEndLoc() const { return EndLoc; }
	  void setEndLoc(SourceLocation L) { EndLoc = L; }

	  SourceRange getSourceRange() const {
	    return SourceRange(StartLoc, EndLoc);
	  }

	  static bool classof(const Stmt *T) {
	    return T->getStmtClass() == DefnCmdClass;
	  }
	  static bool classof(const DefnCmd *) { return true; }

  // Iterators over subexpressions.
	  child_range children() {
		  return child_range(child_iterator(DG.begin(), DG.end()),
				                 child_iterator(DG.end(), DG.end()));
	  }

	  typedef DefnGroupRef::iterator defn_iterator;
	  typedef DefnGroupRef::const_iterator const_defn_iterator;

	  defn_iterator defn_begin() { return DG.begin(); }
	  defn_iterator defn_end() { return DG.end(); }
	  const_defn_iterator defn_begin() const { return DG.begin(); }
	  const_defn_iterator defn_end() const { return DG.end(); }
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_DEFN_H_ */
