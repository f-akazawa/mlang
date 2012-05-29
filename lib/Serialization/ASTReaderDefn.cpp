//===--- ASTReaderDefn.cpp - Decl Deserialization ---------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the ASTReader::ReadDeclRecord method, which is the
// entrypoint for loading a decl.
//
//===----------------------------------------------------------------------===//

#include "ASTCommon.h"
#include "mlang/Serialization/ASTReader.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/CmdBlock.h"
#include "mlang/AST/DefnVisitor.h"
#include "mlang/AST/DefnGroup.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Expr.h"
using namespace mlang;
using namespace mlang::serialization;

//===----------------------------------------------------------------------===//
// Declaration deserialization
//===----------------------------------------------------------------------===//

namespace mlang {
  class ASTDefnReader : public DefnVisitor<ASTDefnReader, void> {
    ASTReader &Reader;
    ASTReader::PerFileData &F;
    llvm::BitstreamCursor &Cursor;
    const DefnID ThisDeclID;
    typedef ASTReader::RecordData RecordData;
    const RecordData &Record;
    unsigned &Idx;
    TypeID TypeIDForTypeDefn;

    uint64_t GetCurrentCursorOffset();
    SourceLocation ReadSourceLocation(const RecordData &R, unsigned &I) {
      return Reader.ReadSourceLocation(F, R, I);
    }
    SourceRange ReadSourceRange(const RecordData &R, unsigned &I) {
      return Reader.ReadSourceRange(F, R, I);
    }
//    void ReadQualifierInfo(QualifierInfo &Info,
//                           const RecordData &R, unsigned &I) {
//      Reader.ReadQualifierInfo(F, Info, R, I);
//    }
    void ReadDefinitionNameInfo(DefinitionNameInfo &NameInfo,
                                const RecordData &R, unsigned &I) {
      Reader.ReadDefinitionNameInfo(F, NameInfo, R, I);
    }

//    void ReadCXXDefinitionData(struct UserClassDefn::DefinitionData &Data,
//                               const RecordData &R, unsigned &I);

    void InitializeCXXDefinitionData(UserClassDefn *D,
                                     UserClassDefn *DefinitionDecl,
                                     const RecordData &Record, unsigned &Idx);
  public:
    ASTDefnReader(ASTReader &Reader, ASTReader::PerFileData &F,
                  llvm::BitstreamCursor &Cursor, DefnID thisDeclID,
                  const RecordData &Record, unsigned &Idx)
      : Reader(Reader), F(F), Cursor(Cursor), ThisDeclID(thisDeclID),
        Record(Record), Idx(Idx), TypeIDForTypeDefn(0) { }

    static void attachPreviousDefn(Defn *D, Defn *previous);

    void Visit(Defn *D);

    void VisitDefn(Defn *D);
    void VisitTranslationUnitDefn(TranslationUnitDefn *TU);
    void VisitNamedDefn(NamedDefn *ND);
    void VisitNamespaceDefn(NamespaceDefn *D);
    void VisitTypeDefn(TypeDefn *RD);
    void VisitStructDefn(StructDefn *SD);
    void VisitCellDefn(CellDefn *SD);
    void VisitUserClassDefn(UserClassDefn *D);
    void VisitValueDefn(ValueDefn *VD);
    void VisitFunctionDefn(FunctionDefn *FD);
    void VisitClassMethodDefn(ClassMethodDefn *D);
    void VisitClassConstructorDefn(ClassConstructorDefn *D);
    void VisitClassDestructorDefn(ClassDestructorDefn *D);
    void VisitMemberDefn(MemberDefn *FD);
    void VisitVarDefn(VarDefn *VD);
    void VisitImplicitParamDefn(ImplicitParamDefn *PD);
    void VisitParamVarDefn(ParamVarDefn *PD);
    void VisitScriptDefn(ScriptDefn *BD);

    std::pair<uint64_t, uint64_t> VisitDefnContext(DefnContext *DC);
    //template <typename T> void VisitRedeclarable(Redeclarable<T> *D);

  };
}

uint64_t ASTDefnReader::GetCurrentCursorOffset() {
  uint64_t Off = 0;
  for (unsigned I = 0, N = Reader.Chain.size(); I != N; ++I) {
    ASTReader::PerFileData &F = *Reader.Chain[N - I - 1];
    if (&Cursor == &F.DefnsCursor) {
      Off += F.DefnsCursor.GetCurrentBitNo();
      break;
    }
    Off += F.SizeInBits;
  }
  return Off;
}

