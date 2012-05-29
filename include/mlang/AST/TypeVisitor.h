//===--- TypeVisitor.h - Visitor for Type subclasses ------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the TypeVisitor interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_TYPEVISITOR_H_
#define MLANG_AST_TYPEVISITOR_H_

#include "mlang/AST/Type.h"

namespace mlang {

#define DISPATCH(CLASS) \
  return static_cast<ImplClass*>(this)->Visit ## CLASS(static_cast<CLASS*>(T))

template<typename ImplClass, typename RetTy=void>
class TypeVisitor {
public:
  RetTy Visit(RawType *T) {
    // Top switch stmt: dispatch to VisitFooType for each FooType.
    switch (T->getTypeClass()) {
    default: assert(0 && "Unknown type class!");
#define ABSTRACT_TYPE(CLASS, PARENT)
#define TYPE(CLASS, PARENT) case RawType::CLASS: DISPATCH(CLASS##Type);
#include "mlang/AST/TypeNodes.def"
    }
  }

  // If the implementation chooses not to implement a certain visit method, fall
  // back on superclass.
#define TYPE(CLASS, PARENT) RetTy Visit##CLASS##Type(CLASS##Type *T) {       \
  DISPATCH(PARENT);                                                          \
}
#include "mlang/AST/TypeNodes.def"

  // Base case, ignore it. :)
  RetTy VisitType(Type*) { return RetTy(); }
};

#undef DISPATCH

}  // end namespace mlang

#endif /* MLANG_AST_TYPEVISITOR_H_ */
