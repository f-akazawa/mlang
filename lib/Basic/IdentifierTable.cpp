//===--- IdentifierTable.cpp - Symbol Table Support -----------------------===//
//
// Copyright (C) 2010 Yabin Hu @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements IdentifierTable.
//
//===----------------------------------------------------------------------===//
/*

Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
              2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009 John W. Eaton
Copyright (C) 2009 VZLU Prague, a.s.
  
This file is part of Octave.

Octave is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

Octave is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Octave; see the file COPYING.  If not, see
<http://www.gnu.org/licenses/>.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mlang/Basic/IdentifierTable.h"
#include "mlang/Basic/LangOptions.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>

#if 0
#include "oct-env.h"
#include "oct-time.h"
#include "file-ops.h"
#include "file-stat.h"
#include "defun.h"
#include "dirfns.h"
#include "input.h"
#include "load-path.h"
#include "symtab.h"
#include "ov-fcn.h"
#include "ov-usr-fcn.h"
#include "pager.h"
#include "parse.h"
#include "pt-arg-list.h"
#include "unwind-prot.h"
#include "utils.h"
#include "debug.h"
#endif

using namespace mlang;

IdentifierIterator::~IdentifierIterator() { }

IdentifierInfoLookup::~IdentifierInfoLookup() {}

namespace {
  /// \brief A simple identifier lookup iterator that represents an
  /// empty sequence of identifiers.
  class EmptyLookupIterator : public IdentifierIterator
  {
  public:
    virtual llvm::StringRef Next() { return llvm::StringRef(); }
  };
}

IdentifierIterator *IdentifierInfoLookup::getIdentifiers() const {
  return new EmptyLookupIterator();
}

ExternalIdentifierLookup::~ExternalIdentifierLookup() {}

//===----------------------------------------------------------------------===//
// IdentifierTable Implementation
//===----------------------------------------------------------------------===//
IdentifierTable::IdentifierTable(const LangOptions &LangOpts,
                                 IdentifierInfoLookup* externalLookup)
  : HashTable(8192), // Start with space for 8K identifiers.
    ExternalLookup(externalLookup) {

  // Populate the identifier table with info about keywords for the current
  // language.
  AddKeywords(LangOpts);
}

// Constants for TokenKinds.def
namespace {
  enum {
    KEYALL = 1,
    KEYMATLAB = 2,
    KEYOCTAVE = 4,
    KEYGMAT = 8
  };
}

/// AddKeyword - This method is used to associate a token ID with specific
/// identifiers because they are language keywords.  This causes the lexer to
/// automatically map matching identifiers to specialized token codes.
///
/// The C90/C99/CPP/CPP0x flags are set to 2 if the token should be
/// enabled in the specified langauge, set to 1 if it is an extension
/// in the specified language, and set to 0 if disabled in the
/// specified language.
static void AddKeyword(llvm::StringRef Keyword,
                       tok::TokenKind TokenCode, unsigned Flags,
                       const LangOptions &LangOpts, IdentifierTable &Table) {
  unsigned AddResult = 0;
  if (Flags & KEYALL) AddResult = 2;
  else if (LangOpts.MATLABKeywords && (Flags & KEYMATLAB)) AddResult = 2;
  else if (LangOpts.OCTAVEKeywords && (Flags & KEYOCTAVE)) AddResult = 2;

  // Don't add this keyword if disabled in this language.
  if (AddResult == 0) return;

  IdentifierInfo &Info = Table.get(Keyword, TokenCode);
  //Info.setIsExtensionToken(AddResult == 1);
}

/// AddKeywords - Add all keywords to the symbol table.
///
void IdentifierTable::AddKeywords(const LangOptions &LangOpts) {
  // Add keywords and tokens for the current language.
#define KEYWORD(NAME, FLAGS) \
  AddKeyword(llvm::StringRef(#NAME), tok::kw_ ## NAME,  \
             FLAGS, LangOpts, *this);
#define ALIAS(NAME, TOK, FLAGS) \
  AddKeyword(llvm::StringRef(NAME), tok::kw_ ## TOK,  \
             FLAGS, LangOpts, *this);
#include "mlang/Basic/TokenKinds.def"
}

//===----------------------------------------------------------------------===//
// Stats Implementation
//===----------------------------------------------------------------------===//

/// PrintStats - Print statistics about how well the identifier table is doing
/// at hashing identifiers.
void IdentifierTable::PrintStats() const {
  unsigned NumBuckets = HashTable.getNumBuckets();
  unsigned NumIdentifiers = HashTable.getNumItems();
  unsigned NumEmptyBuckets = NumBuckets-NumIdentifiers;
  unsigned AverageIdentifierSize = 0;
  unsigned MaxIdentifierLength = 0;

  // TODO: Figure out maximum times an identifier had to probe for -stats.
  for (llvm::StringMap<IdentifierInfo*, llvm::BumpPtrAllocator>::const_iterator
       I = HashTable.begin(), E = HashTable.end(); I != E; ++I) {
    unsigned IdLen = I->getKeyLength();
    AverageIdentifierSize += IdLen;
    if (MaxIdentifierLength < IdLen)
      MaxIdentifierLength = IdLen;
  }

  fprintf(stderr, "\n*** Identifier Table Stats:\n");
  fprintf(stderr, "# Identifiers:   %d\n", NumIdentifiers);
  fprintf(stderr, "# Empty Buckets: %d\n", NumEmptyBuckets);
  fprintf(stderr, "Hash density (#identifiers per bucket): %f\n",
          NumIdentifiers/(double)NumBuckets);
  fprintf(stderr, "Ave identifier length: %f\n",
          (AverageIdentifierSize/(double)NumIdentifiers));
  fprintf(stderr, "Max identifier length: %d\n", MaxIdentifierLength);

  // Compute statistics about the memory allocated for identifiers.
  HashTable.getAllocator().PrintStats();
}

#if 0
IdentifierTable *IdentifierTable::instance = 0;

IdentifierTable::scope_id_cache *IdentifierTable::scope_id_cache::instance = 0;

std::map<IdentifierTable::scope_id, IdentifierTable*> IdentifierTable::all_instances;

std::map<std::string, octave_value> IdentifierTable::global_table;

std::map<std::string, IdentifierTable::FunctionInfo> IdentifierTable::fcn_table;

std::map<std::string, std::set<std::string> > IdentifierTable::class_precedence_table;

std::map<std::string, std::list<std::string> > IdentifierTable::parent_map;

const IdentifierTable::scope_id IdentifierTable::xglobal_scope = 0;
const IdentifierTable::scope_id IdentifierTable::xtop_scope = 1;

IdentifierTable::scope_id IdentifierTable::xcurrent_scope = 1;

IdentifierTable::context_id IdentifierTable::xcurrent_context = 0;

// Should Octave always check to see if function files have changed
// since they were last compiled?
static int Vignore_function_time_stamp = 1;

void
IdentifierTable::IdentifierInfo::IdentifierInfoRep::dump
  (std::ostream& os, const std::string& prefix) const
{
  octave_value val = varval (xcurrent_context);

  os << prefix << name;

  if (val.is_defined ())
    {
      os << " ["
         << (is_local () ? "l" : "")
         << (is_automatic () ? "a" : "")
         << (is_formal () ? "f" : "")
         << (is_hidden () ? "h" : "")
         << (is_inherited () ? "i" : "")
         << (is_global () ? "g" : "")
         << (is_persistent () ? "p" : "")
         << "] ";
      val.dump (os);
    }

  os << "\n";
}

octave_value
IdentifierTable::IdentifierInfo::find (const octave_value_list& args) const
{
  octave_value retval;

  if (is_global ())
    retval = IdentifierTable::global_varref (name ());
  else
    {
      retval = varval ();

      if (retval.is_undefined ())
        {
          // Use cached FunctionInfo pointer if possible.
          if (rep->finfo)
            retval = rep->finfo->find (args);
          else
            { 
              retval = IdentifierTable::find_function (name (), args);

              if (retval.is_defined ())
                rep->finfo = get_fcn_info (name ());
            }
        }
    }

  return retval;
}

// Check the load path to see if file that defined this is still
// visible.  If the file is no longer visible, then erase the
// definition and move on.  If the file is visible, then we also
// need to check to see whether the file has changed since the the
// function was loaded/parsed.  However, this check should only
// happen once per prompt (for files found from relative path
// elements, we also check if the working directory has changed
// since the last time the function was loaded/parsed).
//
// FIXME -- perhaps this should be done for all loaded functions when
// the prompt is printed or the directory has changed, and then we
// would not check for it when finding symbol definitions.

static inline bool
load_out_of_date_fcn (const std::string& ff, const std::string& dir_name,
                      octave_value& function,
                      const std::string& dispatch_type = std::string ())
{
  bool retval = false;

  octave_function *fcn = load_fcn_from_file (ff, dir_name, dispatch_type);

  if (fcn)
    {
      retval = true;

      function = octave_value (fcn);
    }
  else
    function = octave_value ();

  return retval;
}

bool
out_of_date_check (octave_value& function,
                   const std::string& dispatch_type,
                   bool check_relative)
{
  bool retval = false;

  octave_function *fcn = function.function_value (true);

  if (fcn)
    {
      // FIXME -- we need to handle nested functions properly here.

      if (! fcn->is_nested_function ())
        {
          std::string ff = fcn->fcn_file_name ();

          if (! ff.empty ())
            {
              octave_time tc = fcn->time_checked ();

              bool relative = check_relative && fcn->is_relative ();

              if (tc < Vlast_prompt_time
                  || (relative && tc < Vlast_chdir_time))
                {
                  bool clear_breakpoints = false;
                  std::string nm = fcn->name ();

                  bool is_same_file = false;

                  std::string file;
                  std::string dir_name;

                  if (check_relative)
                    {
                      int nm_len = nm.length ();

                      if (octave_env::absolute_pathname (nm)
                          && ((nm_len > 4 && (nm.substr (nm_len-4) == ".oct"
                                              || nm.substr (nm_len-4) == ".mex"))
                              || (nm_len > 2 && nm.substr (nm_len-2) == ".m")))
                        file = nm;
                      else
                        {
                          // We don't want to make this an absolute name,
                          // because load_fcn_file looks at the name to
                          // decide whether it came from a relative lookup.

                          if (! dispatch_type.empty ())
                            file = load_path::find_method (dispatch_type, nm,
                                                           dir_name);

                          // Maybe it's an autoload?
                          if (file.empty ())
                            file = lookup_autoload (nm);

                          if (file.empty ())
                            file = load_path::find_fcn (nm, dir_name);
                        }

                      if (! file.empty ())
                        is_same_file = same_file (file, ff);
                    }
                  else
                    {
                      is_same_file = true;
                      file = ff;
                    }

                  if (file.empty ())
                    {
                      // Can't see this function from current
                      // directory, so we should clear it.

                      function = octave_value ();

                      clear_breakpoints = true;
                    }
                  else if (is_same_file)
                    {
                      // Same file.  If it is out of date, then reload it.

                      octave_time ottp = fcn->time_parsed ();
                      time_t tp = ottp.unix_time ();

                      fcn->mark_fcn_file_up_to_date (octave_time ());

                      if (! (Vignore_function_time_stamp == 2
                             || (Vignore_function_time_stamp
                                 && fcn->is_system_fcn_file ())))
                        {
                          file_stat fs (ff);

                          if (fs)
                            {
                              if (fs.is_newer (tp))
                                {
                                  retval = load_out_of_date_fcn (ff, dir_name,
                                                                 function,
                                                                 dispatch_type);

                                  clear_breakpoints = true;
                                }
                            }
                          else
                            {
                              function = octave_value ();

                              clear_breakpoints = true;
                            }
                        }
                    }
                  else
                    {
                      // Not the same file, so load the new file in
                      // place of the old.

                      retval = load_out_of_date_fcn (file, dir_name, function,
                                                     dispatch_type);

                      clear_breakpoints = true;
                    }

                  // If the function has been replaced then clear any 
                  // breakpoints associated with it
                  if (clear_breakpoints)
                    bp_table::remove_all_breakpoints_in_file (nm, true);
                }
            }
        }
    }

  return retval;
}

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::load_private_function
  (const std::string& dir_name)
{
  octave_value retval;

  std::string file_name = load_path::find_private_fcn (dir_name, name);

  if (! file_name.empty ())
    {
      octave_function *fcn = load_fcn_from_file (file_name, dir_name);

      if (fcn)
        {
          std::string class_name;

          size_t pos = dir_name.find_last_of (file_ops::dir_sep_chars ());

          if (pos != std::string::npos)
            {
              std::string tmp = dir_name.substr (pos+1);

              if (tmp[0] == '@')
                class_name = tmp.substr (1);
            }

          fcn->mark_as_private_function (class_name);

          retval = octave_value (fcn);

          private_functions[dir_name] = retval;
        }
    }

  return retval;
}

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::load_class_constructor (void)
{
  octave_value retval;

  std::string dir_name;

  std::string file_name = load_path::find_method (name, name, dir_name);

  if (! file_name.empty ())
    {
      octave_function *fcn = load_fcn_from_file (file_name, dir_name, name);

      if (fcn)
        {
          retval = octave_value (fcn);

          class_constructors[name] = retval;
        }
    }

  return retval;
}

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::load_class_method
  (const std::string& dispatch_type)
{
  octave_value retval;

  if (name == dispatch_type)
    retval = load_class_constructor ();
  else
    {
      std::string dir_name;

      std::string file_name = load_path::find_method (dispatch_type, name,
                                                      dir_name);

      if (! file_name.empty ())
        {
          octave_function *fcn = load_fcn_from_file (file_name, dir_name,
                                                     dispatch_type);

          if (fcn)
            {
              retval = octave_value (fcn);

              class_methods[dispatch_type] = retval;
            }
        }

      if (retval.is_undefined ())
        {
          // Search parent classes

          const_parent_map_iterator r = parent_map.find (dispatch_type);

          if (r != parent_map.end ())
            {
              const std::list<std::string>& plist = r->second;
              std::list<std::string>::const_iterator it = plist.begin ();

              while (it != plist.end ())
                {
                  retval = find_method (*it);

                  if (retval.is_defined ()) 
                    {
                      class_methods[dispatch_type] = retval;
                      break;
                    }

                  it++;
                }
            }
        }
    }

  return retval;
}

void
IdentifierTable::FunctionInfo::fcn_info_rep::print_dispatch (std::ostream& os) const
{
  if (dispatch_map.empty ())
    os << "dispatch: " << name << " is not overloaded" << std::endl;
  else
    {
      os << "Overloaded function " << name << ":\n\n";

      for (dispatch_map_const_iterator p = dispatch_map.begin ();
           p != dispatch_map.end (); p++)
        os << "  " << name << " (" << p->first << ", ...) -> " 
           << p->second << " (" << p->first << ", ...)\n";

      os << std::endl;
    }
}

std::string
IdentifierTable::FunctionInfo::fcn_info_rep::help_for_dispatch (void) const
{
  std::string retval;

  if (! dispatch_map.empty ())
    {
      retval = "Overloaded function:\n\n";

      for (dispatch_map_const_iterator p = dispatch_map.begin ();
           p != dispatch_map.end (); p++)
        retval += "  " + p->second + " (" + p->first + ", ...)\n\n";
    }

  return retval;
}

// :-) JWE, can you parse this? Returns a 2D array with second dimension equal
// to btyp_num_types (static constant). Only the leftmost dimension can be
// variable in C/C++. Typedefs are boring.

static builtin_type_t (*build_sup_table (void))[btyp_num_types]
{
  static builtin_type_t sup_table[btyp_num_types][btyp_num_types];
  for (int i = 0; i < btyp_num_types; i++)
    for (int j = 0; j < btyp_num_types; j++)
      {
        builtin_type_t ityp = static_cast<builtin_type_t> (i);
        builtin_type_t jtyp = static_cast<builtin_type_t> (j);
        // FIXME: Is this really right?
        bool use_j = 
          (jtyp == btyp_func_handle || ityp == btyp_bool
           || (btyp_isarray (ityp) 
               && (! btyp_isarray (jtyp) 
                   || (btyp_isinteger (jtyp) && ! btyp_isinteger (ityp))
                   || ((ityp == btyp_double || ityp == btyp_complex || ityp == btyp_char)
                       && (jtyp == btyp_float || jtyp == btyp_float_complex)))));

        sup_table[i][j] = use_j ? jtyp : ityp;
      }

  return sup_table;
}

std::string
get_dispatch_type (const octave_value_list& args, 
                   builtin_type_t& builtin_type)
{
  static builtin_type_t (*sup_table)[btyp_num_types] = build_sup_table ();
  std::string dispatch_type;

  int n = args.length ();

  if (n > 0)
    {
      int i = 0;
      builtin_type = args(0).builtin_type ();
      if (builtin_type != btyp_unknown)
        {
          for (i = 1; i < n; i++)
            {
              builtin_type_t bti = args(i).builtin_type ();
              if (bti != btyp_unknown)
                builtin_type = sup_table[builtin_type][bti];
              else
                {
                  builtin_type = btyp_unknown;
                  break;
                }
            }
        }

      if (builtin_type == btyp_unknown)
        {
          // There's a non-builtin class in the argument list.
          dispatch_type = args(i).class_name ();

          for (int j = i+1; j < n; j++)
            {
              octave_value arg = args(j);

              if (arg.builtin_type () == btyp_unknown)
                {
                  std::string cname = arg.class_name ();

                  // Only switch to type of ARG if it is marked superior
                  // to the current DISPATCH_TYPE.
                  if (! IdentifierTable::is_superiorto (dispatch_type, cname)
                      && IdentifierTable::is_superiorto (cname, dispatch_type))
                    dispatch_type = cname;
                }
            }
        }
      else
        dispatch_type = btyp_class_name[builtin_type];
    }
  else
    builtin_type = btyp_unknown;

  return dispatch_type;
}

std::string
get_dispatch_type (const octave_value_list& args)
{
  builtin_type_t builtin_type;
  return get_dispatch_type (args, builtin_type);
}

// Find the definition of NAME according to the following precedence
// list:
//
//   variable
//   subfunction
//   private function
//   class constructor
//   class method
//   legacy dispatch
//   command-line function
//   autoload function
//   function on the path
//   built-in function

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::find (const octave_value_list& args,
                                            bool local_funcs)
{
  octave_value retval = xfind (args, local_funcs);

  if (! retval.is_defined ())
    {
      // It is possible that the user created a file on the fly since
      // the last prompt or chdir, so try updating the load path and
      // searching again.

      load_path::update ();

      retval = xfind (args, local_funcs);
    }

  return retval;
}

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::xfind (const octave_value_list& args,
                                             bool local_funcs)
{
  if (local_funcs)
    {
      // Subfunction.  I think it only makes sense to check for
      // subfunctions if we are currently executing a function defined
      // from a .m file.

      scope_val_iterator r = subfunctions.find (xcurrent_scope);

      octave_user_function *curr_fcn = IdentifierTable::get_curr_fcn ();

      if (r != subfunctions.end ())
        {
          // FIXME -- out-of-date check here.

          return r->second;
        }
      else
        {
          if (curr_fcn)
            {
              scope_id pscope = curr_fcn->parent_fcn_scope ();

              if (pscope > 0)
                {
                  r = subfunctions.find (pscope);

                  if (r != subfunctions.end ())
                    {
                      // FIXME -- out-of-date check here.

                      return r->second;
                    }
                }
            }
        }

      // Private function.

      if (curr_fcn)
        {
          std::string dir_name = curr_fcn->dir_name ();

          if (! dir_name.empty ())
            {
              str_val_iterator q = private_functions.find (dir_name);

              if (q == private_functions.end ())
                {
                  octave_value val = load_private_function (dir_name);

                  if (val.is_defined ())
                    return val;
                }
              else
                {
                  octave_value& fval = q->second;

                  if (fval.is_defined ())
                    out_of_date_check (fval);

                  if (fval.is_defined ())
                    return fval;
                  else
                    {
                      octave_value val = load_private_function (dir_name);

                      if (val.is_defined ())
                        return val;
                    }
                }
            }
        }
    }

  // Class constructors.  The class name and function name are the same.

  str_val_iterator q = class_constructors.find (name);

  if (q == class_constructors.end ())
    {
      octave_value val = load_class_constructor ();

      if (val.is_defined ())
        return val;
    }
  else
    {
      octave_value& fval = q->second;

      if (fval.is_defined ())
        out_of_date_check (fval, name);

      if (fval.is_defined ())
        return fval;
      else
        {
          octave_value val = load_class_constructor ();

          if (val.is_defined ())
            return val;
        }
    }

  // Class methods.

  if (! args.empty ())
    {
      std::string dispatch_type = get_dispatch_type (args);

      octave_value fcn = find_method (dispatch_type);

      if (fcn.is_defined ())
        return fcn;
    }

  // Legacy dispatch.  

  if (! args.empty () && ! dispatch_map.empty ())
    {
      std::string dispatch_type = args(0).type_name ();

      std::string fname;

      dispatch_map_iterator p = dispatch_map.find (dispatch_type);

      if (p == dispatch_map.end ())
        p = dispatch_map.find ("any");

      if (p != dispatch_map.end ())
        {
          fname = p->second;

          octave_value fcn
            = IdentifierTable::find_function (fname, args);

          if (fcn.is_defined ())
            return fcn;
        }
    }

  // Command-line function.

  if (cmdline_function.is_defined ())
    return cmdline_function;

  // Autoload?

  octave_value fcn = find_autoload ();

  if (fcn.is_defined ())
    return fcn;

  // Function on the path.

  fcn = find_user_function ();

  if (fcn.is_defined ())
    return fcn;

  // Built-in function (might be undefined).

  return built_in_function;
}

// Find the definition of NAME according to the following precedence
// list:
//
//   built-in function
//   function on the path
//   autoload function
//   command-line function
//   private function
//   subfunction

// This function is used to implement the "builtin" function, which
// searches for "built-in" functions.  In Matlab, "builtin" only
// returns functions that are actually built-in to the interpreter.
// But since the list of built-in functions is different in Octave and
// Matlab, we also search up the precedence list until we find
// something that matches.  Note that we are only searching by name,
// so class methods, constructors, and legacy dispatch functions are
// skipped.

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::builtin_find (void)
{
  octave_value retval = x_builtin_find ();

  if (! retval.is_defined ())
    {
      // It is possible that the user created a file on the fly since
      // the last prompt or chdir, so try updating the load path and
      // searching again.

      load_path::update ();

      retval = x_builtin_find ();
    }

  return retval;
}

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::x_builtin_find (void)
{
  // Built-in function.
  if (built_in_function.is_defined ())
    return built_in_function;

  // Function on the path.

  octave_value fcn = find_user_function ();

  if (fcn.is_defined ())
    return fcn;

  // Autoload?

  fcn = find_autoload ();

  if (fcn.is_defined ())
    return fcn;

  // Command-line function.

  if (cmdline_function.is_defined ())
    return cmdline_function;

  // Private function.

  octave_user_function *curr_fcn = IdentifierTable::get_curr_fcn ();

  if (curr_fcn)
    {
      std::string dir_name = curr_fcn->dir_name ();

      if (! dir_name.empty ())
        {
          str_val_iterator q = private_functions.find (dir_name);

          if (q == private_functions.end ())
            {
              octave_value val = load_private_function (dir_name);

              if (val.is_defined ())
                return val;
            }
          else
            {
              octave_value& fval = q->second;

              if (fval.is_defined ())
                out_of_date_check (fval);

              if (fval.is_defined ())
                return fval;
              else
                {
                  octave_value val = load_private_function (dir_name);

                  if (val.is_defined ())
                    return val;
                }
            }
        }
    }

  // Subfunction.  I think it only makes sense to check for
  // subfunctions if we are currently executing a function defined
  // from a .m file.

  scope_val_iterator r = subfunctions.find (xcurrent_scope);

  if (r != subfunctions.end ())
    {
      // FIXME -- out-of-date check here.

      return r->second;
    }
  else if (curr_fcn)
    {
      scope_id pscope = curr_fcn->parent_fcn_scope ();

      if (pscope > 0)
        {
          r = subfunctions.find (pscope);

          if (r != subfunctions.end ())
            {
              // FIXME -- out-of-date check here.

              return r->second;
            }
        }
    }

  return octave_value ();
}

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::find_method (const std::string& dispatch_type)
{
  octave_value retval;

  str_val_iterator q = class_methods.find (dispatch_type);

  if (q == class_methods.end ())
    {
      octave_value val = load_class_method (dispatch_type);

      if (val.is_defined ())
        return val;
    }
  else
    {
      octave_value& fval = q->second;

      if (fval.is_defined ())
        out_of_date_check (fval, dispatch_type);

      if (fval.is_defined ())
        return fval;
      else
        {
          octave_value val = load_class_method (dispatch_type);

          if (val.is_defined ())
            return val;
        }
    }

  return retval;
}

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::find_autoload (void)
{
  octave_value retval;

  // Autoloaded function.

  if (autoload_function.is_defined ())
    out_of_date_check (autoload_function);

  if (! autoload_function.is_defined ())
    {
      std::string file_name = lookup_autoload (name);

      if (! file_name.empty ())
        {
          size_t pos = file_name.find_last_of (file_ops::dir_sep_chars ());

          std::string dir_name = file_name.substr (0, pos);

          octave_function *fcn = load_fcn_from_file (file_name, dir_name,
                                                     "", name, true);

          if (fcn)
            autoload_function = octave_value (fcn);
        }
    }

  return autoload_function;
}

octave_value
IdentifierTable::FunctionInfo::fcn_info_rep::find_user_function (void)
{
  // Function on the path.

  if (function_on_path.is_defined ())
    out_of_date_check (function_on_path);

  if (! function_on_path.is_defined ())
    {
      std::string dir_name;

      std::string file_name = load_path::find_fcn (name, dir_name);

      if (! file_name.empty ())
        {
          octave_function *fcn = load_fcn_from_file (file_name, dir_name);

          if (fcn)
            function_on_path = octave_value (fcn);
        }
    }

  return function_on_path;
}

// Insert INF_CLASS in the set of class names that are considered
// inferior to SUP_CLASS.  Return FALSE if INF_CLASS is currently
// marked as superior to  SUP_CLASS.

bool
IdentifierTable::set_class_relationship (const std::string& sup_class,
                                      const std::string& inf_class)
{
  class_precedence_table_const_iterator p
    = class_precedence_table.find (inf_class);

  if (p != class_precedence_table.end ())
    {
      const std::set<std::string>& inferior_classes = p->second;

      std::set<std::string>::const_iterator q
        = inferior_classes.find (sup_class);

      if (q != inferior_classes.end ())
        return false;
    }

  class_precedence_table[sup_class].insert (inf_class);

  return true;
}

// Has class A been marked as superior to class B?  Also returns
// TRUE if B has been marked as inferior to A, since we only keep
// one table, and convert inferiort information to a superiorto
// relationship.  Two calls are required to determine whether there
// is no relationship between two classes:
//
//  if (IdentifierTable::is_superiorto (a, b))
//    // A is superior to B, or B has been marked inferior to A.
//  else if (IdentifierTable::is_superiorto (b, a))
//    // B is superior to A, or A has been marked inferior to B.
//  else
//    // No relation.

bool
IdentifierTable::is_superiorto (const std::string& a, const std::string& b)
{
  bool retval = false;

  class_precedence_table_const_iterator p = class_precedence_table.find (a);

  if (p != class_precedence_table.end ())
    {
      const std::set<std::string>& inferior_classes = p->second;
      std::set<std::string>::const_iterator q = inferior_classes.find (b);

      if (q != inferior_classes.end ())
        retval = true;
    }

  return retval;
}

static std::string
fcn_file_name (const octave_value& fcn)
{
  const octave_function *f = fcn.function_value ();

  return f ? f->fcn_file_name () : std::string ();
}

void
IdentifierTable::FunctionInfo::fcn_info_rep::dump
  (std::ostream& os, const std::string& prefix) const
{
  os << prefix << name
     << " ["
     << (cmdline_function.is_defined () ? "c" : "")
     << (built_in_function.is_defined () ? "b" : "")
     << "]\n";

  std::string tprefix = prefix + "  ";

  if (autoload_function.is_defined ())
    os << tprefix << "autoload: "
       << fcn_file_name (autoload_function) << "\n";

  if (function_on_path.is_defined ())
    os << tprefix << "function from path: "
       << fcn_file_name (function_on_path) << "\n";

  if (! subfunctions.empty ())
    {
      for (scope_val_const_iterator p = subfunctions.begin ();
           p != subfunctions.end (); p++)
        os << tprefix << "subfunction: " << fcn_file_name (p->second)
           << " [" << p->first << "]\n";
    }

  if (! private_functions.empty ())
    {
      for (str_val_const_iterator p = private_functions.begin ();
           p != private_functions.end (); p++)
        os << tprefix << "private: " << fcn_file_name (p->second)
           << " [" << p->first << "]\n";
    }

  if (! class_constructors.empty ())
    {
      for (str_val_const_iterator p = class_constructors.begin ();
           p != class_constructors.end (); p++)
        os << tprefix << "constructor: " << fcn_file_name (p->second)
           << " [" << p->first << "]\n";
    }

  if (! class_methods.empty ())
    {
      for (str_val_const_iterator p = class_methods.begin ();
           p != class_methods.end (); p++)
        os << tprefix << "method: " << fcn_file_name (p->second)
           << " [" << p->first << "]\n";
    }

  if (! dispatch_map.empty ())
    {
      for (dispatch_map_const_iterator p = dispatch_map.begin ();
           p != dispatch_map.end (); p++)
        os << tprefix << "dispatch: " << fcn_file_name (p->second)
           << " [" << p->first << "]\n";
    }
}

octave_value
IdentifierTable::find (const std::string& name,
                    const octave_value_list& args, 
                    bool skip_variables,
                    bool local_funcs)
{
  IdentifierTable *inst = get_instance (xcurrent_scope);

  return inst
    ? inst->do_find (name, args, skip_variables, local_funcs)
    : octave_value ();
}

octave_value
IdentifierTable::builtin_find (const std::string& name)
{
  IdentifierTable *inst = get_instance (xcurrent_scope);

  return inst ? inst->do_builtin_find (name) : octave_value ();
}

octave_value
IdentifierTable::find_function (const std::string& name,
                             const octave_value_list& args,
                             bool local_funcs)
{
  octave_value retval;

  if (! name.empty () && name[0] == '@')
    {
      // Look for a class specific function.
      std::string dispatch_type = 
        name.substr (1, name.find_first_of (file_ops::dir_sep_str ()) - 1);

      std::string method = 
        name.substr (name.find_last_of (file_ops::dir_sep_str ()) + 1, 
                     std::string::npos);

      retval = find_method (method, dispatch_type);
    }
  else
    {
      size_t pos = name.find_first_of (Vfilemarker);

      if (pos == std::string::npos)
        retval = find (name, args, true, local_funcs);
      else
        {
          std::string fcn_scope = name.substr (0, pos);
          scope_id stored_scope = xcurrent_scope;
          xcurrent_scope = xtop_scope;
          octave_value parent = find_function (name.substr(0, pos),
                                               octave_value_list (), false);

          if (parent.is_defined ())
            {
              octave_function *parent_fcn = parent.function_value ();

              if (parent_fcn)
                {
                  xcurrent_scope = parent_fcn->scope ();

                  if (xcurrent_scope > 1)
                    retval = find_function (name.substr (pos + 1), args);
                }
            }

          xcurrent_scope = stored_scope;
        }
    }

  return retval;
}

void
IdentifierTable::dump (std::ostream& os, scope_id scope)
{
  if (scope == xglobal_scope)
    dump_global (os);
  else
    {
      IdentifierTable *inst = get_instance (scope, false);

      if (inst)
        {
          os << "*** dumping symbol table scope " << scope
             << " (" << inst->table_name << ")\n\n";

          std::map<std::string, octave_value> sfuns
            = IdentifierTable::subfunctions_defined_in_scope (scope);

          if (! sfuns.empty ())
            {
              os << "  subfunctions defined in this scope:\n";

              for (std::map<std::string, octave_value>::const_iterator p = sfuns.begin ();
                   p != sfuns.end (); p++)
                os << "    " << p->first << "\n";

              os << "\n";
            }

          inst->do_dump (os);
        }
    }
}

void
IdentifierTable::dump_global (std::ostream& os)
{
  if (! global_table.empty ())
    {
      os << "*** dumping global symbol table\n\n";

      for (global_table_const_iterator p = global_table.begin ();
           p != global_table.end (); p++)
        {
          std::string nm = p->first;
          octave_value val = p->second;

          os << "  " << nm << " ";
          val.dump (os);
          os << "\n";
        }
    }
}

void
IdentifierTable::dump_functions (std::ostream& os)
{
  if (! fcn_table.empty ())
    {
      os << "*** dumping globally visible functions from symbol table\n"
         << "    (c=commandline, b=built-in)\n\n";

      for (fcn_table_const_iterator p = fcn_table.begin ();
           p != fcn_table.end (); p++)
        p->second.dump (os, "  ");

      os << "\n";
    }
}

void
IdentifierTable::stash_dir_name_for_subfunctions (scope_id scope,
                                               const std::string& dir_name)
{
  // FIXME -- is this the best way to do this?  Maybe it would be
  // better if we had a map from scope to list of subfunctions
  // stored with the function.  Do we?

  for (fcn_table_const_iterator p = fcn_table.begin ();
       p != fcn_table.end (); p++)
    {
      std::pair<std::string, octave_value> tmp
        = p->second.subfunction_defined_in_scope (scope);

      std::string nm = tmp.first;

      if (! nm.empty ())
        {
          octave_value& fcn = tmp.second;

          octave_user_function *f = fcn.user_function_value ();

          if (f)
            f->stash_dir_name (dir_name);
        }
    }
}

octave_value
IdentifierTable::do_find (const std::string& name,
                       const octave_value_list& args,
                       bool skip_variables,
                       bool local_funcs)
{
  octave_value retval;

  // Variable.

  if (! skip_variables)
    {
      table_iterator p = table.find (name);

      if (p != table.end ())
        {
          IdentifierInfo sr = p->second;

          // FIXME -- should we be using something other than varref here?

          if (sr.is_global ())
            return IdentifierTable::global_varref (name);
          else
            {
              octave_value& val = sr.varref ();

              if (val.is_defined ())
                return val;
            }
        }
    }

  fcn_table_iterator p = fcn_table.find (name);

  if (p != fcn_table.end ())
    return p->second.find (args, local_funcs);
  else
    {
      FunctionInfo finfo (name);

      octave_value fcn = finfo.find (args, local_funcs);

      if (fcn.is_defined ())
        fcn_table[name] = finfo;

      return fcn;
    }

  return retval;
}

octave_value
IdentifierTable::do_builtin_find (const std::string& name)
{
  octave_value retval;

  fcn_table_iterator p = fcn_table.find (name);

  if (p != fcn_table.end ())
    return p->second.builtin_find ();
  else
    {
      FunctionInfo finfo (name);

      octave_value fcn = finfo.builtin_find ();

      if (fcn.is_defined ())
        fcn_table[name] = finfo;

      return fcn;
    }

  return retval;
}

void IdentifierTable::do_dump (std::ostream& os) {
  if (! persistent_table.empty ())
    {
      os << "  persistent variables in this scope:\n\n";

      for (persistent_table_const_iterator p = persistent_table.begin ();
           p != persistent_table.end (); p++)
        {
          std::string nm = p->first;
          octave_value val = p->second;

          os << "    " << nm << " ";
          val.dump (os);
          os << "\n";
        }

      os << "\n";
    }

  if (! table.empty ())
    {
      os << "  other symbols in this scope (l=local; a=auto; f=formal\n"
         << "    h=hidden; i=inherited; g=global; p=persistent)\n\n";

      for (table_const_iterator p = table.begin (); p != table.end (); p++)
        p->second.dump (os, "    ");

      os << "\n";
    }
}

void IdentifierTable::cleanup(void) {
	// Clear variables in top scope.
	all_instances[xtop_scope]->clear_variables();

	// Clear function table. This is a hard clear, ignoring mlocked functions.
	fcn_table.clear();

	// Clear variables in global scope.
	// FIXME: are there any?
	all_instances[xglobal_scope]->clear_variables();

	// Clear global variables.
	global_table.clear();

	// Delete all possibly remaining scopes.
	for (all_instances_iterator iter = all_instances.begin(); iter
	!= all_instances.end(); iter++) {
		scope_id scope = iter->first;
		if (scope != xglobal_scope && scope != xtop_scope)
			scope_id_cache::free(scope);

		// First zero the table entry to avoid possible duplicate delete.
		IdentifierTable *inst = iter->second;
		iter->second = 0;

		// Now delete the scope. Note that there may be side effects, such as
		// deleting other scopes.
		delete inst;
	}
}
#endif
