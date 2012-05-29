//===-- DefnContextInternals.h - DefnContext Representation -----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the data structures used in the implementation
//  of DefnContext.
//
//===----------------------------------------------------------------------===//
#ifndef MLANG_AST_DEFNCONTEXTINTERNALS_H_
#define MLANG_AST_DEFNCONTEXTINTERNALS_H_

#include "mlang/AST/Defn.h"
#include "mlang/AST/DefinitionName.h"
#include "mlang/AST/DefnSub.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/SmallVector.h"
#include <algorithm>

namespace mlang {

/// StoredDefnsList - This is an array of decls optimized a common case of only
/// containing one entry.
struct StoredDefnsList {

  /// DefnsTy - When in vector form, this is what the Data pointer points to.
  typedef llvm::SmallVector<NamedDefn *, 4> DefnsTy;

  /// \brief The stored data, which will be either a pointer to a NamedDefn,
  /// or a pointer to a vector.
  llvm::PointerUnion<NamedDefn *, DefnsTy *> Data;

public:
  StoredDefnsList() {}

  StoredDefnsList(const StoredDefnsList &RHS) : Data(RHS.Data) {
    if (DefnsTy *RHSVec = RHS.getAsVector())
      Data = new DefnsTy(*RHSVec);
  }

  ~StoredDefnsList() {
    // If this is a vector-form, free the vector.
    if (DefnsTy *Vector = getAsVector())
      delete Vector;
  }

  StoredDefnsList &operator=(const StoredDefnsList &RHS) {
    if (DefnsTy *Vector = getAsVector())
      delete Vector;
    Data = RHS.Data;
    if (DefnsTy *RHSVec = RHS.getAsVector())
      Data = new DefnsTy(*RHSVec);
    return *this;
  }

  bool isNull() const { return Data.isNull(); }

  NamedDefn *getAsDefn() const {
    return Data.dyn_cast<NamedDefn *>();
  }

  DefnsTy *getAsVector() const {
    return Data.dyn_cast<DefnsTy *>();
  }

  void setOnlyValue(NamedDefn *ND) {
    assert(!getAsVector() && "Not inline");
    Data = ND;
    // Make sure that Data is a plain NamedDefn* so we can use its address
    // at getLookupResult.
    assert(*(NamedDefn **)&Data == ND &&
           "PointerUnion mangles the NamedDefn pointer!");
  }

  void remove(NamedDefn *D) {
    assert(!isNull() && "removing from empty list");
    if (NamedDefn *Singleton = getAsDefn()) {
      assert(Singleton == D && "list is different singleton");
      (void)Singleton;
      Data = (NamedDefn *)0;
      return;
    }

    DefnsTy &Vec = *getAsVector();
    DefnsTy::iterator I = std::find(Vec.begin(), Vec.end(), D);
    assert(I != Vec.end() && "list does not contain decl");
    Vec.erase(I);

    assert(std::find(Vec.begin(), Vec.end(), D)
             == Vec.end() && "list still contains decl");
  }

  /// getLookupResult - Return an array of all the defns that this list
  /// represents.
  DefnContext::lookup_result getLookupResult() {
    if (isNull())
      return DefnContext::lookup_result(DefnContext::lookup_iterator(0),
                                        DefnContext::lookup_iterator(0));

    // If we have a single NamedDefn, return it.
    if (getAsDefn()) {
      assert(!isNull() && "Empty list isn't allowed");

      // Data is a raw pointer to a NamedDefn*, return it.
      void *Ptr = &Data;
      return DefnContext::lookup_result((NamedDefn**)Ptr, (NamedDefn**)Ptr+1);
    }

    assert(getAsVector() && "Must have a vector at this point");
    DefnsTy &Vector = *getAsVector();

    // Otherwise, we have a range result.
    return DefnContext::lookup_result(&Vector[0], &Vector[0]+Vector.size());
  }

  /// HandleRedefinition - If this is a redefinition of an existing defn,
  /// replace the old one with D and return true.  Otherwise return false.
  bool HandleRedefinition(NamedDefn *D) {
    // Most defns only have one entry in their list, special case it.
    if (NamedDefn *OldD = getAsDefn()) {
      if (!D->definitionReplaces(OldD))
        return false;
      setOnlyValue(D);
      return true;
    }

    // Determine if this definition is actually a redefinition.
    DefnsTy &Vec = *getAsVector();
    for (DefnsTy::iterator OD = Vec.begin(), ODEnd = Vec.end();
         OD != ODEnd; ++OD) {
      NamedDefn *OldD = *OD;
      if (D->definitionReplaces(OldD)) {
    	  *OD = D;
    	  return true;
      }
    }

    return false;
  }

  /// AddSubsequentDefn - This is called on the second and later defn when it is
  /// not a redefinition to merge it into the appropriate place in our list.
  ///
  void AddSubsequentDefn(NamedDefn *D) {
    // If this is the second defn added to the list, convert this to vector
    // form.
    if (NamedDefn *OldD = getAsDefn()) {
      DefnsTy *VT = new DefnsTy();
      VT->push_back(OldD);
      Data = VT;
    }

    DefnsTy &Vec = *getAsVector();

    Vec.push_back(D);
  }
};

class StoredDefnsMap
  : public llvm::DenseMap<DefinitionName, StoredDefnsList> {

public:
  static void DestroyAll(StoredDefnsMap *Map);

private:
  friend class ASTContext; // walks the chain deleting these
  friend class DefnContext;
  StoredDefnsMap* Previous;
};

} // end namespace mlang

#endif /* MLANG_AST_DEFNCONTEXTINTERNALS_H_ */
