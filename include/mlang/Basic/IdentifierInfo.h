//===--- IdentifierInfo.h - Identifier info in Symbol Table -----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines IdentifierInfo.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_IDENTIFIER_INFO_H_
#define MLANG_BASIC_IDENTIFIER_INFO_H_

#include "llvm/ADT/StringMap.h"
#include <cassert>

namespace mlang {
/// IdentifierInfo - One of these records is kept for each identifier that
/// is lexed.  This contains information about:
///  1. whether the token was #define'd,
///  2. whether the token is a language keyword,
///  3. or if it is a front-end token of some sort (e.g. a variable
///     or function name).
/// The preprocessor keeps this information in a set, and
/// all tok::identifier tokens have a pointer to one of these.
class IdentifierInfo {
	friend class IdentifierTable;

	// Note: DON'T make TokenID a 'tok::TokenKind'; MSVC will treat it as a
	//       signed char and TokenKinds > 127 won't be handled correctly.
	unsigned TokenID            : 8; // Front-end token ID or tok::identifier.
	unsigned BuiltinID          :10; // some built-in ID symbol,
	                                 // e.g. builtin (__builtin_inf).
	bool IsMlangOperatorKeyword : 1; // True if it is a Mlang operator keyword.
	bool NeedsHandleIdentifier  : 1; // See "RecomputeNeedsHandleIdentifier".
	bool IsFromAST              : 1; // True if identfier first appeared in an AST
	                                 // file and wasn't modified since.
	bool RevertedTokenID        : 1; // True if RevertTokenIDToIdentifier was
	                                 // called.

	void *FETokenInfo;         // Managed by the language front-end.
	llvm::StringMapEntry<IdentifierInfo*> *Entry;

	IdentifierInfo(const IdentifierInfo&);  // NONCOPYABLE.
	void operator=(const IdentifierInfo&);  // NONASSIGNABLE.

public:
	IdentifierInfo():TokenID(tok::Identifier), BuiltinID(0), IsFromAST(false) {
		IsMlangOperatorKeyword = false;
		NeedsHandleIdentifier = false;
		RevertedTokenID = false;
		FETokenInfo = 0;
		Entry = 0;
	}

	/// isStr - Return true if this is the identifier for the specified string.
	/// This is intended to be used for string literals only: II->isStr("foo").
	template <std::size_t StrLen>
	bool isStr(const char (&Str)[StrLen]) const {
		return getLength() == StrLen-1 && !memcmp(getNameStart(), Str, StrLen-1);
	}

	/// getNameStart - Return the beginning of the actual string for this
	/// identifier.  The returned string is properly null terminated.
	///
	const char *getNameStart() const {
		if (Entry)
			return Entry->getKeyData();
		// FIXME: This is gross. It would be best not to embed specific details
		// of the PTH file format here.
		// The 'this' pointer really points to a
		// std::pair<IdentifierInfo, const char*>, where internal pointer
		// points to the external string data.
		typedef std::pair<IdentifierInfo, const char*> actualtype;
		return ((const actualtype*) this)->second;
	}

	/// getLength - Efficiently return the length of this identifier info.
	///
	unsigned getLength() const {
		if (Entry)
			return Entry->getKeyLength();
		// FIXME: This is gross. It would be best not to embed specific details
		// of the PTH file format here.
		// The 'this' pointer really points to a
		// std::pair<IdentifierInfo, const char*>, where internal pointer
		// points to the external string data.
		typedef std::pair<IdentifierInfo, const char*> actualtype;
		const char* p = ((const actualtype*) this)->second - 2;
		return (((unsigned) p[0]) | (((unsigned) p[1]) << 8)) - 1;
	}

	/// getName - Return the actual identifier string.
	llvm::StringRef getName() const {
		return llvm::StringRef(getNameStart(), getLength());
	}

	/// getTokenID - If this is a source-language token (e.g. 'for'), this API
	/// can be used to cause the lexer to map identifiers to source-language
	/// tokens.
	tok::TokenKind getTokenID() const {
		return (tok::TokenKind) TokenID;
	}

	/// \brief True if RevertTokenIDToIdentifier() was called.
	bool hasRevertedTokenIDToIdentifier() const {
		return RevertedTokenID;
	}

	/// \brief Revert TokenID to tok::identifier; used for GNU libstdc++ 4.2
	/// compatibility.
	///
	/// TokenID is normally read-only but there are 2 instances where we revert it
	/// to tok::identifier for libstdc++ 4.2. Keep track of when this happens
	/// using this method so we can inform serialization about it.
	void RevertTokenIDToIdentifier() {
		assert(TokenID != tok::Identifier && "Already at tok::identifier");
		TokenID = tok::Identifier;
		RevertedTokenID = true;
	}

