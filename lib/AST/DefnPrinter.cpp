//===--- DefnPrinter.cpp - Printing implementation for Decl ASTs ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Decl::dump method, which pretty print the
// AST back out to C/Objective-C/C++/Objective-C++ code.
//
//===----------------------------------------------------------------------===//
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnVisitor.h"
#include "mlang/AST/Defn.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/AST/PrettyPrinter.h"
#include "llvm/Support/raw_ostream.h"
using namespace mlang;

namespace {
  class DefnPrinter : public DefnVisitor<DefnPrinter> {
    llvm::raw_ostream &Out;
    ASTContext &Context;
    PrintingPolicy Policy;
    unsigned Indentation;

    llvm::raw_ostream& Indent() { return Indent(Indentation); }
    llvm::raw_ostream& Indent(unsigned Indentation);
    void ProcessDefnGroup(llvm::SmallVectorImpl<Defn*>& Defns);

    void Print(AccessSpecifier AS);

  public:
    DefnPrinter(llvm::raw_ostream &Out, ASTContext &Context,
                const PrintingPolicy &Policy,
                unsigned Indentation = 0)
      : Out(Out), Context(Context), Policy(Policy), Indentation(Indentation) { }

    void VisitDefnContext(DefnContext *DC, bool Indent = true);

    void VisitTranslationUnitDefn(TranslationUnitDefn *D);
    void VisitStructDefn(StructDefn *D);
    void VisitCellDefn(CellDefn *D);
    void VisitUserClassDefn(UserClassDefn *D);
    void VisitTypeDefn(TypeDefn *D);
    void VisitFunctionDefn(FunctionDefn *D);
    void VisitScriptDefn(ScriptDefn *D);
    void VisitMemberDefn(MemberDefn *D);
    void VisitVarDefn(VarDefn *D);
    void VisitParamVarDefn(ParamVarDefn *D);
    void VisitNamespaceDefn(NamespaceDefn *D);
  };
}

void Defn::print(llvm::raw_ostream &Out, unsigned Indentation) const {
  print(Out, getASTContext().PrintingPolicy, Indentation);
}

void Defn::print(llvm::raw_ostream &Out, const PrintingPolicy &Policy,
                 unsigned Indentation) const {
  DefnPrinter Printer(Out, getASTContext(), Policy, Indentation);
  Printer.Visit(const_cast<Defn*>(this));
}

static Type GetBaseType(Type T) {
  // FIXME: This should be on the Type class!
  Type BaseType = T;
  while (!BaseType->isSpecifierType()) {
    if (const FunctionHandleType* PTy = BaseType->getAs<FunctionHandleType>())
      BaseType = PTy->getPointeeType();
    else if (const ArrayType* ATy = dyn_cast<ArrayType>(BaseType))
      BaseType = ATy->getElementType();
//    else if (const FunctionType* FTy = BaseType->getAs<FunctionType>())
//      BaseType = FTy->getResultType();
    else if (const VectorType *VTy = BaseType->getAs<VectorType>())
      BaseType = VTy->getElementType();
    else
      assert(0 && "Unknown declarator!");
  }
  return BaseType;
}

static Type getDefnType(Defn* D) {
  if (ValueDefn* VD = dyn_cast<ValueDefn>(D))
    return VD->getType();
  return Type();
}

void Defn::printGroup(Defn** Begin, unsigned NumDefns,
                      llvm::raw_ostream &Out, const PrintingPolicy &Policy,
                      unsigned Indentation) {
  if (NumDefns == 1) {
    (*Begin)->print(Out, Policy, Indentation);
    return;
  }

  Defn** End = Begin + NumDefns;
  TypeDefn* TD = dyn_cast<TypeDefn>(*Begin);
  if (TD)
    ++Begin;

  PrintingPolicy SubPolicy(Policy);
  if (TD) {
    TD->print(Out, Policy, Indentation);
    Out << " ";
    SubPolicy.SuppressTag = true;
  }

  bool isFirst = true;
  for ( ; Begin != End; ++Begin) {
    if (isFirst) {
      SubPolicy.SuppressSpecifiers = false;
      isFirst = false;
    } else {
      if (!isFirst) Out << ", ";
      SubPolicy.SuppressSpecifiers = true;
    }

    (*Begin)->print(Out, SubPolicy, Indentation);
  }
}

