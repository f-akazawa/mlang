//===--- TranslationUnit.h - Interface for a translation unit ---*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  Abstract interface for a translation unit.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_TRANSLATIONUNIT_H_
#define MLANG_FRONTEND_TRANSLATIONUNIT_H_

namespace mlang {
  class ASTContext;
  class Diagnostic;
  class Preprocessor;

namespace idx {
  class DefnReferenceMap;

/// \brief Abstract interface for a translation unit.
class TranslationUnit {
public:
  virtual ~TranslationUnit();
  virtual ASTContext &getASTContext() = 0;
  virtual Preprocessor &getPreprocessor() = 0;
  virtual Diagnostic &getDiagnostic() = 0;
  virtual DefnReferenceMap &getDefnReferenceMap() = 0;
};

} // namespace idx

} // namespace mlang

#endif /* MLANG_FRONTEND_TRANSLATIONUNIT_H_ */