	/// getBuiltinID - Return a value indicating whether this is a builtin
	/// function.  0 is not-built-in.  1 is builtin-for-some-nonprimary-target.
	/// 2+ are specific builtin functions.
	unsigned getBuiltinID() const {
		return BuiltinID;
	}
	void setBuiltinID(unsigned ID) {
		BuiltinID = ID;
	}

	bool getIsFromAST() const {
		return IsFromAST;
	}
	void setIsFromAST(bool FromAST) {
		IsFromAST = FromAST;
	}
	void setIsFromAST() {
		setIsFromAST(true);
	}
	/// isMlangOperatorKeyword/setIsMlangOperatorKeyword controls whether
	/// this identifier is a mlang alternate representation of an operator.
	void setIsMlangOperatorKeyword(bool Val = true) {
		IsMlangOperatorKeyword = Val;
		if (Val)
			NeedsHandleIdentifier = 1;
		else
			RecomputeNeedsHandleIdentifier();
	}
	bool isMlangOperatorKeyword() const { return IsMlangOperatorKeyword; }

	/// getFETokenInfo/setFETokenInfo - The language front-end is allowed to
	/// associate arbitrary metadata with this token.
	template<typename T>
	T *getFETokenInfo() const {
		return static_cast<T*> (FETokenInfo);
	}
	void setFETokenInfo(void *T) {
		FETokenInfo = T;
	}

	/// isHandleIdentifierCase - Return true if the Preprocessor::HandleIdentifier
	/// must be called on a token of this identifier.  If this returns false, we
	/// know that HandleIdentifier will not affect the token.
	bool isHandleIdentifierCase() const {
		return NeedsHandleIdentifier;
	}

private:
	/// RecomputeNeedsHandleIdentifier - The Preprocessor::HandleIdentifier does
	/// several special (but rare) things to identifiers of various sorts.  For
	/// example, it changes the "for" keyword token from tok::identifier to
	/// tok::for.
	///
	/// This method is very tied to the definition of HandleIdentifier.  Any
	/// change to it should be reflected here.
	void RecomputeNeedsHandleIdentifier() {
		NeedsHandleIdentifier =	isMlangOperatorKeyword();
	}
};

/// \brief An iterator that walks over all of the known identifiers
/// in the lookup table.
///
/// Since this iterator uses an abstract interface via virtual
/// functions, it uses an object-oriented interface rather than the
/// more standard C++ STL iterator interface. In this OO-style
/// iteration, the single function \c Next() provides dereference,
/// advance, and end-of-sequence checking in a single
/// operation. Subclasses of this iterator type will provide the
/// actual functionality.
class IdentifierIterator {
private:
  IdentifierIterator(const IdentifierIterator&); // Do not implement
  IdentifierIterator &operator=(const IdentifierIterator&); // Do not implement

protected:
  IdentifierIterator() { }

public:
  virtual ~IdentifierIterator();

  /// \brief Retrieve the next string in the identifier table and
  /// advances the iterator for the following string.
  ///
  /// \returns The next string in the identifier table. If there is
  /// no such string, returns an empty \c llvm::StringRef.
  virtual llvm::StringRef Next() = 0;
};

/// IdentifierInfoLookup - An abstract class used by IdentifierTable that
///  provides an interface for performing lookups from strings
/// (const char *) to IdentiferInfo objects.
class IdentifierInfoLookup {
public:
  virtual ~IdentifierInfoLookup();

  /// get - Return the identifier token info for the specified named identifier.
  ///  Unlike the version in IdentifierTable, this returns a pointer instead
  ///  of a reference.  If the pointer is NULL then the IdentifierInfo cannot
  ///  be found.
  virtual IdentifierInfo* get(llvm::StringRef Name) = 0;

  /// \brief Retrieve an iterator into the set of all identifiers
  /// known to this identifier lookup source.
  ///
  /// This routine provides access to all of the identifiers known to
  /// the identifier lookup, allowing access to the contents of the
  /// identifiers without introducing the overhead of constructing
  /// IdentifierInfo objects for each.
  ///
  /// \returns A new iterator into the set of known identifiers. The
  /// caller is responsible for deleting this iterator.
  virtual IdentifierIterator *getIdentifiers() const;
};

/// \brief An abstract class used to resolve numerical identifier
/// references (meaningful only to some external source) into
/// IdentifierInfo pointers.
class ExternalIdentifierLookup {
public:
  virtual ~ExternalIdentifierLookup();

  /// \brief Return the identifier associated with the given ID number.
  ///
  /// The ID 0 is associated with the NULL identifier.
  virtual IdentifierInfo *GetIdentifier(unsigned ID) = 0;
};
} // end namespace mlang
#endif /* MLANG_BASIC_IDENTIFIER_INFO_H_ */
