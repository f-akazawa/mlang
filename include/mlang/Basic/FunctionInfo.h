//===--- FunctionInfo.h - Symbol Info of Function Name ----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines FunctionInfo.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_FUNCTION_INFO_H_
#define MLANG_BASIC_FUNCTION_INFO_H_

namespace mlang {
class FunctionInfo {
public:

	typedef std::map<std::string, std::string> dispatch_map_type;

	typedef std::map<scope_id, octave_value>::const_iterator scope_val_const_iterator;
	typedef std::map<scope_id, octave_value>::iterator scope_val_iterator;

	typedef std::map<std::string, octave_value>::const_iterator str_val_const_iterator;
	typedef std::map<std::string, octave_value>::iterator str_val_iterator;

	typedef dispatch_map_type::const_iterator dispatch_map_const_iterator;
	typedef dispatch_map_type::iterator dispatch_map_iterator;

	friend class IdentifierTable;

private:

	class FunctionInfoRep
	{
	public:
		FunctionInfoRep (const std::string& nm)
		: name (nm), subfunctions (), private_functions (),
		class_constructors (), class_methods (), cmdline_function (),
		autoload_function (), function_on_path (), built_in_function (),
		count (1) {}

		octave_value load_private_function (const std::string& dir_name);

		octave_value load_class_constructor (void);

		octave_value load_class_method (const std::string& dispatch_type);

		octave_value find (const octave_value_list& args, bool local_funcs);

		octave_value builtin_find (void);

		octave_value find_method (const std::string& dispatch_type);

		octave_value find_autoload (void);

		octave_value find_user_function (void);

		bool is_user_function_defined (void) const
		{
			return function_on_path.is_defined ();
		}

		octave_value find_function (const octave_value_list& args, bool local_funcs)
		{
			return find (args, local_funcs);
		}

		void lock_subfunction (scope_id scope)
		{
			scope_val_iterator p = subfunctions.find (scope);

			if (p != subfunctions.end ())
			p->second.lock ();
		}

		void unlock_subfunction (scope_id scope)
		{
			scope_val_iterator p = subfunctions.find (scope);

			if (p != subfunctions.end ())
			p->second.unlock ();
		}

		std::pair<std::string, octave_value>
		subfunction_defined_in_scope (scope_id scope) const
		{
			scope_val_const_iterator p = subfunctions.find (scope);

			return p == subfunctions.end ()
			? std::pair<std::string, octave_value> ()
			: std::pair<std::string, octave_value> (name, p->second);
		}

		void erase_subfunction (scope_id scope)
		{
			scope_val_iterator p = subfunctions.find (scope);

			if (p != subfunctions.end ())
			subfunctions.erase (p);
		}

		void install_cmdline_function (const octave_value& f)
		{
			cmdline_function = f;
		}

		void install_subfunction (const octave_value& f, scope_id scope)
		{
			subfunctions[scope] = f;
		}

		void install_user_function (const octave_value& f)
		{
			function_on_path = f;
		}

		void install_built_in_function (const octave_value& f)
		{
			built_in_function = f;
		}

		template <class T>
		void
		clear_unlocked (std::map<T, octave_value>& map)
		{
			typename std::map<T, octave_value>::iterator p = map.begin ();

			while (p != map.end ())
			{
				if (p->second.islocked ())
				p++;
				else
				map.erase (p++);
			}
		}

		void clear_cmdline_function (void)
		{
			if (! cmdline_function.islocked ())
			cmdline_function = octave_value ();
		}

		void clear_autoload_function (void)
		{
			if (! autoload_function.islocked ())
			autoload_function = octave_value ();
		}

		// FIXME -- should this also clear the cmdline and other "user
		// defined" functions?
		void clear_user_function (void)
		{
			if (! function_on_path.islocked ())
			{
				function_on_path.erase_subfunctions ();

				function_on_path = octave_value ();
			}
		}

		void clear_mex_function (void)
		{
			if (function_on_path.is_mex_function ())
			clear_user_function ();
		}

		void clear (void)
		{
			clear_unlocked (subfunctions);
			clear_unlocked (private_functions);
			clear_unlocked (class_constructors);
			clear_unlocked (class_methods);
			clear_cmdline_function ();
			clear_autoload_function ();
			clear_user_function ();
		}

		void add_dispatch (const std::string& type, const std::string& fname)
		{
			dispatch_map[type] = fname;
		}

		void clear_dispatch (const std::string& type)
		{
			dispatch_map_iterator p = dispatch_map.find (type);

			if (p != dispatch_map.end ())
			dispatch_map.erase (p);
		}

