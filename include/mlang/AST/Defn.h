//===--- Defn.h - Class for representing Definition -------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the Defn class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_DEFN_H_
#define MLANG_AST_DEFN_H_

#include "mlang/Basic/SourceLocation.h"
#include "mlang/Basic/Specifiers.h"
#include "llvm/Support/PrettyStackTrace.h"

namespace llvm {
class raw_ostream;
}

namespace mlang {
class ASTContext;
class ASTMutationListener;
class DefnContext;
class Cmd;
class TranslationUnitDefn;
struct PrintingPolicy;

/// Defn - This represents one definition, e.g. a variable, classdef,
///  function, struct, etc.
///
class Defn {
public:
	/// \brief Lists the kind of concrete classes of Defn.
	enum Kind {
#define DEFN(DERIVED, BASE) DERIVED,
#define ABSTRACT_DEFN(DEFN)
#define DEFN_RANGE(BASE, START, END) \
        first##BASE = START, last##BASE = END,
#define LAST_DEFN_RANGE(BASE, START, END) \
        first##BASE = START, last##BASE = END
#include "mlang/AST/DefnNodes.inc"
	};

	/// \brief A placeholder type used to construct an empty shell of a
	/// defn-derived type that will be filled in later (e.g., by some
	/// deserialization method).
	struct EmptyShell {	};

	/// \brief IDNamespace - The different namespaces in which
	/// definitions may appear.
	///
	enum IDNamespace {
		/// An ordinary variable, defined with various assignments.
		/// e.g. a = 10, b = fft(c).
		/// we can construct a struct like this,
		/// e.g. s = struct('a',10, 'b', 'hello world').
		/// here the 'struct' is built-in function name.
		/// we can also construct a use-defined class object like this.
		IDNS_Ordinary = 0x01,

		/// A struct/class/cell definition
		IDNS_Type = 0x02,

		/// A struct/class member, defined within member expression
		IDNS_Member = 0x04,

		/// This is an script call command definition. An script definition
		/// *introduces* a number of other definitions into the current
		/// scope.
		IDNS_Script = 0x08,

		/// This is an external package, which is in some sub directory with
		/// directory name leading with a '@' to denote a package.
		IDNS_Package = 0x10
	};

private:
	/// NextDefnInContext - The next definition within the same lexical
	/// DefnContext. These pointers form the linked list that is
	/// traversed via DefnContext's defns_begin()/defns_end().
	Defn *NextDefnInContext; // set by DefnContext

	friend class DefnContext;

	/// DefnCtx - Holds a DefnContext* object
	DefnContext *DefnCtx;

	/// Loc - The start location of this defn.
	SourceLocation Loc;

	/// DefnKind - This indicates which class this is.
	unsigned DefnKind :8;

	/// InvalidDefn - This indicates a semantic error occurred.
	unsigned InvalidDefn :1;

	/// Implicit - Whether this definition was implicitly generated by
	/// the implementation rather than explicitly written by the user.
	unsigned Implicit :1;

protected:
	/// Access - Used by classdef for the access specifier.
	// NOTE: VC++ treats enums as signed, avoid using the AccessSpecifier enum
	unsigned Access :2;

	/// It can be SystemSpace, ImportingSpace and WorkSpace
	unsigned PCHLevel :2;

	/// IdentifierNamespace - This specifies what IDNS_* namespace this lives in.
	unsigned IdentifierNamespace :8;

	/// \brief Whether the \c CachedLinkage field is active.
	///
	/// This field is only valid for NamedDefns subclasses.
	mutable unsigned HasCachedLinkage :1;

	/// \brief If \c HasCachedLinkage, the linkage of this definition.
	///
	/// This field is only valid for NamedDefns subclasses.
	mutable unsigned CachedLinkage :2;

private:
	void CheckAccessDefnContext() const;

protected:
	/// Constructors and destructor
	Defn(Kind DK, DefnContext *DC, SourceLocation L) :
		NextDefnInContext(0), DefnCtx(DC), Loc(L), DefnKind(DK),
		InvalidDefn(0), Implicit(false),
		Access(AS_none), PCHLevel(0),
		IdentifierNamespace(getIdentifierNamespaceForKind(DK)),
		HasCachedLinkage(0) {
		if (Defn::CollectingStats())
			add(DK);
	}

	Defn(Kind DK, EmptyShell Empty) :
		NextDefnInContext(0), DefnKind(DK), InvalidDefn(0),
		Implicit(false), Access(AS_none), PCHLevel(0),
		IdentifierNamespace(getIdentifierNamespaceForKind(DK)),
		HasCachedLinkage(0) {
		if (Defn::CollectingStats())
			add(DK);
	}

	virtual ~Defn();

public:
	/// \brief Source range that this definition covers.
	virtual SourceRange getSourceRange() const {
		return SourceRange(getLocation(), getLocation());
	}
	SourceLocation getLocStart() const {
		return getSourceRange().getBegin();
	}
	SourceLocation getLocEnd() const {
		return getSourceRange().getEnd();
	}

	SourceLocation getLocation() const {
		return Loc;
	}
	void setLocation(SourceLocation L) {
		Loc = L;
	}

	Kind getKind() const {
		return static_cast<Kind> (DefnKind);
	}
	const char *getDefnKindName() const;

