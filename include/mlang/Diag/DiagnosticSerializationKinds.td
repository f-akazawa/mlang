//==--- DiagnosticSerializationKinds.td - serialization diagnostics -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

let Component = "Serialization" in {
def err_fe_unable_to_read_pch_file : Error<
  "unable to read PCH file: '%0'">;
def err_fe_not_a_pch_file : Error<
  "input is not a PCH file: '%0'">;
def err_fe_pch_malformed : Error<
  "malformed or corrupted PCH file: '%0'">, DefaultFatal;
def err_fe_pch_malformed_block : Error<
  "malformed block record in PCH file: '%0'">, DefaultFatal;
def err_fe_pch_error_at_end_block : Error<
  "error at end of module block in PCH file: '%0'">, DefaultFatal;
def err_not_a_pch_file : Error<
  "'%0' does not appear to be a precompiled header file">, DefaultFatal;
}
