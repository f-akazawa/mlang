//===--- CodeGenTypes.cpp - TBAA information for LLVM CodeGen -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the code that manages TBAA information and defines the TBAA policy
// for the optimizer to use. Relevant standards text includes:
//
//   C99 6.5p7
//   C++ [basic.lval] (p10 in n3126, p15 in some earlier versions)
//
//===----------------------------------------------------------------------===//

#include "CodeGenTBAA.h"
#include "mlang/AST/ASTContext.h"
#include "mlang/AST/Mangle.h"
#include "llvm/LLVMContext.h"
#include "llvm/Metadata.h"
#include "llvm/Constants.h"
#include "llvm/Type.h"
using namespace mlang;
using namespace CodeGen;

CodeGenTBAA::CodeGenTBAA(ASTContext &Ctx, llvm::LLVMContext& VMContext,
                         const LangOptions &Features, MangleContext &MContext)
  : Context(Ctx), VMContext(VMContext), Features(Features), MContext(MContext),
    Root(0), Char(0) {
}

CodeGenTBAA::~CodeGenTBAA() {
}

llvm::MDNode *CodeGenTBAA::getRoot() {
  // Define the root of the tree. This identifies the tree, so that
  // if our LLVM IR is linked with LLVM IR from a different front-end
  // (or a different version of this front-end), their TBAA trees will
  // remain distinct, and the optimizer will treat them conservatively.
  if (!Root)
    Root = getTBAAInfoForNamedType("Simple C/C++ TBAA", 0);

  return Root;
}

llvm::MDNode *CodeGenTBAA::getChar() {
  // Define the root of the tree for user-accessible memory. C and C++
  // give special powers to char and certain similar types. However,
  // these special powers only cover user-accessible memory, and doesn't
  // include things like vtables.
  if (!Char)
    Char = getTBAAInfoForNamedType("omnipotent char", getRoot());

  return Char;
}

/// getTBAAInfoForNamedType - Create a TBAA tree node with the given string
/// as its identifier, and the given Parent node as its tree parent.
llvm::MDNode *CodeGenTBAA::getTBAAInfoForNamedType(llvm::StringRef NameStr,
                                                   llvm::MDNode *Parent,
                                                   bool Readonly) {
  // Currently there is only one flag defined - the readonly flag.
  llvm::Value *Flags = 0;
  if (Readonly)
    Flags = llvm::ConstantInt::get(llvm::Type::getInt64Ty(VMContext), true);

  // Set up the mdnode operand list.
  llvm::Value *Ops[] = {
    llvm::MDString::get(VMContext, NameStr),
    Parent,
    Flags
  };

  // Create the mdnode.
  unsigned Len = llvm::array_lengthof(Ops) - !Flags;
  return llvm::MDNode::get(VMContext, llvm::ArrayRef<llvm::Value*>(Ops, Len));
}

static bool TypeHasMayAlias(Type QTy) {
  return false;
}

llvm::MDNode *
CodeGenTBAA::getTBAAInfo(Type QTy) {
  // If the type has the may_alias attribute (even on a typedef), it is
  // effectively in the general char alias class.
  if (TypeHasMayAlias(QTy))
    return getChar();

  const RawType *Ty = QTy.getRawTypePtr();

  if (llvm::MDNode *N = MetadataCache[Ty])
    return N;

  // Handle builtin types.
  if (const SimpleNumericType *BTy = dyn_cast<SimpleNumericType>(Ty)) {
    switch (BTy->getKind()) {
    // Character types are special and can alias anything.
    // In C++, this technically only includes "char" and "unsigned char",
    // and not "signed char". In C, it includes all three. For now,
    // the risk of exploiting this detail in C++ seems likely to outweigh
    // the benefit.
    case SimpleNumericType::Char:
      return getChar();

    // Unsigned types can alias their corresponding signed types.
    case SimpleNumericType::Int8:
      return getTBAAInfo(Context.Int8Ty);
    case SimpleNumericType::UInt8:
      return getTBAAInfo(Context.Int8Ty);
    case SimpleNumericType::UInt64:
      return getTBAAInfo(Context.Int64Ty);

    // Treat all other builtin types as distinct types. This includes
    // treating wchar_t, char16_t, and char32_t as distinct from their
    // "underlying types".
    default:
      return MetadataCache[Ty] =
               getTBAAInfoForNamedType(BTy->getName(), getChar());
    }
  }

  // For now, handle any other kind of type conservatively.
  return MetadataCache[Ty] = getChar();
}
