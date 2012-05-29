//===--- CodeGenTBAA.h - TBAA information for LLVM CodeGen ------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This is the code that manages TBAA information and defines the TBAA policy
// for the optimizer to use.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CODEGENTBAA_H_
#define MLANG_CODEGEN_CODEGENTBAA_H_

#include "llvm/LLVMContext.h"
#include "llvm/ADT/DenseMap.h"

namespace llvm {
  class LLVMContext;
  class MDNode;
}

namespace mlang {
  class ASTContext;
  class LangOptions;
  class MangleContext;
  class RawType;
  class Type;

namespace CodeGen {
  class CGRecordLayout;

/// CodeGenTBAA - This class organizes the cross-module state that is used
/// while lowering AST types to LLVM types.
class CodeGenTBAA {
  ASTContext &Context;
  llvm::LLVMContext& VMContext;
  const LangOptions &Features;
  MangleContext &MContext;

  /// MetadataCache - This maps mlang::Types to llvm::MDNodes describing them.
  llvm::DenseMap<const RawType *, llvm::MDNode *> MetadataCache;

  llvm::MDNode *Root;
  llvm::MDNode *Char;

  /// getRoot - This is the mdnode for the root of the metadata type graph
  /// for this translation unit.
  llvm::MDNode *getRoot();

  /// getChar - This is the mdnode for "char", which is special, and any types
  /// considered to be equivalent to it.
  llvm::MDNode *getChar();

  llvm::MDNode *getTBAAInfoForNamedType(llvm::StringRef NameStr,
                                        llvm::MDNode *Parent,
                                        bool Readonly = false);

public:
  CodeGenTBAA(ASTContext &Ctx, llvm::LLVMContext &VMContext,
              const LangOptions &Features,
              MangleContext &MContext);
  ~CodeGenTBAA();

  /// getTBAAInfo - Get the TBAA MDNode to be used for a dereference
  /// of the given type.
  llvm::MDNode *getTBAAInfo(Type QTy);
};

}  // end namespace CodeGen
}  // end namespace mlang

#endif /* MLANG_CODEGEN_CODEGENTBAA_H_ */
