//===--- RecursiveASTVisitor.h - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the RecursiveASTVisitor interface, which recursively
//  traverses the entire AST.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_RECURSIVE_ASTVISITOR_H_
#define MLANG_AST_RECURSIVE_ASTVISITOR_H_

#include "mlang/AST/CmdAll.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/NestedNameSpecifier.h"
#include "mlang/AST/Type.h"

// The following three macros are used for meta programming.  The code
// using them is responsible for defining macro OPERATOR().

// All unary operators.
#define UNARYOP_LIST()                          \
  OPERATOR(Transpose) OPERATOR(Quote)        \
  OPERATOR(PostInc)   OPERATOR(PostDec)         \
  OPERATOR(PreInc)    OPERATOR(PreDec)          \
  OPERATOR(Plus)      OPERATOR(Minus)           \
  OPERATOR(LNot)      OPERATOR(Handle)


// All binary operators (excluding compound assign operators).
#define BINOP_LIST() \
  OPERATOR(Power)    OPERATOR(MatrixPower)      \
  OPERATOR(Mul)   OPERATOR(RDiv)  OPERATOR(LDiv)      \
  OPERATOR(MatrixMul)   OPERATOR(MatrixRDiv)  OPERATOR(MatrixLDiv)      \
  OPERATOR(Add)   OPERATOR(Sub)  OPERATOR(Shl)        \
  OPERATOR(Shr)                                       \
                                                      \
  OPERATOR(LT)    OPERATOR(GT)   OPERATOR(LE)         \
  OPERATOR(GE)    OPERATOR(EQ)   OPERATOR(NE)         \
  OPERATOR(And)   OPERATOR(Or)                        \
  OPERATOR(LAnd)  OPERATOR(LOr)                       \
                                                      \
  OPERATOR(Assign)                                    \
  OPERATOR(Colon) OPERATOR(Comma)

// All compound assign operators.
#define CAO_LIST()                                                        \
  OPERATOR(MatPower) OPERATOR(MatMul)                               \
  OPERATOR(MatRDiv)  OPERATOR(MatLDiv)                              \
  OPERATOR(Power)                                                         \
  OPERATOR(Mul) OPERATOR(RDiv) OPERATOR(LDiv) OPERATOR(Add) OPERATOR(Sub) \
  OPERATOR(Shl) OPERATOR(Shr) OPERATOR(And) OPERATOR(Or)

