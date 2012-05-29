//===--- Expr.cpp - Expression AST Node Implementation --------------------===//
//
// Copyright (C) 2010 Yabin Hu @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements the Expr class and subclasses.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/Expr.h"
#include "mlang/AST/APValue.h"
#include "mlang/AST/DefnSub.h"
#include "mlang/AST/NestedNameSpecifier.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/AST/StmtVisitor.h"
#include "mlang/Lex/LiteralSupport.h"
#include "mlang/Lex/Lexer.h"
#include "mlang/Basic/Builtins.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/TargetInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
using namespace mlang;

void Expr::ANCHOR() {} // key function for Expr class.

/// isKnownToHaveBooleanValue - Return true if this is an integer expression
/// that is known to return 0 or 1.  This happens for _Bool/bool expressions
/// but also int expressions which are produced by things like comparisons in
/// C.
bool Expr::isKnownToHaveBooleanValue() const {
  // If this value has _Bool type, it is obvious 0/1.
  if (getType()->isLogicalType()) return true;
  // If this is a non-scalar-integer type, we don't care enough to try. 
  if (!getType()->isNumericType()) return false;
  
  if (const ParenExpr *PE = dyn_cast<ParenExpr>(this))
    return PE->getSubExpr()->isKnownToHaveBooleanValue();
  
  if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(this)) {
    switch (UO->getOpcode()) {
    case UO_Plus:
      return UO->getSubExpr()->isKnownToHaveBooleanValue();
    default:
      return false;
    }
  }
  
  if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(this)) {
    switch (BO->getOpcode()) {
    default: return false;
    case BO_LT:   // Relational operators.
    case BO_GT:
    case BO_LE:
    case BO_GE:
    case BO_EQ:   // Equality operators.
    case BO_NE:
    case BO_LAnd: // AND operator.
    case BO_LOr:  // Logical OR operator.
      return true;
        
    case BO_And:  // Bitwise AND operator.
    case BO_Or:   // Bitwise OR operator.
      // Handle things like (x==2)|(y==12).
      return BO->getLHS()->isKnownToHaveBooleanValue() &&
             BO->getRHS()->isKnownToHaveBooleanValue();
        
    case BO_Comma:
    case BO_Assign:
      return BO->getRHS()->isKnownToHaveBooleanValue();
    }
  }
  
  return false;
}

//===----------------------------------------------------------------------===//
// Primary Expressions.
//===----------------------------------------------------------------------===//
void APNumericStorage::setIntValue(ASTContext &C, const llvm::APInt &Val) {
  if (hasAllocation())
    C.Deallocate(pVal);

  BitWidth = Val.getBitWidth();
  unsigned NumWords = Val.getNumWords();
  const uint64_t* Words = Val.getRawData();
  if (NumWords > 1) {
    pVal = new (C) uint64_t[NumWords];
    std::copy(Words, Words + NumWords, pVal);
  } else if (NumWords == 1)
    VAL = Words[0];
  else
    VAL = 0;
}

IntegerLiteral *
IntegerLiteral::Create(ASTContext &C, const llvm::APInt &V,
                       Type type, SourceLocation l) {
  return new (C) IntegerLiteral(C, V, type, l);
}

IntegerLiteral *
IntegerLiteral::Create(ASTContext &C, EmptyShell Empty) {
  return new (C) IntegerLiteral(Empty);
}

FloatingLiteral *
FloatingLiteral::Create(ASTContext &C, const llvm::APFloat &V,
                        bool isexact, Type Type, SourceLocation L) {
  return new (C) FloatingLiteral(C, V, isexact, Type, L);
}

FloatingLiteral *
FloatingLiteral::Create(ASTContext &C, EmptyShell Empty) {
  return new (C) FloatingLiteral(Empty);
}

/// getValueAsApproximateDouble - This returns the value as an inaccurate
/// double.  Note that this may cause loss of precision, but is useful for
/// debugging dumps, etc.
double
FloatingLiteral::getValueAsApproximateDouble() const {
  llvm::APFloat V = getValue();
  bool ignored;
  V.convert(llvm::APFloat::IEEEdouble, llvm::APFloat::rmNearestTiesToEven,
            &ignored);
  return V.convertToDouble();
}

