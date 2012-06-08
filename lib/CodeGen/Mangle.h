//===--- Mangle.h - Mangle C++ Names ----------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// Implements OOP name mangling according to the Itanium C++ ABI,
// which is used in GCC 3.2 and newer (and many compilers that are
// ABI-compatible with GCC):
//
//   http://www.codesourcery.com/public/cxx-abi/abi.html
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_MANGLE_H
#define MLANG_CODEGEN_MANGLE_H

#include "CGOOP.h"
#include "GlobalDefn.h"
#include "mlang/AST/Type.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"

namespace mlang {
  class ASTContext;
  class ScriptDefn;
  class ClassConstructorDefn;
  class ClassDestructorDefn;
  class ClassMethodDefn;
  class FunctionDefn;
  class NamedDefn;
  class VarDefn;

namespace CodeGen {
  struct ThisAdjustment;
  struct ThunkInfo;

/// MangleBuffer - a convenient class for storing a name which is
/// either the result of a mangling or is a constant string with
/// external memory ownership.
class MangleBuffer {
public:
  void setString(llvm::StringRef Ref) {
    String = Ref;
  }

  llvm::SmallVectorImpl<char> &getBuffer() {
    return Buffer;
  }

  llvm::StringRef getString() const {
    if (!String.empty()) return String;
    return Buffer.str();
  }

  operator llvm::StringRef() const {
    return getString();
  }

private:
  llvm::StringRef String;
  llvm::SmallString<256> Buffer;
};

/// MangleContext - Context for tracking state which persists across multiple
/// calls to the C++ name mangler.
class MangleContext {
  ASTContext &Context;
  DiagnosticsEngine &Diags;

  llvm::DenseMap<const NamedDefn *, uint64_t> AnonStructIds;
  unsigned Discriminator;
  llvm::DenseMap<const NamedDefn*, unsigned> Uniquifier;
  llvm::DenseMap<const ScriptDefn*, unsigned> GlobalBlockIds;
  llvm::DenseMap<const ScriptDefn*, unsigned> LocalBlockIds;

public:
  explicit MangleContext(ASTContext &Context,
                         DiagnosticsEngine &Diags)
    : Context(Context), Diags(Diags) { }

  virtual ~MangleContext() { }

  ASTContext &getASTContext() const { return Context; }

  DiagnosticsEngine &getDiags() const { return Diags; }

  void startNewFunction() { LocalBlockIds.clear(); }

  uint64_t getAnonymousStructId(const NamedDefn *TD) {
    std::pair<llvm::DenseMap<const NamedDefn *,
      uint64_t>::iterator, bool> Result =
      AnonStructIds.insert(std::make_pair(TD, AnonStructIds.size()));
    return Result.first->second;
  }

  unsigned getBlockId(const ScriptDefn *BD, bool Local) {
    llvm::DenseMap<const ScriptDefn *, unsigned> &BlockIds
      = Local? LocalBlockIds : GlobalBlockIds;
    std::pair<llvm::DenseMap<const ScriptDefn *, unsigned>::iterator, bool>
      Result = BlockIds.insert(std::make_pair(BD, BlockIds.size()));
    return Result.first->second;
  }
  
  /// @name Mangler Entry Points
  /// @{

  virtual bool shouldMangleDefnName(const NamedDefn *D);
  virtual void mangleName(const NamedDefn *D, llvm::SmallVectorImpl<char> &);
  virtual void mangleThunk(const ClassMethodDefn *MD,
                          const ThunkInfo &Thunk,
                          llvm::SmallVectorImpl<char> &);
  virtual void mangleCXXDtorThunk(const ClassDestructorDefn *DD,
                                  ClassDtorType Type,
                                  const ThisAdjustment &ThisAdjustment,
                                  llvm::SmallVectorImpl<char> &);
  virtual void mangleReferenceTemporary(const VarDefn *D,
                                        llvm::SmallVectorImpl<char> &);
  virtual void mangleCXXVTable(const UserClassDefn *RD,
                               llvm::SmallVectorImpl<char> &);
  virtual void mangleCXXVTT(const UserClassDefn *RD,
                            llvm::SmallVectorImpl<char> &);
  virtual void mangleCXXCtorVTable(const UserClassDefn *RD, int64_t Offset,
                                   const UserClassDefn *Type,
                                   llvm::SmallVectorImpl<char> &);
  virtual void mangleCXXRTTI(Type T, llvm::SmallVectorImpl<char> &);
  virtual void mangleCXXRTTIName(Type T, llvm::SmallVectorImpl<char> &);
  virtual void mangleCXXCtor(const ClassConstructorDefn *D, ClassCtorType Type,
                             llvm::SmallVectorImpl<char> &);
  virtual void mangleCXXDtor(const ClassDestructorDefn *D, ClassDtorType Type,
                             llvm::SmallVectorImpl<char> &);
  void mangleBlock(GlobalDefn GD,
                   const ScriptDefn *BD, llvm::SmallVectorImpl<char> &);

  // This is pretty lame.
  void mangleItaniumGuardVariable(const VarDefn *D,
                                  llvm::SmallVectorImpl<char> &);

  void mangleInitDiscriminator() {
    Discriminator = 0;
  }

  bool getNextDiscriminator(const NamedDefn *ND, unsigned &disc) {
    unsigned &discriminator = Uniquifier[ND];
    if (!discriminator)
      discriminator = ++Discriminator;
    if (discriminator == 1)
      return false;
    disc = discriminator-2;
    return true;
  }
  /// @}
};

/// MiscNameMangler - Mangles Objective-C method names and blocks.
class MiscNameMangler {
  MangleContext &Context;
  llvm::raw_svector_ostream Out;
  
  ASTContext &getASTContext() const { return Context.getASTContext(); }

public:
  MiscNameMangler(MangleContext &C, llvm::SmallVectorImpl<char> &Res);

  llvm::raw_svector_ostream &getStream() { return Out; }
  
  void mangleBlock(GlobalDefn GD, const ScriptDefn *BD);
};

}
}

#endif