namespace mlang {
// A helper macro to implement short-circuiting when recursing.  It
// invokes CALL_EXPR, which must be a method call, on the derived
// object (s.t. a user of RecursiveASTVisitor can override the method
// in CALL_EXPR).
#define TRY_TO(CALL_EXPR) \
  do { if (!getDerived().CALL_EXPR) return false; } while (0)

/// \brief A class that does preorder depth-first traversal on the
/// entire Mlang AST and visits each node.
///
/// This class performs three distinct tasks:
///   1. traverse the AST (i.e. go to each node);
///   2. at a given node, walk up the class hierarchy, starting from
///      the node's dynamic type, until the top-most class (e.g. Stmt,
///      Defn, or Type) is reached.
///   3. given a (node, class) combination, where 'class' is some base
///      class of the dynamic type of 'node', call a user-overridable
///      function to actually visit the node.
///
/// These tasks are done by three groups of methods, respectively:
///   1. TraverseDefn(Defn *x) does task #1.  It is the entry point
///      for traversing an AST rooted at x.  This method simply
///      dispatches (i.e. forwards) to TraverseFoo(Foo *x) where Foo
///      is the dynamic type of *x, which calls WalkUpFromFoo(x) and
///      then recursively visits the child nodes of x.
///      TraverseStmt(Stmt *x) and TraverseType(Type x) work
///      similarly.
///   2. WalkUpFromFoo(Foo *x) does task #2.  It does not try to visit
///      any child node of x.  Instead, it first calls WalkUpFromBar(x)
///      where Bar is the direct parent class of Foo (unless Foo has
///      no parent), and then calls VisitFoo(x) (see the next list item).
///   3. VisitFoo(Foo *x) does task #3.
///
/// These three method groups are tiered (Traverse* > WalkUpFrom* >
/// Visit*).  A method (e.g. Traverse*) may call methods from the same
/// tier (e.g. other Traverse*) or one tier lower (e.g. WalkUpFrom*).
/// It may not call methods from a higher tier.
///
/// Note that since WalkUpFromFoo() calls WalkUpFromBar() (where Bar
/// is Foo's super class) before calling VisitFoo(), the result is
/// that the Visit*() methods for a given node are called in the
/// top-down order (e.g. for a node of type NamedDefn, the order will
/// be VisitDefn(), VisitNamedDefn(), and then VisitNamespaceDefn()).
///
/// This scheme guarantees that all Visit*() calls for the same AST
/// node are grouped together.  In other words, Visit*() methods for
/// different nodes are never interleaved.
///
/// Clients of this visitor should subclass the visitor (providing
/// themselves as the template argument, using the curiously recurring
/// template pattern) and override any of the Traverse*, WalkUpFrom*,
/// and Visit* methods for declarations, types, statements,
/// expressions, or other AST nodes where the visitor should customize
/// behavior.  Most users only need to override Visit*.  Advanced
/// users may override Traverse* and WalkUpFrom* to implement custom
/// traversal strategies.  Returning false from one of these overridden
/// functions will abort the entire traversal.
///
/// By default, this visitor tries to visit every part of the explicit
/// source code exactly once.  The default policy towards templates
/// is to descend into the 'pattern' class or function body, not any
/// explicit or implicit instantiations.  Explicit specializations
/// are still visited, and the patterns of partial specializations
/// are visited separately.  This behavior can be changed by
/// overriding shouldVisitTemplateInstantiations() in the derived class
/// to return true, in which case all known implicit and explicit
/// instantiations will be visited at the same time as the pattern
/// from which they were produced.
template<typename Derived>
class RecursiveASTVisitor {
public:
  /// \brief Return a reference to the derived class.
  Derived &getDerived() { return *static_cast<Derived*>(this); }

  /// \brief Return whether this visitor should recurse into
  /// template instantiations.
  bool shouldVisitTemplateInstantiations() const { return false; }

  /// \brief Return whether this visitor should recurse into the types of
  /// TypeLocs.
  bool shouldWalkTypesOfTypeLocs() const { return true; }

  /// \brief Recursively visit a statement or expression, by
  /// dispatching to Traverse*() based on the argument's dynamic type.
  ///
  /// \returns false if the visitation was terminated early, true
  /// otherwise (including when the argument is NULL).
  bool TraverseStmt(Stmt *S);

  /// \brief Recursively visit a type, by dispatching to
  /// Traverse*Type() based on the argument's getTypeClass() property.
  ///
  /// \returns false if the visitation was terminated early, true
  /// otherwise (including when the argument is a Null type).
  bool TraverseType(Type T);

  /// \brief Recursively visit a declaration, by dispatching to
  /// Traverse*Defn() based on the argument's dynamic type.
  ///
  /// \returns false if the visitation was terminated early, true
  /// otherwise (including when the argument is NULL).
  bool TraverseDefn(Defn *D);

  /// \brief Recursively visit a C++ nested-name-specifier.
  ///
  /// \returns false if the visitation was terminated early, true otherwise.
  bool TraverseNestedNameSpecifier(NestedNameSpecifier *NNS);

  /// \brief Recursively visit a constructor initializer.  This
  /// automatically dispatches to another visitor for the initializer
  /// expression, but not for the name of the initializer, so may
  /// be overridden for clients that need access to the name.
  ///
  /// \returns false if the visitation was terminated early, true otherwise.
  //bool TraverseConstructorInitializer(ClassCtorInitializer *Init);

  // ---- Methods on Stmts ----

  // Declare Traverse*() for all concrete Stmt classes.
#define ABSTRACT_STMT(STMT)
#define STMT(CLASS, PARENT)                                     \
  bool Traverse##CLASS(CLASS *S);
#include "mlang/AST/StmtNodes.inc"
  // The above header #undefs ABSTRACT_STMT and STMT upon exit.