StringLiteral *
StringLiteral::Create(ASTContext &C, const char *StrData,
                      unsigned ByteLength, Type Ty,
                      const SourceLocation *Loc, unsigned NumStrs) {
  // Allocate enough space for the StringLiteral plus an array of locations for
  // any concatenated string tokens.
  void *Mem = C.Allocate(sizeof(StringLiteral)+
                         sizeof(SourceLocation)*(NumStrs-1),
                         llvm::alignOf<StringLiteral>());
  StringLiteral *SL = new (Mem) StringLiteral(Ty);

  // OPTIMIZE: could allocate this appended to the StringLiteral.
  char *AStrData = new (C, 1) char[ByteLength];
  memcpy(AStrData, StrData, ByteLength);
  SL->StrData = AStrData;
  SL->ByteLength = ByteLength;
  SL->TokLocs[0] = Loc[0];
  SL->NumConcatenated = NumStrs;

  if (NumStrs != 1)
    memcpy(&SL->TokLocs[1], Loc+1, sizeof(SourceLocation)*(NumStrs-1));
  return SL;
}

StringLiteral *
StringLiteral::CreateEmpty(ASTContext &C, unsigned NumStrs) {
  void *Mem = C.Allocate(sizeof(StringLiteral)+
                         sizeof(SourceLocation)*(NumStrs-1),
                         llvm::alignOf<StringLiteral>());
  StringLiteral *SL = new (Mem) StringLiteral(Type());
  SL->StrData = 0;
  SL->ByteLength = 0;
  SL->NumConcatenated = NumStrs;
  return SL;
}

void
StringLiteral::setString(ASTContext &C, llvm::StringRef Str) {
  char *AStrData = new (C, 1) char[Str.size()];
  memcpy(AStrData, Str.data(), Str.size());
  StrData = AStrData;
  ByteLength = Str.size();
}

/// getLocationOfByte - Return a source location that points to the specified
/// byte of this string literal.
///
/// Strings are amazingly complex.  They can be formed from multiple tokens and
/// can have escape sequences in them in addition to the usual trigraph and
/// escaped newline business.  This routine handles this complexity.
///
SourceLocation
StringLiteral::getLocationOfByte(unsigned ByteNo, const SourceManager &SM,
                  const LangOptions &Features, const TargetInfo &Target) const {

  // Loop over all of the tokens in this string until we find the one that
  // contains the byte we're looking for.
  unsigned TokNo = 0;
  while (1) {
    assert(TokNo < getNumConcatenated() && "Invalid byte number!");
    SourceLocation StrTokLoc = getStrTokenLoc(TokNo);
    
    // Get the spelling of the string so that we can get the data that makes up
    // the string literal, not the identifier for the macro it is potentially
    // expanded through.
    SourceLocation StrTokSpellingLoc = SM.getSpellingLoc(StrTokLoc);
    
    // Re-lex the token to get its length and original spelling.
    std::pair<FileID, unsigned> LocInfo =SM.getDecomposedLoc(StrTokSpellingLoc);
    bool Invalid = false;
    llvm::StringRef Buffer = SM.getBufferData(LocInfo.first, &Invalid);
    if (Invalid)
      return StrTokSpellingLoc;
    
    const char *StrData = Buffer.data()+LocInfo.second;
    
    // Create a langops struct and enable trigraphs.  This is sufficient for
    // relexing tokens.
    LangOptions LangOpts;
    
    // Create a lexer starting at the beginning of this token.
    Lexer TheLexer(StrTokSpellingLoc, Features, Buffer.begin(), StrData,
                   Buffer.end());
    Token TheTok;
    TheLexer.LexFromRawLexer(TheTok);
    
    // Use the StringLiteralParser to compute the length of the string in bytes.
    StringLiteralParser SLP(&TheTok, 1, SM, Features, Target);
    unsigned TokNumBytes = SLP.GetStringLength();
    
    // If the byte is in this token, return the location of the byte.
    if (ByteNo < TokNumBytes ||
        (ByteNo == TokNumBytes && TokNo == getNumConcatenated())) {
      unsigned Offset = SLP.getOffsetOfStringByte(TheTok, ByteNo); 
      
      // Now that we know the offset of the token in the spelling, use the
      // preprocessor to get the offset in the original source.
      return Lexer::AdvanceToTokenCharacter(StrTokLoc, Offset, SM, Features);
    }
    
    // Move to the next string token.
    ++TokNo;
    ByteNo -= TokNumBytes;
  }
}



