//===--- Scope.h - Symbol Scope for Mlang  ----------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines Scope and scope_id_cache.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SEMA_SCOPE_H_
#define MLANG_SEMA_SCOPE_H_

#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/SmallPtrSet.h"

namespace mlang {
class Defn;

/// Scope - A scope is a transient data structure that is used while parsing the
/// program.  It assists with resolving identifiers to the appropriate precious
/// definition.
///
/// MATLAB stores variables in a part of memory called a workspace. The base
/// workspace holds variables created during your interactive MATLAB session and
/// also any variables created by running M-file scripts. Variables created at
/// the MATLAB command prompt can also be used by scripts without having to
/// declare them as global.
///
/// Functions do not use the base workspace. Every function has its own function
/// workspace. Each function workspace is kept separate from the base workspace
/// and all other workspaces to protect the integrity of the data used by that
/// function. Even subfunctions that are defined in the same M-file have a
/// separate function workspace.
class Scope {
public:
	/// ScopeKind - These are bitfields that are or'd together when creating a
	/// scope, which defines the sorts of things the scope contains.
	enum ScopeFlags {
		/// TopScope - This is current base workspace, top-level scope.
		TopScope = 0x01,

		/// FnScope - This indicates that the scope corresponds to a function.
		FnScope = 0x02,

		/// ClassScope
		ClassScope = 0x04,

		// GlobalScope - This is global scope, which can cross multiple workspace.
		GlobalScope = 0x08
	};

private:
	/// The parent scope for this scope.  This is null for the top scope.
	/// script defn body scope, primary function scope and primary class body
	/// scope always have null parent, as long as they don't have global
	/// declarations in it.
	Scope *AnyParent;

	/// Depth - This is the depth of this scope.  The top and global scope
	/// has depth 0. Depth will increment when a new embedded function is
	/// encountered.
	unsigned short Depth;

	/// Flags - This contains a set of ScopeFlags, which indicates how the scope
	/// interrelates with other control flow statements.
	unsigned short Flags;

	/// FnParent - If this scope has a parent scope that is a function body, this
	/// pointer is non-null and points to it.
	Scope *FnParent;

	/// ClassParent - If this scope has a parent scope that is a classdef body, this
	/// pointer is non-null and points to it.
	Scope *ClassParent;

	/// GlobalParent - If this scope has global declarations, this pointer
	/// is non-null and points to it.
	Scope *GlobalParent;

	/// DefnsInScope - This keeps track of all definitions in this scope.  When
	/// the definition is added to the scope, it is set as the current
	/// definition for the identifier in the IdentifierTable.  When the scope is
	/// popped, these definitions are removed from the IdentifierTable's notion
	/// of current definition.  It is up to the current Action implementation to
	/// implement these semantics.
	/// The size can grow up automatically. 32 is initial.
	typedef llvm::SmallPtrSet<Defn *, 32> DefnSetTy;
	DefnSetTy DefnsInScope;

	/// Entity - The entity with which this scope is associated. For
	/// example, the entity of a class scope is the class itself, the
	/// entity of a function scope is a function, etc. This field is
	/// maintained by the Action implementation.
	void *Entity;

	/// \brief Used to determine if errors occurred in this scope.
	DiagnosticErrorTrap ErrorTrap;

public:
	Scope(Scope *Parent, unsigned ScopeFlags, Diagnostic &Diag) :
		ErrorTrap(Diag) {
		Init(Parent, ScopeFlags);
	}

	/// getFlags - Return the flags for this scope.
	///
	unsigned getFlags() const {
		return Flags;
	}
	void setFlags(unsigned F) {
		Flags = F;
	}

	/// getParent - Return the scope that this is nested in.
	///
	const Scope *getParent() const {
		return AnyParent;
	}
	Scope *getParent() {
		return AnyParent;
	}

	/// getFnParent - Return the closest scope that is a function body.
	///
	const Scope *getFnParent() const {
		return FnParent;
	}
	Scope *getFnParent() {
		return FnParent;
	}

	/// getClassParent - Return the closest scope that is a classdef body.
	///
	const Scope *getClassParent() const {
		return ClassParent;
	}
	Scope *getClassParent() {
		return ClassParent;
	}

	/// getGlobalParent - Return the closest scope that is a function body.
	///
	const Scope *getGlobalParent() const {
		return GlobalParent;
	}
	Scope *getGlobalParent() {
		return GlobalParent;
	}
	void SetGlobalParent(Scope *S) {
		GlobalParent = S;
	}

	typedef DefnSetTy::iterator defn_iterator;
	defn_iterator defn_begin() const {
		return DefnsInScope.begin();
	}
	defn_iterator defn_end() const {
		return DefnsInScope.end();
	}
	bool defn_empty() const {
		return DefnsInScope.empty();
	}

	void AddDefn(Defn *D) {
		DefnsInScope.insert(D);
	}

	void RemoveDefn(Defn *D) {
		DefnsInScope.erase(D);
	}

	/// isDefnScope - Return true if this is the scope that the specified defn is
	/// defined in.
	bool isDefnScope(Defn *D) {
		return DefnsInScope.count(D) != 0;
	}

	void* getEntity() const {
		return Entity;
	}
	void setEntity(void *E) {
		Entity = E;
	}

	bool hasErrorOccurred() const {
		return ErrorTrap.hasErrorOccurred();
	}

	/// isClassScope - Return true if this scope is a class definition scope.
	bool isClassScope() const {
		return (getFlags() & Scope::ClassScope);
	}

	/// Init - This is used by the parser to implement scope caching.
	///
	void Init(Scope *Parent, unsigned ScopeFlags) {
		AnyParent = Parent;
		GlobalParent = 0;
		Depth = AnyParent ? AnyParent->Depth + 1 : 0;
		Flags = ScopeFlags;

		if (AnyParent) {
			FnParent = AnyParent->FnParent;
			ClassParent = AnyParent->ClassParent;
		} else {
			FnParent = ClassParent = 0;
		}

		// If this scope is a function or class definition, remember it.
		if (Flags & FnScope)
			FnParent = this;
		if (Flags & ClassScope)
			ClassParent = this;

		DefnsInScope.clear();
		Entity = 0;
		ErrorTrap.reset();
	}
};

} // end of namespace mlang
#endif /* MLANG_SEMA_SCOPE_H_ */
