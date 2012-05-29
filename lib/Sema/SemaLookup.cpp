//===--------------------- SemaLookup.cpp - Name Lookup  ------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements name lookup for mlang.
//
//===----------------------------------------------------------------------===//
#include "mlang/Sema/Sema.h"
#include "mlang/Sema/Lookup.h"
#include "mlang/Sema/Scope.h"
#include "mlang/Sema/ScopeInfo.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnContext.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/DefnSub.h"
#include "mlang/Basic/Builtins.h"
#include "mlang/Basic/LangOptions.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/ErrorHandling.h"
#include <limits>
#include <list>
#include <set>
#include <vector>
#include <iterator>
#include <utility>
#include <algorithm>

using namespace mlang;
using namespace sema;


//===----------------------------------------------------------------------===//
// File scope data structures and functions
//===----------------------------------------------------------------------===//
static inline unsigned getIDNS(Sema::LookupNameKind NameKind) {
  unsigned IDNS = 0;
  switch (NameKind) {
  case Sema::LookupOrdinaryName:
    IDNS = Defn::IDNS_Ordinary | Defn::IDNS_Script;
    break;

  case Sema::LookupTypeName:
	  IDNS = Defn::IDNS_Type | Defn::IDNS_Script;
    break;

  case Sema::LookupMemberName:
    IDNS = Defn::IDNS_Member | Defn::IDNS_Script;
    break;

  case Sema::LookupNestedNameSpecifierName:
    IDNS = Defn::IDNS_Type | Defn::IDNS_Script;
    break;

  case Sema::LookupExternalPackageName:
    IDNS = Defn::IDNS_Package;
    break;

  case Sema::LookupAnyName:
    IDNS = Defn::IDNS_Ordinary | Defn::IDNS_Type | Defn::IDNS_Member |
           Defn::IDNS_Script;
    break;
  }
  return IDNS;
}

/// LookupBuiltin - Lookup for built-in functions
static bool LookupBuiltin(Sema &S, LookupResult &R) {
	Sema::LookupNameKind NameKind = R.getLookupKind();

	// If we didn't find a use of this identifier, and if the identifier
	// corresponds to a compiler builtin, create the defn object for the builtin
	// now, injecting it into system scope, and return it.
	if (NameKind == Sema::LookupOrdinaryName) {
		IdentifierInfo *II = R.getLookupName().getAsIdentifierInfo();
		if (II) {
			// If this is a builtin on this (or all) targets, create the defn.
			if (unsigned BuiltinID = II->getBuiltinID()) {
				if (NamedDefn *D = S.LazilyCreateBuiltin((IdentifierInfo *)II,
	                                                BuiltinID, S.BaseWorkspace,
	                                                /*R.isForRedeclaration()*/false,
	                                                R.getNameLoc())) {
					R.addDefn(D);
					return true;
				}

				//FIXME yabin
				// should i deal with this situation in gmat?
//				if (R.isForRedeclaration()) {
//					// If we're redeclaring this function anyway, forget that
//					// this was a builtin at all.
//					S.Context.BuiltinInfo.ForgetBuiltin(BuiltinID,
//							S.Context.Idents);
//				}

				return false;
			}
		}
	}

	return false;
}

//===----------------------------------------------------------------------===//
// IdDefnInfoMap class
//===----------------------------------------------------------------------===//

/// IdDefnInfoMap - Associates IdDefnInfos with declaration names.
/// Allocates 'pools' (vectors of IdDefnInfos) to avoid allocating each
/// individual IdDefnInfo to heap.
class IdentifierResolver::IdDefnInfoMap {
  static const unsigned int POOL_SIZE = 512;

  /// We use our own linked-list implementation because it is sadly
  /// impossible to add something to a pre-C++0x STL container without
  /// a completely unnecessary copy.
  struct IdDefnInfoPool {
    IdDefnInfoPool(IdDefnInfoPool *Next) : Next(Next) {}

    IdDefnInfoPool *Next;
    IdDefnInfo Pool[POOL_SIZE];
  };

  IdDefnInfoPool *CurPool;
  unsigned int CurIndex;

public:
  IdDefnInfoMap() : CurPool(0), CurIndex(POOL_SIZE) {}

