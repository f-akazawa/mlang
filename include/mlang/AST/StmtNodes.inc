#ifndef ABSTRACT_STMT
#  define ABSTRACT_STMT(Type) Type
#endif

#ifndef STMT_RANGE
#  define STMT_RANGE(Base, First, Last)
#endif

#ifndef LAST_STMT_RANGE
#  define LAST_STMT_RANGE(Base, First, Last) STMT_RANGE(Base, First, Last)
#endif

#ifndef EXPR
#  define EXPR(Type, Base) STMT(Type, Base)
#endif
ABSTRACT_STMT(EXPR(Expr, Stmt))

#ifndef DEFNREFEXPR
#  define DEFNREFEXPR(Type, Base) EXPR(Type, Base)
#endif
DEFNREFEXPR(DefnRefExpr, Expr)
#undef DEFNREFEXPR

#ifndef CHARACTERLITERAL
#  define CHARACTERLITERAL(Type, Base) EXPR(Type, Base)
#endif
CHARACTERLITERAL(CharacterLiteral, Expr)
#undef CHARACTERLITERAL

#ifndef STRINGLITERAL
#  define STRINGLITERAL(Type, Base) EXPR(Type, Base)
#endif
STRINGLITERAL(StringLiteral, Expr)
#undef STRINGLITERAL

#ifndef INTEGERLITERAL
#  define INTEGERLITERAL(Type, Base) EXPR(Type, Base)
#endif
INTEGERLITERAL(IntegerLiteral, Expr)
#undef INTEGERLITERAL

#ifndef INITLISTEXPR
#  define INITLISTEXPR(Type, Base) EXPR(Type, Base)
#endif
INITLISTEXPR(InitListExpr, Expr)
#undef INITLISTEXPR

#ifndef FLOATINGLITERAL
#  define FLOATINGLITERAL(Type, Base) EXPR(Type, Base)
#endif
FLOATINGLITERAL(FloatingLiteral, Expr)
#undef FLOATINGLITERAL

#ifndef IMAGINARYLITERAL
#  define IMAGINARYLITERAL(Type, Base) EXPR(Type, Base)
#endif
IMAGINARYLITERAL(ImaginaryLiteral, Expr)
#undef IMAGINARYLITERAL

#ifndef ROWVECTOREXPR
#  define ROWVECTOREXPR(Type, Base) EXPR(Type, Base)
#endif
ROWVECTOREXPR(RowVectorExpr, Expr)
#undef ROWVECTOREXPR

#ifndef CONCATEXPR
#  define CONCATEXPR(Type, Base) EXPR(Type, Base)
#endif
CONCATEXPR(ConcatExpr, Expr)
#undef CONCATEXPR

#ifndef MULTIOUTPUT
#  define MULTIOUTPUT(Type, Base) EXPR(Type, Base)
#endif
MULTIOUTPUT(MultiOutput, Expr)
#undef MULTIOUTPUT

#ifndef CELLCONCATEXPR
#  define CELLCONCATEXPR(Type, Base) EXPR(Type, Base)
#endif
CELLCONCATEXPR(CellConcatExpr, Expr)
#undef CELLCONCATEXPR

#ifndef ARRAYINDEX
#  define ARRAYINDEX(Type, Base) EXPR(Type, Base)
#endif
ARRAYINDEX(ArrayIndex, Expr)
#undef ARRAYINDEX

#ifndef FUNCTIONCALL
#  define FUNCTIONCALL(Type, Base) EXPR(Type, Base)
#endif
FUNCTIONCALL(FunctionCall, Expr)
#undef FUNCTIONCALL

#ifndef MEMBEREXPR
#  define MEMBEREXPR(Type, Base) EXPR(Type, Base)
#endif
MEMBEREXPR(MemberExpr, Expr)
#undef MEMBEREXPR

#ifndef UNARYOPERATOR
#  define UNARYOPERATOR(Type, Base) EXPR(Type, Base)
#endif
UNARYOPERATOR(UnaryOperator, Expr)
#undef UNARYOPERATOR

#ifndef BINARYOPERATOR
#  define BINARYOPERATOR(Type, Base) EXPR(Type, Base)
#endif
BINARYOPERATOR(BinaryOperator, Expr)
#ifndef COMPOUNDASSIGNOPERATOR
#  define COMPOUNDASSIGNOPERATOR(Type, Base) BINARYOPERATOR(Type, Base)
#endif
COMPOUNDASSIGNOPERATOR(CompoundAssignOperator, BinaryOperator)
#undef COMPOUNDASSIGNOPERATOR
STMT_RANGE(BinaryOperator, BinaryOperator, CompoundAssignOperator)
#undef BINARYOPERATOR

#ifndef FUNCHANDLE
#  define FUNCHANDLE(Type, Base) EXPR(Type, Base)
#endif
FUNCHANDLE(FuncHandle, Expr)
#undef FUNCHANDLE

#ifndef ANONFUNCHANDLE
#  define ANONFUNCHANDLE(Type, Base) EXPR(Type, Base)
#endif
ANONFUNCHANDLE(AnonFuncHandle, Expr)
#undef ANONFUNCHANDLE

