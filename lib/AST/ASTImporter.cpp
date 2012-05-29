//===--- ASTImporter.cpp - Importing ASTs from other Contexts ---*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTImporter class which imports AST nodes from one
//  context into another context.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/ASTImporter.h"

#include "mlang/AST/ASTContext.h"
#include "mlang/AST/ASTDiagnostic.h"
#include "mlang/AST/DefinitionName.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/DefnVisitor.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/NestedNameSpecifier.h"
#include "mlang/AST/StmtVisitor.h"
#include "mlang/AST/TypeVisitor.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Basic/IdentifierInfo.h"
#include "mlang/Basic/SourceManager.h"
#include "llvm/Support/MemoryBuffer.h"
#include <deque>

using namespace mlang;

namespace {
  class ASTNodeImporter : public TypeVisitor<ASTNodeImporter, Type>,
                          public DefnVisitor<ASTNodeImporter, Defn *>,
                          public StmtVisitor<ASTNodeImporter, Stmt *> {
    ASTImporter &Importer;
    
  public:
    explicit ASTNodeImporter(ASTImporter &Importer) : Importer(Importer) { }
    
    using TypeVisitor<ASTNodeImporter, Type>::Visit;
    using DefnVisitor<ASTNodeImporter, Defn *>::Visit;
    using StmtVisitor<ASTNodeImporter, Stmt *>::Visit;

    // Importing types
    Type VisitType(RawType *T);
    Type VisitSimpleNumericType(SimpleNumericType *T);
    Type VisitStructType(StructType *T);
    Type VisitCellType(CellType *T);
    Type VisitClassdefType(ClassdefType *T);
    Type VisitVectorType(VectorType *T);
    Type VisitMatrixType(MatrixType *T);
    Type VisitNDArrayType(NDArrayType *T);
    Type VisitFunctionNoProtoType(FunctionNoProtoType *T);
    Type VisitFunctionProtoType(FunctionProtoType *T);
    Type VisitFunctionHandleType(FunctionHandleType *T);
    Type VisitLValueReferenceType(LValueReferenceType *T);
    Type VisitRValueReferenceType(RValueReferenceType *T);
                            
    // Importing Definition
    bool ImportDefnParts(NamedDefn *D, DefnContext *&DC,
                         DefnContext *&LexicalDC, DefinitionName &Name,
                         SourceLocation &Loc);
    void ImportDefinitionNameLoc(const DefinitionNameInfo &From,
                                  DefinitionNameInfo& To);
    void ImportDefnContext(DefnContext *FromDC);
    bool ImportDefinition(UserClassDefn *From, UserClassDefn *To);
    bool IsStructuralMatch(UserClassDefn *FromRecord, UserClassDefn *ToRecord);

    Defn *VisitDefn(Defn *D);
    Defn *VisitNamespaceDefn(NamespaceDefn *D);
    Defn *VisitUserClassDefn(UserClassDefn *D);
    Defn *VisitStructDefn(StructDefn *D);
    Defn *VisitCellDefn(CellDefn *D);
    Defn *VisitFunctionDefn(FunctionDefn *D);
    Defn *VisitClassMethodDefn(ClassMethodDefn *D);
    Defn *VisitClassConstructorDefn(ClassConstructorDefn *D);
    Defn *VisitClassDestructorDefn(ClassDestructorDefn *D);
    Defn *VisitMemberDefn(MemberDefn *D);
    Defn *VisitVarDefn(VarDefn *D);
    Defn *VisitParamVarDefn(ParamVarDefn *D);

    // Importing statements
    Stmt *VisitStmt(Stmt *S);

    // Importing expressions
    Expr *VisitExpr(Expr *E);
    Expr *VisitDefnRefExpr(DefnRefExpr *E);
    Expr *VisitIntegerLiteral(IntegerLiteral *E);
    Expr *VisitCharacterLiteral(CharacterLiteral *E);
    Expr *VisitParenExpr(ParenExpr *E);
    Expr *VisitUnaryOperator(UnaryOperator *E);
    Expr *VisitBinaryOperator(BinaryOperator *E);
    Expr *VisitCompoundAssignOperator(CompoundAssignOperator *E);
  };
}

