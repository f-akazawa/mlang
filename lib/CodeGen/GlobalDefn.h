//===--- GlobalDefn.h - Global declaration holder ---------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// A GlobalDefn can hold either a regular variable/function or a C++ ctor/dtor
// together with its type.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_GLOBALDEFN_H_
#define MLANG_CODEGEN_GLOBALDEFN_H_

#include "CGOOP.h"
#include "mlang/AST/DefnOOP.h"

namespace mlang {

namespace CodeGen {

/// GlobalDefn - represents a global declaration. This can either be a
/// ClassConstructorDefn and the constructor type (Base, Complete).
/// a ClassDestructorDefn and the destructor type (Base, Complete) or
/// a VarDefn, a FunctionDefn or a ScriptDefn.
class GlobalDefn {
  llvm::PointerIntPair<const Defn*, 2> Value;

  void Init(const Defn *D) {
    assert(!isa<ClassConstructorDefn>(D) && "Use other ctor with ctor decls!");
    assert(!isa<ClassDestructorDefn>(D) && "Use other ctor with dtor decls!");

    Value.setPointer(D);
  }

public:
  GlobalDefn() {}

  GlobalDefn(const VarDefn *D) { Init(D);}
  GlobalDefn(const FunctionDefn *D) { Init(D); }
  GlobalDefn(const ScriptDefn *D) { Init(D); }
  GlobalDefn(const ClassMethodDefn *D) { Init(D); }

  GlobalDefn(const ClassConstructorDefn *D, ClassCtorType Type)
  : Value(D, Type) {}
  GlobalDefn(const ClassDestructorDefn *D, ClassDtorType Type)
  : Value(D, Type) {}

  GlobalDefn getCanonicalDefn() const {
    GlobalDefn CanonGD;
    CanonGD.Value.setPointer(Value.getPointer());
    CanonGD.Value.setInt(Value.getInt());
    
    return CanonGD;
  }

  const Defn *getDefn() const { return Value.getPointer(); }

  ClassCtorType getCtorType() const {
    assert(isa<ClassConstructorDefn>(getDefn()) && "Defn is not a ctor!");
    return static_cast<ClassCtorType>(Value.getInt());
  }

  ClassDtorType getDtorType() const {
    assert(isa<ClassDestructorDefn>(getDefn()) && "Decl is not a dtor!");
    return static_cast<ClassDtorType>(Value.getInt());
  }
  
  friend bool operator==(const GlobalDefn &LHS, const GlobalDefn &RHS) {
    return LHS.Value == RHS.Value;
  }
  
  void *getAsOpaquePtr() const { return Value.getOpaqueValue(); }

  static GlobalDefn getFromOpaquePtr(void *P) {
    GlobalDefn GD;
    GD.Value.setFromOpaqueValue(P);
    return GD;
  }
};

} // end namespace CodeGen
} // end namespace mlang

namespace llvm {
  template<class> struct DenseMapInfo;

  template<> struct DenseMapInfo<mlang::CodeGen::GlobalDefn> {
    static inline mlang::CodeGen::GlobalDefn getEmptyKey() {
      return mlang::CodeGen::GlobalDefn();
    }
  
    static inline mlang::CodeGen::GlobalDefn getTombstoneKey() {
      return mlang::CodeGen::GlobalDefn::
        getFromOpaquePtr(reinterpret_cast<void*>(-1));
    }

    static unsigned getHashValue(mlang::CodeGen::GlobalDefn GD) {
      return DenseMapInfo<void*>::getHashValue(GD.getAsOpaquePtr());
    }
    
    static bool isEqual(mlang::CodeGen::GlobalDefn LHS,
                        mlang::CodeGen::GlobalDefn RHS) {
      return LHS == RHS;
    }
      
  };
  
  // GlobalDefn isn't *technically* a POD type. However, its copy constructor,
  // copy assignment operator, and destructor are all trivial.
  template <>
  struct isPodLike<mlang::CodeGen::GlobalDefn> {
    static const bool value = true;
  };
} // end namespace llvm

#endif /* MLANG_CODEGEN_GLOBALDEFN_H_ */
