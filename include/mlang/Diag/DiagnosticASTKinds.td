//==--- DiagnosticASTKinds.td - libast diagnostics ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

let Component = "AST" in {
def note_expr_divide_by_zero : Note<"division by zero">;
def err_unsupported_ast_node: Error<"cannot import unsupported AST node %0">;
}
