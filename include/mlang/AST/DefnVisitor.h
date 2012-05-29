//===--- DefnVisitor.h - Visitor for Defn subclasses ------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the DefnVisitor interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_DEFNVISITOR_H_
#define MLANG_AST_DEFNVISITOR_H_

#include "mlang/AST/DefnOOP.h"

namespace mlang {

#define DISPATCH(NAME, CLASS) \
  return static_cast<ImplClass*>(this)-> Visit##NAME(static_cast<CLASS*>(D))

/// \brief A simple visitor class that helps create definition visitors.
template<typename ImplClass, typename RetTy=void>
class DefnVisitor {
public:
  RetTy Visit(Defn *D) {
    switch (D->getKind()) {
      default: assert(false && "Defn that isn't part of DefnNodes.inc!");
#define DEFN(DERIVED, BASE) \
      case Defn::DERIVED: DISPATCH(DERIVED##Defn, DERIVED##Defn);
#define ABSTRACT_DEFN(DEFN)
#include "mlang/AST/DefnNodes.inc"
    }
  }

  // If the implementation chooses not to implement a certain visit
  // method, fall back to the parent.
#define DEFN(DERIVED, BASE) \
  RetTy Visit##DERIVED##Defn(DERIVED##Defn *D) { DISPATCH(BASE, BASE); }
#include "mlang/AST/DefnNodes.inc"

  RetTy VisitDefn(Defn *D) { return RetTy(); }
};

#undef DISPATCH

} // end namespace mlang

#endif /* MLANG_AST_DEFNVISITOR_H_ */
