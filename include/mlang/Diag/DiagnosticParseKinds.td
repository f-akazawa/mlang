//==--- DiagnosticParseKinds.td - libparse diagnostics --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Parser Diagnostics
//===----------------------------------------------------------------------===//

let Component = "Parse" in {
def err_parse_error : Error<"parsing error">;
// Generic errors.
def err_expected_expression : Error<"expected expression">;
def err_expected_type : Error<"expected a type">;
def err_expected_external_declaration : Error<"expected external declaration">;
def err_extraneous_closing_brace : Error<"extraneous closing brace ('}')">;
def err_expected_ident : Error<"expected identifier">;
def err_expected_ident_lparen : Error<"expected identifier or '('">;
def err_expected_ident_lbrace : Error<"expected identifier or '{'">;
def err_expected_lbrace : Error<"expected '{'">;
def err_expected_lparen : Error<"expected '('">;
def err_expected_lparen_or_lbrace : Error<"expected '('or '{'">;
def err_expected_rparen : Error<"expected ')'">;
def err_expected_lsquare : Error<"expected '['">;
def err_expected_rsquare : Error<"expected ']'">;
def err_expected_rbrace : Error<"expected '}'">;
def err_expected_greater : Error<"expected '>'">;
def err_expected_ggg : Error<"expected '>>>'">;
def err_expected_semi_declaration : Error<
  "expected ';' at end of declaration">;
def err_expected_semi_decl_list : Error<
  "expected ';' at end of declaration list">;
def ext_expected_semi_decl_list : ExtWarn<
  "expected ';' at end of declaration list">;
def err_expected_semi_after : Error<"expected ';' after %0">;
def err_expected_semi_after_stmt : Error<"expected ';' after %0 statement">;
def err_expected_semi_after_expr : Error<"expected ';' after expression">;
def err_extraneous_token_before_semi : Error<"extraneous '%0' before ';'">;
}
