//===--- PreprocessorLexer.cpp - Lexer for Mlang pre-processing -*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements the PreprocessorLexer interfaces.
//
//===----------------------------------------------------------------------===//

#include "mlang/Lex/PreprocessorLexer.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Lex/LexDiagnostic.h"
#include "mlang/Basic/SourceManager.h"
using namespace mlang;

/// LexImportFilename - After the preprocessor has parsed an import, lex and
/// (potentially) macro expand the filename.
void PreprocessorLexer::LexImportFilename(Token &FilenameTok) {
  assert(ParsingPreprocessorDirective &&
         ParsingFilename == false &&
         "Must be in a preprocessing directive!");

  // We are now parsing a filename!
  ParsingFilename = true;

  // Lex the filename.
  IndirectLex(FilenameTok);

  // We should have obtained the filename now.
  ParsingFilename = false;

  // No filename?
  if (FilenameTok.is(tok::EoD))
    PP->Diag(FilenameTok.getLocation(), diag::err_pp_expects_filename);
}

/// getFileEntry - Return the FileEntry corresponding to this FileID.  Like
/// getFileID(), this only works for lexers with attached preprocessors.
const FileEntry *PreprocessorLexer::getFileEntry() const {
  return PP->getSourceManager().getFileEntryForID(getFileID());
}