//----------------------------------------------------------------------------
// Structural Equivalence
//----------------------------------------------------------------------------

namespace {
  struct StructuralEquivalenceContext {
    /// \brief AST contexts for which we are checking structural equivalence.
    ASTContext &C1, &C2;
    
    /// \brief The set of "tentative" equivalences between two canonical 
    /// Definition, mapping from a Definition in the first context to the
    /// Definition in the second context that we believe to be equivalent.
    llvm::DenseMap<Defn *, Defn *> TentativeEquivalences;
    
    /// \brief Queue of declarations in the first context whose equivalence
    /// with a declaration in the second context still needs to be verified.
    std::deque<Defn *> DefnsToCheck;
    
    /// \brief Definition (from, to) pairs that are known not to be equivalent
    /// (which we have already complained about).
    llvm::DenseSet<std::pair<Defn *, Defn *> > &NonEquivalentDefns;
    
    /// \brief Whether we're being strict about the spelling of types when 
    /// unifying two types.
    bool StrictTypeSpelling;
    
    StructuralEquivalenceContext(ASTContext &C1, ASTContext &C2,
               llvm::DenseSet<std::pair<Defn *, Defn *> > &NonEquivalentDefns,
                                 bool StrictTypeSpelling = false)
      : C1(C1), C2(C2), NonEquivalentDefns(NonEquivalentDefns),
        StrictTypeSpelling(StrictTypeSpelling) { }

    /// \brief Determine whether the two Definition are structurally
    /// equivalent.
    bool IsStructurallyEquivalent(Defn *D1, Defn *D2);
    
    /// \brief Determine whether the two types are structurally equivalent.
    bool IsStructurallyEquivalent(Type T1, Type T2);

  private:
    /// \brief Finish checking all of the structural equivalences.
    ///
    /// \returns true if an error occurred, false otherwise.
    bool Finish();
    
  public:
    DiagnosticBuilder Diag1(SourceLocation Loc, unsigned DiagID) {
      return C1.getDiagnostics().Report(Loc, DiagID);
    }

    DiagnosticBuilder Diag2(SourceLocation Loc, unsigned DiagID) {
      return C2.getDiagnostics().Report(Loc, DiagID);
    }
  };
}

static bool IsStructurallyEquivalent(StructuralEquivalenceContext &Context,
                                     Type T1, Type T2);
static bool IsStructurallyEquivalent(StructuralEquivalenceContext &Context,
                                     Defn *D1, Defn *D2);

/// \brief Determine if two APInts have the same value, after zero-extending
/// one of them (if needed!) to ensure that the bit-widths match.
static bool IsSameValue(const llvm::APInt &I1, const llvm::APInt &I2) {
  if (I1.getBitWidth() == I2.getBitWidth())
    return I1 == I2;
  
  if (I1.getBitWidth() > I2.getBitWidth())
    return I1 == I2.zext(I1.getBitWidth());
  
  return I1.zext(I2.getBitWidth()) == I2;
}

/// \brief Determine if two APSInts have the same value, zero- or sign-extending
/// as needed.
static bool IsSameValue(const llvm::APSInt &I1, const llvm::APSInt &I2) {
  if (I1.getBitWidth() == I2.getBitWidth() && I1.isSigned() == I2.isSigned())
    return I1 == I2;
  
  // Check for a bit-width mismatch.
  if (I1.getBitWidth() > I2.getBitWidth())
    return IsSameValue(I1, I2.extend(I1.getBitWidth()));
  else if (I2.getBitWidth() > I1.getBitWidth())
    return IsSameValue(I1.extend(I2.getBitWidth()), I2);
  
  // We have a signedness mismatch. Turn the signed value into an unsigned 
  // value.
  if (I1.isSigned()) {
    if (I1.isNegative())
      return false;
    
    return llvm::APSInt(I1, true) == I2;
  }
 
  if (I2.isNegative())
    return false;
  
  return I1 == llvm::APSInt(I2, true);
}

