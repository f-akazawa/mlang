//===--- IdentifierTable.h - Identifier Table for Mlang  --------*- C++ -*-===//
//
// Copyright (C) 2010 Yabin Hu @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines mlang IdentifierTable.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_IDENTIFIER_TABLE_H_
#define MLANG_BASIC_IDENTIFIER_TABLE_H_

#include "mlang/Basic/TokenKinds.h"
#include "mlang/Basic/IdentifierInfo.h"
//#include "mlang/Basic/FunctionInfo.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/PointerLikeTypeTraits.h"
#include <string>
#include <cassert>

#include <deque>
#include <list>
#include <map>
#include <set>


//#include "glob-match.h"
//#include "regex-match.h"

//class tree_argument_list;
//class octave_user_function;
//class octave_value;
//#include "oct-obj.h"
//#include "ov.h"

namespace mlang {
class scope_id_cache;
class Scope;
//class IdentifierInfo;
//class IdentifierInfoLookup;
class LangOptions;
//class FunctionInfo;
class SourceLocation;

/// IdentifierLocPair - A simple pair of identifier info and location.
typedef std::pair<IdentifierInfo*, SourceLocation> IdentifierLocPair;

/// IdentifierTable - This table implements an efficient mapping from strings to
/// IdentifierInfo nodes.  It has no other purpose, but this is an
/// extremely performance-critical piece of the code, as each occurrance of
/// every identifier goes through here when lexed.
class IdentifierTable {
	// describe multi-level scope
	typedef int scope_id;

	// describe multi-level context
	typedef size_t context_id;

	// Shark shows that using MallocAllocator is *much* slower than using this
	// BumpPtrAllocator!
	typedef llvm::StringMap<IdentifierInfo*, llvm::BumpPtrAllocator> HashTableTy;
	HashTableTy HashTable;

	IdentifierInfoLookup* ExternalLookup;

public:

	/// IdentifierTable ctor - Create the identifier table, populating it with
	/// info about the language keywords for the language specified by LangOpts.
	IdentifierTable(const LangOptions &LangOpts,
			IdentifierInfoLookup* externalLookup = 0);

	/// \brief Set the external identifier lookup mechanism.
	void setExternalIdentifierLookup(IdentifierInfoLookup *IILookup) {
		ExternalLookup = IILookup;
	}

	/// \brief Retrieve the external identifier lookup object, if any.
	IdentifierInfoLookup *getExternalIdentifierLookup() const {
		return ExternalLookup;
	}

	llvm::BumpPtrAllocator& getAllocator() {
		return HashTable.getAllocator();
	}

	/// get - Return the identifier token info for the specified named identifier.
	///
	IdentifierInfo &get(llvm::StringRef Name) {
		llvm::StringMapEntry<IdentifierInfo*> &Entry =
				HashTable.GetOrCreateValue(Name);

		IdentifierInfo *II = Entry.getValue();
		if (II)
			return *II;

		// No entry; if we have an external lookup, look there first.
		if (ExternalLookup) {
			II = ExternalLookup->get(Name);
			if (II) {
				// Cache in the StringMap for subsequent lookups.
				Entry.setValue(II);
				return *II;
			}
		}

		// Lookups failed, make a new IdentifierInfo.
		void *Mem = getAllocator().Allocate<IdentifierInfo> ();
		II = new (Mem) IdentifierInfo();
		Entry.setValue(II);

		// Make sure getName() knows how to find the IdentifierInfo
		// contents.
		II->Entry = &Entry;

		return *II;
	}

	IdentifierInfo &get(llvm::StringRef Name, tok::TokenKind TokenCode) {
		IdentifierInfo &II = get(Name);
		II.TokenID = TokenCode;
		return II;
	}

	IdentifierInfo &get(const char *NameStart, const char *NameEnd) {
		return get(llvm::StringRef(NameStart, NameEnd - NameStart));
	}

	IdentifierInfo &get(const char *Name, size_t NameLen) {
		return get(llvm::StringRef(Name, NameLen));
	}

	/// \brief Gets an IdentifierInfo for the given name without consulting
	///        external sources.
	///
	/// This is a version of get() meant for external sources that want to
	/// introduce or modify an identifier. If they called get(), they would
	/// likely end up in a recursion.
	IdentifierInfo &getOwn(llvm::StringRef Name) {
		llvm::StringMapEntry<IdentifierInfo*> &Entry =
				HashTable.GetOrCreateValue(Name);

		IdentifierInfo *II = Entry.getValue();
		if (!II) {

			// Lookups failed, make a new IdentifierInfo.
			void *Mem = getAllocator().Allocate<IdentifierInfo> ();
			II = new (Mem) IdentifierInfo();
			Entry.setValue(II);

			// Make sure getName() knows how to find the IdentifierInfo
			// contents.
			II->Entry = &Entry;
		}

		return *II;
	}