#ifndef PARENEXPR
#  define PARENEXPR(Type, Base) EXPR(Type, Base)
#endif
PARENEXPR(ParenExpr, Expr)
#undef PARENEXPR

#ifndef OPAQUEVALUEEXPR
#  define OPAQUEVALUEEXPR(Type, Base) EXPR(Type, Base)
#endif
OPAQUEVALUEEXPR(OpaqueValueExpr, Expr)
#undef OPAQUEVALUEEXPR

#ifndef PARENLISTEXPR
#  define PARENLISTEXPR(Type, Base) EXPR(Type, Base)
#endif
PARENLISTEXPR(ParenListExpr, Expr)
#undef PARENLISTEXPR

#ifndef COLONEXPR
#  define COLONEXPR(Type, Base) EXPR(Type, Base)
#endif
COLONEXPR(ColonExpr, Expr)
#undef COLONEXPR

STMT_RANGE(Expr, DefnRefExpr, ColonExpr)
#undef EXPR

#ifndef CMD
#  define CMD(Type, Base) STMT(Type, Base)
#endif
ABSTRACT_STMT(CMD(Cmd, Stmt))

#ifndef DEFNCMD
#  define DEFNCMD(Type, Base) CMD(Type, Base)
#endif
DEFNCMD(DefnCmd, Cmd)
#undef DEFNCMD

#ifndef BREAKCMD
#  define BREAKCMD(Type, Base) CMD(Type, Base)
#endif
BREAKCMD(BreakCmd, Cmd)
#undef BREAKCMD

#ifndef CATCHCMD
#  define CATCHCMD(Type, Base) CMD(Type, Base)
#endif
CATCHCMD(CatchCmd, Cmd)
#undef CATCHCMD

#ifndef TRYCMD
#  define TRYCMD(Type, Base) CMD(Type, Base)
#endif
TRYCMD(TryCmd, Cmd)
#undef TRYCMD

#ifndef CONTINUECMD
#  define CONTINUECMD(Type, Base) CMD(Type, Base)
#endif
CONTINUECMD(ContinueCmd, Cmd)
#undef CONTINUESCMD

#ifndef FORCMD
#  define FORCMD(Type, Base) CMD(Type, Base)
#endif
FORCMD(ForCmd, Cmd)
#undef FORCMD

#ifndef IFCMD
#  define IFCMD(Type, Base) CMD(Type, Base)
#endif
IFCMD(IfCmd, Cmd)
#undef IFCMD

#ifndef ELSEIFCMD
#  define ELSEIFCMD(Type, Base) CMD(Type, Base)
#endif
ELSEIFCMD(ElseIfCmd, Cmd)
#undef ELSEIFCMD

#ifndef ELSECMD
#  define ELSECMD(Type, Base) CMD(Type, Base)
#endif
ELSECMD(ElseCmd, Cmd)
#undef ELSECMD

#ifndef THENCMD
#  define THENCMD(Type, Base) CMD(Type, Base)
#endif
THENCMD(ThenCmd, Cmd)
#undef THENCMD

#ifndef NULLCMD
#  define NULLCMD(Type, Base) CMD(Type, Base)
#endif
NULLCMD(NullCmd, Cmd)
#undef NULLCMD

#ifndef RETURNCMD
#  define RETURNCMD(Type, Base) CMD(Type, Base)
#endif
RETURNCMD(ReturnCmd, Cmd)
#undef RETURNCMD

#ifndef SWITCHCMD
#  define SWITCHCMD(Type, Base) CMD(Type, Base)
#endif
SWITCHCMD(SwitchCmd, Cmd)
#undef SWITCHCMD

#ifndef CASECMD
#  define CASECMD(Type, Base) CMD(Type, Base)
#endif
CASECMD(CaseCmd, Cmd)
#undef CASECMD

#ifndef OTHERWISECMD
#  define OTHERWISECMD(Type, Base) CMD(Type, Base)
#endif
OTHERWISECMD(OtherwiseCmd, Cmd)
#undef OTHERWISECMD

#ifndef BLOCKCMD
#  define BLOCKCMD(Type, Base) CMD(Type, Base)
#endif
BLOCKCMD(BlockCmd, Cmd)
#undef BLOCKCMD

#ifndef SCOPEDEFNCMD
#  define SCOPEDEFNCMD(Type, Base) CMD(Type, Base)
#endif
SCOPEDEFNCMD(ScopeDefnCmd, Cmd)
#undef SCOPEDEFNCMD

#ifndef WHILECMD
#  define WHILECMD(Type, Base) CMD(Type, Base)
#endif
WHILECMD(WhileCmd, Cmd)
#undef WHILECMD

STMT_RANGE(Cmd, BreakCmd, WhileCmd)
#undef CMD

LAST_STMT_RANGE(Stmt, CharacterLiteral, WhileCmd)

#undef STMT
#undef STMT_RANGE
#undef LAST_STMT_RANGE
#undef ABSTRACT_STMT