/// getOpcodeStr - Turn an Opcode enum value into the punctuation char it
/// corresponds to, e.g. "sizeof" or "[pre]++".
const char *
UnaryOperator::getOpcodeStr(Opcode Op) {
  switch (Op) {
  default: assert(0 && "Unknown unary operator");
  case UO_PostInc: return "++";
  case UO_PostDec: return "--";
  case UO_PreInc:  return "++";
  case UO_PreDec:  return "--";
  case UO_Plus:    return "+";
  case UO_Minus:   return "-";
  case UO_LNot:     return "~";
  //case UO_Not:    return "!";
  case UO_Handle:    return "@";
  }
}

//===----------------------------------------------------------------------===//
// Postfix Operators.
//===----------------------------------------------------------------------===//
ArrayIndex::ArrayIndex(ASTContext& C, StmtClass SC, Expr *fn, Expr **args,
                   unsigned numargs, Type t, ExprValueKind VK,
                   SourceLocation rparenloc, bool isCell)
  : Expr(SC, t, VK), NumArgs(numargs), CellIndexed(isCell) {

  SubExprs = new (C) Stmt*[numargs+1];
  SubExprs[BASE] = fn;
  for (unsigned i = 0; i != numargs; ++i) {
    SubExprs[i+INDEX_START] = args[i];
  }

  RParenLoc = rparenloc;
}

ArrayIndex::ArrayIndex(ASTContext& C, Expr *fn, Expr **args,
                   unsigned numargs, Type t, ExprValueKind VK,
                   SourceLocation rparenloc, bool isCell)
  : Expr(ArrayIndexClass, t, VK), NumArgs(numargs), CellIndexed(isCell) {

  SubExprs = new (C) Stmt*[numargs+1];
  SubExprs[BASE] = fn;
  for (unsigned i = 0; i != numargs; ++i) {
    SubExprs[i+INDEX_START] = args[i];
  }

  RParenLoc = rparenloc;
}

ArrayIndex::ArrayIndex(ASTContext &C, StmtClass SC, EmptyShell Empty)
  : Expr(SC, Empty), SubExprs(0), NumArgs(0), CellIndexed(false) {
  // FIXME: Why do we allocate this?
  SubExprs = new (C) Stmt*[1];
}

FunctionCall::FunctionCall(ASTContext& C, StmtClass SC, Expr *fn, Expr **args,
                   unsigned numargs, Type t, ExprValueKind VK,
                   SourceLocation rparenloc)
  : Expr(SC, t, VK), NumArgs(numargs) {

  SubExprs = new (C) Stmt*[numargs+1];
  SubExprs[FN] = fn;
  for (unsigned i = 0; i != numargs; ++i) {
    SubExprs[i+ARGS_START] = args[i];
  }

  RParenLoc = rparenloc;
}

FunctionCall::FunctionCall(ASTContext& C, Expr *fn, Expr **args, unsigned numargs,
                   Type t, ExprValueKind VK, SourceLocation rparenloc)
  : Expr(FunctionCallClass, t, VK),
    NumArgs(numargs) {

  SubExprs = new (C) Stmt*[numargs+1];
  SubExprs[FN] = fn;
  for (unsigned i = 0; i != numargs; ++i) {
    SubExprs[i+ARGS_START] = args[i];
  }

  RParenLoc = rparenloc;
}