/// \brief Determine structural equivalence of two expressions.
static bool IsStructurallyEquivalent(StructuralEquivalenceContext &Context,
                                     Expr *E1, Expr *E2) {
  if (!E1 || !E2)
    return E1 == E2;
  
  // FIXME: Actually perform a structural comparison!
  return true;
}

/// \brief Determine whether two identifiers are equivalent.
static bool IsStructurallyEquivalent(const IdentifierInfo *Name1,
                                     const IdentifierInfo *Name2) {
  if (!Name1 || !Name2)
    return Name1 == Name2;
  
  return Name1->getName() == Name2->getName();
}

/// \brief Determine whether two nested-name-specifiers are equivalent.
static bool IsStructurallyEquivalent(StructuralEquivalenceContext &Context,
                                     NestedNameSpecifier *NNS1,
                                     NestedNameSpecifier *NNS2) {
  // FIXME: Implement!
  return true;
}

/// \brief Determine structural equivalence for the common part of array 
/// types.
static bool IsArrayStructurallyEquivalent(StructuralEquivalenceContext &Context,
                                          const ArrayType *Array1, 
                                          const ArrayType *Array2) {
  if (!IsStructurallyEquivalent(Context, 
                                Array1->getElementType(), 
                                Array2->getElementType()))
    return false;
//  if (Array1->getSizeModifier() != Array2->getSizeModifier())
//    return false;
//  if (Array1->getIndexTypeQualifiers() != Array2->getIndexTypeQualifiers())
//    return false;
  
  return true;
}

/// \brief Determine structural equivalence of two types.
static bool IsStructurallyEquivalent(StructuralEquivalenceContext &Context,
                                     Type T1, Type T2) {
  if (T1.isNull() || T2.isNull())
    return T1.isNull() && T2.isNull();
  
  return true;
}

/// \brief Determine structural equivalence of two records.
static bool IsStructurallyEquivalent(StructuralEquivalenceContext &Context,
                                     UserClassDefn *D1, UserClassDefn *D2) {
  return true;
}
     
/// \brief Determine structural equivalence of two Definition.
static bool IsStructurallyEquivalent(StructuralEquivalenceContext &Context,
                                     Defn *D1, Defn *D2) {
  // FIXME: Check for known structural equivalences via a callback of some sort.
  
  // Check whether we already know that these two Definition are not
  // structurally equivalent.
  if (Context.NonEquivalentDefns.count(std::make_pair(D1, D2)))
    return false;
  
  // Determine whether we've already produced a tentative equivalence for D1.
  Defn *&EquivToD1 = Context.TentativeEquivalences[D1];
  if (EquivToD1)
    return EquivToD1 == D2;
  
  // Produce a tentative equivalence D1 <-> D2, which will be checked later.
  EquivToD1 = D2;
  Context.DefnsToCheck.push_back(D1);
  return true;
}

bool StructuralEquivalenceContext::IsStructurallyEquivalent(Defn *D1,
                                                            Defn *D2) {
  if (!::IsStructurallyEquivalent(*this, D1, D2))
    return false;
  
  return !Finish();
}

bool StructuralEquivalenceContext::IsStructurallyEquivalent(Type T1,
                                                            Type T2) {
  if (!::IsStructurallyEquivalent(*this, T1, T2))
    return false;
  
  return !Finish();
}

