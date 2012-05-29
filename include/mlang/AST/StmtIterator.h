//===--- StmtIterator.h - Iterators for Statements ------------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the StmtIterator and ConstStmtIterator classes.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_STMT_ITERATOR_H_
#define MLANG_AST_STMT_ITERATOR_H_

#include "llvm/Support/DataTypes.h"
#include <cassert>
#include <cstddef>
#include <iterator>

namespace mlang {
class Defn;
class Stmt;

class StmtIteratorBase {
protected:
	enum { DefnMode = 0x1, DefnGroupMode = 0x2, Flags = 0x2 };

  Stmt **stmt;

  union { Defn *defn; Defn **DGI; };
  uintptr_t RawPtr;
  Defn **DGE;

  bool inDefn() const {
	  return (RawPtr & Flags) == DefnMode;
    }

  bool inDefnGroup() const {
	  return (RawPtr & Flags) == DefnGroupMode;
    }

  bool inStmt() const {
      return (RawPtr & Flags) == 0;
    }

  void * getRawPtr()const { return reinterpret_cast<void *>(RawPtr); }

  void NextDefn(bool ImmediateAdvance = true);
  bool HandleDefn(Defn* D);

  Stmt*& GetDefnExpr() const;

  StmtIteratorBase(Stmt **s) : stmt(s), defn(0), RawPtr(0) {}
  StmtIteratorBase(Defn *d, Stmt **s);
  StmtIteratorBase(Defn **dgi, Defn **dge);
  StmtIteratorBase() : stmt(0), defn(0), RawPtr(0) {}
};


template <typename DERIVED, typename REFERENCE>
class StmtIteratorImpl : public StmtIteratorBase,
                         public std::iterator<std::forward_iterator_tag,
                                              REFERENCE, ptrdiff_t,
                                              REFERENCE, REFERENCE> {
protected:
  StmtIteratorImpl(const StmtIteratorBase& RHS) : StmtIteratorBase(RHS) {}
public:
  StmtIteratorImpl() {}
  StmtIteratorImpl(Stmt **s) : StmtIteratorBase(s) {}
  StmtIteratorImpl(Defn **dgi, Defn **dge) : StmtIteratorBase(dgi, dge) {}
  StmtIteratorImpl(Defn *d, Stmt **s) : StmtIteratorBase(d, s) {}

  DERIVED& operator++() {
	  if (inDefn() || inDefnGroup()) {
		  NextDefn();
	  } else {
		  ++stmt;
	  }

	  return static_cast<DERIVED&>(*this);
  }

  DERIVED operator++(int) {
    DERIVED tmp = static_cast<DERIVED&>(*this);
    operator++();
    return tmp;
  }

  bool operator==(const DERIVED& RHS) const {
	  return stmt == RHS.stmt && defn == RHS.defn && RawPtr == RHS.RawPtr;
  }

  bool operator!=(const DERIVED& RHS) const {
	  return stmt != RHS.stmt || defn != RHS.defn || RawPtr != RHS.RawPtr;
  }

  REFERENCE operator*() const {
	  return (REFERENCE) (inStmt() ? *stmt : GetDefnExpr());
  }

  REFERENCE operator->() const { return operator*(); }
};

struct StmtIterator : public StmtIteratorImpl<StmtIterator,Stmt*&> {
  explicit StmtIterator() : StmtIteratorImpl<StmtIterator,Stmt*&>() {}

  StmtIterator(Stmt** S) : StmtIteratorImpl<StmtIterator,Stmt*&>(S) {}

  StmtIterator(Defn** dgi, Defn** dge)
     : StmtIteratorImpl<StmtIterator,Stmt*&>(dgi, dge) {}

  StmtIterator(Defn* D, Stmt **s = 0)
      : StmtIteratorImpl<StmtIterator,Stmt*&>(D, s) {}
};

struct ConstStmtIterator : public StmtIteratorImpl<ConstStmtIterator,
                                                   const Stmt*> {
  explicit ConstStmtIterator() :
    StmtIteratorImpl<ConstStmtIterator,const Stmt*>() {}

  ConstStmtIterator(const StmtIterator& RHS) :
    StmtIteratorImpl<ConstStmtIterator,const Stmt*>(RHS) {}
};

/// A range of statement iterators.
///
/// This class provides some extra functionality beyond std::pair
/// in order to allow the following idiom:
///   for (StmtRange range = stmt->children(); range; ++range)
struct StmtRange: std::pair<StmtIterator, StmtIterator> {
	StmtRange() {
	}
	StmtRange(const StmtIterator &begin, const StmtIterator &end) :
		std::pair<StmtIterator, StmtIterator>(begin, end) {
	}

	bool empty() const {
		return first == second;
	}
	operator bool() const {
		return !empty();
	}

	Stmt *operator->() const {
		return first.operator->();
	}
	Stmt *&operator*() const {
		return first.operator*();
	}

	StmtRange &operator++() {
		assert(!empty() && "incrementing on empty range");
		++first;
		return *this;
	}

	StmtRange operator++(int) {
		assert(!empty() && "incrementing on empty range");
		StmtRange copy = *this;
		++first;
		return copy;
	}

	friend const StmtIterator &begin(const StmtRange &range) {
		return range.first;
	}
	friend const StmtIterator &end(const StmtRange &range) {
		return range.second;
	}
};

/// A range of const statement iterators.
///
/// This class provides some extra functionality beyond std::pair
/// in order to allow the following idiom:
///   for (ConstStmtRange range = stmt->children(); range; ++range)
struct ConstStmtRange: std::pair<ConstStmtIterator, ConstStmtIterator> {
	ConstStmtRange() {
	}
	ConstStmtRange(const ConstStmtIterator &begin, const ConstStmtIterator &end) :
		std::pair<ConstStmtIterator, ConstStmtIterator>(begin, end) {
	}
	ConstStmtRange(const StmtRange &range) :
		std::pair<ConstStmtIterator, ConstStmtIterator>(range.first,
				range.second) {
	}
	ConstStmtRange(const StmtIterator &begin, const StmtIterator &end) :
		std::pair<ConstStmtIterator, ConstStmtIterator>(begin, end) {
	}

	bool empty() const {
		return first == second;
	}
	operator bool() const {
		return !empty();
	}

	const Stmt *operator->() const {
		return first.operator->();
	}
	const Stmt *operator*() const {
		return first.operator*();
	}

	ConstStmtRange &operator++() {
		assert(!empty() && "incrementing on empty range");
		++first;
		return *this;
	}

	ConstStmtRange operator++(int) {
		assert(!empty() && "incrementing on empty range");
		ConstStmtRange copy = *this;
		++first;
		return copy;
	}

	friend const ConstStmtIterator &begin(const ConstStmtRange &range) {
		return range.first;
	}
	friend const ConstStmtIterator &end(const ConstStmtRange &range) {
		return range.second;
	}
};

} // end namespace mlang

#endif /* MLANG_AST_STMT_ITERATOR_H_ */
