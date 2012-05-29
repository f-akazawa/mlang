//===--- ASTConsumers.cpp - ASTConsumer implementations -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// AST Consumer Implementations.
//
//===----------------------------------------------------------------------===//

#include "mlang/Frontend/ASTConsumers.h"
#include "mlang/Basic/SourceManager.h"
#include "mlang/Basic/FileManager.h"
#include "mlang/Diag/Diagnostic.h"
#include "mlang/AST/AST.h"
#include "mlang/AST/ASTConsumer.h"
#include "mlang/AST/RecordLayout.h"
#include "mlang/AST/PrettyPrinter.h"
#include "llvm/Module.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Path.h"
using namespace mlang;

//===----------------------------------------------------------------------===//
/// ASTPrinter - Pretty-printer and dumper of ASTs

namespace {
  class ASTPrinter : public ASTConsumer {
    llvm::raw_ostream &Out;
    bool Dump;

  public:
    ASTPrinter(llvm::raw_ostream* o = NULL, bool Dump = false)
      : Out(o? *o : llvm::outs()), Dump(Dump) { }

    virtual void HandleTranslationUnit(ASTContext &Context) {
      PrintingPolicy Policy = Context.PrintingPolicy;
      Policy.Dump = Dump;
      Context.getTranslationUnitDefn()->print(Out, Policy);
    }
  };
} // end anonymous namespace

ASTConsumer *mlang::CreateASTPrinter(llvm::raw_ostream* out) {
  return new ASTPrinter(out);
}

ASTConsumer *mlang::CreateASTDumper() {
  return new ASTPrinter(0, true);
}

//===----------------------------------------------------------------------===//
/// ASTViewer - AST Visualization

namespace {
  class ASTViewer : public ASTConsumer {
    ASTContext *Context;
  public:
    void Initialize(ASTContext &Context) {
      this->Context = &Context;
    }

    virtual void HandleTopLevelDefn(DefnGroupRef D) {
      for (DefnGroupRef::iterator I = D.begin(), E = D.end(); I != E; ++I)
        HandleTopLevelSingleDefn(*I);
    }

    void HandleTopLevelSingleDefn(Defn *D);
  };
} // end anonymous namespace

void ASTViewer::HandleTopLevelSingleDefn(Defn *D) {
  if (isa<FunctionDefn>(D)) {
    D->print(llvm::errs());

    if (Stmt *Body = D->getBody()) {
    	llvm::errs() << '\n';
    	Body->viewAST();
    	llvm::errs() << '\n';
    }
  } else if(isa<ScriptDefn>(D)) {
	  llvm::errs() << "In a script\n";
	  D->print(llvm::errs());
	  if (Cmd *Body = D->getBody()) {
		  llvm::errs() << '\n';
		  Body->viewAST();
		  llvm::errs() << '\n';
	  }
  } else
	  //FIXME default ast view behavior
	  llvm::errs() << "I love yuanyuan zhang!\n";
}

ASTConsumer *mlang::CreateASTViewer() { return new ASTViewer(); }

//===----------------------------------------------------------------------===//
/// DefnContextPrinter - Defn and DefnContext Visualization

namespace {

class DefnContextPrinter : public ASTConsumer {
  llvm::raw_ostream& Out;
public:
  DefnContextPrinter() : Out(llvm::errs()) {}

  void HandleTranslationUnit(ASTContext &C) {
    PrintDefnContext(C.getTranslationUnitDefn(), 4);
  }

  void PrintDefnContext(const DefnContext* DC, unsigned Indentation);
};
}  // end anonymous namespace

