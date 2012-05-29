//===--- Stmt.h - Classes for representing statements -----------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the Stmt interface and subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_STMT_H_
#define MLANG_AST_STMT_H_

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "mlang/AST/PrettyPrinter.h"
#include "mlang/AST/StmtIterator.h"
#include "mlang/AST/DefnGroup.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/Basic/SourceLocation.h"
#include <string>

using llvm::dyn_cast_or_null;

namespace llvm {
class FoldingSetNodeID;
}

namespace mlang {
class ASTContext;
class Expr;
class SourceManager;
class Stmt;

//===----------------------------------------------------------------------===//
// ExprIterator - Iterators for iterating over Stmt* arrays that contain
// only Expr*.  This is needed because AST nodes use Stmt* arrays to store
// references to children (to be compatible with StmtIterator).
//===----------------------------------------------------------------------===//
class ExprIterator {
	Stmt** I;
public:
	ExprIterator(Stmt** i) :
		I(i) {
	}
	ExprIterator() :
		I(0) {
	}
	ExprIterator& operator++() {
		++I;
		return *this;
	}
	ExprIterator operator-(size_t i) {
		return I - i;
	}
	ExprIterator operator+(size_t i) {
		return I + i;
	}
	Expr* operator[](size_t idx);
	// FIXME: Verify that this will correctly return a signed distance.
	signed operator-(const ExprIterator& R) const {
		return I - R.I;
	}
	Expr* operator*() const;
	Expr* operator->() const;
	bool operator==(const ExprIterator& R) const {
		return I == R.I;
	}
	bool operator!=(const ExprIterator& R) const {
		return I != R.I;
	}
	bool operator>(const ExprIterator& R) const {
		return I > R.I;
	}
	bool operator>=(const ExprIterator& R) const {
		return I >= R.I;
	}
};

class ConstExprIterator {
	const Stmt * const *I;
public:
	ConstExprIterator(const Stmt * const *i) :
		I(i) {
	}
	ConstExprIterator() :
		I(0) {
	}
	ConstExprIterator& operator++() {
		++I;
		return *this;
	}
	ConstExprIterator operator+(size_t i) const {
		return I + i;
	}
	ConstExprIterator operator-(size_t i) const {
		return I - i;
	}
	const Expr * operator[](size_t idx) const;
	signed operator-(const ConstExprIterator& R) const {
		return I - R.I;
	}
	const Expr * operator*() const;
	const Expr * operator->() const;
	bool operator==(const ConstExprIterator& R) const {
		return I == R.I;
	}
	bool operator!=(const ConstExprIterator& R) const {
		return I != R.I;
	}
	bool operator>(const ConstExprIterator& R) const {
		return I > R.I;
	}
	bool operator>=(const ConstExprIterator& R) const {
		return I >= R.I;
	}
};

//===----------------------------------------------------------------------===//
// Stmt - Base class for AST nodes
//===----------------------------------------------------------------------===//
class Stmt {
public:
	enum StmtClass {
		NoStmtClass = 0,
#define STMT(CLASS, PARENT) CLASS##Class,
#define STMT_RANGE(BASE, FIRST, LAST) \
          first##BASE##Constant=FIRST##Class, last##BASE##Constant=LAST##Class,
#define LAST_STMT_RANGE(BASE, FIRST, LAST) \
          first##BASE##Constant=FIRST##Class, last##BASE##Constant=LAST##Class
#define ABSTRACT_STMT(STMT)
#include "mlang/AST/StmtNodes.inc"
	};

	// Make vanilla 'new' and 'delete' illegal for Stmts.
protected:
	void* operator new(size_t bytes) throw () {
		assert(0 && "Stmts cannot be allocated with regular 'new'.");
		return 0;
	}
	void operator delete(void* data) throw () {
		assert(0 && "Stmts cannot be released with regular 'delete'.");
	}

	class StmtBitfields {
		friend class Stmt;

		/// \brief The statement class.
		unsigned sClass :8;
	};

	enum {
		NumStmtBits = 8
	};

	class CmdBitfields {
		friend class Cmd;
		unsigned :NumStmtBits;

		unsigned ValueKind :2;
		unsigned ObjectKind :2;
	};

  class BlockCmdBitfields {
		friend class BlockCmd;
		unsigned :NumStmtBits;

		unsigned NumStmts :32 - NumStmtBits;
	};

	class ExprBitfields {
		friend class Expr;
		unsigned :NumStmtBits;

		unsigned ValueKind :2;
	};

	union {
		StmtBitfields StmtBits;
		CmdBitfields CmdBits;
		BlockCmdBitfields BlockCmdBits;
		ExprBitfields ExprBits;
	};

public:
	// Only allow allocation of Stmts using the allocator in ASTContext
	// or by doing a placement new.
	void* operator new(size_t bytes, ASTContext& C, unsigned alignment = 8) throw () {
		return ::operator new(bytes, C, alignment);
	}

