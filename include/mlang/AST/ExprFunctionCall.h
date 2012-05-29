//===--- FunctionCallExpr.h - Function Call Expression ----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the FucntionCall class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXPR_FUNCTION_CALL_H_
#define MLANG_AST_EXPR_FUNCTION_CALL_H_

#include "mlang/AST/Expr.h"

namespace mlang {
class Defn;
class FunctionDefn;
class ClassMethodDefn;

/// FunctionCall - Represents a function call (C99 6.5.2.2, C++ [expr.call]).
/// FunctionCall itself represents a normal function call, e.g., "f(x, 2)",
/// while its subclasses may represent alternative syntax that (semantically)
/// results in a function call. For example, CXXOperatorCallExpr is
/// a subclass for overloaded operator calls that use operator syntax, e.g.,
/// "str1 + str2" to resolve to a function call.
class FunctionCall : public Expr {
  enum { FN=0, ARGS_START=1 };
  Stmt **SubExprs;
  unsigned NumArgs;
  SourceLocation RParenLoc;

protected:
  // This version of the constructor is for derived classes.
  FunctionCall(ASTContext& C, StmtClass SC, Expr *fn, Expr **args, unsigned numargs,
               Type t, ExprValueKind VK, SourceLocation rparenloc);

public:
  FunctionCall(ASTContext& C, Expr *fn, Expr **args, unsigned numargs,
               Type t, ExprValueKind VK, SourceLocation rparenloc);

  /// \brief Build an empty call expression.
  FunctionCall(ASTContext &C, StmtClass SC, EmptyShell Empty);

  const Expr *getCallee() const { return cast<Expr>(SubExprs[FN]); }
  Expr *getCallee() { return cast<Expr>(SubExprs[FN]); }
  void setCallee(Expr *F) { SubExprs[FN] = F; }

  Defn *getCalleeDefn();
  const Defn *getCalleeDefn() const {
      return const_cast<FunctionCall*>(this)->getCalleeDefn();
    }

  /// \brief If the callee is a FunctionDecl, return it. Otherwise return 0.
  FunctionDefn *getDirectCallee();
  const FunctionDefn *getDirectCallee() const {
	  return const_cast<FunctionCall*>(this)->getDirectCallee();
  }

  /// getNumArgs - Return the number of actual arguments to this call.
  ///
  unsigned getNumArgs() const { return NumArgs; }

  /// getIdx - Return the specified argument.
  Expr *getArg(unsigned Arg) {
    assert(Arg < NumArgs && "Arg access out of range!");
    return cast<Expr>(SubExprs[Arg+ARGS_START]);
  }
  const Expr *getArg(unsigned Arg) const {
    assert(Arg < NumArgs && "Arg access out of range!");
    return cast<Expr>(SubExprs[Arg+ARGS_START]);
  }

  /// setArg - Set the specified argument.
  void setArg(unsigned Arg, Expr *ArgExpr) {
    assert(Arg < NumArgs && "Arg access out of range!");
    SubExprs[Arg+ARGS_START] = ArgExpr;
  }

  /// setNumArgs - This changes the number of arguments present in this call.
  /// Any orphaned expressions are deleted by this, and any new operands are set
  /// to null.
  void setNumArgs(ASTContext& C, unsigned NumArgs);

  typedef ExprIterator arg_iterator;
  typedef ConstExprIterator const_arg_iterator;

  arg_iterator arg_begin() { return SubExprs+ARGS_START; }
  arg_iterator arg_end() { return SubExprs+ARGS_START+getNumArgs(); }
  const_arg_iterator arg_begin() const { return SubExprs+ARGS_START; }
  const_arg_iterator arg_end() const { return SubExprs+ARGS_START+getNumArgs();}

  /// getNumCommas - Return the number of commas that must have been present in
  /// this function call.
  unsigned getNumCommas() const { return NumArgs ? NumArgs - 1 : 0; }

  /// isBuiltinCall - If this is a call to a builtin, return the builtin ID.  If
  /// not, return 0.
  unsigned isBuiltinCall(ASTContext &Context) const;

  /// getCallReturnType - Get the return type of the call expr. This is not
  /// always the type of the expr itself, if the return type is a reference
  /// type.
  Type getCallReturnType() const;

  SourceLocation getRParenLoc() const { return RParenLoc; }
  void setRParenLoc(SourceLocation L) { RParenLoc = L; }

  virtual SourceRange getSourceRange() const {
    return SourceRange(getCallee()->getLocStart(), getRParenLoc());
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == FunctionCallClass;
  }
  static bool classof(const FunctionCall *) { return true; }

  // Iterators
  child_range children() {
	  return child_range(&SubExprs[0], &SubExprs[0]+ NumArgs + ARGS_START);
  }
};

class ClassMethodCall : public FunctionCall {
public:
};
} // end namespace mlang

#endif /* MLANG_AST_EXPR_FUNCTION_CALL_H_ */
