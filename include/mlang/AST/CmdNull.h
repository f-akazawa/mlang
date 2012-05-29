//===--- CmdNull.h - Null statement  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the NullCmd.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_NULL_H_
#define MLANG_AST_CMD_NULL_H_

#include "mlang/AST/Stmt.h"

namespace mlang {
/// NullCmd - This is the null statement ";".
///
class NullCmd : public Cmd {
  SourceLocation SemiLoc;

public:
  explicit NullCmd(SourceLocation L)
    : Cmd(NullCmdClass), SemiLoc(L) {}

  /// \brief Build an empty null statement.
  explicit NullCmd(EmptyShell Empty) : Cmd(NullCmdClass, Empty) { }

  SourceLocation getSemiLoc() { return SemiLoc; }
  const SourceLocation getSemiLoc() const { return SemiLoc; }
  void setSemiLoc(SourceLocation L) { SemiLoc = L; }

  virtual SourceRange getSourceRange() const { return SourceRange(getSemiLoc()); }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == NullCmdClass;
  }
  static bool classof(const NullCmd *) { return true; }

  // Iterators
  child_range children() {
	  return child_range();
  }

  friend class ASTStmtReader;
  friend class ASTStmtWriter;
};
} // end namespace mlang

#endif /* MLANG_AST_CMD_NULL_H_ */
