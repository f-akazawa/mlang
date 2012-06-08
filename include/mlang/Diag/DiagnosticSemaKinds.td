//==--- DiagnosticSemaKinds.td - libsema diagnostics ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Semantic Analysis
//===----------------------------------------------------------------------===//

let Component = "Sema" in {
def err_unavailable : Error<"%0 is unavailable">;
def err_unavailable_message : Error<"%0 is unavailable: %1">;

def err_typecheck_call_too_many_args_at_most : Error<
  "too many %select{|||execution configuration }0arguments to "
  "%select{function|block|method|kernel function}0 call, "
  "expected at most %1, have %2">;
def err_typecheck_call_too_many_args_at_most_one : Error<
  "too many %select{|||execution configuration }0arguments to "
  "%select{function|block|method|kernel function}0 call, "
  "expected at most single argument %1, have %2 arguments">;
def err_argument_invalid_range : Error<
  "argument should be a value from %0 to %1">;
def err_constant_integer_arg_type : Error<
  "argument to %0 must be a constant integer">;
def err_builtin_longjmp_invalid_val : Error<
  "argument to __builtin_longjmp must be a constant 1">;
def warn_missing_format_string : Warning<
  "format string missing">, InGroup<Format>;
def warn_format_nonliteral_noargs : Warning<
  "format string is not a string literal (potentially insecure)">,
  InGroup<FormatSecurity>;
def warn_format_nonliteral : Warning<
  "format string is not a string literal">,
  InGroup<FormatNonLiteral>, DefaultIgnore;
def warn_floatingpoint_eq : Warning<
  "comparing floating point with == or != is unsafe">,
  InGroup<DiagGroup<"float-equal">>, DefaultIgnore;
def warn_mixed_sign_comparison : Warning<
  "comparison of integers of different signs: %0 and %1">,
  InGroup<SignCompare>, DefaultIgnore;
def warn_lunsigned_always_true_comparison : Warning<
  "comparison of unsigned%select{| enum}2 expression %0 is always %1">,
  InGroup<TautologicalCompare>;
def warn_runsigned_always_true_comparison : Warning<
  "comparison of %0 unsigned%select{| enum}2 expression is always %1">,
  InGroup<TautologicalCompare>;
def err_typecheck_decl_incomplete_type : Error<
  "variable has incomplete type %0">;

// Declarations
def err_parameter_name_omitted : Error<"parameter name omitted">;

def err_ref_non_value : Error<"%0 does not refer to a value">;

// Semantic analysis of constant literals.
def warn_float_overflow : Warning<
  "magnitude of floating-point constant too large for type %0; maximum is %1">,
  InGroup<LiteralRange>;
def warn_float_underflow : Warning<
  "magnitude of floating-point constant too small for type %0; minimum is %1">,
  InGroup<LiteralRange>;
} // end of sema component.

