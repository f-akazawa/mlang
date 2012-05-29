//===--- Mangle.cpp - Mangle C++ Names --------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements generic name mangling support for blocks and Objective-C.
//
//===----------------------------------------------------------------------===//
#include "mlang/AST/Mangle.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/DefnOOP.h"
#include "mlang/AST/ExprAll.h"
#include "mlang/Basic/ABI.h"
#include "mlang/Basic/SourceManager.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"

#define MANGLE_CHECKER 0

#if MANGLE_CHECKER
#include <cxxabi.h>
#endif

using namespace mlang;

// FIXME: For blocks we currently mimic GCC's mangling scheme, which leaves
// much to be desired. Come up with a better mangling scheme.

namespace {

static void mangleFunctionBlock(MangleContext &Context,
                                llvm::StringRef Outer,
                                const ScriptDefn *BD,
                                llvm::raw_ostream &Out) {
  Out << "__" << Outer << "_block_invoke_" << Context.getBlockId(BD, true);
}

static void checkMangleDC(const DefnContext *DC, const ScriptDefn *BD) {
#ifndef NDEBUG
  const DefnContext *ExpectedDC = BD->getDefnContext();
  while (isa<ScriptDefn>(ExpectedDC))
    ExpectedDC = ExpectedDC->getParent();
  // In-class initializers for non-static data members are lexically defined
  // within the class, but are mangled as if they were specified as constructor
  // member initializers.
  if (isa<UserClassDefn>(ExpectedDC) && DC != ExpectedDC)
    DC = DC->getParent();
  assert(DC == ExpectedDC && "Given defn context did not match expected!");
#endif
}

}

void MangleContext::mangleGlobalBlock(const ScriptDefn *BD,
                                      llvm::raw_ostream &Out) {
  Out << "__block_global_" << getBlockId(BD, false);
}

void MangleContext::mangleCtorBlock(const ClassConstructorDefn *CD,
                                    ClassCtorType CT, const ScriptDefn *BD,
                                    llvm::raw_ostream &ResStream) {
  checkMangleDC(CD, BD);
  llvm::SmallString<64> Buffer;
  llvm::raw_svector_ostream Out(Buffer);
  mangleCXXCtor(CD, CT, Out);
  Out.flush();
  mangleFunctionBlock(*this, Buffer, BD, ResStream);
}

void MangleContext::mangleDtorBlock(const ClassDestructorDefn *DD,
                                    ClassDtorType DT, const ScriptDefn *BD,
                                    llvm::raw_ostream &ResStream) {
  checkMangleDC(DD, BD);
  llvm::SmallString<64> Buffer;
  llvm::raw_svector_ostream Out(Buffer);
  mangleCXXDtor(DD, DT, Out);
  Out.flush();
  mangleFunctionBlock(*this, Buffer, BD, ResStream);
}

void MangleContext::mangleBlock(const DefnContext *DC, const ScriptDefn *BD,
                                llvm::raw_ostream &Out) {
  assert(!isa<ClassConstructorDefn>(DC) && !isa<ClassDestructorDefn>(DC));
  checkMangleDC(DC, BD);

  llvm::SmallString<64> Buffer;
  llvm::raw_svector_ostream Stream(Buffer);
  const NamedDefn *ND = cast<NamedDefn>(DC);
  if (IdentifierInfo *II = ND->getIdentifier())
	  Stream << II->getName();
  else {
      // FIXME: We were doing a mangleUnqualifiedName() before, but that's
      // a private member of a class that will soon itself be private to the
      // Itanium C++ ABI object. What should we do now? Right now, I'm just
      // calling the mangleName() method on the MangleContext; is there a
      // better way?
      mangleName(ND, Stream);
    }

  Stream.flush();
  mangleFunctionBlock(*this, Buffer, BD, Out);
}

void MangleContext::mangleBlock(const ScriptDefn *BD,
                                llvm::raw_ostream &Out) {
  const DefnContext *DC = BD->getDefnContext();
  while (isa<ScriptDefn>(DC))
    DC = DC->getParent();
  if (DC->isFunctionOrMethod())
    mangleBlock(DC, BD, Out);
  else
    mangleGlobalBlock(BD, Out);
}
