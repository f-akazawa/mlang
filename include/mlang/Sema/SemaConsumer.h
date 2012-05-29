//===--- SemaConsumer.h - Abstract interface for AST semantics --*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the SemaConsumer class, a subclass of
//  ASTConsumer that is used by AST clients that also require
//  additional semantic analysis.
//
//===----------------------------------------------------------------------===//
#ifndef MLANG_SEMA_SEMA_CONSUMER_H_
#define MLANG_SEMA_SEMA_CONSUMER_H_

#include "mlang/AST/ASTConsumer.h"

namespace mlang {
  class Sema;

  /// \brief An abstract interface that should be implemented by
  /// clients that read ASTs and then require further semantic
  /// analysis of the entities in those ASTs.
  class SemaConsumer : public ASTConsumer {
  public:
    SemaConsumer() {
      ASTConsumer::SemaConsumer = true;
    }

    /// \brief Initialize the semantic consumer with the Sema instance
    /// being used to perform semantic analysis on the abstract syntax
    /// tree.
    virtual void InitializeSema(Sema &S) {}

    /// \brief Inform the semantic consumer that Sema is no longer available.
    virtual void ForgetSema() {}

    // isa/cast/dyn_cast support
    static bool classof(const ASTConsumer *Consumer) {
      return Consumer->SemaConsumer;
    }
    static bool classof(const SemaConsumer *) { return true; }
  };
}

#endif /* MLANG_SEMA_SEMA_CONSUMER_H_ */