  ~IdDefnInfoMap() {
    IdDefnInfoPool *Cur = CurPool;
    while (IdDefnInfoPool *P = Cur) {
      Cur = Cur->Next;
      delete P;
    }
  }

  /// Returns the IdDefnInfo associated to the DefinitionName.
  /// It creates a new IdDefnInfo if one was not created before for this id.
  IdDefnInfo &operator[](DefinitionName Name);
};


//===----------------------------------------------------------------------===//
// IdDefnInfo Implementation
//===----------------------------------------------------------------------===//

/// RemoveDefn - Remove the defn from the scope chain.
/// The defn must already be part of the defn chain.
void IdentifierResolver::IdDefnInfo::RemoveDefn(NamedDefn *D) {
  for (DefnsTy::iterator I = Defns.end(); I != Defns.begin(); --I) {
    if (D == *(I-1)) {
      Defns.erase(I-1);
      return;
    }
  }

  assert(0 && "Didn't find this defn on its identifier's chain!");
}

bool
IdentifierResolver::IdDefnInfo::ReplaceDefn(NamedDefn *Old, NamedDefn *New) {
  for (DefnsTy::iterator I = Defns.end(); I != Defns.begin(); --I) {
    if (Old == *(I-1)) {
      *(I - 1) = New;
      return true;
    }
  }

  return false;
}


//===----------------------------------------------------------------------===//
// IdentifierResolver Implementation
//===----------------------------------------------------------------------===//

IdentifierResolver::IdentifierResolver(const LangOptions &langOpt)
    : LangOpt(langOpt), IdDefnInfos(new IdDefnInfoMap) {
}
IdentifierResolver::~IdentifierResolver() {
  delete IdDefnInfos;
}

/// isDefnInScope - If 'Ctx' is a function/method, isDefnInScope returns true
/// if 'D' is in Scope 'S', otherwise 'S' is ignored and isDefnInScope returns
/// true if 'D' belongs to the given declaration context.
bool IdentifierResolver::isDefnInScope(Defn *D, DefnContext *Ctx,
                                       ASTContext &Context, Scope *S) const {
  //Ctx = Ctx->getRedefnContext();

  if (Ctx->isFunctionOrMethod()) {
    // Ignore the scopes associated within transparent declaration contexts.
    while (S->getEntity() /*&&
           ((DefnContext *)S->getEntity())->isTransparentContext()*/)
      S = S->getParent();

    if (S->isDefnScope(D))
      return true;
    if (LangOpt.OOP) {
      // C++ 3.3.2p3:
      // The name declared in a catch exception-declaration is local to the
      // handler and shall not be redefined in the outermost block of the
      // handler.
      // C++ 3.3.2p4:
      // Names declared in the for-init-statement, and in the condition of if,
      // while, for, and switch statements are local to the if, while, for, or
      // switch statement (including the controlled statement), and shall not be
      // redefined in a subsequent condition of that statement nor in the
      // outermost block (or, for the if statement, any of the outermost blocks)
      // of the controlled statement.
      //
      assert(S->getParent() && "No TUScope?");
      if (S->getParent()->getFlags() & Scope::FnScope)
        return S->getParent()->isDefnScope(D);
    }
    return false;
  }

  // return D->getDefnContext()->Equals(Ctx);
  return false;
}

/// AddDefn - Link the defn to its shadowed defn chain.
void IdentifierResolver::AddDefn(NamedDefn *D) {
  DefinitionName Name = D->getDefnName();
  if (IdentifierInfo *II = Name.getAsIdentifierInfo())
    II->setIsFromAST(false);

  void *Ptr = Name.getFETokenInfo<void>();

  if (!Ptr) {
    Name.setFETokenInfo(D);
    return;
  }

  IdDefnInfo *IDI;

  if (isDefnPtr(Ptr)) {
    Name.setFETokenInfo(NULL);
    IDI = &(*IdDefnInfos)[Name];
    NamedDefn *PrevD = static_cast<NamedDefn*>(Ptr);
    IDI->AddDefn(PrevD);
  } else
    IDI = toIdDefnInfo(Ptr);

  IDI->AddDefn(D);
}

