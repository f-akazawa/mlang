//===--- ASTConsumers.h - ASTConsumer implementations -----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares AST consumers.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_DRIVER_ASTCONSUMERS_H_
#define MLANG_DRIVER_ASTCONSUMERS_H_

namespace llvm {
  class raw_ostream;
  namespace sys { class Path; }
}

namespace mlang {

class ASTConsumer;
class CodeGenOptions;
class Diagnostic;
class FileManager;
class LangOptions;
class Preprocessor;
class TargetOptions;

// AST pretty-printer: prints out the AST in a format that is close to the
// original C code.  The output is intended to be in a format such that
// clang could re-parse the output back into the same AST, but the
// implementation is still incomplete.
ASTConsumer *CreateASTPrinter(llvm::raw_ostream *OS);

// AST dumper: dumps the raw AST in human-readable form to stderr; this is
// intended for debugging.
ASTConsumer *CreateASTDumper();

// AST XML-dumper: dumps out the AST to stderr in a very detailed XML
// format; this is intended for particularly intense debugging.
ASTConsumer *CreateASTDumperXML(llvm::raw_ostream &OS);

// Graphical AST viewer: for each function definition, creates a graph of
// the AST and displays it with the graph viewer "dotty".  Also outputs
// function declarations to stderr.
ASTConsumer *CreateASTViewer();

// DeclContext printer: prints out the DeclContext tree in human-readable form
// to stderr; this is intended for debugging.
ASTConsumer *CreateDefnContextPrinter();

} // end namespace mlang

#endif /* MLANG_DRIVER_ASTCONSUMERS_H_ */