	void* operator new(size_t bytes, ASTContext* C, unsigned alignment = 8) throw () {
		return ::operator new(bytes, *C, alignment);
	}

	void* operator new(size_t bytes, void* mem) throw () {
		return mem;
	}

	void operator delete(void*, ASTContext&, unsigned) throw () {	}

	void operator delete(void*, ASTContext*, unsigned) throw () {	}

	void operator delete(void*, std::size_t) throw () {	}

	void operator delete(void*, void*) throw () {	}

public:
	/// \brief A placeholder type used to construct an empty shell of a
	/// type, that will be filled in later (e.g., by some de-serialization).
	struct EmptyShell {
	};

protected:
	/// \brief Construct an empty statement.
	explicit Stmt(StmtClass SC, EmptyShell) {
		StmtBits.sClass = SC;
		if (Stmt::CollectingStats())
			Stmt::addStmtClass(SC);
	}

public:
	Stmt(StmtClass SC) {
		StmtBits.sClass = SC;
		if (Stmt::CollectingStats())
			Stmt::addStmtClass(SC);
	}
	virtual ~Stmt() {	}

	StmtClass getStmtClass() const {
		return static_cast<StmtClass> (StmtBits.sClass);
	}
	const char *getStmtClassName() const;

	/// SourceLocation tokens are not useful in isolation - they are low level
	/// value objects created/interpreted by SourceManager. We assume AST
	/// clients will have a pointer to the respective SourceManager.
	SourceRange getSourceRange() const;
	SourceLocation getLocStart() const {
		return getSourceRange().getBegin();
	}
	SourceLocation getLocEnd() const {
		return getSourceRange().getEnd();
	}

	// global temp stats (until we have a per-module visitor)
	static void addStmtClass(const StmtClass s);
	static bool CollectingStats(bool Enable = false);
	static void PrintStats();

	/// dump - This does a local dump of the specified AST fragment.  It dumps the
	/// specified node and a few nodes underneath it, but not the whole subtree.
	/// This is useful in a debugger.
	void dump() const;
	void dump(SourceManager &SM) const;
	void dump(llvm::raw_ostream &OS, SourceManager &SM) const;

	/// dumpAll - This does a dump of the specified AST fragment and all subtrees.
	void dumpAll() const;
	void dumpAll(SourceManager &SM) const;

	/// dumpPretty/printPretty - These two methods do a "pretty print" of the AST
	/// back to its original source language syntax.
	void dumpPretty(ASTContext& Context) const;
	void printPretty(llvm::raw_ostream &OS, PrinterHelper *Helper,
			const PrintingPolicy &Policy, unsigned Indentation = 0) const {
		printPretty(OS, *(ASTContext*) 0, Helper, Policy, Indentation);
	}
	void printPretty(llvm::raw_ostream &OS, ASTContext &Context,
			PrinterHelper *Helper, const PrintingPolicy &Policy,
			unsigned Indentation = 0) const;

	/// viewAST - Visualize an AST rooted at this Stmt* using GraphViz.  Only
	///   works on systems with GraphViz (Mac OS X) or dot+gv installed.
	void viewAST() const;  	// in StmtViz.cpp

	// Implement isa<T> support.
	static bool classof(const Stmt *) {
		return true;
	}

	/// hasImplicitControlFlow - Some statements (e.g. short circuited operations)
	///  contain implicit control-flow in the order their subexpressions
	///  are evaluated.  This predicate returns true if this statement has
	///  such implicit control-flow.  Such statements are also specially handled
	///  within CFGs.
	bool hasImplicitControlFlow() const;

	/// Child Iterators: All subclasses must implement child_begin and child_end
	///  to permit easy iteration over the substatements/subexpessions of an
	///  AST node.  This permits easy iteration over all nodes in the AST.
	typedef StmtIterator child_iterator;
	typedef ConstStmtIterator const_child_iterator;

	typedef StmtRange          child_range;
	typedef ConstStmtRange     const_child_range;

	child_range children();
	const_child_range children() const {
		return const_cast<Stmt*>(this)->children();
	}

	child_iterator child_begin() { return children().first; }
	child_iterator child_end() { return children().second; }

	const_child_iterator child_begin() const { return children().first; }
	const_child_iterator child_end() const { return children().second; }

	/// \brief Produce a unique representation of the given statement.
	///
	/// \brief ID once the profiling operation is complete, will contain
	/// the unique representation of the given statement.
	///
	/// \brief Context the AST context in which the statement resides
	void Profile(llvm::FoldingSetNodeID &ID, ASTContext &Context);
};

//===----------------------------------------------------------------------===//
// Cmd - Base class for command AST nodes, just same as Stmt, but implemented
// here for clear logical hierarchy.
//===----------------------------------------------------------------------===//
class Cmd : public Stmt {
public:
	Cmd(StmtClass SC) :Stmt(SC) { }
protected:
	Cmd(StmtClass SC, EmptyShell ES) : Stmt(SC, ES) {}
};

} // end namespace mlang

#endif /* MLANG_AST_STMT_H_ */