/// RemoveDefn - Unlink the defn from its shadowed defn chain.
/// The defn must already be part of the defn chain.
void IdentifierResolver::RemoveDefn(NamedDefn *D) {
  assert(D && "null param passed");
  DefinitionName Name = D->getDefnName();
  if (IdentifierInfo *II = Name.getAsIdentifierInfo())
    II->setIsFromAST(false);

  void *Ptr = Name.getFETokenInfo<void>();

  assert(Ptr && "Didn't find this defn on its identifier's chain!");

  if (isDefnPtr(Ptr)) {
    assert(D == Ptr && "Didn't find this defn on its identifier's chain!");
    Name.setFETokenInfo(NULL);
    return;
  }

  return toIdDefnInfo(Ptr)->RemoveDefn(D);
}

bool IdentifierResolver::ReplaceDefn(NamedDefn *Old, NamedDefn *New) {
  assert(Old->getDefnName() == New->getDefnName() &&
			"Cannot replace a defn with another  of a different name");

	DefinitionName Name = Old->getDefnName();
	if (IdentifierInfo *II = Name.getAsIdentifierInfo())
		II->setIsFromAST(false);

	void *Ptr = Name.getFETokenInfo<void> ();

	if (!Ptr)
		return false;

	if (isDefnPtr(Ptr)) {
		if (Ptr == Old) {
			Name.setFETokenInfo(New);
			return true;
		}
		return false;
	}

	return toIdDefnInfo(Ptr)->ReplaceDefn(Old, New);
}

/// begin - Returns an iterator for defns with name 'Name'.
IdentifierResolver::iterator
IdentifierResolver::begin(DefinitionName Name) {
  void *Ptr = Name.getFETokenInfo<void>();
  if (!Ptr) return end();

  if (isDefnPtr(Ptr))
    return iterator(static_cast<NamedDefn*>(Ptr));

  IdDefnInfo *IDI = toIdDefnInfo(Ptr);

  IdDefnInfo::DefnsTy::iterator I = IDI->defns_end();
  if (I != IDI->defns_begin())
    return iterator(I-1);
  // No decls found.
  return end();
}

void IdentifierResolver::AddDefnToIdentifierChain(IdentifierInfo *II,
                                                  NamedDefn *D) {
  II->setIsFromAST(false);
  void *Ptr = II->getFETokenInfo<void>();

  if (!Ptr) {
    II->setFETokenInfo(D);
    return;
  }

  IdDefnInfo *IDI;

  if (isDefnPtr(Ptr)) {
    II->setFETokenInfo(NULL);
    IDI = &(*IdDefnInfos)[II];
    NamedDefn *PrevD = static_cast<NamedDefn*>(Ptr);
    IDI->AddDefn(PrevD);
  } else
    IDI = toIdDefnInfo(Ptr);

  IDI->AddDefn(D);
}

//===----------------------------------------------------------------------===//
// IdDefnInfoMap Implementation
//===----------------------------------------------------------------------===//

/// Returns the IdDefnInfo associated to the DefinitionName.
/// It creates a new IdDefnInfo if one was not created before for this id.
IdentifierResolver::IdDefnInfo &
IdentifierResolver::IdDefnInfoMap::operator[](DefinitionName Name) {
  void *Ptr = Name.getFETokenInfo<void>();

  if (Ptr) return *toIdDefnInfo(Ptr);

  if (CurIndex == POOL_SIZE) {
    CurPool = new IdDefnInfoPool(CurPool);
    CurIndex = 0;
  }
  IdDefnInfo *IDI = &CurPool->Pool[CurIndex];
  Name.setFETokenInfo(reinterpret_cast<void*>(
                              reinterpret_cast<uintptr_t>(IDI) | 0x1)
                                                                     );
  ++CurIndex;
  return *IDI;
}

void IdentifierResolver::iterator::incrementSlowCase() {
  NamedDefn *D = **this;
  void *InfoPtr = D->getDefnName().getFETokenInfo<void>();
  assert(!isDefnPtr(InfoPtr) && "Defn with wrong id ?");
  IdDefnInfo *Info = toIdDefnInfo(InfoPtr);

  BaseIter I = getIterator();
  if (I != Info->defns_begin())
    *this = iterator(I-1);
  else // No more decls.
    *this = iterator();
}

