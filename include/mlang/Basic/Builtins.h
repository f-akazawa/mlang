//===--- Builtins.h - Builtin function header -------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file defines enum values for all the target-independent builtin
// functions.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_BUILTINS_H_
#define MLANG_BASIC_BUILTINS_H_

#include <cstring>

namespace llvm {
template<typename T> class SmallVectorImpl;
}

namespace mlang {
class TargetInfo;
class IdentifierTable;
//class ASTContext;

enum LanguageID {
  MATLAB_LANG = 0x1,     // builtin for matlab only.
  OCTAVE_LANG  = 0x2,   // builtin for octave only.
  Gmat = 0x4,  // builtin for gmat
  ALL_LANGUAGES = (MATLAB_LANG|OCTAVE_LANG|Gmat) //builtin is for all languages.
};

namespace Builtin {
enum ID {
	NotBuiltin = 0, // This is not a builtin function.
#define BUILTIN(ID, RETTYPE, ARGTYPE, ATTRS) BI##ID,
#include "mlang/Basic/Builtins.def"
	FirstTSBuiltin
};

struct Info {
	const char *Name, *RetsType, *ArgsType, *Attributes, *HeaderName;
	LanguageID builtin_lang;
	bool Suppressed;

	bool operator==(const Info &RHS) const {
		return !strcmp(Name, RHS.Name) && !strcmp(RetsType, RHS.RetsType) &&
				   !strcmp(ArgsType, RHS.ArgsType) &&
				   !strcmp(Attributes, RHS.Attributes);
	}
	bool operator!=(const Info &RHS) const {
		return !(*this == RHS);
	}
};

/// Builtin::Context - This holds information about target-independent and
/// target-specific builtins, allowing easy queries by clients.
class Context {
	const Info *TSRecords;
	unsigned NumTSRecords;

public:
	explicit Context(const TargetInfo &Target);

	/// InitializeBuiltins - Mark the identifiers for all the builtins with their
	/// appropriate builtin ID # and mark any non-portable builtin identifiers as
	/// such.
	void InitializeBuiltins(IdentifierTable &Table, bool NoBuiltins = false);

	/// \brief Popular the vector with the names of all of the builtins.
	void GetBuiltinNames(llvm::SmallVectorImpl<const char *> &Names,
			bool NoBuiltins);

	/// Builtin::GetName - Return the identifier name for the specified builtin,
	/// e.g. "__builtin_abs".
	const char *GetName(unsigned ID) const {
		return GetRecord(ID).Name;
	}

	/// GetRetsTypeString - Get the return value types descriptor string for the
	/// specified builtin.
	const char *GetRetsTypeString(unsigned ID) const {
		return GetRecord(ID).RetsType;
	}

	const char *GetArgsTypeString(unsigned ID) const {
		return GetRecord(ID).ArgsType;
	}
	/// isConst - Return true if this function has no side effects and doesn't
	/// read memory.
	bool isConst(unsigned ID) const {
		return strchr(GetRecord(ID).Attributes, 'c') != 0;
	}

	/// isNoThrow - Return true if we know this builtin never throws an exception.
	bool isNoThrow(unsigned ID) const {
		return strchr(GetRecord(ID).Attributes, 'n') != 0;
	}

	/// isNoReturn - Return true if we know this builtin never returns.
	bool isNoReturn(unsigned ID) const {
		return strchr(GetRecord(ID).Attributes, 'r') != 0;
	}

	/// isLibFunction - Return true if this is a builtin for a libc/libm function,
	/// with a "__builtin_" prefix (e.g. __builtin_abs).
	bool isLibFunction(unsigned ID) const {
		return strchr(GetRecord(ID).Attributes, 'F') != 0;
	}

	/// \brief Determines whether this builtin is a predefined libc/libm
	/// function, such as "malloc", where we know the signature a
	/// priori.
	bool isPredefinedLibFunction(unsigned ID) const {
		return strchr(GetRecord(ID).Attributes, 'f') != 0;
	}

	/// \brief If this is a library function that comes from a specific
	/// header, retrieve that header name.
	const char *getHeaderName(unsigned ID) const {
		return GetRecord(ID).HeaderName;
	}

	/// \brief Determine whether this builtin is like printf in its
	/// formatting rules and, if so, set the index to the format string
	/// argument and whether this function as a va_list argument.
	bool isPrintfLike(unsigned ID, unsigned &FormatIdx, bool &HasVAListArg);

	/// \brief Determine whether this builtin is like scanf in its
	/// formatting rules and, if so, set the index to the format string
	/// argument and whether this function as a va_list argument.
	bool isScanfLike(unsigned ID, unsigned &FormatIdx, bool &HasVAListArg);

	/// isConstWithoutErrno - Return true if this function has no side
	/// effects and doesn't read memory, except for possibly errno. Such
	/// functions can be const when the MathErrno lang option is
	/// disabled.
	bool isConstWithoutErrno(unsigned ID) const {
		return strchr(GetRecord(ID).Attributes, 'e') != 0;
	}

private:
	const Info &GetRecord(unsigned ID) const;
};

} // end namespace mlang::Builtin
} // end namespace mlang
#endif /* MLANG_BASIC_BUILTINS_H_ */