void DefnContextPrinter::PrintDefnContext(const DefnContext* DC,
                                          unsigned Indentation) {
  // Print DefnContext name.
  switch (DC->getDefnKind()) {
  case Defn::TranslationUnit:
    Out << "[translation unit] " << DC;
    break;
  case Defn::Namespace: {
    Out << "[namespace] ";
    const NamespaceDefn* ND = cast<NamespaceDefn>(DC);
    Out << ND;
    break;
  }
  case Defn::Struct: {
    const StructDefn* RD = cast<StructDefn>(DC);
    Out << "[struct] ";
    Out << RD;
    break;
  }
  case Defn::UserClass: {
    const UserClassDefn* RD = cast<UserClassDefn>(DC);
    Out << "[class] ";
    Out << RD << ' ' << DC;
    break;
  }
  case Defn::Script:
    Out << "[Script]";
    break;
  case Defn::Function: {
    const FunctionDefn* FD = cast<FunctionDefn>(DC);
    Out << "[function] ";
    Out << FD;
    // Print the parameters.
    Out << "(";
    bool PrintComma = false;
    for (FunctionDefn::param_const_iterator I = FD->param_begin(),
           E = FD->param_end(); I != E; ++I) {
      if (PrintComma)
        Out << ", ";
      else
        PrintComma = true;
      Out << *I;
    }
    Out << ")";
    break;
  }
  case Defn::ClassMethod: {
    const ClassMethodDefn* D = cast<ClassMethodDefn>(DC);
    if (D->isImplicit())
      Out << "(c++ method) ";
    else
      Out << "<c++ method> ";
    Out << D;
    // Print the parameters.
    Out << "(";
    bool PrintComma = false;
    for (FunctionDefn::param_const_iterator I = D->param_begin(),
           E = D->param_end(); I != E; ++I) {
      if (PrintComma)
        Out << ", ";
      else
        PrintComma = true;
      Out << *I;
    }
    Out << ")";

    break;
  }
  case Defn::ClassConstructor: {
    const ClassConstructorDefn* D = cast<ClassConstructorDefn>(DC);
    if (D->isImplicit())
      Out << "(c++ ctor) ";
    else
      Out << "<c++ ctor> ";
    Out << D;
    // Print the parameters.
    Out << "(";
    bool PrintComma = false;
    for (FunctionDefn::param_const_iterator I = D->param_begin(),
           E = D->param_end(); I != E; ++I) {
      if (PrintComma)
        Out << ", ";
      else
        PrintComma = true;
      Out << *I;
    }
    Out << ")";

    break;
  }
  case Defn::ClassDestructor: {
    const ClassDestructorDefn* D = cast<ClassDestructorDefn>(DC);
    if (D->isImplicit())
      Out << "(c++ dtor) ";
    else
      Out << "<c++ dtor> ";
    Out << D;

    break;
  }

  default:
    assert(0 && "a defn that inherits DefnContext isn't handled");
  }

  Out << "\n";

  // Print decls in the DefnContext.
  for (DefnContext::defn_iterator I = DC->defns_begin(), E = DC->defns_end();
       I != E; ++I) {
    for (unsigned i = 0; i < Indentation; ++i)
      Out << "  ";

    Defn::Kind DK = I->getKind();
    switch (DK) {
    case Defn::Namespace:
    case Defn::UserClass:
    case Defn::Script:
    case Defn::Function:
    case Defn::ClassMethod:
    case Defn::ClassConstructor:
    case Defn::ClassDestructor:
    {
      DefnContext* DC = cast<DefnContext>(*I);
      PrintDefnContext(DC, Indentation+2);
      break;
    }
    case Defn::Member: {
      MemberDefn *FD = cast<MemberDefn>(*I);
      Out << "<field> " << FD << '\n';
      break;
    }
    case Defn::Var: {
      VarDefn* VD = cast<VarDefn>(*I);
      Out << "<var> " << VD << '\n';
      break;
    }
    case Defn::ImplicitParam: {
      ImplicitParamDefn* IPD = cast<ImplicitParamDefn>(*I);
      Out << "<implicit parameter> " << IPD << '\n';
      break;
    }
    case Defn::ParamVar: {
      ParamVarDefn* PVD = cast<ParamVarDefn>(*I);
      Out << "<parameter> " << PVD << '\n';
      break;
    }


    default:
      Out << "DefnKind: " << DK << '"' << *I << "\"\n";
      assert(0 && "decl unhandled");
    }
  }
}

ASTConsumer *mlang::CreateDefnContextPrinter() {
  return new DefnContextPrinter();
}

//===----------------------------------------------------------------------===//
/// ASTDumperXML - In-depth XML dumping.

namespace {
class ASTDumpXML : public ASTConsumer {
  llvm::raw_ostream &OS;

public:
  ASTDumpXML(llvm::raw_ostream &OS) : OS(OS) {}

  void HandleTranslationUnit(ASTContext &C) {
    C.getTranslationUnitDefn()->dumpXML(OS);
  }  
};
}

ASTConsumer *mlang::CreateASTDumperXML(llvm::raw_ostream &OS) {
  return new ASTDumpXML(OS);
}
