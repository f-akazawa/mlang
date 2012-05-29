//===--- CommandLineSourceLoc.h - Parsing for source locations-*- C++ -*---===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Command line parsing for source locations.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_COMMANDLINE_SOURCELOC_H_
#define MLANG_FRONTEND_COMMANDLINE_SOURCELOC_H_

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

namespace mlang {

/// \brief A source location that has been parsed on the command line.
struct ParsedSourceLocation {
  std::string FileName;
  unsigned Line;
  unsigned Column;

public:
  /// Construct a parsed source location from a string; the Filename is empty on
  /// error.
  static ParsedSourceLocation FromString(llvm::StringRef Str) {
    ParsedSourceLocation PSL;
    std::pair<llvm::StringRef, llvm::StringRef> ColSplit = Str.rsplit(':');
    std::pair<llvm::StringRef, llvm::StringRef> LineSplit =
      ColSplit.first.rsplit(':');

    // If both tail splits were valid integers, return success.
    if (!ColSplit.second.getAsInteger(10, PSL.Column) &&
        !LineSplit.second.getAsInteger(10, PSL.Line)) {
      PSL.FileName = LineSplit.first;

      // On the command-line, stdin may be specified via "-". Inside the
      // compiler, stdin is called "<stdin>".
      if (PSL.FileName == "-")
        PSL.FileName = "<stdin>";
    }

    return PSL;
  }
};

} // end namespace mlang

namespace llvm {
  namespace cl {
  /// \brief Command-line option parser that parses source locations.
  ///
  /// Source locations are of the form filename:line:column.
  template<>
  class parser<mlang::ParsedSourceLocation> :
    public basic_parser<mlang::ParsedSourceLocation> {
  public:
    inline bool parse(Option &O, StringRef ArgName, StringRef ArgValue,
    		              mlang::ParsedSourceLocation &Val);
    };

  bool parser<mlang::ParsedSourceLocation>:: parse(Option &O,
		  StringRef ArgName, StringRef ArgValue,
		  mlang::ParsedSourceLocation &Val) {
	  using namespace mlang;

	  Val = ParsedSourceLocation::FromString(ArgValue);
	  if (Val.FileName.empty()) {
		  errs() << "error: "
				  << "source location must be of the form filename:line:column\n";
		  return true;
	  }

	  return false;
  }

  } // end namespace cl
} // end namespace llvm

#endif /* MLANG_FRONTEND_COMMANDLINE_SOURCELOC_H_ */