void DefnContext::dumpDefnContext() const {
  // Get the translation unit
  const DefnContext *DC = this;
  while (!DC->isTranslationUnit())
    DC = DC->getParent();
  
  ASTContext &Ctx = cast<TranslationUnitDefn>(DC)->getASTContext();
  DefnPrinter Printer(llvm::errs(), Ctx, Ctx.PrintingPolicy, 0);
  Printer.VisitDefnContext(const_cast<DefnContext *>(this), /*Indent=*/false);
}

void Defn::dump() const {
  print(llvm::errs());
}

llvm::raw_ostream& DefnPrinter::Indent(unsigned Indentation) {
  for (unsigned i = 0; i != Indentation; ++i)
    Out << "  ";
  return Out;
}

void DefnPrinter::ProcessDefnGroup(llvm::SmallVectorImpl<Defn*>& Defns) {
  this->Indent();
  Defn::printGroup(Defns.data(), Defns.size(), Out, Policy, Indentation);
  Out << ";\n";
  Defns.clear();

}

void DefnPrinter::Print(AccessSpecifier AS) {
  switch(AS) {
  case AS_none:      assert(0 && "No access specifier!"); break;
  case AS_public:    Out << "public"; break;
  case AS_protected: Out << "protected"; break;
  case AS_private:   Out << "private"; break;
  }
}

//----------------------------------------------------------------------------
// Common C declarations
//----------------------------------------------------------------------------

void DefnPrinter::VisitDefnContext(DefnContext *DC, bool Indent) {
  if (Indent)
    Indentation += Policy.Indentation;

  llvm::SmallVector<Defn*, 2> Defns;
  for (DefnContext::defn_iterator D = DC->defns_begin(), DEnd = DC->defns_end();
       D != DEnd; ++D) {

    if (!Policy.Dump) {
      // Skip over implicit declarations in pretty-printing mode.
      if (D->isImplicit()) continue;
      // FIXME: Ugly hack so we don't pretty-print the builtin declaration
      // of __builtin_va_list or __[u]int128_t.  There should be some other way
      // to check that.
      if (NamedDefn *ND = dyn_cast<NamedDefn>(*D)) {
        if (IdentifierInfo *II = ND->getIdentifier()) {
          if (II->isStr("__builtin_va_list") ||
              II->isStr("__int128_t") || II->isStr("__uint128_t"))
            continue;
        }
      }
    }

    // The next bits of code handles stuff like "struct {int x;} a,b"; we're
    // forced to merge the declarations because there's no other way to
    // refer to the struct in question.  This limited merging is safe without
    // a bunch of other checks because it only merges declarations directly
    // referring to the tag, not typedefs.
    //
    // Check whether the current declaration should be grouped with a previous
    // unnamed struct.
    Type CurDefnType = getDefnType(*D);
    if (!Defns.empty() && !CurDefnType.isNull()) {
      Type BaseType = GetBaseType(CurDefnType);
      if (!BaseType.isNull() && isa<HeteroContainerType>(BaseType) &&
          cast<HeteroContainerType>(BaseType)->getDefn() == Defns[0]) {
        Defns.push_back(*D);
        continue;
      }
    }

    // If we have a merged group waiting to be handled, handle it now.
    if (!Defns.empty())
      ProcessDefnGroup(Defns);

    // If the current declaration is an unnamed tag type, save it
    // so we can merge it with the subsequent declaration(s) using it.
    if (isa<TypeDefn>(*D) && !cast<TypeDefn>(*D)->getIdentifier()) {
      Defns.push_back(*D);
      continue;
    }

    this->Indent();
    Visit(*D);

    // FIXME: Need to be able to tell the DefnPrinter when
    const char *Terminator = 0;
    if (isa<FunctionDefn>(*D))
      Terminator = 0;
    else if (isa<NamespaceDefn>(*D))
      Terminator = 0;
    else
      Terminator = ";";

    if (Terminator)
      Out << Terminator;
    Out << "\n";
  }

  if (!Defns.empty())
    ProcessDefnGroup(Defns);

  if (Indent)
    Indentation -= Policy.Indentation;
}