		void print_dispatch (std::ostream& os) const;

		std::string help_for_dispatch (void) const;

		dispatch_map_type get_dispatch (void) const {return dispatch_map;}

		void dump (std::ostream& os, const std::string& prefix) const;

		std::string name;

		// Scope id to function object.
		std::map<scope_id, octave_value> subfunctions;

		// Directory name to function object.
		std::map<std::string, octave_value> private_functions;

		// Class name to function object.
		std::map<std::string, octave_value> class_constructors;

		// Dispatch type to function object.
		std::map<std::string, octave_value> class_methods;

		// Legacy dispatch map (dispatch type name to function name).
		dispatch_map_type dispatch_map;

		octave_value cmdline_function;

		octave_value autoload_function;

		octave_value function_on_path;

		octave_value built_in_function;

		size_t count;

	private:

		octave_value xfind (const octave_value_list& args, bool local_funcs);

		octave_value x_builtin_find (void);

		// No copying!

		FunctionInfoRep (const FunctionInfoRep&);

		FunctionInfoRep& operator = (const FunctionInfoRep&);
	};

public:

	FunctionInfo(const std::string& nm = std::string ())
	: rep (new fcn_info_rep (nm)) {}

	FunctionInfo(const fcn_info& fi) : rep (fi.rep)
	{
		rep->count++;
	}

	FunctionInfo& operator = (const fcn_info& fi)
	{
		if (this != &fi)
		{
			if (--rep->count == 0)
			delete rep;

			rep = fi.rep;
			rep->count++;
		}

		return *this;
	}

	~FunctionInfo (void)
	{
		if (--rep->count == 0)
		delete rep;
	}

	octave_value find (const octave_value_list& args = octave_value_list (),
			bool local_funcs = true)
	{
		return rep->find (args, local_funcs);
	}

	octave_value builtin_find (void)
	{
		return rep->builtin_find ();
	}

	octave_value find_method (const std::string& dispatch_type) const
	{
		return rep->find_method (dispatch_type);
	}

	octave_value find_built_in_function (void) const
	{
		return rep->built_in_function;
	}

	octave_value find_autoload (void)
	{
		return rep->find_autoload ();
	}

	octave_value find_user_function (void)
	{
		return rep->find_user_function ();
	}

	bool is_user_function_defined (void) const
	{
		return rep->is_user_function_defined ();
	}

	octave_value find_function (const octave_value_list& args = octave_value_list (),
			bool local_funcs = true)
	{
		return rep->find_function (args, local_funcs);
	}

	void lock_subfunction (scope_id scope)
	{
		rep->lock_subfunction (scope);
	}

	void unlock_subfunction (scope_id scope)
	{
		rep->unlock_subfunction (scope);
	}

	std::pair<std::string, octave_value>
	subfunction_defined_in_scope (scope_id scope = xcurrent_scope) const
	{
		return rep->subfunction_defined_in_scope (scope);
	}

	void erase_subfunction (scope_id scope)
	{
		rep->erase_subfunction (scope);
	}

	void install_cmdline_function (const octave_value& f)
	{
		rep->install_cmdline_function (f);
	}

	void install_subfunction (const octave_value& f, scope_id scope)
	{
		rep->install_subfunction (f, scope);
	}

	void install_user_function (const octave_value& f)
	{
		rep->install_user_function (f);
	}

	void install_built_in_function (const octave_value& f)
	{
		rep->install_built_in_function (f);
	}

	void clear (void) {rep->clear ();}

	void clear_user_function (void) {rep->clear_user_function ();}

	void clear_autoload_function (void) {rep->clear_autoload_function ();}

	void clear_mex_function (void) {rep->clear_mex_function ();}

	void add_dispatch (const std::string& type, const std::string& fname)
	{
		rep->add_dispatch (type, fname);
	}

	void clear_dispatch (const std::string& type)
	{
		rep->clear_dispatch (type);
	}

	void print_dispatch (std::ostream& os) const
	{
		rep->print_dispatch (os);
	}

	std::string help_for_dispatch (void) const {return rep->help_for_dispatch ();}

	dispatch_map_type get_dispatch (void) const
	{
		return rep->get_dispatch ();
	}

	void dump (std::ostream& os, const std::string& prefix = std::string ()) const
	{
		rep->dump (os, prefix);
	}

private:

	FunctionInfoRep *rep;
};

} // end of namespace mlang
#endif /* MLANG_BASIC_FUNCTION_INFO_H_ */