bool StructuralEquivalenceContext::Finish() {
  while (!DefnsToCheck.empty()) {
		// Check the next Definition.
		Defn *D1 = DefnsToCheck.front();
		DefnsToCheck.pop_front();

		Defn *D2 = TentativeEquivalences[D1];
		assert(D2 && "Unrecorded tentative equivalence?");

		bool Equivalent = true;

		// FIXME: Switch on all Definition kinds. For now, we're just going to
		// check the obvious ones.
		if (UserClassDefn *Record1 = dyn_cast<UserClassDefn>(D1)) {
			if (UserClassDefn *Record2 = dyn_cast<UserClassDefn>(D2)) {
				// Check for equivalent structure names.
				IdentifierInfo *Name1 = Record1->getIdentifier();
				IdentifierInfo *Name2 = Record2->getIdentifier();
				if (!::IsStructurallyEquivalent(Name1, Name2)
						|| !::IsStructurallyEquivalent(*this, Record1, Record2))
					Equivalent = false;
			} else {
				// Record/non-record mismatch.
				Equivalent = false;
			}
		}

		if (!Equivalent) {
			// Note that these two Definition are not equivalent (and we already
			// know about it).
			NonEquivalentDefns.insert(std::make_pair(D1, D2));
			return true;
		}
		// FIXME: Check other Definition kinds!
	}

	return false;
}

//----------------------------------------------------------------------------
// Import Types
//----------------------------------------------------------------------------

Type ASTNodeImporter::VisitType(RawType *T) {
  Importer.FromDiag(SourceLocation(), diag::err_unsupported_ast_node)
    << T->getTypeClassName();
  return Type();
}

Type ASTNodeImporter::VisitSimpleNumericType(SimpleNumericType *T) {
  return Type();
}

Type ASTNodeImporter::VisitStructType(StructType *T) {
	StructDefn *ToDefn
    = dyn_cast_or_null<StructDefn>(Importer.Import(T->getDefn()));
  if (!ToDefn)
    return Type();

  return Importer.getToContext().getTypeDefnType(ToDefn);
}

Type ASTNodeImporter::VisitCellType(CellType *T) {
  CellDefn *ToDefn
    = dyn_cast_or_null<CellDefn>(Importer.Import(T->getDefn()));
  if (!ToDefn)
    return Type();

  return Importer.getToContext().getTypeDefnType(ToDefn);
}

Type ASTNodeImporter::VisitClassdefType(ClassdefType *T) {
  UserClassDefn *ToDefn
    = dyn_cast_or_null<UserClassDefn>(Importer.Import(T->getDefn()));
  if (!ToDefn)
    return Type();

  return Importer.getToContext().getTypeDefnType(ToDefn);
}

Type ASTNodeImporter::VisitMatrixType(MatrixType *T) {
  Type ToElementType = Importer.Import(T->getElementType());
  if (ToElementType.isNull())
    return Type();
  unsigned R = T->getNumRows();
  unsigned C = T->getNumColumns();
  MatrixType::MatrixKind K = T->getMatrixKind();
  return Importer.getToContext().getMatrixType(ToElementType,
                                               R, C, K);
}

Type ASTNodeImporter::VisitNDArrayType(NDArrayType *T) {
  Type ToElementType = Importer.Import(T->getElementType());
  if (ToElementType.isNull())
    return Type();

  DimVector dv = T->getDimensions();
  return Importer.getToContext().getNDArrayType(ToElementType,
                                                dv);
}

Type ASTNodeImporter::VisitVectorType(VectorType *T) {
  Type ToElementType = Importer.Import(T->getElementType());
  if (ToElementType.isNull())
    return Type();
  
  return Importer.getToContext().getVectorType(ToElementType, 
                                               T->getNumElements(),
                                               T->getVectorKind(),
                                               T->isRowVector());
}

Type ASTNodeImporter::VisitFunctionNoProtoType(FunctionNoProtoType *T) {
  // FIXME: What happens if we're importing a function without a prototype 
  // into C++? Should we make it variadic?
  Type ToResultType = Importer.Import(T->getResultType());
  if (ToResultType.isNull())
    return Type();

  return Importer.getToContext().getFunctionNoProtoType(T->getResultType(),
                                                        T->getExtInfo());
}

