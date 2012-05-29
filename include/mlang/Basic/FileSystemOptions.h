//===--- FileSystemOptions.h - File System Options --------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the FileSystemOptions interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_FILESYSTEM_OPTIONS_H_
#define MLANG_BASIC_FILESYSTEM_OPTIONS_H_

#include <string>

namespace mlang {

/// \brief Keeps track of options that affect how file operations are performed.
class FileSystemOptions {
public:
  /// \brief If set, paths are resolved as if the working directory was
  /// set to the value of WorkingDir.
  std::string WorkingDir;
};

} // end namespace mlang

#endif /* MLANG_BASIC_FILESYSTEM_OPTIONS_H_ */
