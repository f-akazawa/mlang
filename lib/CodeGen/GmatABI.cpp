//===--- GmatABI.cpp - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#include "CGOOPABI.h"
#include "CodeGenModule.h"
#include "mlang/AST/Mangle.h"

using namespace mlang;
using namespace mlang::CodeGen;

namespace {

class NVGmatABI : public CGOOPABI {
	bool NV;
public:
	NVGmatABI(CodeGenModule &CGM) : CGOOPABI(CGM), NV(false) {}
	virtual MangleContext &getMangleContext() {	}
	virtual void BuildConstructorSignature(const ClassConstructorDefn *Ctor,
	                                         ClassCtorType T,
	                                         Type &ResTy,
	                               llvm::SmallVectorImpl<Type> &ArgTys) {}
	virtual void BuildDestructorSignature(const ClassDestructorDefn *Dtor,
	                                        ClassDtorType T,
	                                        Type &ResTy,
	                               llvm::SmallVectorImpl<Type> &ArgTys){}
	virtual void BuildInstanceFunctionParams(CodeGenFunction &CGF,
	                                           Type &ResTy,
	                                           FunctionArgList &Params){}
	virtual void EmitInstanceFunctionProlog(CodeGenFunction &CGF){}
};

class AMDGmatABI : public CGOOPABI {
	bool AMD;
public:
	AMDGmatABI(CodeGenModule &CGM) : CGOOPABI(CGM), AMD(false) {}
	virtual MangleContext &getMangleContext() { }
	virtual void BuildConstructorSignature(const ClassConstructorDefn *Ctor,
	                                         ClassCtorType T,
	                                         Type &ResTy,
	                               llvm::SmallVectorImpl<Type> &ArgTys) {}
	virtual void BuildDestructorSignature(const ClassDestructorDefn *Dtor,
	                                        ClassDtorType T,
	                                        Type &ResTy,
	                               llvm::SmallVectorImpl<Type> &ArgTys){}
	virtual void BuildInstanceFunctionParams(CodeGenFunction &CGF,
	                                           Type &ResTy,
	                                           FunctionArgList &Params) {}
	virtual void EmitInstanceFunctionProlog(CodeGenFunction &CGF) {}
};
}

CodeGen::CGOOPABI *CodeGen::CreateNVGmatABI(CodeGenModule &CGM) {
  return new NVGmatABI(CGM);
}

CodeGen::CGOOPABI *CodeGen::CreateAMDGmatABI(CodeGenModule &CGM) {
  return new AMDGmatABI(CGM);
}
