//==--- DiagnosticLexKinds.td - liblex diagnostics ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Lexer Diagnostics
//===----------------------------------------------------------------------===//

let Component = "Lex", CategoryName = "Lexical or Preprocessor Issue" in {
def null_in_string : Warning<"null character(s) preserved in string literal">,
  InGroup<NullCharacter>;
def null_in_char : Warning<"null character(s) preserved in character literal">,
  InGroup<NullCharacter>;
def null_in_file : Warning<"null character ignored">, InGroup<NullCharacter>;
def warn_nested_block_comment : Warning<"'/*' within block comment">,
    InGroup<Comment>;
def escaped_newline_block_comment_end : Warning<
  "escaped newline between */ characters at block comment end">,
  InGroup<Comment>;
def backslash_newline_space : Warning<
  "backslash and newline separated by space">,
  InGroup<DiagGroup<"backslash-newline-escape">>;

def ext_multi_line_bcpl_comment : Extension<"multi-line // comment">,
  InGroup<Comment>;
def ext_bcpl_comment : Extension<
  "// comments are not allowed in this language">,
  InGroup<Comment>;
def ext_no_newline_eof : Extension<"no newline at end of file">,
  InGroup<DiagGroup<"newline-eof">>;

def warn_unterminated_string : ExtWarn<"missing terminating '\"' character">;
def warn_unterminated_char : ExtWarn<"missing terminating ' character">;
def err_empty_character : Error<"empty character constant">;
def err_unterminated_block_comment : Error<"unterminated /* comment">;
def err_invalid_character_to_charify : Error<
  "invalid argument to convert to character">;
def ext_multichar_character_literal : ExtWarn<
  "multi-character character constant">, InGroup<MultiChar>;
def ext_four_char_character_literal : Extension<
  "multi-character character constant">, InGroup<FourByteMultiChar>;

// Literal
def ext_nonstandard_escape : Extension<
  "use of non-standard escape character '\\%0'">;
def ext_unknown_escape : ExtWarn<"unknown escape sequence '\\%0'">;
def err_hex_escape_no_digits : Error<"\\x used with no following hex digits">;
def err_ucn_escape_no_digits : Error<"\\u used with no following hex digits">;
def err_ucn_escape_invalid : Error<"invalid universal character">;
def err_ucn_escape_incomplete : Error<"incomplete universal character name">;
def err_ucn_escape_basic_scs : Error<
  "character '%0' cannot be specified by a universal character name">;
def err_ucn_control_character : Error<
  "universal character name refers to a control character">;
def ext_binary_literal : Extension<
  "binary integer literals are an extension">;
def err_pascal_string_too_long : Error<"Pascal string is too long">;
def warn_octal_escape_too_large : ExtWarn<"octal escape sequence out of range">;
def warn_hex_escape_too_large : ExtWarn<"hex escape sequence out of range">;
def ext_string_too_long : Extension<"string literal of length %0 exceeds "
  "maximum length %1 that %select{C90|ISO C99|C++}2 compilers are required to "
  "support">, InGroup<OverlengthStrings>;
def err_character_too_large : Error<
  "character too large for enclosing character literal type">;
def err_invalid_decimal_digit : Error<"invalid digit '%0' in decimal constant">;
def err_invalid_binary_digit : Error<"invalid digit '%0' in binary constant">;
def err_invalid_octal_digit : Error<"invalid digit '%0' in octal constant">;
def err_invalid_suffix_integer_constant : Error<
  "invalid suffix '%0' on integer constant">;
def err_invalid_suffix_float_constant : Error<
  "invalid suffix '%0' on floating constant">;
def warn_extraneous_char_constant : Warning<
  "extraneous characters in character constant ignored">;
def warn_char_constant_too_large : Warning<
  "character constant too long for its type">;
def err_exponent_has_no_digits : Error<"exponent has no digits">;
def ext_imaginary_constant : Extension<"imaginary constants are an extension">;
def err_hexconstant_requires_exponent : Error<
  "hexadecimal floating constants require an exponent">;
def err_hexconstant_requires_digits : Error<
  "hexadecimal floating constants require a significand">;
def err_pp_invalid_directive : Error<"invalid preprocessing directive">;
def err_pp_file_not_found : Error<"'%0' file not found">, DefaultFatal;
def err_pp_error_opening_file : Error<
  "error opening file '%0': %1">, DefaultFatal;
def err_pp_empty_filename : Error<"empty filename">;
def err_pp_include_too_deep : Error<"#include nested too deeply">;
def err_pp_expects_filename : Error<"expected \"FILENAME\" or <FILENAME>">;
}