Type ASTNodeImporter::VisitFunctionProtoType(FunctionProtoType *T) {
  Type ToResultType = Importer.Import(T->getResultType());
  if (ToResultType.isNull())
    return Type();
  
  return ToResultType;
}

Type ASTNodeImporter::VisitFunctionHandleType(FunctionHandleType *T) {
  Type ToResultType = Importer.Import(T->getPointeeType());
  if (ToResultType.isNull())
    return Type();

  return ToResultType;
}

Type ASTNodeImporter::VisitLValueReferenceType(LValueReferenceType *T) {
  // FIXME: Check for C++ support in "to" context.
  Type ToPointeeType = Importer.Import(T->getPointeeTypeAsWritten());
  if (ToPointeeType.isNull())
    return Type();

  return Importer.getToContext().getLValueReferenceType(ToPointeeType, false);
}

Type ASTNodeImporter::VisitRValueReferenceType(RValueReferenceType *T) {
  // FIXME: Check for C++0x support in "to" context.
  Type ToPointeeType = Importer.Import(T->getPointeeTypeAsWritten());
  if (ToPointeeType.isNull())
    return Type();

  return Importer.getToContext().getRValueReferenceType(ToPointeeType);
}
//----------------------------------------------------------------------------
// Import Definitions
//----------------------------------------------------------------------------
bool ASTNodeImporter::ImportDefnParts(NamedDefn *D, DefnContext *&DC,
                                      DefnContext *&LexicalDC,
                                      DefinitionName &Name,
                                      SourceLocation &Loc) {
  // Import the context of this definition.
  DC = Importer.ImportContext(D->getDefnContext());
  if (!DC)
    return true;
  
  // Import the name of this definition.
  Name = Importer.Import(D->getDefnName());
  if (D->getDefnName() && !Name)
    return true;
  
  // Import the location of this definition.
  Loc = Importer.Import(D->getLocation());
  return false;
}

void
ASTNodeImporter::ImportDefinitionNameLoc(const DefinitionNameInfo &From,
                                          DefinitionNameInfo& To) {
  // NOTE: To.Name and To.Loc are already imported.
  // We only have to import To.LocInfo.
  switch (To.getName().getNameKind()) {
  case DefinitionName::NK_Identifier:
    return;
  case DefinitionName::NK_ConstructorName:
  case DefinitionName::NK_DestructorName: {
//    TypeSourceInfo *FromTInfo = From.getNamedTypeInfo();
//    To.setNamedTypeInfo(Importer.Import(FromTInfo));
    return;
  }
  default:
    assert(0 && "Unknown name kind.");
  }
}

void ASTNodeImporter::ImportDefnContext(DefnContext *FromDC) {
  for (DefnContext::defn_iterator From = FromDC->defns_begin(),
                               FromEnd = FromDC->defns_end();
       From != FromEnd;
       ++From)
    Importer.Import(*From);
}

bool ASTNodeImporter::ImportDefinition(UserClassDefn *From, UserClassDefn *To) {
  return false;
}

bool ASTNodeImporter::IsStructuralMatch(UserClassDefn *FromRecord,
                                        UserClassDefn *ToRecord) {
  StructuralEquivalenceContext Ctx(Importer.getFromContext(),
                                   Importer.getToContext(),
                                   Importer.getNonEquivalentDefns());
  return Ctx.IsStructurallyEquivalent(FromRecord, ToRecord);
}

Defn *ASTNodeImporter::VisitDefn(Defn *D) {
  Importer.FromDiag(D->getLocation(), diag::err_unsupported_ast_node)
    << D->getDefnKindName();
  return 0;
}

Defn *ASTNodeImporter::VisitNamespaceDefn(NamespaceDefn *D) {
  // Import the major distinguishing characteristics of this namespace.
  DefnContext *DC, *LexicalDC;
  DefinitionName Name;
  SourceLocation Loc;
  if (ImportDefnParts(D, DC, LexicalDC, Name, Loc))
    return 0;
  
  return NULL;
}

