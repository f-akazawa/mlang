//===--- AnalysisBasedWarnings.h - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SEMA_ANALYSIS_BASED_WARNINGS_H_
#define MLANG_SEMA_ANALYSIS_BASED_WARNINGS_H_

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"

namespace mlang {
class Defn;
class FunctionDefn;
class Type;
class Sema;

namespace sema {

class AnalysisBasedWarnings {
public:
  class Policy {
    friend class AnalysisBasedWarnings;
    // The warnings to run.
    unsigned enableCheckFallThrough : 1;
    unsigned enableCheckUnreachable : 1;
  public:
    Policy();
    void disableCheckFallThrough() { enableCheckFallThrough = 0; }
  };

private:
  Sema &S;
  Policy DefaultPolicy;

  enum VisitFlag { NotVisited = 0, Visited = 1, Pending = 2 };
  llvm::DenseMap<const FunctionDefn*, VisitFlag> VisitedFD;

  void IssueWarnings(Policy P, const Defn *D, Type BlockTy);

public:
  AnalysisBasedWarnings(Sema &s);

  Policy getDefaultPolicy() { return DefaultPolicy; }

  // void IssueWarnings(Policy P, const BlockCmd *E);
  void IssueWarnings(Policy P, const FunctionDefn *D);
};

} // end namespace sema
} // end namespace mlang

#endif /* MLANG_SEMA_ANALYSIS_BASED_WARNINGS_H_ */