FunctionCall::FunctionCall(ASTContext &C, StmtClass SC, EmptyShell Empty)
  : Expr(SC, Empty), SubExprs(0), NumArgs(0) {
  // FIXME: Why do we allocate this?
  SubExprs = new (C) Stmt*[1];
}

/// setNumArgs - This changes the number of arguments present in this call.
/// Any orphaned expressions are deleted by this, and any new operands are set
/// to null.
void
FunctionCall::setNumArgs(ASTContext& C, unsigned NumArgs) {
  // No change, just return.
  if (NumArgs == getNumArgs()) return;

  // If shrinking # arguments, just delete the extras and forgot them.
  if (NumArgs < getNumArgs()) {
    this->NumArgs = NumArgs;
    return;
  }

  // Otherwise, we are growing the # arguments.  New an bigger argument array.
  Stmt **NewSubExprs = new (C) Stmt*[NumArgs+1];
  // Copy over args.
  for (unsigned i = 0; i != getNumArgs()+ARGS_START; ++i)
    NewSubExprs[i] = SubExprs[i];
  // Null out new args.
  for (unsigned i = getNumArgs()+ARGS_START; i != NumArgs+ARGS_START; ++i)
    NewSubExprs[i] = 0;

  if (SubExprs) C.Deallocate(SubExprs);
  SubExprs = NewSubExprs;
  this->NumArgs = NumArgs;
}

/// isBuiltinCall - If this is a call to a builtin, return the builtin ID.  If
/// not, return 0.
unsigned FunctionCall::isBuiltinCall(ASTContext &Context) const {
  return 0;
}

Type FunctionCall::getCallReturnType() const {
  Type CalleeType = getCallee()->getType();

  return CalleeType;
}

Defn *FunctionCall::getCalleeDefn() {
  //FIXME huyabin
	return NULL;
}

/// getOpcodeStr - Turn an Opcode enum value into the punctuation char it
/// corresponds to, e.g. "<<=".
const char *
BinaryOperator::getOpcodeStr(Opcode Op) {
  switch (Op) {
  case BO_MatrixPower:    return "^";
  case BO_MatrixMul:      return "*";
  case BO_MatrixRDiv:     return "/";
  case BO_MatrixLDiv:     return "\\";
  case BO_Power:          return ".^";
  case BO_Mul:            return ".*";
  case BO_RDiv:           return "./";
  case BO_LDiv:           return ".\\";
  case BO_Add:            return "+";
  case BO_Sub:            return "-";
  case BO_Shl:            return "<<";
  case BO_Shr:            return ">>";
  case BO_LT:             return "<";
  case BO_GT:             return ">";
  case BO_LE:             return "<=";
  case BO_GE:             return ">=";
  case BO_EQ:             return "==";
  case BO_NE:             return "!=";
  case BO_And:            return "&";
  case BO_Or:             return "|";
  case BO_LAnd:           return "&&";
  case BO_LOr:            return "||";
  case BO_Assign:         return "=";
  case BO_MatPowerAssign: return "^=";
  case BO_PowerAssign:    return ".^=";
  case BO_MatMulAssign:   return "*=";
  case BO_MulAssign:      return ".*=";
  case BO_MatRDivAssign:  return "/=";
  case BO_RDivAssign:     return "./=";
  case BO_MatLDivAssign:  return "\\=";
  case BO_LDivAssign:     return ".\\=";
  case BO_AddAssign:      return "+=";
  case BO_SubAssign:      return "-=";
  case BO_ShlAssign:      return "<<=";
  case BO_ShrAssign:      return ">>=";
  case BO_AndAssign:      return "&=";
  case BO_OrAssign:       return "|=";
  case BO_Comma:          return ",";
  default:
	  return "";
  }

  return "";
}

InitListExpr::InitListExpr(ASTContext &C, SourceLocation lbraceloc,
                           Expr **initExprs, unsigned numInits,
                           SourceLocation rbraceloc)
  : Expr(InitListExprClass, Type(), VK_RValue),
    InitExprs(C, numInits),
    LBraceLoc(lbraceloc), RBraceLoc(rbraceloc), SyntacticForm(0),
    HadArrayRangeDesignator(false)
{
  InitExprs.insert(C, InitExprs.end(), initExprs, initExprs+numInits);
}