Defn *ASTNodeImporter::VisitUserClassDefn(UserClassDefn *D) {
  return NULL;
}

Defn *ASTNodeImporter::VisitFunctionDefn(FunctionDefn *D) {
  return NULL;
}

Defn *ASTNodeImporter::VisitClassMethodDefn(ClassMethodDefn *D) {
  return VisitFunctionDefn(D);
}

Defn *ASTNodeImporter::VisitClassConstructorDefn(ClassConstructorDefn *D) {
  return VisitClassMethodDefn(D);
}

Defn *ASTNodeImporter::VisitClassDestructorDefn(ClassDestructorDefn *D) {
  return VisitClassMethodDefn(D);
}

Defn *ASTNodeImporter::VisitMemberDefn(MemberDefn *D) {
  return NULL;
}

Defn *ASTNodeImporter::VisitVarDefn(VarDefn *D) {
  // Import the major distinguishing characteristics of a variable.
  DefnContext *DC, *LexicalDC;
  DefinitionName Name;
  SourceLocation Loc;
  if (ImportDefnParts(D, DC, LexicalDC, Name, Loc))
    return 0;
  
  return NULL;
}

Defn *ASTNodeImporter::VisitParamVarDefn(ParamVarDefn *D) {
  return NULL;
}

//----------------------------------------------------------------------------
// Import Statements
//----------------------------------------------------------------------------

Stmt *ASTNodeImporter::VisitStmt(Stmt *S) {
  Importer.FromDiag(S->getLocStart(), diag::err_unsupported_ast_node)
    << S->getStmtClassName();
  return 0;
}

//----------------------------------------------------------------------------
// Import Expressions
//----------------------------------------------------------------------------
Expr *ASTNodeImporter::VisitExpr(Expr *E) {
  Importer.FromDiag(E->getLocStart(), diag::err_unsupported_ast_node)
    << E->getStmtClassName();
  return 0;
}

Expr *ASTNodeImporter::VisitDefnRefExpr(DefnRefExpr *E) {
  NestedNameSpecifier *Qualifier = 0;
  if (E->getQualifier()) {
    Qualifier = Importer.Import(E->getQualifier());
    if (!E->getQualifier())
      return 0;
  }
  
  ValueDefn *ToD = cast_or_null<ValueDefn>(Importer.Import(E->getDefn()));
  if (!ToD)
    return 0;
  
  Type T = Importer.Import(E->getType());
  if (T.isNull())
    return 0;
  
  return DefnRefExpr::Create(Importer.getToContext(), Qualifier,
                             Importer.Import(E->getQualifierRange()),
                             ToD,
                             Importer.Import(E->getLocation()),
                             T, E->getValueKind());
}

Expr *ASTNodeImporter::VisitIntegerLiteral(IntegerLiteral *E) {
  Type T = Importer.Import(E->getType());
  if (T.isNull())
    return 0;

  return IntegerLiteral::Create(Importer.getToContext(), 
                                E->getValue(), T,
                                Importer.Import(E->getLocation()));
}

Expr *ASTNodeImporter::VisitCharacterLiteral(CharacterLiteral *E) {
  Type T = Importer.Import(E->getType());
  if (T.isNull())
    return 0;
  
  return new (Importer.getToContext()) CharacterLiteral(E->getValue(), 
                                                        T,
                                          Importer.Import(E->getLocation()));
}

Expr *ASTNodeImporter::VisitParenExpr(ParenExpr *E) {
  Expr *SubExpr = Importer.Import(E->getSubExpr());
  if (!SubExpr)
    return 0;
  
  return new (Importer.getToContext()) 
                                  ParenExpr(Importer.Import(E->getLParen()),
                                            Importer.Import(E->getRParen()),
                                            SubExpr, SubExpr->getType(),
                                            SubExpr->getValueKind());
}