	typedef HashTableTy::const_iterator iterator;
	typedef HashTableTy::const_iterator const_iterator;

	iterator begin() const {
		return HashTable.begin();
	}
	iterator end() const {
		return HashTable.end();
	}
	unsigned size() const {
		return HashTable.size();
	}

	/// PrintStats - Print some statistics to stderr that indicate how well the
	/// hashing is doing.
	void PrintStats() const;

	void AddKeywords(const LangOptions &LangOpts);

#if 0
	static scope_id global_scope(void) {
		return xglobal_scope;
	}
	static scope_id top_scope(void) {
		return xtop_scope;
	}
	static scope_id current_scope(void) {
		return xcurrent_scope;
	}
	static context_id current_context(void) {
		return xcurrent_context;
	}

	static scope_id alloc_scope(void) {
		return scope_id_cache::alloc();
	}
	static void set_scope(scope_id scope) {
		if (scope == xglobal_scope)
			error("can't set scope to global");
		else if (scope != xcurrent_scope) {
			all_instances_iterator p = all_instances.find(scope);

			if (p == all_instances.end()) {
				symbol_table *inst = new symbol_table();

				if (inst)
					all_instances[scope] = instance = inst;
			} else
				instance = p->second;

			xcurrent_scope = scope;
			xcurrent_context = 0;
		}
	}
	static void set_scope_and_context(scope_id scope, context_id context) {
		if (scope == xglobal_scope)
			error("can't set scope to global");
		else {
			if (scope != xcurrent_scope) {
				all_instances_iterator p = all_instances.find(scope);

				if (p == all_instances.end())
					error("scope not found!");
				else {
					instance = p->second;

					xcurrent_scope = scope;

					xcurrent_context = context;
				}
			} else
				xcurrent_context = context;
		}
	}
	static void erase_scope(scope_id scope) {
		assert(scope != xglobal_scope);

		all_instances_iterator p = all_instances.find(scope);

		if (p != all_instances.end()) {
			delete p->second;

			all_instances.erase(p);

			free_scope(scope);
		}
	}

	static void erase_subfunctions_in_scope(scope_id scope) {
		for (fcn_table_iterator q = fcn_table.begin(); q != fcn_table.end(); q++)
			q->second.erase_subfunction(scope);
	}

	static scope_id dup_scope(scope_id scope) {
		scope_id retval = -1;

		IdentifierTable *inst = get_instance(scope);

		if (inst) {
			scope_id new_scope = alloc_scope();

			IdentifierTable *new_symbol_table = new IdentifierTable();

			if (new_symbol_table) {
				all_instances[new_scope] = new_symbol_table;

				inst->do_dup_scope(*new_symbol_table);

				retval = new_scope;
			}
		}

		return retval;
	}

	static std::list<scope_id> scopes(void) {
		return scope_id_cache::scopes();
	}

	static IdentifierInfo find_symbol(const std::string& name, scope_id scope =
			xcurrent_scope) {
		IdentifierTable *inst = get_instance(scope);

		return inst ? inst->do_find_symbol(name) : IdentifierInfo();
	}

	static void inherit(scope_id scope, scope_id donor_scope,
			context_id donor_context) {
		IdentifierTable *inst = get_instance(scope);

		if (inst) {
			IdentifierTable *donor_symbol_table = get_instance(donor_scope);

			if (donor_symbol_table)
				inst->do_inherit(*donor_symbol_table, donor_context);
		}
	}

	static bool at_top_level(void) {
		return xcurrent_scope == xtop_scope;
	}

	// Find a value corresponding to the given name in the table.
	static octave_value find(const std::string& name,
			const octave_value_list& args = octave_value_list(),
			bool skip_variables = false, bool local_funcs = true);

	static octave_value builtin_find(const std::string& name);

	// Insert a new name in the table.
	static IdentifierInfo& insert(const std::string& name) {
		static IdentifierInfo foobar;

		IdentifierTable *inst = get_instance(xcurrent_scope);

		return inst ? inst->do_insert(name) : foobar;
	}

	static void force_variable(const std::string& name, scope_id scope =
			xcurrent_scope, context_id context = xcurrent_context) {
		IdentifierTable *inst = get_instance(scope);

		if (inst)
			inst->do_force_variable(name, context);
	}

	static octave_value& varref(const std::string& name, scope_id scope =
			xcurrent_scope, context_id context = xcurrent_context) {
		static octave_value foobar;

		IdentifierTable *inst = get_instance(scope);

		return inst ? inst->do_varref(name, context) : foobar;
	}