void InitListExpr::reserveInits(ASTContext &C, unsigned NumInits) {
  if (NumInits > InitExprs.size())
    InitExprs.reserve(C, NumInits);
}

void InitListExpr::resizeInits(ASTContext &C, unsigned NumInits) {
  InitExprs.resize(C, NumInits, 0);
}

Expr *InitListExpr::updateInit(ASTContext &C, unsigned Init, Expr *expr) {
  if (Init >= InitExprs.size()) {
    InitExprs.insert(C, InitExprs.end(), Init - InitExprs.size() + 1, 0);
    InitExprs.back() = expr;
    return 0;
  }

  Expr *Result = cast_or_null<Expr>(InitExprs[Init]);
  InitExprs[Init] = expr;
  return Result;
}

SourceRange InitListExpr::getSourceRange() const {
  if (SyntacticForm)
    return SyntacticForm->getSourceRange();
  SourceLocation Beg = LBraceLoc, End = RBraceLoc;
  if (Beg.isInvalid()) {
    // Find the first non-null initializer.
    for (InitExprsTy::const_iterator I = InitExprs.begin(),
                                     E = InitExprs.end();
      I != E; ++I) {
      if (Stmt *S = *I) {
        Beg = S->getLocStart();
        break;
      }
    }
  }
  if (End.isInvalid()) {
    // Find the first non-null initializer from the end.
    for (InitExprsTy::const_reverse_iterator I = InitExprs.rbegin(),
                                             E = InitExprs.rend();
      I != E; ++I) {
      if (Stmt *S = *I) {
        End = S->getSourceRange().getEnd();
        break;
      }
    }
  }
  return SourceRange(Beg, End);
}

//===----------------------------------------------------------------------===//
// Generic Expression Routines
//===----------------------------------------------------------------------===//
/// isUnusedResultAWarning - Return true if this immediate expression should
/// be warned about if the result is unused.  If so, fill in Loc and Ranges
/// with location to warn on and the source range[s] to report with the
/// warning.
bool
Expr::isUnusedResultAWarning(SourceLocation &Loc, SourceRange &R1,
                                  SourceRange &R2, ASTContext &Ctx) const {
  return false;
}

Expr*
Expr::IgnoreParens() {
  Expr* E = this;
  while (true) {
	  if (ParenExpr* P = dyn_cast<ParenExpr>(E)) {
		  E = P->getSubExpr();
		  continue;
	  }
  }
  return E;
}

/// IgnoreParenCasts - Ignore parentheses and casts.  Strip off any ParenExpr
/// or CastExprs or ImplicitCastExprs, returning their operand.
Expr * Expr::IgnoreParenCasts() {
  Expr *E = this;
  while (true) {
    if (ParenExpr* P = dyn_cast<ParenExpr>(E)) {
      E = P->getSubExpr();
      continue;
    }
    return E;
  }
}

Expr * Expr::IgnoreParenImpCasts() {
  Expr *E = this;
  while (true) {
    if (ParenExpr *P = dyn_cast<ParenExpr>(E)) {
    	E = P->getSubExpr();
    	continue;
    }
  }
  return E;
}

/// IgnoreParenNoopCasts - Ignore parentheses and casts that do not change the
/// value (including ptr->int casts of the same size).  Strip off any
/// ParenExpr or CastExprs, returning their operand.
Expr *
Expr::IgnoreParenNoopCasts(ASTContext &Ctx) {
  Expr *E = this;
  while (true) {
		if (ParenExpr *P = dyn_cast<ParenExpr>(E)) {
			E = P->getSubExpr();
			continue;
		}

		return E;
	}
}

//Expr::LValueClassification Expr::ClassifyLValue(ASTContext & Ctx) const
//{
//}

bool Expr::refersToVectorElement() const
{
	const Expr *E = this->IgnoreParens();

	if (const ArrayIndex *ASE = dyn_cast<ArrayIndex>(E))
		return ASE->getBase()->getType()->isVectorType();

  return false;
}