void ASTDefnReader::Visit(Defn *D) {
  DefnVisitor<ASTDefnReader, void>::Visit(D);

  if (TypeDefn *TD = dyn_cast<TypeDefn>(D)) {
    // if we have a fully initialized TypeDecl, we can safely read its type now.
    TD->setTypeForDefn(Reader.GetType(TypeIDForTypeDefn).getRawTypePtr());
  } else if (FunctionDefn *FD = dyn_cast<FunctionDefn>(D)) {
    // FunctionDecl's body was written last after all other Stmts/Exprs.
    if (Record[Idx++])
      FD->setLazyBody(GetCurrentCursorOffset());
  }
}

void ASTDefnReader::VisitDefn(Defn *D) {
  D->setDefnContext(cast_or_null<DefnContext>(Reader.GetDefn(Record[Idx++])));

  D->setLocation(ReadSourceLocation(Record, Idx));
  D->setInvalidDefn(Record[Idx++]);
  if (Record[Idx++]) { // hasAttrs
//    AttrVec Attrs;
//    Reader.ReadAttributes(F, Attrs, Record, Idx);
//    D->setAttrs(Attrs);
  }
  D->setImplicit(Record[Idx++]);
//  D->setUsed(Record[Idx++]);
//  D->setReferenced(Record[Idx++]);
  D->setAccess((AccessSpecifier)Record[Idx++]);
  D->setPCHLevel(Record[Idx++] + (F.Type <= ASTReader::PCH));
}

void ASTDefnReader::VisitTranslationUnitDefn(TranslationUnitDefn *TU) {
  VisitDefn(TU);
//  TU->setAnonymousNamespace(
//                    cast_or_null<NamespaceDefn>(Reader.GetDefn(Record[Idx++])));
}

void ASTDefnReader::VisitNamedDefn(NamedDefn *ND) {
  VisitDefn(ND);
  ND->setDefnName(Reader.ReadDefinitionName(Record, Idx));
}

void ASTDefnReader::VisitTypeDefn(TypeDefn *TD) {
  VisitNamedDefn(TD);
  //TD->setLocStart(ReadSourceLocation(Record, Idx));
  // Delay type reading until after we have fully initialized the decl.
  TypeIDForTypeDefn = Record[Idx++];
}


void ASTDefnReader::VisitStructDefn(StructDefn *RD) {
  VisitTypeDefn(RD);

}

void ASTDefnReader::VisitCellDefn(CellDefn *RD) {
  VisitTypeDefn(RD);

}

void ASTDefnReader::VisitValueDefn(ValueDefn *VD) {
  VisitNamedDefn(VD);
  VD->setType(Reader.GetType(Record[Idx++]));
}

void ASTDefnReader::VisitFunctionDefn(FunctionDefn *FD) {
  VisitValueDefn(FD);

  // Read in the parameters.
  unsigned NumParams = Record[Idx++];
  llvm::SmallVector<ParamVarDefn *, 16> Params;
  Params.reserve(NumParams);
  for (unsigned I = 0; I != NumParams; ++I)
    Params.push_back(cast<ParamVarDefn>(Reader.GetDefn(Record[Idx++])));
  FD->setParams(*Reader.getContext(), Params.data(), NumParams);
}

void ASTDefnReader::VisitMemberDefn(MemberDefn *FD) {
  VisitValueDefn(FD);

}

void ASTDefnReader::VisitVarDefn(VarDefn *VD) {
  VisitValueDefn(VD);

}

void ASTDefnReader::VisitImplicitParamDefn(ImplicitParamDefn *PD) {
  VisitVarDefn(PD);
}

void ASTDefnReader::VisitParamVarDefn(ParamVarDefn *PD) {
  VisitVarDefn(PD);

}

void ASTDefnReader::VisitScriptDefn(ScriptDefn *BD) {
  VisitDefn(BD);
  BD->setBody(cast_or_null<BlockCmd>(Reader.ReadStmt(F)));
}

void ASTDefnReader::VisitNamespaceDefn(NamespaceDefn *D) {
  VisitNamedDefn(D);

}

void ASTDefnReader::VisitUserClassDefn(UserClassDefn *D) {
  VisitTypeDefn(D);

}

void ASTDefnReader::VisitClassMethodDefn(ClassMethodDefn *D) {
  VisitFunctionDefn(D);
  unsigned NumOverridenMethods = Record[Idx++];
  while (NumOverridenMethods--) {
	  // ClassMethodDefn *MD = cast<ClassMethodDefn>(Reader.GetDefn(Record[Idx++]));
    // Avoid invariant checking of CXXMethodDecl::addOverriddenMethod,
    // MD may be initializing.
    //Reader.getContext()->addOverriddenMethod(D, MD);
  }
}