void DefnPrinter::VisitTranslationUnitDefn(TranslationUnitDefn *D) {
  VisitDefnContext(D, false);
}

void DefnPrinter::VisitTypeDefn(TypeDefn *D) {
  Out << D->getKindName();
  if (D->getIdentifier())
    Out << ' ' << D;

  Out << " {\n";
  VisitDefnContext(D);
  Indent() << "}";
}

void DefnPrinter::VisitFunctionDefn(FunctionDefn *D) {
  if (!Policy.SuppressSpecifiers) {
    if (D->isInlineSpecified())           Out << "inline ";
  }

  PrintingPolicy SubPolicy(Policy);
  SubPolicy.SuppressSpecifiers = false;
  std::string Proto = D->getNameInfo().getAsString();

  Type Ty = D->getType();

  if (isa<FunctionType>(Ty)) {
    const FunctionType *AFT = Ty->getAs<FunctionType>();
    const FunctionProtoType *FT = 0;
    FT = dyn_cast<FunctionProtoType>(AFT);

    Proto += "(";
    if (FT) {
      llvm::raw_string_ostream POut(Proto);
      DefnPrinter ParamPrinter(POut, Context, SubPolicy, Indentation);
      for (unsigned i = 0, e = D->getNumParams(); i != e; ++i) {
        if (i) POut << ", ";
        ParamPrinter.VisitParamVarDefn(D->getParamDefn(i));
      }

      if (FT->isVariadic()) {
        if (D->getNumParams()) POut << ", ";
        POut << "...";
      }
    }
    D->getBody()->printPretty(Out, Context, 0, SubPolicy, Indentation);
    Out << '\n';
  }

    Proto += ")";
    

    Out << Proto;
}

void DefnPrinter::VisitScriptDefn(ScriptDefn *D) {
  VisitDefnContext(D);
  D->getBody()->printPretty(Out, Context, 0, Policy, Indentation);
  Out << '\n';
}

void DefnPrinter::VisitMemberDefn(MemberDefn *D) {
  std::string Name = D->getNameAsString();
  D->getType().getAsStringInternal(Name, Policy);
  Out << Name;
}

void DefnPrinter::VisitVarDefn(VarDefn *D) {
  if (!Policy.SuppressSpecifiers && D->getStorageClass() != SC_None)
    Out << VarDefn::getStorageClassSpecifierString(D->getStorageClass()) << " ";

  std::string Name = D->getNameAsString();
  Type T = D->getType();
  if (ParamVarDefn *Parm = dyn_cast<ParamVarDefn>(D))
    T = Parm->getType();
  T.getAsStringInternal(Name, Policy);
  Out << Name;
}

void DefnPrinter::VisitParamVarDefn(ParamVarDefn *D) {
  VisitVarDefn(D);
}

void DefnPrinter::VisitStructDefn(StructDefn *D) {
  VisitTypeDefn(D);
}

void DefnPrinter::VisitCellDefn(CellDefn *D) {
  VisitTypeDefn(D);
}
//----------------------------------------------------------------------------
// OOP definitions
//----------------------------------------------------------------------------
void DefnPrinter::VisitNamespaceDefn(NamespaceDefn *D) {
  Out << "namespace " << D << " {\n";
  VisitDefnContext(D);
  Indent() << "}";
}

void DefnPrinter::VisitUserClassDefn(UserClassDefn *D) {
  Out << D->getKindName();
  if (D->getIdentifier())
    Out << ' ' << D;

  if (D) {
    // Print the base classes
    if (D->getNumBases()) {
      Out << " : ";
      for (UserClassDefn::base_class_iterator Base = D->bases_begin(),
             BaseEnd = D->bases_end(); Base != BaseEnd; ++Base) {
        if (Base != D->bases_begin())
          Out << ", ";

        if (Base->isVirtual())
          Out << "virtual ";

        AccessSpecifier AS = Base->getAccessSpecifierAsWritten();
        if (AS != AS_none)
          Print(AS);
        Out << " " << Base->getType().getAsString(Policy);

        if (Base->isPackExpansion())
          Out << "...";
      }
    }

    // Print the class definition
    // FIXME: Doesn't print access specifiers, e.g., "public:"
    Out << " {\n";
    VisitDefnContext(D);
    Indent() << "}";
  }
}