//===----------------------------------------------------------------------===//
// LookupResult
//===----------------------------------------------------------------------===//

void LookupResult::configure() {
	IDNS = getIDNS(LookupKind);
}

void LookupResult::sanity() const {
	assert(ResultKind != NotFound || Defns.size() == 0);
	assert(ResultKind != Found || Defns.size() == 1);
}

void LookupResult::resolveKind() {
	unsigned N = Defns.size();

	// Fast case: no possible ambiguity.
	if (N == 0) {
		assert(ResultKind == NotFound || ResultKind == NotFoundInCurrentInstantiation);
		return;
	}

	// If there's a single , we need to examine it to decide what
	// kind of lookup this is.
	if (N == 1) {
		return;
	}

	// Don't do any extra resolution if we've already resolved as ambiguous.
	if (ResultKind == Ambiguous)
		return;

	llvm::SmallPtrSet<NamedDefn*, 16> Unique;
	llvm::SmallPtrSet<Type, 16> UniqueTypes;

	bool Ambiguous = false;
	bool HasTag = false, HasFunction = false, HasNonFunction = false;
	bool HasFunctionTemplate = false, HasUnresolved = false;

	unsigned UniqueTagIndex = 0;

	unsigned I = 0;
	while (I < N) {
		NamedDefn *D = Defns[I]->getUnderlyingDefn();

		// Redeclarations of types via typedef can occur both within a scope
		// and, through using declarations and directives, across scopes. There is
		// no ambiguity if they all refer to the same type, so unique based on the
		// canonical type.
		if (TypeDefn *TD = dyn_cast<TypeDefn>(D)) {
			if (!TD->getDefnContext()->isRecord()) {
				Type T = SemaRef.Context.getTypeDefnType(TD);
//				if (!UniqueTypes.insert(SemaRef.Context.getType(T))) {
//					// The type is not unique; pull something off the back and continue
//					// at this index.
//					Defns[I] = Defns[--N];
//					continue;
//				}
			}
		}

		if (!Unique.insert(D)) {
			// If it's not unique, pull something off the back (and
			// continue at this index).
			Defns[I] = Defns[--N];
			continue;
		}

		// Otherwise, do some defn type analysis and then continue.

		/*if (isa<UnresolvedUsingValueDefn> (D)) {
			HasUnresolved = true;
		} else*/ if (isa<TypeDefn> (D)) {
			if (HasTag)
				Ambiguous = true;
			UniqueTagIndex = I;
			HasTag = true;
		} else if (isa<FunctionDefn> (D)) {
			HasFunction = true;
		} else {
			if (HasNonFunction)
				Ambiguous = true;
			HasNonFunction = true;
		}
		I++;
	}

	// C++ [basic.scope.hiding]p2:
	//   A class name or enumeration name can be hidden by the name of
	//   an object, function, or enumerator declared in the same
	//   scope. If a class or enumeration name and an object, function,
	//   or enumerator are declared in the same scope (in any order)
	//   with the same name, the class or enumeration name is hidden
	//   wherever the object, function, or enumerator name is visible.
	// But it's still an error if there are distinct tag types found,
	// even if they're not visible. (ref?)
	if (HideTags && HasTag && !Ambiguous && (HasFunction || HasNonFunction
			|| HasUnresolved)) {
//		if (Defns[UniqueTagIndex]->getDefnContext()->getRedefnContext()->Equals(
//				Defns[UniqueTagIndex ? 0 : N - 1]->getDefnContext()->getRedeclContext()))
//			Defns[UniqueTagIndex] = Defns[--N];
//		else
			Ambiguous = true;
	}

	Defns.set_size(N);

	if (HasNonFunction && (HasFunction || HasUnresolved))
		Ambiguous = true;

	if (Ambiguous)
		setAmbiguous(LookupResult::AmbiguousReference);
//	else if (HasUnresolved)
//		ResultKind = LookupResult::FoundUnresolvedValue;
	else if (N > 1 || HasFunctionTemplate)
		ResultKind = LookupResult::FoundOverloaded;
	else
		ResultKind = LookupResult::Found;
}