	Defn *getNextDefnInContext() {
		return NextDefnInContext;
	}
	const Defn *getNextDefnInContext() const {
		return NextDefnInContext;
	}

	DefnContext *getDefnContext() {
		return DefnCtx;
	}
	const DefnContext *getDefnContext() const {
		return const_cast<Defn*>(this)->getDefnContext();
	}
	/// setDefnContext - Set both the semantic and lexical DefnContext
	/// to DC.
	void setDefnContext(DefnContext *DC);

	TranslationUnitDefn *getTranslationUnitDefn();
	const TranslationUnitDefn *getTranslationUnitDefn() const {
		return const_cast<Defn*> (this)->getTranslationUnitDefn();
	}

	ASTContext &getASTContext() const;

	void setAccess(AccessSpecifier AS) {
		Access = AS;
//#ifndef NDEBUG
//		CheckAccessDefnContext();
//#endif
	}

	AccessSpecifier getAccess() const {
#ifndef NDEBUG
		// FIXME yabin
//		CheckAccessDefnContext();
#endif
		return AccessSpecifier(Access);
	}
	/// setInvalidDefn - Indicates the Defn had a semantic error. This
	/// allows for graceful error recovery.
	void setInvalidDefn(bool Invalid = true);
	bool isInvalidDefn() const {
		return (bool) InvalidDefn;
	}

	/// isImplicit - Indicates whether the definition was implicitly
	/// generated by the implementation. If false, this definition
	/// was written explicitly in the source code.
	bool isImplicit() const {
		return Implicit;
	}
	void setImplicit(bool I = true) {
		Implicit = I;
	}

	/// \brief Retrieve the level of precompiled header from which this
	/// definition was generated.
	///
	/// The PCH level of a definition describes where the definition originated
	/// from. A PCH level of 0 indicates that the definition was parsed from
	/// source. A PCH level of 1 indicates that the definition was loaded from
	/// a top-level AST file. A PCH level 2 indicates that the definition was
	/// loaded from a PCH file the AST file depends on, and so on.
	unsigned getPCHLevel() const {
		return PCHLevel;
	}

	/// \brief The maximum PCH level that any definition may have.
	static const unsigned MaxPCHLevel = 3;

	/// \brief Set the PCH level of this definition.
	void setPCHLevel(unsigned Level) {
		assert(Level <= MaxPCHLevel && "PCH level exceeds the maximum");
		PCHLevel = Level;
	}

	unsigned getIdentifierNamespace() const {
		return IdentifierNamespace;
	}
	bool isInIdentifierNamespace(unsigned NS) const {
		return getIdentifierNamespace() & NS;
	}
	static unsigned getIdentifierNamespaceForKind(Kind DK);

	bool hasTypeIdentifierNamespace() const {
		return isTypeIdentifierNamespace(getIdentifierNamespace());
	}
	static bool isTypeIdentifierNamespace(unsigned NS) {
		return (NS & IDNS_Type);
	}

protected:
	/// \brief Returns the next redefinition or itself if this is the only defn.
	///
	/// Defn subclasses that can be redefined should override this method so that
	/// Defn::redefn_iterator can iterate over them.
	virtual Defn *getNextRedefinition() {
		return this;
	}

public:
	/// getBody - This method returns the top-level Stmt* of that body.
	virtual Cmd* getBody() const {
		return 0;
	}

	/// \brief Returns false if this Defn represents a persistent or global var
	/// declaration, or an import command. Otherwise, it will return true.
	virtual bool hasBody() const {
		return getBody() != 0;
	}

	/// getBodyBodyEnd - Gets the end of the body, if a body exists.
	/// This works either the body is a CompoundStmt or a TryStmt.
	SourceLocation getBodyEnd() const;

	// global temp stats (until we have a per-module visitor)
	static void add(Kind k);
	static bool CollectingStats(bool Enable = false);
	static void PrintStats();

  // Implement isa/cast/dyncast/etc.
	static bool classof(const Defn *) { return true; }
	static bool classofKind(Kind K) { return true; }
	static DefnContext *castToDefnContext(const Defn *);
	static Defn *castFromDefnContext(const DefnContext *);

	void print(llvm::raw_ostream &Out, unsigned Indentation = 0) const;
	void print(llvm::raw_ostream &Out, const PrintingPolicy &Policy,
	           unsigned Indentation = 0) const;
	static void printGroup(Defn** Begin, unsigned NumDefns,
			                   llvm::raw_ostream &Out, const PrintingPolicy &Policy,
			                   unsigned Indentation = 0);
	void dump() const;
	void dumpXML() const;
	void dumpXML(llvm::raw_ostream &OS) const;

protected:
  ASTMutationListener *getASTMutationListener() const;
};

/// PrettyStackTraceDefn - If a crash occurs, indicate that it happened when
/// doing something to a specific defn.
class PrettyStackTraceDefn : public llvm::PrettyStackTraceEntry {
  const Defn *TheDefn;
  SourceLocation Loc;
  SourceManager &SM;
  const char *Message;

public:
  PrettyStackTraceDefn(const Defn *theDefn, SourceLocation L,
                       SourceManager &sm, const char *Msg)
  : TheDefn(theDefn), Loc(L), SM(sm), Message(Msg) {}

  virtual void print(llvm::raw_ostream &OS) const;
};

} // end namespace mlang

#endif /* MLANG_AST_DEFN_H_ */