Expr *ASTNodeImporter::VisitUnaryOperator(UnaryOperator *E) {
  Type T = Importer.Import(E->getType());
  if (T.isNull())
    return 0;

  Expr *SubExpr = Importer.Import(E->getSubExpr());
  if (!SubExpr)
    return 0;
  
  return new (Importer.getToContext()) UnaryOperator(SubExpr, E->getOpcode(),
                                                     T, E->getValueKind(),
                                 Importer.Import(E->getOperatorLoc()));
}

Expr *ASTNodeImporter::VisitBinaryOperator(BinaryOperator *E) {
  Type T = Importer.Import(E->getType());
  if (T.isNull())
    return 0;

  Expr *LHS = Importer.Import(E->getLHS());
  if (!LHS)
    return 0;
  
  Expr *RHS = Importer.Import(E->getRHS());
  if (!RHS)
    return 0;
  
  return new (Importer.getToContext()) BinaryOperator(LHS, RHS, E->getOpcode(),
                                                      T, E->getValueKind(),
                                          Importer.Import(E->getOperatorLoc()));
}

Expr *ASTNodeImporter::VisitCompoundAssignOperator(CompoundAssignOperator *E) {
  Type T = Importer.Import(E->getType());
  if (T.isNull())
    return 0;
  
  Type CompLHSType = Importer.Import(E->getComputationLHSType());
  if (CompLHSType.isNull())
    return 0;
  
  Type CompResultType = Importer.Import(E->getComputationResultType());
  if (CompResultType.isNull())
    return 0;
  
  Expr *LHS = Importer.Import(E->getLHS());
  if (!LHS)
    return 0;
  
  Expr *RHS = Importer.Import(E->getRHS());
  if (!RHS)
    return 0;
  
  return new (Importer.getToContext()) 
                        CompoundAssignOperator(LHS, RHS, E->getOpcode(),
                                               T,
                                               CompLHSType, CompResultType,
                                          Importer.Import(E->getOperatorLoc()));
}

//===----------------------------------------------------------------------===//
// ASTImporter Implementation
//===----------------------------------------------------------------------===//
ASTImporter::ASTImporter(ASTContext &ToContext, FileManager &ToFileManager,
                         ASTContext &FromContext, FileManager &FromFileManager)
  : ToContext(ToContext), FromContext(FromContext),
    ToFileManager(ToFileManager), FromFileManager(FromFileManager) {
  ImportedDefns[FromContext.getTranslationUnitDefn()]
    = ToContext.getTranslationUnitDefn();
}

ASTImporter::~ASTImporter() { }

Type ASTImporter::Import(Type FromT) {
  if (FromT.isNull())
    return Type();
  
  return Type();
}

Defn *ASTImporter::Import(Defn *FromD) {
  if (!FromD)
    return 0;

  return NULL;
}

DefnContext *ASTImporter::ImportContext(DefnContext *FromDC) {
  if (!FromDC)
    return FromDC;

  return cast_or_null<DefnContext>(Import(cast<Defn>(FromDC)));
}

Expr *ASTImporter::Import(Expr *FromE) {
  if (!FromE)
    return 0;

  return cast_or_null<Expr>(Import(cast<Stmt>(FromE)));
}

Stmt *ASTImporter::Import(Stmt *FromS) {
  if (!FromS)
    return 0;

  // Check whether we've already imported this definition.
  llvm::DenseMap<Stmt *, Stmt *>::iterator Pos = ImportedStmts.find(FromS);
  if (Pos != ImportedStmts.end())
    return Pos->second;
  
  // Import the type
  ASTNodeImporter Importer(*this);
  Stmt *ToS = Importer.Visit(FromS);
  if (!ToS)
    return 0;
  
  // Record the imported definition.
  ImportedStmts[FromS] = ToS;
  return ToS;
}

NestedNameSpecifier *ASTImporter::Import(NestedNameSpecifier *FromNNS) {
  if (!FromNNS)
    return 0;

  // FIXME: Implement!
  return 0;
}

