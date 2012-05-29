//===--- ASTDiagnostic.cpp - Diagnostic Printing Hooks for AST Nodes ------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This file implements a diagnostic formatting hook for AST elements.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/ASTDiagnostic.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/Type.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlang;

// Returns a desugared version of the Type, and marks ShouldAKA as true
// whenever we remove significant sugar from the type.
static Type Desugar(ASTContext &Context, Type QT, bool &ShouldAKA) {
  return Type();
}

/// \brief Convert the given type to a string suitable for printing as part of 
/// a diagnostic.
///
/// There are three main criteria when determining whether we should have an
/// a.k.a. clause when pretty-printing a type:
///
/// 1) Some types provide very minimal sugar that doesn't impede the
///    user's understanding --- for example, elaborated type
///    specifiers.  If this is all the sugar we see, we don't want an
///    a.k.a. clause.
/// 2) Some types are technically sugared but are much more familiar
///    when seen in their sugared form --- for example, va_list,
///    vector types, and the magic Objective C types.  We don't
///    want to desugar these, even if we do produce an a.k.a. clause.
/// 3) Some types may have already been desugared previously in this diagnostic.
///    if this is the case, doing another "aka" would just be clutter.
///
/// \param Context the context in which the type was allocated
/// \param Ty the type to print
static std::string
ConvertTypeToDiagnosticString(ASTContext &Context, Type Ty,
                              const Diagnostic::ArgumentValue *PrevArgs,
                              unsigned NumPrevArgs) {
  // FIXME: Playing with std::string is really slow.
  std::string S = Ty.getAsString(Context.PrintingPolicy);

  // Check to see if we already desugared this type in this
  // diagnostic.  If so, don't do it again.
  bool Repeated = false;
  for (unsigned i = 0; i != NumPrevArgs; ++i) {
    // TODO: Handle ak_declcontext case.
    if (PrevArgs[i].first == Diagnostic::ak_qualtype) {
      void *Ptr = (void*)PrevArgs[i].second;
      Type PrevTy(Type::getFromOpaquePtr(Ptr));
      if (PrevTy == Ty) {
        Repeated = true;
        break;
      }
    }
  }

  // Consider producing an a.k.a. clause if removing all the direct
  // sugar gives us something "significantly different".
  if (!Repeated) {
    bool ShouldAKA = false;
    Type DesugaredTy = Desugar(Context, Ty, ShouldAKA);
    if (ShouldAKA) {
      S = "'" + S + "' (aka '";
      S += DesugaredTy.getAsString(Context.PrintingPolicy);
      S += "')";
      return S;
    }
  }

  S = "'" + S + "'";
  return S;
}

void mlang::FormatASTNodeDiagnosticArgument(Diagnostic::ArgumentKind Kind,
		intptr_t Val, const char *Modifier, unsigned ModLen,
		const char *Argument, unsigned ArgLen,
		const Diagnostic::ArgumentValue *PrevArgs,
		unsigned NumPrevArgs, llvm::SmallVectorImpl<char> &Output,
		void *Cookie) {
  ASTContext &Context = *static_cast<ASTContext*>(Cookie);
  
  std::string S;
  bool NeedQuotes = true;
  
  switch (Kind) {
    default: assert(0 && "unknown ArgumentKind");
    case Diagnostic::ak_qualtype: {
      assert(ModLen == 0 && ArgLen == 0 &&
             "Invalid modifier for Type argument");
      
      Type Ty(Type::getFromOpaquePtr(reinterpret_cast<void*>(Val)));
      S = ConvertTypeToDiagnosticString(Context, Ty, PrevArgs, NumPrevArgs);
      NeedQuotes = false;
      break;
    }
    case Diagnostic::ak_declarationname: {
      DefinitionName N = DefinitionName::getFromOpaqueInteger(Val);
      S = N.getAsString();
      
      if (ModLen == 9 && !memcmp(Modifier, "objcclass", 9) && ArgLen == 0)
        S = '+' + S;
      else if (ModLen == 12 && !memcmp(Modifier, "objcinstance", 12)
                && ArgLen==0)
        S = '-' + S;
      else
        assert(ModLen == 0 && ArgLen == 0 &&
               "Invalid modifier for DefinitionName argument");
      break;
    }
    case Diagnostic::ak_nameddecl: {
      bool Qualified;
      if (ModLen == 1 && Modifier[0] == 'q' && ArgLen == 0)
        Qualified = true;
      else {
        assert(ModLen == 0 && ArgLen == 0 &&
               "Invalid modifier for NamedDefn* argument");
        Qualified = false;
      }
      reinterpret_cast<NamedDefn*>(Val)->
      getNameForDiagnostic(S, Context.PrintingPolicy, Qualified);
      break;
    }
    case Diagnostic::ak_nestednamespec: {
      llvm::raw_string_ostream OS(S);
      reinterpret_cast<NestedNameSpecifier*>(Val)->print(OS,
                                                        Context.PrintingPolicy);
      NeedQuotes = false;
      break;
    }
    case Diagnostic::ak_declcontext: {
      DefnContext *DC = reinterpret_cast<DefnContext *> (Val);
      assert(DC && "Should never have a null declaration context");
      
      if (DC->isTranslationUnit()) {
        S = "the global scope";
      } else if (TypeDefn *Type = dyn_cast<TypeDefn>(DC)) {
        S = ConvertTypeToDiagnosticString(Context, 
                                          Context.getTypeDefnType(Type),
                                          PrevArgs, NumPrevArgs);
      } else {
        // FIXME: Get these strings from some localized place
        NamedDefn *ND = cast<NamedDefn>(DC);
        if (isa<NamespaceDefn>(ND))
          S += "namespace ";
        else if (isa<FunctionDefn>(ND))
          S += "function ";
        
        S += "'";
        ND->getNameForDiagnostic(S, Context.PrintingPolicy, true);
        S += "'";
      }
      NeedQuotes = false;
      break;
    }
  }
  
  if (NeedQuotes)
    Output.push_back('\'');
  
  Output.append(S.begin(), S.end());
  
  if (NeedQuotes)
    Output.push_back('\'');
}
