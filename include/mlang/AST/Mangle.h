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

#include "mlang/AST/Type.h"
#include "mlang/Basic/ABI.h"
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
  Diagnostic &Diags;

  llvm::DenseMap<const NamedDefn *, uint64_t> AnonStructIds;
  unsigned Discriminator;
  llvm::DenseMap<const NamedDefn*, unsigned> Uniquifier;
  llvm::DenseMap<const ScriptDefn*, unsigned> GlobalBlockIds;
  llvm::DenseMap<const ScriptDefn*, unsigned> LocalBlockIds;
  
public:
  explicit MangleContext(ASTContext &Context,
                         Diagnostic &Diags)
    : Context(Context), Diags(Diags) { }

  virtual ~MangleContext() { }

  ASTContext &getASTContext() const { return Context; }

  Diagnostic &getDiags() const { return Diags; }

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

  virtual bool shouldMangleDefnName(const NamedDefn *D) = 0;
  virtual void mangleName(const NamedDefn *D, llvm::raw_ostream &)=0;
  virtual void mangleThunk(const ClassMethodDefn *MD,
                           const ThunkInfo &Thunk,
                           llvm::raw_ostream &) = 0;
  virtual void mangleCXXDtorThunk(const ClassDestructorDefn *DD, ClassDtorType Type,
                                  const ThisAdjustment &ThisAdjustment,
                                  llvm::raw_ostream &) = 0;
  virtual void mangleReferenceTemporary(const VarDefn *D,
		                                    llvm::raw_ostream &) = 0;
  virtual void mangleCXXVTable(const UserClassDefn *RD,
		                           llvm::raw_ostream &) = 0;
  virtual void mangleCXXVTT(const UserClassDefn *RD,
		                        llvm::raw_ostream &) = 0;
  virtual void mangleCXXCtorVTable(const UserClassDefn *RD, int64_t Offset,
                                   const UserClassDefn *Type,
                                   llvm::raw_ostream &) = 0;
  virtual void mangleCXXRTTI(Type T, llvm::raw_ostream &) = 0;
  virtual void mangleCXXRTTIName(Type T, llvm::raw_ostream &) = 0;
  virtual void mangleCXXCtor(const ClassConstructorDefn *D, ClassCtorType Type,
		                         llvm::raw_ostream &) = 0;
  virtual void mangleCXXDtor(const ClassDestructorDefn *D, ClassDtorType Type,
		                         llvm::raw_ostream &) = 0;
  void mangleGlobalBlock(const ScriptDefn *BD,
                         llvm::raw_ostream &Out);
  void mangleCtorBlock(const ClassConstructorDefn *CD, ClassCtorType CT,
                       const ScriptDefn *BD, llvm::raw_ostream &Out);
  void mangleDtorBlock(const ClassDestructorDefn *CD, ClassDtorType DT,
                       const ScriptDefn *BD, llvm::raw_ostream &Out);
  void mangleBlock(const DefnContext *DC,
                   const ScriptDefn *BD, llvm::raw_ostream &Out);
  // Do the right thing.
  void mangleBlock(const ScriptDefn *BD, llvm::raw_ostream &Out);

  // This is pretty lame.
  void mangleItaniumGuardVariable(const VarDefn *D,
		                              llvm::raw_ostream &Out) {
	  assert(0 && "Target does not support mangling guard variables");
  }

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

} // end namespace mlang

#endif
