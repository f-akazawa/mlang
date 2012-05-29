//===--- PreprocessorOutputOptions.h ----------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_PREPROCESSOR_OUTPUTOPTIONS_H_
#define MLANG_FRONTEND_PREPROCESSOR_OUTPUTOPTIONS_H_

namespace mlang {

/// PreprocessorOutputOptions - Options for controlling the C preprocessor
/// output (e.g., -E).
class PreprocessorOutputOptions {
public:
  unsigned ShowCPP : 1;            ///< Print normal preprocessed output.
  unsigned ShowComments : 1;       ///< Show comments.
  unsigned ShowLineMarkers : 1;    ///< Show #line markers.

public:
  PreprocessorOutputOptions() {
    ShowCPP = 1;
    ShowComments = 0;
    ShowLineMarkers = 1;
  }
};

}  // end namespace mlang

#endif /* MLANG_FRONTEND_PREPROCESSOR_OUTPUTOPTIONS_H_ */
