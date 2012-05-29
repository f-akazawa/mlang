//===--- DefnContext.cpp - Definition Context -------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file implements DefnContext.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/DefnContext.h"
#include "mlang/AST/DefnContextInternals.h"
#include "mlang/AST/ASTMutationListener.h"
#include "mlang/AST/ExternalASTSource.h"
#include <cstdio>

using namespace mlang;

//===----------------------------------------------------------------------===//
// DefnContext Implementation
//===----------------------------------------------------------------------===//

const char *DefnContext::getDefnKindName() const {
  switch (DefnKind) {
  default: assert(0 && "Definition context not in DefnNodes.inc!");
#define DEFN(DERIVED, BASE) case Defn::DERIVED: return #DERIVED;
#define ABSTRACT_DEFN(DEFN)
#include "mlang/AST/DefnNodes.inc"
  }
}

bool DefnContext::classof(const Defn *D) {
  switch (D->getKind()) {
#define DEFN(NAME, BASE)
#define DEFN_CONTEXT(NAME) case Defn::NAME:
#define DEFN_CONTEXT_BASE(NAME)
#include "mlang/AST/DefnNodes.inc"
      return true;
    default:
#define DEFN(NAME, BASE)
#define DEFN_CONTEXT_BASE(NAME)                 \
      if (D->getKind() >= Defn::first##NAME &&  \
          D->getKind() <= Defn::last##NAME)     \
        return true;
#include "mlang/AST/DefnNodes.inc"
      return false;
  }
}

DefnContext::~DefnContext() { }

std::pair<Defn *, Defn *>
DefnContext::BuildDefnChain(const llvm::SmallVectorImpl<Defn*> &Defns) {
  // Build up a chain of declarations via the Decl::NextDeclInContext field.
  Defn *FirstNewDefn = 0;
  Defn *PrevDefn = 0;
  for (unsigned I = 0, N = Defns.size(); I != N; ++I) {
    Defn *D = Defns[I];
    if (PrevDefn)
      PrevDefn->NextDefnInContext = D;
    else
      FirstNewDefn = D;

    PrevDefn = D;
  }

  return std::make_pair(FirstNewDefn, PrevDefn);
}

/// \brief Load the definitions within this lexical storage from an
/// external source.
void DefnContext::LoadDefnsFromExternalStorage() const {
  ExternalASTSource *Source = getParentASTContext().getExternalSource();
  assert(hasExternalLexicalStorage() && Source && "No external storage?");

  // Notify that we have a DefnContext that is initializing.
  ExternalASTSource::Deserializing ADefnContext(Source);

  llvm::SmallVector<Defn*, 64> Defns;
//  if (Source->FindExternalLexicalDefns(this, Defns))
//    return;

  // There is no longer any lexical storage in this context
  ExternalLexicalStorage = false;

  if (Defns.empty())
    return;

  // We may have already loaded just the fields of this record, in which case
  // don't add the decls, just replace the FirstDefn/LastDefn chain.
//  if (const RecordDefn *RD = dyn_cast<RecordDefn>(this))
//    if (RD->LoadedFieldsFromExternalStorage) {
//      llvm::tie(FirstDefn, LastDefn) = BuildDefnChain(Defns);
//      return;
//    }

  // Splice the newly-read declarations into the beginning of the list
  // of declarations.
  Defn *ExternalFirst, *ExternalLast;
  llvm::tie(ExternalFirst, ExternalLast) = BuildDefnChain(Defns);
  ExternalLast->NextDefnInContext = FirstDefn;
  FirstDefn = ExternalFirst;
  if (!LastDefn)
    LastDefn = ExternalLast;
}

DefnContext::lookup_result
ExternalASTSource::SetNoExternalVisibleDefnsForName(const DefnContext *DC,
                                                    DefinitionName Name) {
  ASTContext &Context = DC->getParentASTContext();
  StoredDefnsMap *Map;
  if (!(Map = DC->LookupPtr))
    Map = DC->CreateStoredDefnsMap(Context);

  StoredDefnsList &List = (*Map)[Name];
  assert(List.isNull());
  (void) List;

  return DefnContext::lookup_result();
}

DefnContext::lookup_result
ExternalASTSource::SetExternalVisibleDefnsForName(const DefnContext *DC,
                                                  DefinitionName Name,
                                    llvm::SmallVectorImpl<NamedDefn*> &Defns) {
  ASTContext &Context = DC->getParentASTContext();;

  StoredDefnsMap *Map;
  if (!(Map = DC->LookupPtr))
    Map = DC->CreateStoredDefnsMap(Context);

  StoredDefnsList &List = (*Map)[Name];
  for (unsigned I = 0, N = Defns.size(); I != N; ++I) {
    if (List.isNull())
      List.setOnlyValue(Defns[I]);
    else
      List.AddSubsequentDefn(Defns[I]);
  }

  return List.getLookupResult();
}

void ExternalASTSource::MaterializeVisibleDefnsForName(const DefnContext *DC,
                                                       DefinitionName Name,
                                     llvm::SmallVectorImpl<NamedDefn*> &Defns) {
  assert(DC->LookupPtr);
  StoredDefnsMap &Map = *DC->LookupPtr;

  // If there's an entry in the table the visible decls for this name have
  // already been deserialized.
  if (Map.find(Name) == Map.end()) {
    StoredDefnsList &List = Map[Name];
    for (unsigned I = 0, N = Defns.size(); I != N; ++I) {
      if (List.isNull())
        List.setOnlyValue(Defns[I]);
      else
        List.AddSubsequentDefn(Defns[I]);
    }
  }
}

DefnContext::defn_iterator DefnContext::noload_defns_begin() const {
  return defn_iterator(FirstDefn);
}

DefnContext::defn_iterator DefnContext::noload_defns_end() const {
  return defn_iterator();
}

DefnContext::defn_iterator DefnContext::defns_begin() const {
  if (hasExternalLexicalStorage())
    LoadDefnsFromExternalStorage();

  // FIXME: Check whether we need to load some declarations from
  // external storage.
  return defn_iterator(FirstDefn);
}

DefnContext::defn_iterator DefnContext::defns_end() const {
  if (hasExternalLexicalStorage())
    LoadDefnsFromExternalStorage();

  return defn_iterator();
}

bool DefnContext::defns_empty() const {
  if (hasExternalLexicalStorage())
    LoadDefnsFromExternalStorage();

  return !FirstDefn;
}

void DefnContext::removeDefn(Defn *D) {
  assert((D->NextDefnInContext || D == LastDefn) &&
         "defn is not in defns list");

  // Remove D from the decl chain.  This is O(n) but hopefully rare.
  if (D == FirstDefn) {
    if (D == LastDefn)
      FirstDefn = LastDefn = 0;
    else
      FirstDefn = D->NextDefnInContext;
  } else {
    for (Defn *I = FirstDefn; true; I = I->NextDefnInContext) {
      assert(I && "decl not found in linked list");
      if (I->NextDefnInContext == D) {
        I->NextDefnInContext = D->NextDefnInContext;
        if (D == LastDefn) LastDefn = I;
        break;
      }
    }
  }

  // Mark that D is no longer in the decl chain.
  D->NextDefnInContext = 0;

  // Remove D from the lookup table if necessary.
  if (isa<NamedDefn>(D)) {
    NamedDefn *ND = cast<NamedDefn>(D);

    StoredDefnsMap *Map = LookupPtr;
    if (!Map) return;

    StoredDefnsMap::iterator Pos = Map->find(ND->getDefnName());
    assert(Pos != Map->end() && "no lookup entry for decl");
    Pos->second.remove(ND);
  }
}

void DefnContext::addHiddenDefn(Defn *D) {
  assert(!D->getNextDefnInContext() && D != LastDefn &&
         "Defn already inserted into a DefnContext");

  if (FirstDefn) {
    LastDefn->NextDefnInContext = D;
    LastDefn = D;
  } else {
    FirstDefn = LastDefn = D;
  }

  // Notify a C++ record declaration that we've added a member, so it can
  // update it's class-specific state.
//  if (UserClassDefn *Record = dyn_cast<UserClassDefn>(this))
//    Record->addedMember(D);
}

void DefnContext::addDefn(Defn *D) {
  addHiddenDefn(D);

  if (NamedDefn *ND = dyn_cast<NamedDefn>(D))
    ND->getDefnContext()->makeDefnVisibleInContext(ND);
}

DefnContext::lookup_result
DefnContext::lookup(DefinitionName Name) {
  if (hasExternalVisibleStorage()) {
    // Check to see if we've already cached the lookup results.
    if (LookupPtr) {
      StoredDefnsMap::iterator I = LookupPtr->find(Name);
      if (I != LookupPtr->end())
        return I->second.getLookupResult();
    }

    ExternalASTSource *Source = getParentASTContext().getExternalSource();
    return Source->FindExternalVisibleDefnsByName(this, Name);
  }

  StoredDefnsMap::iterator Pos = LookupPtr->find(Name);
  if (Pos == LookupPtr->end())
    return lookup_result(lookup_iterator(0), lookup_iterator(0));
  return Pos->second.getLookupResult();
}

DefnContext::lookup_const_result
DefnContext::lookup(DefinitionName Name) const {
  return const_cast<DefnContext*>(this)->lookup(Name);
}

void DefnContext::makeDefnVisibleInContext(NamedDefn *D, bool Recoverable) {
  // If we already have a lookup data structure, perform the insertion
  // into it. If we haven't deserialized externally stored decls, deserialize
  // them so we can add the decl. Otherwise, be lazy and don't build that
  // structure until someone asks for it.
  if (LookupPtr || !Recoverable || hasExternalVisibleStorage())
    makeDefnVisibleInContextImpl(D);

  Defn *DCAsDefn = cast<Defn>(this);
  // Notify that a defn was made visible unless it's a Tag being defined.
  if (!(isa<TypeDefn>(DCAsDefn) &&
		  cast<TypeDefn>(DCAsDefn)->isBeingDefined()))
	  if (ASTMutationListener *L = DCAsDefn->getASTMutationListener())
		  L->AddedVisibleDefn(this, D);
}

bool DefnContext::Encloses(const DefnContext *DC) const {
	// TODO yabin
	return false;
}

void DefnContext::makeDefnVisibleInContextImpl(NamedDefn *D) {
  // Skip unnamed definitions.
  if (!D->getDefnName())
    return;

  ASTContext *C = 0;
  if (!LookupPtr) {
    C = &getParentASTContext();
    CreateStoredDefnsMap(*C);
  }

  // If there is an external AST source, load any declarations it knows about
  // with this declaration's name.
  // If the lookup table contains an entry about this name it means that we
  // have already checked the external source.
  if (ExternalASTSource *Source = getParentASTContext().getExternalSource())
    if (hasExternalVisibleStorage() &&
        LookupPtr->find(D->getDefnName()) == LookupPtr->end())
      Source->FindExternalVisibleDefnsByName(this, D->getDefnName());

  // Insert this declaration into the map.
  StoredDefnsList &DefnNameEntries = (*LookupPtr)[D->getDefnName()];
  if (DefnNameEntries.isNull()) {
    DefnNameEntries.setOnlyValue(D);
    return;
  }

  // If it is possible that this is a redefinition, check to see if there is
  // already a decl for which declarationReplaces returns true.  If there is
  // one, just replace it and return.
  if (DefnNameEntries.HandleRedefinition(D))
    return;

  // Put this declaration into the appropriate slot.
  DefnNameEntries.AddSubsequentDefn(D);
}

void DefnContext::MaterializeVisibleDefnsFromExternalStorage() {
  ExternalASTSource *Source = getParentASTContext().getExternalSource();
  assert(hasExternalVisibleStorage() && Source && "No external storage?");

  if (!LookupPtr)
    CreateStoredDefnsMap(getParentASTContext());
  Source->MaterializeVisibleDefns(this);
}

//===----------------------------------------------------------------------===//
// Creation and Destruction of StoredDefnsMaps.                               //
//===----------------------------------------------------------------------===//

StoredDefnsMap *DefnContext::CreateStoredDefnsMap(ASTContext &C) const {
  assert(!LookupPtr && "context already has a defns map");

  StoredDefnsMap *M = new StoredDefnsMap();
  M->Previous = C.LastSDM;
  C.LastSDM = M;
  LookupPtr = M;
  return M;
}

void ASTContext::ReleaseDefnContextMaps() {
  // It's okay to delete DependentStoredDefnsMaps via a StoredDefnsMap
  // pointer because the subclass doesn't add anything that needs to
  // be deleted.
  StoredDefnsMap::DestroyAll(LastSDM);
}

void StoredDefnsMap::DestroyAll(StoredDefnsMap *Map) {
  while (Map) {
    // Advance the iteration before we invalidate memory.
    StoredDefnsMap* Next = Map->Previous;

    delete Map;

    Map = Next;
  }
}