  // Define WalkUpFrom*() and empty Visit*() for all Stmt classes.
  bool WalkUpFromStmt(Stmt *S) { return getDerived().VisitStmt(S); }
  bool VisitStmt(Stmt *S) { return true; }
#define STMT(CLASS, PARENT)                                     \
  bool WalkUpFrom##CLASS(CLASS *S) {                            \
    TRY_TO(WalkUpFrom##PARENT(S));                              \
    TRY_TO(Visit##CLASS(S));                                    \
    return true;                                                \
  }                                                             \
  bool Visit##CLASS(CLASS *S) { return true; }
#include "mlang/AST/StmtNodes.inc"

  // Define Traverse*(), WalkUpFrom*(), and Visit*() for unary
  // operator methods.  Unary operators are not classes in themselves
  // (they're all opcodes in UnaryOperator) but do have visitors.
#define OPERATOR(NAME)                                           \
  bool TraverseUnary##NAME(UnaryOperator *S) {                  \
    TRY_TO(WalkUpFromUnary##NAME(S));                           \
    TRY_TO(TraverseStmt(S->getSubExpr()));                      \
    return true;                                                \
  }                                                             \
  bool WalkUpFromUnary##NAME(UnaryOperator *S) {                \
    TRY_TO(WalkUpFromUnaryOperator(S));                         \
    TRY_TO(VisitUnary##NAME(S));                                \
    return true;                                                \
  }                                                             \
  bool VisitUnary##NAME(UnaryOperator *S) { return true; }

  UNARYOP_LIST()
#undef OPERATOR

  // Define Traverse*(), WalkUpFrom*(), and Visit*() for binary
  // operator methods.  Binary operators are not classes in themselves
  // (they're all opcodes in BinaryOperator) but do have visitors.
#define GENERAL_BINOP_FALLBACK(NAME, BINOP_TYPE)                \
  bool TraverseBin##NAME(BINOP_TYPE *S) {                       \
    TRY_TO(WalkUpFromBin##NAME(S));                             \
    TRY_TO(TraverseStmt(S->getLHS()));                          \
    TRY_TO(TraverseStmt(S->getRHS()));                          \
    return true;                                                \
  }                                                             \
  bool WalkUpFromBin##NAME(BINOP_TYPE *S) {                     \
    TRY_TO(WalkUpFrom##BINOP_TYPE(S));                          \
    TRY_TO(VisitBin##NAME(S));                                  \
    return true;                                                \
  }                                                             \
  bool VisitBin##NAME(BINOP_TYPE *S) { return true; }

#define OPERATOR(NAME) GENERAL_BINOP_FALLBACK(NAME, BinaryOperator)
  BINOP_LIST()
#undef OPERATOR

  // Define Traverse*(), WalkUpFrom*(), and Visit*() for compound
  // assignment methods.  Compound assignment operators are not
  // classes in themselves (they're all opcodes in
  // CompoundAssignOperator) but do have visitors.
#define OPERATOR(NAME) \
  GENERAL_BINOP_FALLBACK(NAME##Assign, CompoundAssignOperator)

  CAO_LIST()
#undef OPERATOR
#undef GENERAL_BINOP_FALLBACK

  // ---- Methods on Types ----
  // FIXME: revamp to take TypeLoc's rather than Types.

  // Declare Traverse*() for all concrete Type classes.
#define ABSTRACT_TYPE(CLASS, BASE)
#define TYPE(CLASS, BASE) \
  bool Traverse##CLASS##Type(CLASS##Type *T);
#include "mlang/AST/TypeNodes.def"
  // The above header #undefs ABSTRACT_TYPE and TYPE upon exit.

  // Define WalkUpFrom*() and empty Visit*() for all Type classes.
  bool WalkUpFromType(Type *T) { return getDerived().VisitType(T); }
  bool VisitType(Type *T) { return true; }
#define TYPE(CLASS, BASE)                                       \
  bool WalkUpFrom##CLASS##Type(CLASS##Type *T) {                \
    TRY_TO(WalkUpFrom##BASE(T));                                \
    TRY_TO(Visit##CLASS##Type(T));                              \
    return true;                                                \
  }                                                             \
  bool Visit##CLASS##Type(CLASS##Type *T) { return true; }
#include "mlang/AST/TypeNodes.def"

  // ---- Methods on Defns ----

  // Declare Traverse*() for all concrete Decl classes.
#define ABSTRACT_DEFN(DEFN)
#define DEFN(CLASS, BASE) \
  bool Traverse##CLASS##Defn(CLASS##Defn *D);
#include "mlang/AST/DefnNodes.inc"
  // The above header #undefs ABSTRACT_DECL and DECL upon exit.

  // Define WalkUpFrom*() and empty Visit*() for all Defn classes.
  bool WalkUpFromDefn(Defn *D) { return getDerived().VisitDefn(D); }
  bool VisitDefn(Defn *D) { return true; }
#define DEFN(CLASS, BASE)                                       \
  bool WalkUpFrom##CLASS##Defn(CLASS##Defn *D) {                \
    TRY_TO(WalkUpFrom##BASE(D));                                \
    TRY_TO(Visit##CLASS##Defn(D));                              \
    return true;                                                \
  }                                                             \
  bool Visit##CLASS##Defn(CLASS##Defn *D) { return true; }
#include "mlang/AST/DefnNodes.inc"

private:
  // These are helper methods used by more than one Traverse* method.
  bool TraverseTypeHelper(TypeDefn *D);
  bool TraverseUserClassHelper(UserClassDefn *D);
  bool TraverseDefnContextHelper(DefnContext *DC);
  bool TraverseFunctionHelper(FunctionDefn *D);
  bool TraverseVarHelper(VarDefn *D);
};

#define DISPATCH(NAME, CLASS, VAR) \
  return getDerived().Traverse##NAME(static_cast<CLASS*>(VAR))

template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseStmt(Stmt *S) {
  if (!S)
    return true;

  // If we have a binary expr, dispatch to the subcode of the binop.  A smart
  // optimizer (e.g. LLVM) will fold this comparison into the switch stmt
  // below.
  if (BinaryOperator *BinOp = dyn_cast<BinaryOperator>(S)) {
    switch (BinOp->getOpcode()) {
#define OPERATOR(NAME) \
    case BO_##NAME: DISPATCH(Bin##NAME, BinaryOperator, S);

    BINOP_LIST()
#undef OPERATOR
#undef BINOP_LIST

#define OPERATOR(NAME)                                          \
    case BO_##NAME##Assign:                          \
      DISPATCH(Bin##NAME##Assign, CompoundAssignOperator, S);

    CAO_LIST()
#undef OPERATOR
#undef CAO_LIST
    }
  } else if (UnaryOperator *UnOp = dyn_cast<UnaryOperator>(S)) {
    switch (UnOp->getOpcode()) {
    case UO_UnaryOpNums:
    	break;
#define OPERATOR(NAME)                                                  \
    case UO_##NAME: DISPATCH(Unary##NAME, UnaryOperator, S);

    UNARYOP_LIST()
#undef OPERATOR
#undef UNARYOP_LIST
    }
  }

  // Top switch stmt: dispatch to TraverseFooStmt for each concrete FooStmt.
  switch (S->getStmtClass()) {
  case Stmt::NoStmtClass: break;
#define ABSTRACT_STMT(STMT)
#define STMT(CLASS, PARENT) \
  case Stmt::CLASS##Class: DISPATCH(CLASS, CLASS, S);
#include "mlang/AST/StmtNodes.inc"
  }

  return true;
}

template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseType(Type T) {
  if (T.isNull())
    return true;

  switch (T->getTypeClass()) {
#define ABSTRACT_TYPE(CLASS, BASE)
#define TYPE(CLASS, BASE) \
  case RawType::CLASS: DISPATCH(CLASS##Type, CLASS##Type, \
                             const_cast<RawType*>(T.getRawTypePtr()));
#include "mlang/AST/TypeNodes.def"
  }

  return true;
}

template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseDefn(Defn *D) {
  if (!D)
    return true;

  // As a syntax visitor, we want to ignore declarations for
  // implicitly-defined declarations (ones not typed explicitly by the
  // user).
  if (D->isImplicit())
    return true;

  switch (D->getKind()) {
#define ABSTRACT_DEFN(DEFN)
#define DEFN(CLASS, BASE) \
  case Defn::CLASS: DISPATCH(CLASS##Defn, CLASS##Defn, D);
#include "mlang/AST/DefnNodes.inc"
 }

  return true;
}

#undef DISPATCH

template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseNestedNameSpecifier(
                                                    NestedNameSpecifier *NNS) {
  if (!NNS)
    return true;

  if (NNS->getPrefix())
    TRY_TO(TraverseNestedNameSpecifier(NNS->getPrefix()));

  switch (NNS->getKind()) {
  case NestedNameSpecifier::Identifier:
  case NestedNameSpecifier::Namespace:
  case NestedNameSpecifier::Global:
    return true;

  case NestedNameSpecifier::TypeSpec:
    TRY_TO(TraverseType(Type(NNS->getAsType(), 0)));
  }

  return true;
}

// ----------------- Type traversal -----------------

// This macro makes available a variable T, the passed-in type.
#define DEF_TRAVERSE_TYPE(TYPE, CODE)                     \
  template<typename Derived>                                           \
  bool RecursiveASTVisitor<Derived>::Traverse##TYPE (TYPE *T) {        \
    TRY_TO(WalkUpFrom##TYPE (T));                                      \
    { CODE; }                                                          \
    return true;                                                       \
  }

DEF_TRAVERSE_TYPE(SimpleNumericType, { })

DEF_TRAVERSE_TYPE(FunctionHandleType, {
    TRY_TO(TraverseType(T->getPointeeType()));
  })

DEF_TRAVERSE_TYPE(LValueReferenceType, {
    TRY_TO(TraverseType(T->getPointeeType()));
  })

DEF_TRAVERSE_TYPE(RValueReferenceType, {
    TRY_TO(TraverseType(T->getPointeeType()));
  })

DEF_TRAVERSE_TYPE(VectorType, {
    TRY_TO(TraverseType(T->getElementType()));
  })

DEF_TRAVERSE_TYPE(MatrixType, {
    TRY_TO(TraverseType(T->getElementType()));
  })

DEF_TRAVERSE_TYPE(NDArrayType, {
    TRY_TO(TraverseType(T->getElementType()));
  })

DEF_TRAVERSE_TYPE(FunctionNoProtoType, {
    TRY_TO(TraverseType(T->getResultType()));
  })

DEF_TRAVERSE_TYPE(FunctionProtoType, {
    TRY_TO(TraverseType(T->getResultType()));

    for (FunctionProtoType::arg_type_iterator A = T->arg_type_begin(),
                                           AEnd = T->arg_type_end();
         A != AEnd; ++A) {
      TRY_TO(TraverseType(*A));
    }

    for (FunctionProtoType::exception_iterator E = T->exception_begin(),
                                            EEnd = T->exception_end();
         E != EEnd; ++E) {
      TRY_TO(TraverseType(*E));
    }
})

#undef DEF_TRAVERSE_TYPE

// ----------------- Defn traversal -----------------
//
// For a Defn, we automate (in the DEF_TRAVERSE_DEFN macro) traversing
// the children that come from the DefnContext associated with it.
// Therefore each Traverse* only needs to worry about children other
// than those.
template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseDefnContextHelper(DefnContext *DC) {
  if (!DC)
    return true;

  for (DefnContext::defn_iterator Child = DC->defns_begin(),
    ChildEnd = DC->defns_end(); Child != ChildEnd; ++Child) {
    // BlockDefns are traversed through BlockExprs.
    //if (!isa<ScriptDefn>(*Child))
    TRY_TO(TraverseDefn(*Child));
  }

  return true;
}

// This macro makes available a variable D, the passed-in defn.
#define DEF_TRAVERSE_DEFN(DEFN, CODE)                           \
template<typename Derived>                                      \
bool RecursiveASTVisitor<Derived>::Traverse##DEFN (DEFN *D) {   \
  TRY_TO(WalkUpFrom##DEFN (D));                                 \
  { CODE; }                                                     \
  TRY_TO(TraverseDefnContextHelper(dyn_cast<DefnContext>(D)));  \
  return true;                                                  \
}

DEF_TRAVERSE_DEFN(ScriptDefn, {
//    TRY_TO(TraverseTypeLoc(D->getSignatureAsWritten()->getTypeLoc()));
    TRY_TO(TraverseStmt(D->getBody()));
    // This return statement makes sure the traversal of nodes in
    // decls_begin()/decls_end() (done in the DEF_TRAVERSE_DECL macro)
    // is skipped - don't remove it.
    //return true;
})

DEF_TRAVERSE_DEFN(TranslationUnitDefn, {
    // Code in an unnamed namespace shows up automatically in
    // decls_begin()/decls_end().  Thus we don't need to recurse on
    // D->getAnonymousNamespace().
})

DEF_TRAVERSE_DEFN(NamespaceDefn, {
    // Code in an unnamed namespace shows up automatically in
    // decls_begin()/decls_end().  Thus we don't need to recurse on
    // D->getAnonymousNamespace().
})

// Helper methods for RecordDecl and its children.
template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseTypeHelper(
    TypeDefn *D) {
  // We shouldn't traverse D->getTypeForDecl(); it's a result of
  // declaring the type, not something that was written in the source.

  //TRY_TO(TraverseNestedNameSpecifierLoc(D->getQualifierLoc()));
  return true;
}

template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseUserClassHelper(
    UserClassDefn *D) {
  if (!TraverseTypeHelper(D))
    return false;

  for (UserClassDefn::base_class_iterator I = D->bases_begin(),
                                            E = D->bases_end();
         I != E; ++I) {
 //     TRY_TO(TraverseTypeLoc(I->getTypeSourceInfo()->getTypeLoc()));
    }

  return true;
}

DEF_TRAVERSE_DEFN(StructDefn, {
    TRY_TO(TraverseTypeHelper(D));
})

DEF_TRAVERSE_DEFN(CellDefn, {
    TRY_TO(TraverseTypeHelper(D));
})

DEF_TRAVERSE_DEFN(UserClassDefn, {
    TRY_TO(TraverseUserClassHelper(D));
})

template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseFunctionHelper(FunctionDefn *D) {
  //TRY_TO(TraverseNestedNameSpecifierLoc(D->getQualifierLoc()));

  // Visit the function type itself, which can be either
  // FunctionNoProtoType or FunctionProtoType, or a typedef.  This
  // also covers the return type and the function parameters,
  // including exception specifications.
  //TRY_TO(TraverseTypeLoc(D->getTypeSourceInfo()->getTypeLoc()));

//  if (ClassConstructorDefn *Ctor = dyn_cast<ClassConstructorDefn>(D)) {
//    // Constructor initializers.
//    for (ClassConstructorDefn::init_iterator I = Ctor->init_begin(),
//                                           E = Ctor->init_end();
//         I != E; ++I) {
//      TRY_TO(TraverseConstructorInitializer(*I));
//    }
//  }
//
//  if (D->isThisDeclarationADefinition()) {
//    TRY_TO(TraverseStmt(D->getBody()));  // Function body.
//  }
  return true;
}

DEF_TRAVERSE_DEFN(FunctionDefn, {
  // We skip decls_begin/decls_end, which are already covered by
  // TraverseFunctionHelper().
  return TraverseFunctionHelper(D);
})

DEF_TRAVERSE_DEFN(ClassMethodDefn, {
  // We skip decls_begin/decls_end, which are already covered by
  // TraverseFunctionHelper().
  return TraverseFunctionHelper(D);
})

DEF_TRAVERSE_DEFN(ClassConstructorDefn, {
  // We skip decls_begin/decls_end, which are already covered by
  // TraverseFunctionHelper().
  return TraverseFunctionHelper(D);
})

DEF_TRAVERSE_DEFN(ClassDestructorDefn, {
  // We skip decls_begin/decls_end, which are already covered by
  // TraverseFunctionHelper().
  return TraverseFunctionHelper(D);
})

template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseVarHelper(VarDefn *D) {
//  TRY_TO(TraverseDeclaratorHelper(D));
  TRY_TO(TraverseStmt(D->getInit()));
  return true;
}

DEF_TRAVERSE_DEFN(VarDefn, {
  TRY_TO(TraverseVarHelper(D));
})

DEF_TRAVERSE_DEFN(MemberDefn, {
    TRY_TO(TraverseVarHelper(D));
//    if (D->isBitField())
//      TRY_TO(TraverseStmt(D->getBitWidth()));
})

DEF_TRAVERSE_DEFN(ImplicitParamDefn, {
    TRY_TO(TraverseVarHelper(D));
})

DEF_TRAVERSE_DEFN(ParamVarDefn, {
  TRY_TO(TraverseVarHelper(D));
//
//    if (D->hasDefaultArg() &&
//        D->hasUninstantiatedDefaultArg() &&
//        !D->hasUnparsedDefaultArg())
//      TRY_TO(TraverseStmt(D->getUninstantiatedDefaultArg()));
//
//    if (D->hasDefaultArg() &&
//        !D->hasUninstantiatedDefaultArg() &&
//        !D->hasUnparsedDefaultArg())
//      TRY_TO(TraverseStmt(D->getDefaultArg()));
  })

DEF_TRAVERSE_DEFN(AnonFunctionDefn, {
//
})

DEF_TRAVERSE_DEFN(ClassAttributesDefn, {
//
})

DEF_TRAVERSE_DEFN(ClassEventDefn, {
//
})

DEF_TRAVERSE_DEFN(ClassEventsAttrsDefn, {
//
})

DEF_TRAVERSE_DEFN(ClassMethodsAttrsDefn, {
//
})

DEF_TRAVERSE_DEFN(ClassPropertyDefn, {
//
})

DEF_TRAVERSE_DEFN(ClassPropertiesAttrsDefn, {
//
})

DEF_TRAVERSE_DEFN(UserObjectDefn, {
//
})

#undef DEF_TRAVERSE_DEFN

// ----------------- Stmt traversal -----------------
//
// For stmts, we automate (in the DEF_TRAVERSE_STMT macro) iterating
// over the children defined in children() (every stmt defines these,
// though sometimes the range is empty).  Each individual Traverse*
// method only needs to worry about children other than those.  To see
// what children() does for a given class, see, e.g.,
//   http://clang.llvm.org/doxygen/Stmt_8cpp_source.html

// This macro makes available a variable S, the passed-in stmt.
#define DEF_TRAVERSE_STMT(STMT, CODE)                                   \
template<typename Derived>                                              \
bool RecursiveASTVisitor<Derived>::Traverse##STMT (STMT *S) {           \
  TRY_TO(WalkUpFrom##STMT(S));                                          \
  { CODE; }                                                             \
  for (Stmt::child_range range = S->children(); range; ++range) {       \
    TRY_TO(TraverseStmt(*range));                                       \
  }                                                                     \
  return true;                                                          \
}

DEF_TRAVERSE_STMT(CatchCmd, {
// TRY_TO(TraverseDefn(S->getExceptionDefn()));
// children() iterates over the handler block.
})

DEF_TRAVERSE_STMT(DefnCmd, {
  for (DefnCmd::defn_iterator I = S->defn_begin(), E = S->defn_end();
       I != E; ++I) {
    TRY_TO(TraverseDefn(*I));
    }
    // Suppress the default iteration over children() by
    // returning.  Here's why: A DeclStmt looks like 'type var [=
    // initializer]'.  The decls above already traverse over the
    // initializers, so we don't have to do it again (which
    // children() would do).
    return true;
  })


// These non-expr stmts (most of them), do not need any action except
// iterating over the children.
DEF_TRAVERSE_STMT(BreakCmd, { })
DEF_TRAVERSE_STMT(TryCmd, { })
DEF_TRAVERSE_STMT(CaseCmd, { })
DEF_TRAVERSE_STMT(BlockCmd, { })
DEF_TRAVERSE_STMT(ContinueCmd, { })
DEF_TRAVERSE_STMT(OtherwiseCmd, { })
DEF_TRAVERSE_STMT(ForCmd, { })
DEF_TRAVERSE_STMT(IfCmd, { })
DEF_TRAVERSE_STMT(NullCmd, { })
DEF_TRAVERSE_STMT(ReturnCmd, { })
DEF_TRAVERSE_STMT(SwitchCmd, { })
DEF_TRAVERSE_STMT(WhileCmd, { })
DEF_TRAVERSE_STMT(ThenCmd, { })
DEF_TRAVERSE_STMT(ElseCmd, { })
DEF_TRAVERSE_STMT(ElseIfCmd, { })
DEF_TRAVERSE_STMT(ScopeDefnCmd, { })



DEF_TRAVERSE_STMT(DefnRefExpr, { })

DEF_TRAVERSE_STMT(MemberExpr, { })

// InitListExpr is a tricky one, because we want to do all our work on
// the syntactic form of the listexpr, but this method takes the
// semantic form by default.  We can't use the macro helper because it
// calls WalkUp*() on the semantic form, before our code can convert
// to the syntactic form.
template<typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseInitListExpr(InitListExpr *S) {
  if (InitListExpr *Syn = S->getSyntacticForm())
    S = Syn;
  TRY_TO(WalkUpFromInitListExpr(S));
  // All we need are the default actions.  FIXME: use a helper function.
  for (Stmt::child_range range = S->children(); range; ++range) {
    TRY_TO(TraverseStmt(*range));
  }
  return true;
}

// These expressions all might take explicit template arguments.
// We traverse those if so.  FIXME: implement these.
DEF_TRAVERSE_STMT(FunctionCall, { })
//DEF_TRAVERSE_STMT(ClassMemberCallExpr, { })

// These exprs (most of them), do not need any action except iterating
// over the children.
DEF_TRAVERSE_STMT(ArrayIndex, { })
DEF_TRAVERSE_STMT(ParenExpr, { })
DEF_TRAVERSE_STMT(ParenListExpr, { })
DEF_TRAVERSE_STMT(OpaqueValueExpr, { })

// These operators (all of them) do not need any action except
// iterating over the children.
DEF_TRAVERSE_STMT(UnaryOperator, { })
DEF_TRAVERSE_STMT(BinaryOperator, { })
DEF_TRAVERSE_STMT(CompoundAssignOperator, { })


// These literals (all of them) do not need any action.
DEF_TRAVERSE_STMT(IntegerLiteral, { })
DEF_TRAVERSE_STMT(CharacterLiteral, { })
DEF_TRAVERSE_STMT(FloatingLiteral, { })
DEF_TRAVERSE_STMT(ImaginaryLiteral, { })
DEF_TRAVERSE_STMT(StringLiteral, { })
DEF_TRAVERSE_STMT(FuncHandle, { })
DEF_TRAVERSE_STMT(AnonFuncHandle, { })
DEF_TRAVERSE_STMT(CellConcatExpr, { })
DEF_TRAVERSE_STMT(ColonExpr, { })
DEF_TRAVERSE_STMT(ConcatExpr, { })
DEF_TRAVERSE_STMT(MultiOutput, { })
DEF_TRAVERSE_STMT(RowVectorExpr, { })

// FIXME: look at the following tricky-seeming exprs to see if we
// need to recurse on anything.  These are ones that have methods
// returning decls or qualtypes or nestednamespecifier -- though I'm
// not sure if they own them -- or just seemed very complicated, or
// had lots of sub-types to explore.
//
// VisitOverloadExpr and its children: recurse on template args? etc?

// FIXME: go through all the stmts and exprs again, and see which of them
// create new types, and recurse on the types (TypeLocs?) of those.
// Candidates:
//
//    http://clang.llvm.org/doxygen/classclang_1_1CXXTypeidExpr.html
//    http://clang.llvm.org/doxygen/classclang_1_1UnaryExprOrTypeTraitExpr.html
//    http://clang.llvm.org/doxygen/classclang_1_1TypesCompatibleExpr.html
//    Every class that has getQualifier.

#undef DEF_TRAVERSE_STMT

#undef TRY_TO
} // end namespace mlang

#endif /* MLANG_AST_RECURSIVE_ASTVISITOR_H_ */