//===----------------------------------------------------------------------===//
// Sema
//===----------------------------------------------------------------------===//
bool Sema::LookupName(LookupResult &R, Scope *S, bool AllowBuiltinCreation) {
	DefinitionName Name = R.getLookupName();
	if (!Name) return false;

	// FIXME yabin
	// LookupNameKind NameKind = R.getLookupKind();

	if (!getLangOptions().OOP) {
		// Unqualified name lookup in non-oop is purely lexical, so
		// search in the definitions attached to the name.
		unsigned IDNS = R.getIdentifierNamespace();

		// Scan up the scope chain looking for a defn that matches this
		// identifier that is in the appropriate namespace.  This search
		// should not take long, as shadowing of names is uncommon, and
		// deep shadowing is extremely uncommon.
		// bool LeftStartingScope = false;

		for (IdentifierResolver::iterator I = IdResolver.begin(Name), IEnd =
				IdResolver.end(); I != IEnd; ++I) {
			if ((*I)->isInIdentifierNamespace(IDNS)) {
				R.addDefn(*I);

				R.resolveKind();
				return true;
			}
		}
	} else {
		// Perform OOP unqualified name lookup.
		if (OOPLookupName(R, S))
			return true;
	}

	// If we didn't find a use of this identifier, and if the identifier
	// corresponds to a compiler builtin, create the defn object for the builtin
	// now, injecting it into top scope, and return it.
	if (AllowBuiltinCreation)
		return LookupBuiltin(*this, R);

	return false;
}

bool Sema::OOPLookupName(LookupResult &R, Scope *S) {
  assert(getLangOptions().OOP && "Can perform only OOP lookup");

  DefinitionName Name = R.getLookupName();

  IdentifierResolver::iterator I = IdResolver.begin(Name),
		  IEnd = IdResolver.end();

  // First we lookup local scope.
	for (; S /*&& !isNamespaceOrTranslationUnitScope(S)*/; S = S->getParent()) {
		DefnContext *Ctx = static_cast<DefnContext *>(S->getEntity());

		// Check whether the IdResolver has anything in this scope.
		bool Found = false;
		for (; I != IEnd && S->isDefnScope(*I); ++I) {
			if (R.isAcceptableDefn(*I)) {
				Found = true;
				R.addDefn(*I);
			}
		}

		if (Found) {
			R.resolveKind();
			if (S->isClassScope())
				if (UserClassDefn *Record = dyn_cast_or_null<UserClassDefn>(Ctx))
					R.setNamingClass(Record);
			return true;
		}

		if (Ctx) {
			for (; Ctx; Ctx = Ctx->getParent()) {
				// We do not look directly into function or method contexts,
				// since all of the local variables and parameters of the
				// function/method are present within the Scope.
				if (Ctx->isFunctionOrMethod()) {
					continue;
				}

				// Perform qualified name lookup into this context.
				// FIXME: In some cases, we know that every name that could be found by
				// this qualified name lookup will also be on the identifier chain. For
				// example, inside a class without any base classes, we never need to
				// perform qualified lookup because all of the members are on top of the
				// identifier chain.
				if (LookupQualifiedName(R, Ctx, /*InUnqualifiedLookup=*/true))
					return true;
			}
		}
	}

  // Stop if we ran out of scopes.
  // FIXME:  This really, really shouldn't be happening.
  if (!S) return false;

  // If we are looking for members, no need to look into global/namespace scope.
  if (R.getLookupKind() == LookupMemberName)
    return false;

  return !R.empty();
}

bool Sema::LookupQualifiedName(LookupResult &R, DefnContext *LookupCtx,
                               bool InUnqualifiedLookup) {
  return false;
}

bool Sema::LookupParsedName(LookupResult &R, Scope *S, bool AllowBuiltinCreation,
		                        bool EnteringContext) {
  return false;
}

void Sema::LookupVisibleDefns(Scope *S, LookupNameKind Kind,
		VisibleDefnConsumer &Consumer, bool IncludeGlobalScope) {

}

void Sema::LookupVisibleDefns(DefnContext *Ctx, LookupNameKind Kind,
		VisibleDefnConsumer &Consumer, bool IncludeGlobalScope) {

}
