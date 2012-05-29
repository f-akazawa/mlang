//===--- PPCallbacks.h - Callbacks for Preprocessor actions -----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the PPCallbacks interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_LEX_PPCALLBACKS_H_
#define MLANG_LEX_PPCALLBACKS_H_

// #include "mlang/Lex/DirectoryLookup.h"
#include "mlang/Basic/SourceManager.h"
#include "llvm/ADT/StringRef.h"
#include <string>

namespace mlang {
class SourceLocation;
class Token;
class IdentifierInfo;

/// PPCallbacks - This interface provides a way to observe the actions of the
/// preprocessor as it does its thing.  Clients can define their hooks here to
/// implement preprocessor level tools.
class PPCallbacks {
public:
	virtual ~PPCallbacks(){}

	enum FileChangeReason {
		EnterFile, ExitFile, RenameFile
	};

	/// FileChanged - This callback is invoked whenever a source file is
	/// entered or exited.  The SourceLocation indicates the new location, and
	/// EnteringFile indicates whether this is because we are entering a new
	/// imported file (when true) or whether we're exiting one because we ran
	/// off the end (when false).
	virtual void FileChanged(SourceLocation Loc, FileChangeReason Reason,
			SrcMgr::CharacteristicKind FileType) {
	}

	/// FileSkipped - This callback is invoked whenever a source file is
	/// skipped as the result of header guard optimization.  ParentFile
	/// is the file that #includes the skipped file.  FilenameTok is the
	/// token in ParentFile that indicates the skipped file.
	virtual void FileSkipped(const FileEntry &ParentFile,
			const Token &FilenameTok, SrcMgr::CharacteristicKind FileType) {
	}

	/// \brief This callback is invoked whenever an inclusion directive of
	/// any kind (\c #include, \c #import, etc.) has been processed, regardless
	/// of whether the inclusion will actually result in an inclusion.
	///
	/// \param HashLoc The location of the '#' that starts the inclusion
	/// directive.
	///
	/// \param IncludeTok The token that indicates the kind of inclusion
	/// directive, e.g., 'include' or 'import'.
	///
	/// \param FileName The name of the file being included, as written in the
	/// source code.
	///
	/// \param IsAngled Whether the file name was enclosed in angle brackets;
	/// otherwise, it was enclosed in quotes.
	///
	/// \param File The actual file that may be included by this inclusion
	/// directive.
	///
	/// \param EndLoc The location of the last token within the inclusion
	/// directive.
	virtual void InclusionDirective(SourceLocation HashLoc, const Token &IncludeTok,
		llvm::StringRef FileName, bool IsAngled, const FileEntry *File,
		SourceLocation EndLoc) {
	}

	/// PragmaComment - This callback is invoked when a #pragma comment directive
	/// is read.
	///
	virtual void PragmaComment(SourceLocation Loc, const IdentifierInfo *Kind,
			const std::string &Str) {
	}

  /// PragmaMessage - This callback is invoked when a #pragma message directive
	/// is read.
	/// \param Loc The location of the message directive.
	/// \param str The text of the message directive.
	///
	virtual void PragmaMessage(SourceLocation Loc, llvm::StringRef Str) {
	}

	/// EndOfMainFile - This callback is invoked when the end of the main file is
	/// reach, no subsequent callbacks will be made.
	virtual void EndOfMainFile() {
	}
};

/// PPChainedCallbacks - Simple wrapper class for chaining callbacks.
class PPChainedCallbacks: public PPCallbacks {
	PPCallbacks *First, *Second;

public:
	PPChainedCallbacks(PPCallbacks *_First, PPCallbacks *_Second) :
		First(_First), Second(_Second) {
	}

	~PPChainedCallbacks() {
		delete Second;
		delete First;
	}

	virtual void FileChanged(SourceLocation Loc, FileChangeReason Reason,
			SrcMgr::CharacteristicKind FileType) {
		First->FileChanged(Loc, Reason, FileType);
		Second->FileChanged(Loc, Reason, FileType);
	}

	virtual void FileSkipped(const FileEntry &ParentFile,
			const Token &FilenameTok, SrcMgr::CharacteristicKind FileType) {
		First->FileSkipped(ParentFile, FilenameTok, FileType);
		Second->FileSkipped(ParentFile, FilenameTok, FileType);
	}

	virtual void EndOfMainFile() {
		First->EndOfMainFile();
		Second->EndOfMainFile();
	}

	virtual void PragmaComment(SourceLocation Loc, const IdentifierInfo *Kind,
	                           const std::string &Str) {
		First->PragmaComment(Loc, Kind, Str);
		Second->PragmaComment(Loc, Kind, Str);
	}

	virtual void PragmaMessage(SourceLocation Loc, llvm::StringRef Str) {
		First->PragmaMessage(Loc, Str);
		Second->PragmaMessage(Loc, Str);
	}
};
} // end namespace mlang

#endif /* MLANG_LEX_PPCALLBACKS_H_ */
