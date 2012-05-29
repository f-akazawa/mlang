//===--- InitPreprocessor.cpp - PP initialization code. ---------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the mlang::InitializePreprocessor function.
//
//===----------------------------------------------------------------------===//

//#include "clang/Basic/Version.h"
#include "mlang/Frontend/Utils.h"
//#include "mlang/Basic/MacroBuilder.h"
#include "mlang/Basic/TargetInfo.h"
#include "mlang/Frontend/FrontendDiagnostic.h"
#include "mlang/Frontend/FrontendOptions.h"
#include "mlang/Frontend/PreprocessorOptions.h"
#include "mlang/Lex/Preprocessor.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Basic/SourceManager.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
using namespace mlang;

// Initialize the remapping of files to alternative contents, e.g.,
// those specified through other files.
static void InitializeFileRemapping(Diagnostic &Diags,
                                    SourceManager &SourceMgr,
                                    FileManager &FileMgr,
                                    const PreprocessorOptions &InitOpts) {
  // Remap files in the source manager (with buffers).
  for (PreprocessorOptions::const_remapped_file_buffer_iterator
         Remap = InitOpts.remapped_file_buffer_begin(),
         RemapEnd = InitOpts.remapped_file_buffer_end();
       Remap != RemapEnd;
       ++Remap) {
    // Create the file entry for the file that we're mapping from.
    const FileEntry *FromFile = FileMgr.getVirtualFile(Remap->first,
                                                Remap->second->getBufferSize(),
                                                       0);
    if (!FromFile) {
      Diags.Report(diag::err_fe_remap_missing_from_file)
        << Remap->first;
      if (!InitOpts.RetainRemappedFileBuffers)
        delete Remap->second;
      continue;
    }

    // Override the contents of the "from" file with the contents of
    // the "to" file.
    SourceMgr.overrideFileContents(FromFile, Remap->second,
                                   InitOpts.RetainRemappedFileBuffers);
  }

  // Remap files in the source manager (with other files).
  for (PreprocessorOptions::const_remapped_file_iterator
         Remap = InitOpts.remapped_file_begin(),
         RemapEnd = InitOpts.remapped_file_end();
       Remap != RemapEnd;
       ++Remap) {
    // Find the file that we're mapping to.
    const FileEntry *ToFile = FileMgr.getFile(Remap->second);
    if (!ToFile) {
      Diags.Report(diag::err_fe_remap_missing_to_file)
      << Remap->first << Remap->second;
      continue;
    }
    
    // Create the file entry for the file that we're mapping from.
    const FileEntry *FromFile = FileMgr.getVirtualFile(Remap->first,
                                                       ToFile->getSize(), 0);
    if (!FromFile) {
      Diags.Report(diag::err_fe_remap_missing_from_file)
      << Remap->first;
      continue;
    }
    
    // Override the contents of the "from" file with the contents of
    // the "to" file.
    SourceMgr.overrideFileContents(FromFile, ToFile);
  }

//  SourceMgr.setOverridenFilesKeepOriginalName(
//                                        InitOpts.RemappedFilesKeepOriginalName);
}

/// InitializePreprocessor - Initialize the preprocessor getting it and the
/// environment ready to process a single file. This returns true on error.
///
void mlang::InitializePreprocessor(Preprocessor &PP,
                                   const PreprocessorOptions &InitOpts,
                                   const ImportSearchOptions &HSOpts,
                                   const FrontendOptions &FEOpts) {

	InitializeFileRemapping(PP.getDiagnostics(), PP.getSourceManager(),
                          PP.getFileManager(), InitOpts);

  // Initialize the import search object.
//  ApplyImportSearchOptions(PP.getImportSearchInfo(), HSOpts,
//                           PP.getLangOptions(),
//                           PP.getTargetInfo().getTriple());
}