void ASTDefnReader::VisitClassConstructorDefn(ClassConstructorDefn *D) {
  VisitClassMethodDefn(D);

}

void ASTDefnReader::VisitClassDestructorDefn(ClassDestructorDefn *D) {
  VisitClassMethodDefn(D);

}

std::pair<uint64_t, uint64_t>
ASTDefnReader::VisitDefnContext(DefnContext *DC) {
  uint64_t LexicalOffset = Record[Idx++];
  uint64_t VisibleOffset = Record[Idx++];
  return std::make_pair(LexicalOffset, VisibleOffset);
}

//===----------------------------------------------------------------------===//
// Attribute Reading
//===----------------------------------------------------------------------===//

/// \brief Reads attributes from the current stream position.
//void ASTReader::ReadAttributes(PerFileData &F, AttrVec &Attrs,
//                               const RecordData &Record, unsigned &Idx) {
//  for (unsigned i = 0, e = Record[Idx++]; i != e; ++i) {
//    Attr *New = 0;
//    attr::Kind Kind = (attr::Kind)Record[Idx++];
//    SourceLocation Loc = ReadSourceLocation(F, Record, Idx);
//
//#include "clang/Serialization/AttrPCHRead.inc"
//
//    assert(New && "Unable to decode attribute?");
//    Attrs.push_back(New);
//  }
//}

//===----------------------------------------------------------------------===//
// ASTReader Implementation
//===----------------------------------------------------------------------===//

/// \brief Note that we have loaded the declaration with the given
/// Index.
///
/// This routine notes that this declaration has already been loaded,
/// so that future GetDecl calls will return this declaration rather
/// than trying to load a new declaration.
inline void ASTReader::LoadedDefn(unsigned Index, Defn *D) {
  assert(!DefnsLoaded[Index] && "Defn loaded twice?");
  DefnsLoaded[Index] = D;
}


/// \brief Determine whether the consumer will be interested in seeing
/// this declaration (via HandleTopLevelDecl).
///
/// This routine should return true for anything that might affect
/// code generation, e.g., inline function definitions, Objective-C
/// declarations with metadata, etc.
static bool isConsumerInterestedIn(Defn *D) {
//  if (VarDefn *Var = dyn_cast<VarDefn>(D))
//    return Var->isFileVarDecl() &&
//           Var->isThisDeclarationADefinition() == VarDefn::Definition;
//  if (FunctionDefn *Func = dyn_cast<FunctionDefn>(D))
//    return Func->doesThisDeclarationHaveABody();
	return false;
}

/// \brief Get the correct cursor and offset for loading a type.
ASTReader::RecordLocation
ASTReader::DefnCursorForIndex(unsigned Index, DefnID ID) {
  // See if there's an override.
  DefnReplacementMap::iterator It = ReplacedDefns.find(ID);
  if (It != ReplacedDefns.end())
    return RecordLocation(It->second.first, It->second.second);

  PerFileData *F = 0;
  for (unsigned I = 0, N = Chain.size(); I != N; ++I) {
    F = Chain[N - I - 1];
    if (Index < F->LocalNumDefns)
      break;
    Index -= F->LocalNumDefns;
  }
  assert(F && F->LocalNumDefns > Index && "Broken chain");
  return RecordLocation(F, F->DefnOffsets[Index]);
}

void ASTDefnReader::attachPreviousDefn(Defn *D, Defn *previous) {
  assert(D && previous);
//  if (TypeDefn *TD = dyn_cast<TypeDefn>(D)) {
//    TD->RedefnLink.setPointer(cast<TypeDefn>(previous));
//  } else if (FunctionDefn *FD = dyn_cast<FunctionDefn>(D)) {
//    FD->RedeclLink.setPointer(cast<FunctionDefn>(previous));
//  } else if (VarDefn *VD = dyn_cast<VarDefn>(D)) {
//    VD->RedeclLink.setPointer(cast<VarDefn>(previous));
//  }
}

void ASTReader::loadAndAttachPreviousDefn(Defn *D, serialization::DefnID ID) {
  Defn *previous = GetDefn(ID);
  ASTDefnReader::attachPreviousDefn(D, previous);
}

/// \brief Read the declaration at the given offset from the AST file.
Defn *ASTReader::ReadDefnRecord(unsigned Index, DefnID ID) {
  return NULL;
}