	static octave_value varval(const std::string& name, scope_id scope =
			xcurrent_scope, context_id context = xcurrent_context) {
		IdentifierTable *inst = get_instance(scope);

		return inst ? inst->do_varval(name, context) : octave_value();
	}

	static octave_value& global_varref(const std::string& name) {
		global_table_iterator p = global_table.find(name);

		return (p == global_table.end()) ? global_table[name] : p->second;
	}

	static octave_value global_varval(const std::string& name) {
		global_table_const_iterator p = global_table.find(name);

		return (p != global_table.end()) ? p->second : octave_value();
	}

	static octave_value& top_level_varref(const std::string& name) {
		return varref(name, top_scope(), 0);
	}

	static octave_value top_level_varval(const std::string& name) {
		return varval(name, top_scope(), 0);
	}

	static octave_value& persistent_varref(const std::string& name) {
		static octave_value foobar;

		IdentifierTable *inst = get_instance(xcurrent_scope);

		return inst ? inst->do_persistent_varref(name) : foobar;
	}

	static octave_value persistent_varval(const std::string& name) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		return inst ? inst->do_persistent_varval(name) : octave_value();
	}

	static void erase_persistent(const std::string& name) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		if (inst)
			inst->do_erase_persistent(name);
	}

	static bool is_variable(const std::string& name) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		return inst ? inst->do_is_variable(name) : false;
	}

	static bool is_built_in_function_name(const std::string& name) {
		octave_value val = find_built_in_function(name);

		return val.is_defined();
	}

	static octave_value find_method(const std::string& name,
			const std::string& dispatch_type) {
		fcn_table_const_iterator p = fcn_table.find(name);

		if (p != fcn_table.end())
			return p->second.find_method(dispatch_type);
		else {
			FunctionInfo finfo(name);

			octave_value fcn = finfo.find_method(dispatch_type);

			if (fcn.is_defined())
				fcn_table[name] = finfo;

			return fcn;
		}
	}

	static octave_value find_built_in_function(const std::string& name) {
		fcn_table_const_iterator p = fcn_table.find(name);

		return (p != fcn_table.end()) ? p->second.find_built_in_function()
				: octave_value();
	}

	static octave_value find_autoload(const std::string& name) {
		fcn_table_iterator p = fcn_table.find(name);

		return (p != fcn_table.end()) ? p->second.find_autoload()
				: octave_value();
	}

	static octave_value find_function(const std::string& name,
			const octave_value_list& args = octave_value_list(),
			bool local_funcs = true);

	static octave_value find_user_function(const std::string& name) {
		fcn_table_iterator p = fcn_table.find(name);

		return (p != fcn_table.end()) ? p->second.find_user_function()
				: octave_value();
	}

	static void install_cmdline_function(const std::string& name,
			const octave_value& fcn) {
		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			finfo.install_cmdline_function(fcn);
		} else {
			FunctionInfo finfo(name);

			finfo.install_cmdline_function(fcn);

			fcn_table[name] = finfo;
		}
	}

	static void install_subfunction(const std::string& name,
			const octave_value& fcn, scope_id scope) {
		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			finfo.install_subfunction(fcn, scope);
		} else {
			FunctionInfo finfo(name);

			finfo.install_subfunction(fcn, scope);

			fcn_table[name] = finfo;
		}
	}

	static void install_user_function(const std::string& name,
			const octave_value& fcn) {
		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			finfo.install_user_function(fcn);
		} else {
			FunctionInfo finfo(name);

			finfo.install_user_function(fcn);

			fcn_table[name] = finfo;
		}
	}

	static void install_built_in_function(const std::string& name,
			const octave_value& fcn) {
		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			finfo.install_built_in_function(fcn);
		} else {
			FunctionInfo finfo(name);

			finfo.install_built_in_function(fcn);

			fcn_table[name] = finfo;
		}
	}

	static void clear(const std::string& name) {
		clear_variable(name);
	}

	static void clear_all(void) {
		clear_variables();

		clear_global_pattern("*");

		clear_functions();
	}

	static void clear_variables(scope_id scope) {
		IdentifierTable *inst = get_instance(scope);

		if (inst)
			inst->do_clear_variables();
	}

	// This is split for unwind_protect.
	static void clear_variables(void) {
		clear_variables(xcurrent_scope);
	}

	static void clear_objects(scope_id scope = xcurrent_scope) {
		IdentifierTable *inst = get_instance(scope);

		if (inst)
			inst->do_clear_objects();
	}

	static void unmark_forced_variables(scope_id scope = xcurrent_scope) {
		IdentifierTable *inst = get_instance(scope);

		if (inst)
			inst->do_unmark_forced_variables();
	}

	static void clear_functions(void) {
		for (fcn_table_iterator p = fcn_table.begin(); p != fcn_table.end(); p++)
			p->second.clear();
	}

	static void clear_function(const std::string& name) {
		clear_user_function(name);
	}

	static void clear_global(const std::string& name) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		if (inst)
			inst->do_clear_global(name);
	}

	static void clear_variable(const std::string& name) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		if (inst)
			inst->do_clear_variable(name);
	}

	static void clear_symbol(const std::string& name) {
		// FIXME -- are we supposed to do both here?

		clear_variable(name);
		clear_function(name);
	}

	static void clear_function_pattern(const std::string& pat) {
		glob_match pattern(pat);

		for (fcn_table_iterator p = fcn_table.begin(); p != fcn_table.end(); p++) {
			if (pattern.match(p->first))
				p->second.clear_user_function();
		}
	}

	static void clear_global_pattern(const std::string& pat) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		if (inst)
			inst->do_clear_global_pattern(pat);
	}

	static void clear_variable_pattern(const std::string& pat) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		if (inst)
			inst->do_clear_variable_pattern(pat);
	}

	static void clear_variable_regexp(const std::string& pat) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		if (inst)
			inst->do_clear_variable_regexp(pat);
	}

	static void clear_symbol_pattern(const std::string& pat) {
		// FIXME -- are we supposed to do both here?

		clear_variable_pattern(pat);
		clear_function_pattern(pat);
	}

	static void clear_user_function(const std::string& name) {
		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			finfo.clear_user_function();
		}
		// FIXME -- is this necessary, or even useful?
		// else
		//   error ("clear: no such function `%s'", name.c_str ());
	}

	// This clears oct and mex files, incl. autoloads.
	static void clear_dld_function(const std::string& name) {
		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			finfo.clear_autoload_function();
			finfo.clear_user_function();
		}
	}

	static void clear_mex_functions(void) {
		for (fcn_table_iterator p = fcn_table.begin(); p != fcn_table.end(); p++) {
			FunctionInfo& finfo = p->second;

			finfo.clear_mex_function();
		}
	}

	static bool set_class_relationship(const std::string& sup_class,
			const std::string& inf_class);

	static bool is_superiorto(const std::string& a, const std::string& b);

	static void alias_built_in_function(const std::string& alias,
			const std::string& name) {
		octave_value fcn = find_built_in_function(name);

		if (fcn.is_defined()) {
			FunctionInfo finfo(alias);

			finfo.install_built_in_function(fcn);

			fcn_table[alias] = finfo;
		} else
			panic("alias: `%s' is undefined", name.c_str());
	}

	static void add_dispatch(const std::string& name, const std::string& type,
			const std::string& fname) {
		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			finfo.add_dispatch(type, fname);
		} else {
			FunctionInfo finfo(name);

			finfo.add_dispatch(type, fname);

			fcn_table[name] = finfo;
		}
	}

	static void clear_dispatch(const std::string& name, const std::string& type) {
		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			finfo.clear_dispatch(type);
		}
	}

	static void print_dispatch(std::ostream& os, const std::string& name) {
		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			finfo.print_dispatch(os);
		}
	}

	static FunctionInfo::dispatch_map_type get_dispatch(const std::string& name) {
		FunctionInfo::dispatch_map_type retval;

		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			retval = finfo.get_dispatch();
		}

		return retval;
	}

	static std::string help_for_dispatch(const std::string& name) {
		std::string retval;

		fcn_table_iterator p = fcn_table.find(name);

		if (p != fcn_table.end()) {
			FunctionInfo& finfo = p->second;

			retval = finfo.help_for_dispatch();
		}

		return retval;
	}

	static void push_context(void) {
		if (xcurrent_scope == xglobal_scope || xcurrent_scope == xtop_scope)
			error("invalid call to xymtab::push_context");
		else {
			IdentifierTable *inst = get_instance(xcurrent_scope);

			if (inst)
				inst->do_push_context();
		}
	}

	static void pop_context(void) {
		if (xcurrent_scope == xglobal_scope || xcurrent_scope == xtop_scope)
			error("invalid call to xymtab::pop_context");
		else {
			IdentifierTable *inst = get_instance(xcurrent_scope);

			if (inst)
				inst->do_pop_context();
		}
	}

	// For unwind_protect.
	static void pop_context(void *) {
		pop_context();
	}

	static void mark_hidden(const std::string& name) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		if (inst)
			inst->do_mark_hidden(name);
	}

	static void mark_global(const std::string& name) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		if (inst)
			inst->do_mark_global(name);
	}

	static std::list<IdentifierInfo> all_variables(scope_id scope =
			xcurrent_scope, context_id context = xcurrent_context,
			bool defined_only = true) {
		IdentifierTable *inst = get_instance(scope);

		return inst ? inst->do_all_variables(context, defined_only)
				: std::list<IdentifierInfo>();
	}

	static std::list<IdentifierInfo> glob(const std::string& pattern) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		return inst ? inst->do_glob(pattern) : std::list<IdentifierInfo>();
	}

	static std::list<IdentifierInfo> regexp(const std::string& pattern) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		return inst ? inst->do_regexp(pattern) : std::list<IdentifierInfo>();
	}

	static std::list<IdentifierInfo> glob_variables(const std::string& pattern) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		return inst ? inst->do_glob(pattern, true)
				: std::list<IdentifierInfo>();
	}

	static std::list<IdentifierInfo> regexp_variables(
			const std::string& pattern) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		return inst ? inst->do_regexp(pattern, true)
				: std::list<IdentifierInfo>();
	}

	static std::list<IdentifierInfo> glob_global_variables(
			const std::string& pattern) {
		std::list<IdentifierInfo> retval;

		glob_match pat(pattern);

		for (global_table_const_iterator p = global_table.begin(); p
		!= global_table.end(); p++) {
			// We generate a list of IdentifierInfo objects so that
			// the results from glob_variables and glob_global_variables
			// may be handled the same way.

			if (pat.match(p->first))
				retval.push_back(IdentifierInfo(p->first, p->second,
						IdentifierInfo::global));
		}

		return retval;
	}

	static std::list<IdentifierInfo> regexp_global_variables(
			const std::string& pattern) {
		std::list<IdentifierInfo> retval;

		regex_match pat(pattern);

		for (global_table_const_iterator p = global_table.begin(); p
		!= global_table.end(); p++) {
			// We generate a list of IdentifierInfo objects so that
			// the results from regexp_variables and regexp_global_variables
			// may be handled the same way.

			if (pat.match(p->first))
				retval.push_back(IdentifierInfo(p->first, p->second,
						IdentifierInfo::global));
		}

		return retval;
	}

	static std::list<IdentifierInfo> glob_variables(
			const string_vector& patterns) {
		std::list<IdentifierInfo> retval;

		size_t len = patterns.length();

		for (size_t i = 0; i < len; i++) {
			std::list<IdentifierInfo> tmp = glob_variables(patterns[i]);

			retval.insert(retval.begin(), tmp.begin(), tmp.end());
		}

		return retval;
	}

	static std::list<IdentifierInfo> regexp_variables(
			const string_vector& patterns) {
		std::list<IdentifierInfo> retval;

		size_t len = patterns.length();

		for (size_t i = 0; i < len; i++) {
			std::list<IdentifierInfo> tmp = regexp_variables(patterns[i]);

			retval.insert(retval.begin(), tmp.begin(), tmp.end());
		}

		return retval;
	}

	static std::list<std::string> user_function_names(void) {
		std::list<std::string> retval;

		for (fcn_table_iterator p = fcn_table.begin(); p != fcn_table.end(); p++) {
			if (p->second.is_user_function_defined())
				retval.push_back(p->first);
		}

		if (!retval.empty())
			retval.sort();

		return retval;
	}

	static std::list<std::string> global_variable_names(void) {
		std::list<std::string> retval;

		for (global_table_const_iterator p = global_table.begin(); p
		!= global_table.end(); p++)
			retval.push_back(p->first);

		retval.sort();

		return retval;
	}

	static std::list<std::string> top_level_variable_names(void) {
		IdentifierTable *inst = get_instance(xtop_scope);

		return inst ? inst->do_variable_names() : std::list<std::string>();
	}

	static std::list<std::string> variable_names(void) {
		IdentifierTable *inst = get_instance(xcurrent_scope);

		return inst ? inst->do_variable_names() : std::list<std::string>();
	}

	static std::list<std::string> built_in_function_names(void) {
		std::list<std::string> retval;

		for (fcn_table_const_iterator p = fcn_table.begin(); p
		!= fcn_table.end(); p++) {
			octave_value fcn = p->second.find_built_in_function();

			if (fcn.is_defined())
				retval.push_back(p->first);
		}

		if (!retval.empty())
			retval.sort();

		return retval;
	}

	static bool is_local_variable(const std::string& name) {
		if (xcurrent_scope == xglobal_scope)
			return false;
		else {
			IdentifierTable *inst = get_instance(xcurrent_scope);

			return inst ? inst->do_is_local_variable(name) : false;
		}
	}

	static bool is_global(const std::string& name) {
		if (xcurrent_scope == xglobal_scope)
			return true;
		else {
			IdentifierTable *inst = get_instance(xcurrent_scope);

			return inst ? inst->do_is_global(name) : false;
		}
	}

	static void dump(std::ostream& os, scope_id scope = xcurrent_scope);

	static void dump_global(std::ostream& os);

	static void dump_functions(std::ostream& os);

	static void cache_name(scope_id scope, const std::string& name) {
		IdentifierTable *inst = get_instance(scope, false);

		if (inst)
			inst->do_cache_name(name);
	}

	static void lock_subfunctions(scope_id scope = xcurrent_scope) {
		for (fcn_table_iterator p = fcn_table.begin(); p != fcn_table.end(); p++)
			p->second.lock_subfunction(scope);
	}

	static void unlock_subfunctions(scope_id scope = xcurrent_scope) {
		for (fcn_table_iterator p = fcn_table.begin(); p != fcn_table.end(); p++)
			p->second.unlock_subfunction(scope);
	}

	static void free_scope(scope_id scope) {
		if (scope == xglobal_scope || scope == xtop_scope)
			error("can't free global or top-level scopes!");
		else
			IdentifierTable::scope_id_cache::free(scope);
	}

	static void stash_dir_name_for_subfunctions(scope_id scope,
			const std::string& dir_name);

	static void add_to_parent_map(const std::string& classname,
			const std::list<std::string>& parent_list) {
		parent_map[classname] = parent_list;
	}

	static octave_user_function *get_curr_fcn(scope_id scope = xcurrent_scope) {
		IdentifierTable *inst = get_instance(scope);
		return inst->curr_fcn;
	}

	static void set_curr_fcn(octave_user_function *curr_fcn, scope_id scope =
			xcurrent_scope) {
		assert(scope != xtop_scope && scope != xglobal_scope);
		IdentifierTable *inst = get_instance(scope);
		// FIXME: normally, functions should not usurp each other's scope.
		// If for any incredible reason this is needed, call
		// set_user_function (0, scope) first.
		assert(inst->curr_fcn == 0 || curr_fcn == 0);
		inst->curr_fcn = curr_fcn;
	}

	static void cleanup(void);