/// hasAnyValueDependentArguments - Determines if any of the expressions
/// in Exprs is value-dependent.
bool
Expr::hasAnyValueDependentArguments(Expr** Exprs, unsigned NumExprs) {
  return false;
}

bool
Expr::isConstantInitializer(ASTContext &Ctx, bool IsForRef) const {
  // This function is attempting whether an expression is an initializer
  // which can be evaluated at compile-time.  isEvaluatable handles most
  // of the cases, but it can't deal with some initializer-specific
  // expressions, and it can't deal with aggregates; we deal with those here,
  // and fall back to isEvaluatable for the other cases.

  // If we ever capture reference-binding directly in the AST, we can
  // kill the second parameter.
  if (IsForRef) {
    EvalResult Result;
    return EvaluateAsLValue(Result, Ctx) && !Result.HasSideEffects;
  }

  switch (getStmtClass()) {
  default: break;
  case StringLiteralClass:
    return true;
  }
  return isEvaluatable(Ctx);
}

//===----------------------------------------------------------------------===//
//  MemberExpr
//===----------------------------------------------------------------------===//
MemberExpr::MemberExpr(Expr *base, ValueDefn *memberdefn,
	                     const DefinitionNameInfo &NameInfo, Type ty,
	                     ExprValueKind VK)
	    : Expr(MemberExprClass, ty, VK),
	      Base(base), MemberDefn(memberdefn), MemberLoc(NameInfo.getLoc()),
	      HasQualifierOrFoundDefn(false) {
	assert(memberdefn->getDefnName() == NameInfo.getName());
}

MemberExpr *MemberExpr::Create(ASTContext &C, Expr *base,
		                           NestedNameSpecifier *qual,
                               SourceRange qualrange,
                               ValueDefn *member,
                               DefnAccessPair founddefn,
                               DefinitionNameInfo nameinfo,
                               Type ty,
                               ExprValueKind vk) {
  std::size_t Size = sizeof(MemberExpr);

  bool hasQualOrFound = (qual != 0 ||
		  founddefn.getDefn() != member ||
		  founddefn.getAccess() != member->getAccess());
  if (hasQualOrFound)
    Size += sizeof(MemberNameQualifier);

  void *Mem = C.Allocate(Size, llvm::alignOf<MemberExpr>());
  MemberExpr *E = new (Mem) MemberExpr(base, member, nameinfo, ty, vk);

  if (hasQualOrFound) {
    E->HasQualifierOrFoundDefn = true;

    MemberNameQualifier *NQ = E->getMemberQualifier();
    NQ->NNS = qual;
    NQ->Range = qualrange;
    NQ->FoundDefn = founddefn;
  }

  return E;
}

DefnAccessPair MemberExpr::getFoundDefn() const {
	if (!HasQualifierOrFoundDefn)
		return DefnAccessPair::make(getMemberDefn(),
				                        getMemberDefn()->getAccess());

	return getMemberQualifier()->FoundDefn;
}

//===----------------------------------------------------------------------===//
//  ParenListExpr
//===----------------------------------------------------------------------===//
ParenListExpr::ParenListExpr(ASTContext& C, SourceLocation lparenloc,
                             Expr **exprs, unsigned nexprs,
                             SourceLocation rparenloc)
  : Expr(ParenListExprClass, Type(), VK_RValue),
    NumExprs(nexprs), LParenLoc(lparenloc), RParenLoc(rparenloc) {

  SubExprs = new (C) Stmt*[nexprs];
  for (unsigned i = 0; i != nexprs; ++i) {
    SubExprs[i] = exprs[i];
  }
}

