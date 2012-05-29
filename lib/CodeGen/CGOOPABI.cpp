//===----- CGOOPABI.cpp - Interface to C++ ABIs -----------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for C++ code generation. Concrete subclasses
// of this implement code generation for specific C++ ABIs.
//
//===----------------------------------------------------------------------===//

#include "CGOOPABI.h"

using namespace mlang;
using namespace CodeGen;

CGOOPABI::~CGOOPABI() { }

static void ErrorUnsupportedABI(CodeGenFunction &CGF,
                                llvm::StringRef S) {
  Diagnostic &Diags = CGF.CGM.getDiags();
  unsigned DiagID = Diags.getCustomDiagID(Diagnostic::Error,
                                          "cannot yet compile %1 in this ABI");
  Diags.Report(CGF.getContext().getFullLoc(CGF.CurCodeDefn->getLocation()),
               DiagID)
    << S;
}

void CGOOPABI::BuildThisParam(CodeGenFunction &CGF, FunctionArgList &Params) {
  const ClassMethodDefn *MD = cast<ClassMethodDefn>(CGF.CurGD.getDefn());

  // FIXME: I'm not entirely sure I like using a fake decl just for code
  // generation. Maybe we can come up with a better way?
//  ImplicitParamDefn *ThisDecl
//    = ImplicitParamDefn::Create(CGM.getContext(), 0, MD->getLocation(),
//                                &CGM.getContext().Idents.get("this"),
//                                MD->getThisType(CGM.getContext()));
//  Params.push_back(std::make_pair(ThisDecl, ThisDecl->getType()));
//  getThisDecl(CGF) = ThisDecl;
}

void CGOOPABI::EmitThisParam(CodeGenFunction &CGF) {
  /// Initialize the 'this' slot.
  assert(getThisDecl(CGF) && "no 'this' variable for function");
  getThisValue(CGF)
    = CGF.Builder.CreateLoad(CGF.GetAddrOfLocalVar(getThisDecl(CGF)),
                             "this");
}

void CGOOPABI::EmitReturnFromThunk(CodeGenFunction &CGF,
                                   RValue RV, Type ResultType) {
  CGF.EmitReturnOfRValue(RV, ResultType);
}

CharUnits CGOOPABI::GetArrayCookieSize(Type ElementType) {
  return CharUnits::Zero();
}

llvm::Value *CGOOPABI::InitializeArrayCookie(CodeGenFunction &CGF,
                                             llvm::Value *NewPtr,
                                             llvm::Value *NumElements,
                                             Type ElementType) {
  // Should never be called.
  ErrorUnsupportedABI(CGF, "array cookie initialization");
  return 0;
}

void CGOOPABI::ReadArrayCookie(CodeGenFunction &CGF, llvm::Value *Ptr,
                               Type ElementType, llvm::Value *&NumElements,
                               llvm::Value *&AllocPtr, CharUnits &CookieSize) {
  ErrorUnsupportedABI(CGF, "array cookie reading");

  // This should be enough to avoid assertions.
  NumElements = 0;
  AllocPtr = llvm::Constant::getNullValue(CGF.Builder.getInt8PtrTy());
  CookieSize = CharUnits::Zero();
}

void CGOOPABI::EmitGuardedInit(CodeGenFunction &CGF,
                               const VarDefn &D,
                               llvm::GlobalVariable *GV) {
  ErrorUnsupportedABI(CGF, "static local variable initialization");
}

llvm::Constant *CGOOPABI::EmitMemberPointer(const ClassMethodDefn *MD) {
//  return GetBogusMemberPointer(CGM,
//                         CGM.getContext().getMemberPointerType(MD->getType(),
//                                         MD->getParent()->getTypeForDefn()));
	return 0;
}

llvm::Constant *CGOOPABI::EmitMemberPointer(const MemberDefn *FD) {
//  return GetBogusMemberPointer(CGM,
//                         CGM.getContext().getMemberPointerType(FD->getType(),
//                                         FD->getParent()->getTypeForDefn()));
	return 0;
}