private:

	typedef std::map<std::string, IdentifierInfo>::const_iterator
	table_const_iterator;
	typedef std::map<std::string, IdentifierInfo>::iterator table_iterator;

	typedef std::map<std::string, octave_value>::const_iterator
	global_table_const_iterator;
	typedef std::map<std::string, octave_value>::iterator global_table_iterator;

	typedef std::map<std::string, octave_value>::const_iterator
	persistent_table_const_iterator;
	typedef std::map<std::string, octave_value>::iterator
	persistent_table_iterator;

	typedef std::map<scope_id, IdentifierTable*>::const_iterator
	all_instances_const_iterator;
	typedef std::map<scope_id, IdentifierTable*>::iterator
	all_instances_iterator;

	typedef std::map<std::string, FunctionInfo>::const_iterator
	fcn_table_const_iterator;
	typedef std::map<std::string, FunctionInfo>::iterator fcn_table_iterator;

	// Name for this table (usually the file name of the function
	// corresponding to the scope);
	std::string table_name;

	// Map from symbol names to symbol info.
	std::map<std::string, IdentifierInfo> table;

	// The associated user code (may be null).
	octave_user_function *curr_fcn;

	// Map from names of global variables to values.
	static std::map<std::string, octave_value> global_table;

	// Map from names of persistent variables to values.
	std::map<std::string, octave_value> persistent_table;

	// Pointer to symbol table for current scope (variables only).
	static IdentifierTable *instance;

	// Map from scope id to symbol table instances.
	static std::map<scope_id, IdentifierTable*> all_instances;

	// Map from function names to function info (subfunctions, private
	// functions, class constructors, class methods, etc.)
	static std::map<std::string, FunctionInfo> fcn_table;

	// Mape from class names to set of classes that have lower
	// precedence.
	static std::map<std::string, std::set<std::string> > class_precedence_table;

	typedef std::map<std::string, std::set<std::string> >::const_iterator
	class_precedence_table_const_iterator;
	typedef std::map<std::string, std::set<std::string> >::iterator
	class_precedence_table_iterator;

	// Map from class names to parent class names.
	static std::map<std::string, std::list<std::string> > parent_map;

	typedef std::map<std::string, std::list<std::string> >::const_iterator
	const_parent_map_iterator;
	typedef std::map<std::string, std::list<std::string> >::iterator
	parent_map_iterator;

	static const scope_id xglobal_scope;
	static const scope_id xtop_scope;

	static scope_id xcurrent_scope;

	static context_id xcurrent_context;

	IdentifierTable(void) :
		table_name(), table(), curr_fcn(0) {
	}

	~IdentifierTable(void) {
	}

	static IdentifierTable *get_instance(scope_id scope, bool create = true) {
		IdentifierTable *retval = 0;

		bool ok = true;

		if (scope != xglobal_scope) {
			if (scope == xcurrent_scope) {
				if (!instance && create) {
					IdentifierTable *inst = new IdentifierTable();

					if (inst) {
						all_instances[scope] = instance = inst;

						if (scope == xtop_scope)
							instance->do_cache_name("top-level");
					}
				}

				if (!instance)
					ok = false;

				retval = instance;
			} else {
				all_instances_iterator p = all_instances.find(scope);

				if (p == all_instances.end()) {
					if (create) {
						retval = new IdentifierTable();

						if (retval)
							all_instances[scope] = retval;
						else
							ok = false;
					} else
						ok = false;
				} else
					retval = p->second;
			}
		}

		if (!ok)
			error("unable to %s IdentifierTable object for scope %d!",
					create ? "create" : "find", scope);

		return retval;
	}

	void insert_symbol_record(const IdentifierInfo& sr) {
		table[sr.name()] = sr;
	}

	void do_dup_scope(IdentifierTable& new_symbol_table) const {
		for (table_const_iterator p = table.begin(); p != table.end(); p++)
			new_symbol_table.insert_symbol_record(p->second.dup());
	}

	IdentifierInfo do_find_symbol(const std::string& name) {
		table_iterator p = table.find(name);

		if (p == table.end())
			return do_insert(name);
		else
			return p->second;
	}

	void do_inherit(IdentifierTable& donor_table, context_id donor_context) {
		for (table_iterator p = table.begin(); p != table.end(); p++) {
			IdentifierInfo& sr = p->second;

			if (!(sr.is_automatic() || sr.is_formal())) {
				std::string nm = sr.name();

				if (nm != "__retval__") {
					octave_value val = donor_table.do_varval(nm, donor_context);

					if (val.is_defined()) {
						sr.varref(0) = val;

						sr.mark_inherited();
					}
				}
			}
		}
	}

	static FunctionInfo *get_fcn_info(const std::string& name) {
		fcn_table_iterator p = fcn_table.find(name);
		return p != fcn_table.end() ? &p->second : 0;
	}

	octave_value do_find(const std::string& name,
			const octave_value_list& args, bool skip_variables,
			bool local_funcs);

	octave_value do_builtin_find(const std::string& name);

	IdentifierInfo& do_insert(const std::string& name) {
		table_iterator p = table.find(name);

		return p == table.end() ? (table[name] = IdentifierInfo(name))
				: p->second;
	}

	void do_force_variable(const std::string& name, context_id context) {
		table_iterator p = table.find(name);

		if (p == table.end()) {
			IdentifierInfo& sr = do_insert(name);

			sr.force_variable(context);
		} else
			p->second.force_variable(context);
	}

	octave_value& do_varref(const std::string& name, context_id context) {
		table_iterator p = table.find(name);

		if (p == table.end()) {
			IdentifierInfo& sr = do_insert(name);

			return sr.varref(context);
		} else
			return p->second.varref(context);
	}

	octave_value do_varval(const std::string& name, context_id context) const {
		table_const_iterator p = table.find(name);

		return (p != table.end()) ? p->second.varval(context) : octave_value();
	}

	octave_value& do_persistent_varref(const std::string& name) {
		persistent_table_iterator p = persistent_table.find(name);

		return (p == persistent_table.end()) ? persistent_table[name]
		                                                        : p->second;
	}

	octave_value do_persistent_varval(const std::string& name) {
		persistent_table_const_iterator p = persistent_table.find(name);

		return (p != persistent_table.end()) ? p->second : octave_value();
	}

	void do_erase_persistent(const std::string& name) {
		persistent_table_iterator p = persistent_table.find(name);

		if (p != persistent_table.end())
			persistent_table.erase(p);
	}

	bool do_is_variable(const std::string& name) const {
		bool retval = false;

		table_const_iterator p = table.find(name);

		if (p != table.end()) {
			const IdentifierInfo& sr = p->second;

			retval = sr.is_variable();
		}

		return retval;
	}

	void do_push_context(void) {
		for (table_iterator p = table.begin(); p != table.end(); p++)
			p->second.push_context();
	}

	void do_pop_context(void) {
		for (table_iterator p = table.begin(); p != table.end();) {
			if (p->second.pop_context() == 0)
				table.erase(p++);
			else
				p++;
		}
	}

	void do_clear_variables(void) {
		for (table_iterator p = table.begin(); p != table.end(); p++)
			p->second.clear();
	}

	void do_clear_objects(void) {
		for (table_iterator p = table.begin(); p != table.end(); p++) {
			IdentifierInfo& sr = p->second;
			octave_value & val = sr.varref();
			if (val.is_object())
				p->second.clear();
		}
	}

	void do_unmark_forced_variables(void) {
		for (table_iterator p = table.begin(); p != table.end(); p++)
			p->second.unmark_forced();
	}

	void do_clear_global(const std::string& name) {
		table_iterator p = table.find(name);

		if (p != table.end()) {
			IdentifierInfo& sr = p->second;

			if (sr.is_global())
				sr.unmark_global();
		}

		global_table_iterator q = global_table.find(name);

		if (q != global_table.end())
			global_table.erase(q);

	}

	void do_clear_variable(const std::string& name) {
		table_iterator p = table.find(name);

		if (p != table.end())
			p->second.clear();
	}

	void do_clear_global_pattern(const std::string& pat) {
		glob_match pattern(pat);

		for (table_iterator p = table.begin(); p != table.end(); p++) {
			IdentifierInfo& sr = p->second;

			if (sr.is_global() && pattern.match(sr.name()))
				sr.unmark_global();
		}

		for (global_table_iterator q = global_table.begin(); q
		!= global_table.end(); q++) {
			if (pattern.match(q->first))
				global_table.erase(q);
		}

	}

	void do_clear_variable_pattern(const std::string& pat) {
		glob_match pattern(pat);

		for (table_iterator p = table.begin(); p != table.end(); p++) {
			IdentifierInfo& sr = p->second;

			if (sr.is_defined() || sr.is_global()) {
				if (pattern.match(sr.name()))
					sr.clear();
			}
		}
	}

	void do_clear_variable_regexp(const std::string& pat) {
		regex_match pattern(pat);

		for (table_iterator p = table.begin(); p != table.end(); p++) {
			IdentifierInfo& sr = p->second;

			if (sr.is_defined() || sr.is_global()) {
				if (pattern.match(sr.name()))
					sr.clear();
			}
		}
	}

	void do_mark_hidden(const std::string& name) {
		do_insert(name).mark_hidden();
	}

	void do_mark_global(const std::string& name) {
		do_insert(name).mark_global();
	}

	std::list<IdentifierInfo> do_all_variables(context_id context,
			bool defined_only) const {
		std::list<IdentifierInfo> retval;

		for (table_const_iterator p = table.begin(); p != table.end(); p++) {
			const IdentifierInfo& sr = p->second;

			if (defined_only && !sr.is_defined(context))
				continue;

			retval.push_back(sr);
		}

		return retval;
	}

	std::list<IdentifierInfo> do_glob(const std::string& pattern,
			bool vars_only = false) const {
		std::list<IdentifierInfo> retval;

		glob_match pat(pattern);

		for (table_const_iterator p = table.begin(); p != table.end(); p++) {
			if (pat.match(p->first)) {
				const IdentifierInfo& sr = p->second;

				if (vars_only && !sr.is_variable())
					continue;

				retval.push_back(sr);
			}
		}

		return retval;
	}

	std::list<IdentifierInfo> do_regexp(const std::string& pattern,
			bool vars_only = false) const {
		std::list<IdentifierInfo> retval;

		regex_match pat(pattern);

		for (table_const_iterator p = table.begin(); p != table.end(); p++) {
			if (pat.match(p->first)) {
				const IdentifierInfo& sr = p->second;

				if (vars_only && !sr.is_variable())
					continue;

				retval.push_back(sr);
			}
		}

		return retval;
	}

	std::list<std::string> do_variable_names(void) {
		std::list<std::string> retval;

		for (table_const_iterator p = table.begin(); p != table.end(); p++) {
			if (p->second.is_variable())
				retval.push_back(p->first);
		}

		retval.sort();

		return retval;
	}

	static std::map<std::string, octave_value> subfunctions_defined_in_scope(
			scope_id scope = xcurrent_scope) {
		std::map<std::string, octave_value> retval;

		for (fcn_table_const_iterator p = fcn_table.begin(); p
		!= fcn_table.end(); p++) {
			std::pair<std::string, octave_value> tmp =
					p->second.subfunction_defined_in_scope(scope);

			std::string nm = tmp.first;

			if (!nm.empty())
				retval[nm] = tmp.second;
		}

		return retval;
	}

	bool do_is_local_variable(const std::string& name) const {
		table_const_iterator p = table.find(name);

		return (p != table.end() && !p->second.is_global()
				&& p->second.is_defined());
	}

	bool do_is_global(const std::string& name) const {
		table_const_iterator p = table.find(name);

		return p != table.end() && p->second.is_global();
	}

	void do_dump(std::ostream& os);

	void do_cache_name(const std::string& name) {
		table_name = name;
	}
#endif
};

#if 0
// global function declaration
extern bool out_of_date_check(octave_value& function,
		const std::string& dispatch_type = std::string(), bool check_relative =	true);
extern std::string get_dispatch_type (const octave_value_list& args);
extern std::string get_dispatch_type (const octave_value_list& args,
		builtin_type_t& builtin_type);
#endif
} //end of namespace mlang
#endif /*MLANG_BASIC_IDENTIFIER_TABLE_H_*/