const OpaqueValueExpr *OpaqueValueExpr::findInCopyConstruct(const Expr *e) {
//  if (const ExprWithCleanups *ewc = dyn_cast<ExprWithCleanups>(e))
//    e = ewc->getSubExpr();
//  if (const MaterializeTemporaryExpr *m = dyn_cast<MaterializeTemporaryExpr>(e))
//    e = m->GetTemporaryExpr();
//  e = cast<ClassConstructExpr>(e)->getIdx(0);
//  while (const ImplicitCastExpr *ice = dyn_cast<ImplicitCastExpr>(e))
//    e = ice->getSubExpr();
  return cast<OpaqueValueExpr>(e);
}

//===----------------------------------------------------------------------===//
//  DefnRefExpr
//===----------------------------------------------------------------------===//
DefnRefExpr::DefnRefExpr(NestedNameSpecifier *Qualifier,
                         SourceRange QualifierRange,
                         ValueDefn *D, SourceLocation NameLoc,
                         Type T, ExprValueKind VK)
  : Expr(DefnRefExprClass, T, VK),
    DecoratedD(D, (Qualifier? HasQualifierFlag : 0)),
    Loc(NameLoc) {
  if (Qualifier) {
    NameQualifier *NQ = getNameQualifier();
    NQ->NNS = Qualifier;
    NQ->Range = QualifierRange;
  }
}

DefnRefExpr::DefnRefExpr(NestedNameSpecifier *Qualifier,
                         SourceRange QualifierRange,
                         ValueDefn *D, const DefinitionNameInfo &NameInfo,
                         Type T, ExprValueKind VK)
  : Expr(DefnRefExprClass, T, VK),
    DecoratedD(D, (Qualifier? HasQualifierFlag : 0)),
    Loc(NameInfo.getLoc()) {
  if (Qualifier) {
    NameQualifier *NQ = getNameQualifier();
    NQ->NNS = Qualifier;
    NQ->Range = QualifierRange;
  }
}


DefnRefExpr *DefnRefExpr::Create(ASTContext &Context,
                                 NestedNameSpecifier *Qualifier,
                                 SourceRange QualifierRange,
                                 ValueDefn *D,
                                 SourceLocation NameLoc,
                                 Type T,
                                 ExprValueKind VK) {
  return Create(Context, Qualifier, QualifierRange, D,
                DefinitionNameInfo(D->getDefnName(), NameLoc),
                T, VK);
}

DefnRefExpr * DefnRefExpr::Create(ASTContext &Context,
                             NestedNameSpecifier *Qualifier,
                             SourceRange QualifierRange,
                             ValueDefn *D,
                             const DefinitionNameInfo &NameInfo,
                             Type T, ExprValueKind VK) {

	std::size_t Size = sizeof(DefnRefExpr);
  if (Qualifier != 0)
    Size += sizeof(NameQualifier);

  void *Mem = Context.Allocate(Size, llvm::alignOf<DefnRefExpr>());
  return new (Mem) DefnRefExpr(Qualifier, QualifierRange, D, NameInfo,
                               T, VK);
}

DefnRefExpr *DefnRefExpr::CreateEmpty(ASTContext &Context, bool HasQualifier) {
  std::size_t Size = sizeof(DefnRefExpr);
  if (HasQualifier)
    Size += sizeof(NameQualifier);

  void *Mem = Context.Allocate(Size, llvm::alignOf<DefnRefExpr>());
  return new (Mem) DefnRefExpr(EmptyShell());
}

SourceRange DefnRefExpr::getSourceRange() const {
  SourceRange R = getNameInfo().getSourceRange();
  if (hasQualifier())
    R.setBegin(getQualifierRange().getBegin());

  return R;
}
//===----------------------------------------------------------------------===//
//  ExprIterator.
//===----------------------------------------------------------------------===//
Expr* ExprIterator::operator[](size_t idx) { return cast<Expr>(I[idx]); }
Expr* ExprIterator::operator*() const { return cast<Expr>(*I); }
Expr* ExprIterator::operator->() const { return cast<Expr>(*I); }
const Expr* ConstExprIterator::operator[](size_t idx) const {
  return cast<Expr>(I[idx]);
}
const Expr* ConstExprIterator::operator*() const { return cast<Expr>(*I); }
const Expr* ConstExprIterator::operator->() const { return cast<Expr>(*I); }
