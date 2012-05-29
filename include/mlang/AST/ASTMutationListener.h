//===--- ASTMutationListener.h - AST Mutation Interface --------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTMutationListener interface.
//
//===----------------------------------------------------------------------===//
#ifndef MLANG_AST_ASTMUTATIONLISTENER_H_
#define MLANG_AST_ASTMUTATIONLISTENER_H_

namespace mlang {
  class Defn;
  class DefnContext;
  class TypeDefn;
  class UserClassDefn;

/// \brief An abstract interface that should be implemented by listeners
/// that want to be notified when an AST entity gets modified after its
/// initial creation.
class ASTMutationListener {
public:
  virtual ~ASTMutationListener();

  /// \brief A new VarDefn definition was completed.
  virtual void CompletedTypeDefinition(const TypeDefn *D) {}

  /// \brief A new definition with name has been added to a DefnContext.
  virtual void AddedVisibleDefn(const DefnContext *DC, const Defn *D) {}

  /// \brief An implicit member was added after the definition was completed.
  virtual void AddedClassImplicitMember(const UserClassDefn *RD, const Defn *D) {}
};

} // end namespace mlang

#endif /* MLANG_AST_ASTMUTATIONLISTENER_H_ */
