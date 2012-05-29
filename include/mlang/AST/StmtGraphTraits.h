//===--- StmtGraphTraits.h - Graph Traits for the class Stmt ----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines a template specialization of llvm::GraphTraits to
//  treat ASTs (Stmt*) as graphs
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_STMT_GRAPH_TRAITS_H_
#define MLANG_AST_STMT_GRAPH_TRAITS_H_

#include "mlang/AST/Stmt.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/DepthFirstIterator.h"

namespace llvm {

//template <typename T> struct GraphTraits;


template <> struct GraphTraits<mlang::Stmt*> {
  typedef mlang::Stmt                       NodeType;
  typedef mlang::Stmt::child_iterator       ChildIteratorType;
  typedef llvm::df_iterator<mlang::Stmt*>   nodes_iterator;

  static NodeType* getEntryNode(mlang::Stmt* S) { return S; }

  static inline ChildIteratorType child_begin(NodeType* N) {
    if (N) return N->child_begin();
    else return ChildIteratorType();
  }

  static inline ChildIteratorType child_end(NodeType* N) {
    if (N) return N->child_end();
    else return ChildIteratorType();
  }

  static nodes_iterator nodes_begin(mlang::Stmt* S) {
    return df_begin(S);
  }

  static nodes_iterator nodes_end(mlang::Stmt* S) {
    return df_end(S);
  }
};


template <> struct GraphTraits<const mlang::Stmt*> {
  typedef const mlang::Stmt                       NodeType;
  typedef mlang::Stmt::const_child_iterator       ChildIteratorType;
  typedef llvm::df_iterator<const mlang::Stmt*>   nodes_iterator;

  static NodeType* getEntryNode(const mlang::Stmt* S) { return S; }

  static inline ChildIteratorType child_begin(NodeType* N) {
    if (N) return N->child_begin();
    else return ChildIteratorType();
  }

  static inline ChildIteratorType child_end(NodeType* N) {
    if (N) return N->child_end();
    else return ChildIteratorType();
  }

  static nodes_iterator nodes_begin(const mlang::Stmt* S) {
    return df_begin(S);
  }

  static nodes_iterator nodes_end(const mlang::Stmt* S) {
    return df_end(S);
  }
};


} // end namespace llvm

#endif /* MLANG_AST_STMT_GRAPH_TRAITS_H_ */