SourceLocation ASTImporter::Import(SourceLocation FromLoc) {
  if (FromLoc.isInvalid())
    return SourceLocation();

  SourceManager &FromSM = FromContext.getSourceManager();
  
  // For now, map everything down to its spelling location, so that we
  // don't have to import macro instantiations.
  // FIXME: Import macro instantiations!
  FromLoc = FromSM.getSpellingLoc(FromLoc);
  std::pair<FileID, unsigned> Decomposed = FromSM.getDecomposedLoc(FromLoc);
  SourceManager &ToSM = ToContext.getSourceManager();
  return ToSM.getLocForStartOfFile(Import(Decomposed.first))
             .getFileLocWithOffset(Decomposed.second);
}

SourceRange ASTImporter::Import(SourceRange FromRange) {
  return SourceRange(Import(FromRange.getBegin()), Import(FromRange.getEnd()));
}

FileID ASTImporter::Import(FileID FromID) {
  llvm::DenseMap<FileID, FileID>::iterator Pos
    = ImportedFileIDs.find(FromID);
  if (Pos != ImportedFileIDs.end())
    return Pos->second;
  
  SourceManager &FromSM = FromContext.getSourceManager();
  SourceManager &ToSM = ToContext.getSourceManager();
  const SrcMgr::SLocEntry &FromSLoc = FromSM.getSLocEntry(FromID);
  assert(FromSLoc.isFile() && "Cannot handle macro instantiations yet");
  
  // Include location of this file.
  SourceLocation ToIncludeLoc = Import(FromSLoc.getFile().getIncludeLoc());
  
  // Map the FileID for to the "to" source manager.
  FileID ToID;
  const SrcMgr::ContentCache *Cache = FromSLoc.getFile().getContentCache();
  if (Cache->OrigEntry) {
    // FIXME: We probably want to use getVirtualFile(), so we don't hit the
    // disk again
    // FIXME: We definitely want to re-use the existing MemoryBuffer, rather
    // than mmap the files several times.
    const FileEntry *Entry = ToFileManager.getFile(Cache->OrigEntry->getName());
    ToID = ToSM.createFileID(Entry, ToIncludeLoc, 
                             FromSLoc.getFile().getFileCharacteristic());
  } else {
    // FIXME: We want to re-use the existing MemoryBuffer!
    const llvm::MemoryBuffer *
        FromBuf = Cache->getBuffer(FromContext.getDiagnostics(), FromSM);
    llvm::MemoryBuffer *ToBuf
      = llvm::MemoryBuffer::getMemBufferCopy(FromBuf->getBuffer(),
                                             FromBuf->getBufferIdentifier());
    ToID = ToSM.createFileIDForMemBuffer(ToBuf);
  }
  
  
  ImportedFileIDs[FromID] = ToID;
  return ToID;
}

DefinitionName ASTImporter::Import(DefinitionName FromName) {
  if (!FromName)
    return DefinitionName();

  // Silence bogus GCC warning
  return DefinitionName();
}

IdentifierInfo *ASTImporter::Import(const IdentifierInfo *FromId) {
  if (!FromId)
    return 0;

  return &ToContext.Idents.get(FromId->getName());
}

DefinitionName ASTImporter::HandleNameConflict(DefinitionName Name,
                                               DefnContext *DC,
                                               unsigned IDNS,
                                               NamedDefn **Defns,
                                               unsigned NumDefns) {
  return Name;
}

DiagnosticBuilder ASTImporter::ToDiag(SourceLocation Loc, unsigned DiagID) {
  return ToContext.getDiagnostics().Report(Loc, DiagID);
}

DiagnosticBuilder ASTImporter::FromDiag(SourceLocation Loc, unsigned DiagID) {
  return FromContext.getDiagnostics().Report(Loc, DiagID);
}

Defn *ASTImporter::Imported(Defn *From, Defn *To) {
  ImportedDefns[From] = To;
  return To;
}

bool ASTImporter::IsStructurallyEquivalent(Type From, Type To) {
  return false;
}
