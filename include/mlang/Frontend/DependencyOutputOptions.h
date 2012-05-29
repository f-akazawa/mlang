//===--- DependencyOutputOptions.h ------------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_DEPENDENCY_OUTPUT_OPTIONS_H_
#define MLANG_FRONTEND_DEPENDENCY_OUTPUT_OPTIONS_H_

#include <string>
#include <vector>

namespace mlang {

/// DependencyOutputOptions - Options for controlling the compiler dependency
/// file generation.
class DependencyOutputOptions {
public:
  unsigned IncludeSystemHeaders : 1; ///< Include system header dependencies.
  unsigned ShowHeaderIncludes : 1;   ///< Show header inclusions (-H).
  unsigned UsePhonyTargets : 1;      ///< Include phony targets for each
                                     /// dependency, which can avoid some 'make'
                                     /// problems.

  /// The file to write dependency output to.
  std::string OutputFile;

  /// The file to write header include output to. This is orthogonal to
  /// ShowHeaderIncludes (-H) and will include headers mentioned in the
  /// predefines buffer. If the output file is "-", output will be sent to
  /// stderr.
  std::string HeaderIncludeOutputFile;

  /// A list of names to use as the targets in the dependency file; this list
  /// must contain at least one entry.
  std::vector<std::string> Targets;

public:
  DependencyOutputOptions() {
    IncludeSystemHeaders = 0;
    ShowHeaderIncludes = 0;
    UsePhonyTargets = 0;
  }
};

}  // end namespace mlang

#endif /* MLANG_FRONTEND_DEPENDENCY_OUTPUT_OPTIONS_H_ */
