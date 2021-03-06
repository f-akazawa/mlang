//==--- DiagnosticFrontendKinds.td - frontend diagnostics -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

let Component = "Frontend" in {
def err_fe_error_opening : Error<"error opening '%0': %1">;
def err_fe_error_reading : Error<"error reading '%0'">;
def err_fe_error_reading_stdin : Error<"error reading stdin">;
def err_fe_error_backend : Error<"error in backend: %0">, DefaultFatal;

// Error generated by the backend.
def err_fe_inline_asm : Error<"%0">, CatInlineAsm;
def note_fe_inline_asm_here : Note<"instantiated into assembly here">;
def err_fe_cannot_link_module : Error<"cannot link module '%0': %1">,
  DefaultFatal;

def err_fe_remap_missing_to_file : Error<
  "could not remap file '%0' to the contents of file '%1'">, DefaultFatal;
def err_fe_remap_missing_from_file : Error<
  "could not remap from missing file '%0'">, DefaultFatal;
def err_fe_unable_to_load_pch : Error<
  "unable to load PCH file">;
def err_fe_unable_to_open_output : Error<
  "unable to open output file '%0': '%1'">;
def err_fe_unable_to_open_logfile : Error<
  "unable to open logfile file '%0': '%1'">;
def err_fe_expected_compiler_job : Error<
  "unable to handle compilation, expected exactly one compiler job in '%0'">;
def err_fe_expected_mlang_command : Error<
  "expected a mlang compiler command">;

def err_verify_missing_start : Error<
  "cannot find start ('{{') of expected %0">;
def err_verify_missing_end : Error<
  "cannot find end ('}}') of expected %0">;
def err_verify_invalid_content : Error<
  "invalid expected %0: %1">;
def err_verify_inconsistent_diags : Error<
  "'%0' diagnostics %select{expected|seen}1 but not %select{seen|expected}1: "
  "%2">;

def warn_unknown_warning_option : Warning<
  "unknown warning option '%0'">,
  InGroup<DiagGroup<"unknown-warning-option"> >;
def warn_unknown_negative_warning_option : Warning<
  "unknown warning option '%0'">,
  InGroup<DiagGroup<"unknown-warning-option"> >;
def warn_unknown_warning_option_suggest : Warning<
  "unknown warning option '%0'; did you mean '%1'?">,
  InGroup<DiagGroup<"unknown-warning-option"> >;
def warn_unknown_negative_warning_option_suggest : Warning<
  "unknown warning option '%0'; did you mean '%1'?">,
  InGroup<DiagGroup<"unknown-warning-option"> >;
def warn_unknown_warning_specifier : Warning<
  "unknown %0 warning specifier: '%1'">,
  InGroup<DiagGroup<"unknown-warning-option"> >;

def err_fe_invalid_plugin_name : Error<
  "unable to find plugin '%0'">;
def err_fe_unable_to_load_plugin : Error<
  "unable to load plugin '%0': '%1'">;

}
