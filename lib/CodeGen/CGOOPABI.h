//===----- CGOOPABI.h - Interface to C++ ABIs -------------------*- C++ -*-===//
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

#ifndef MLANG_CODEGEN_CXXABI_H_
#define MLANG_CODEGEN_CXXABI_H_

#include "CodeGenFunction.h"

namespace llvm {
  class Constant;
  class Type;
  class Value;

  template <class T> class SmallVectorImpl;
}

namespace mlang {
  class ClassConstructorDefn;
  class ClassDestructorDefn;
  class ClassMethodDefn;
  class MangleContext;
  class MemberDefn;
  class UserClassDefn;

namespace CodeGen {
  class CodeGenFunction;
  class CodeGenModule;


/// Implements C++ ABI-specific code generation functions.
class CGOOPABI {
protected:
  CodeGenModule &CGM;

  CGOOPABI(CodeGenModule &CGM) : CGM(CGM) {}

protected:
  ImplicitParamDefn *&getThisDecl(CodeGenFunction &CGF) {
    return CGF.ClassThisDefn;
  }
  llvm::Value *&getThisValue(CodeGenFunction &CGF) {
    return CGF.ClassThisValue;
  }

  ImplicitParamDefn *&getVTTDecl(CodeGenFunction &CGF) {
    return CGF.ClassVTTDefn;
  }
  llvm::Value *&getVTTValue(CodeGenFunction &CGF) {
    return CGF.ClassVTTValue;
  }

  /// Build a parameter variable suitable for 'this'.
  void BuildThisParam(CodeGenFunction &CGF, FunctionArgList &Params);

  /// Perform prolog initialization of the parameter variable suitable
  /// for 'this' emitted by BuildThisParam.
  void EmitThisParam(CodeGenFunction &CGF);

  ASTContext &getContext() const { return CGM.getContext(); }

public:

  virtual ~CGOOPABI();

  /// Gets the mangle context.
  virtual MangleContext &getMangleContext() = 0;

  /// Find the LLVM type used to represent the given member pointer
  /// type.
//  virtual llvm::Type *
//  ConvertMemberPointerType(const MemberPointerType *MPT);

  /// Load a member function from an object and a member function
  /// pointer.  Apply the this-adjustment and set 'This' to the
  /// adjusted value.
//  virtual llvm::Value *
//  EmitLoadOfMemberFunctionPointer(CodeGenFunction &CGF,
//                                  llvm::Value *&This,
//                                  llvm::Value *MemPtr,
//                                  const MemberPointerType *MPT);

  /// Calculate an l-value from an object and a data member pointer.
//  virtual llvm::Value *EmitMemberDataPointerAddress(CodeGenFunction &CGF,
//                                                    llvm::Value *Base,
//                                                    llvm::Value *MemPtr,
//                                            const MemberPointerType *MPT);

  /// Perform a derived-to-base or base-to-derived member pointer
  /// conversion.
//  virtual llvm::Value *EmitMemberPointerConversion(CodeGenFunction &CGF,
//                                                   const CastExpr *E,
//                                                   llvm::Value *Src);

  /// Perform a derived-to-base or base-to-derived member pointer
  /// conversion on a constant member pointer.
//  virtual llvm::Constant *EmitMemberPointerConversion(llvm::Constant *C,
//                                                      const CastExpr *E);

  /// Return true if the given member pointer can be zero-initialized
  /// (in the C++ sense) with an LLVM zeroinitializer.
  // virtual bool isZeroInitializable(const MemberPointerType *MPT);

  /// Create a null member pointer of the given type.
  // virtual llvm::Constant *EmitNullMemberPointer(const MemberPointerType *MPT);

  /// Create a member pointer for the given method.
  virtual llvm::Constant *EmitMemberPointer(const ClassMethodDefn *MD);

  /// Create a member pointer for the given field.
  virtual llvm::Constant *EmitMemberPointer(const MemberDefn *FD);

  /// Emit a comparison between two member pointers.  Returns an i1.
//  virtual llvm::Value *
//  EmitMemberPointerComparison(CodeGenFunction &CGF,
//                              llvm::Value *L,
//                              llvm::Value *R,
//                              const MemberPointerType *MPT,
//                              bool Inequality);

  /// Determine if a member pointer is non-null.  Returns an i1.
//  virtual llvm::Value *
//  EmitMemberPointerIsNotNull(CodeGenFunction &CGF,
//                             llvm::Value *MemPtr,
//                             const MemberPointerType *MPT);

  /// Build the signature of the given constructor variant by adding
  /// any required parameters.  For convenience, ResTy has been
  /// initialized to 'void', and ArgTys has been initialized with the
  /// type of 'this' (although this may be changed by the ABI) and
  /// will have the formal parameters added to it afterwards.
  ///
  /// If there are ever any ABIs where the implicit parameters are
  /// intermixed with the formal parameters, we can address those
  /// then.
  virtual void BuildConstructorSignature(const ClassConstructorDefn *Ctor,
                                         ClassCtorType T,
                                         Type &ResTy,
                               llvm::SmallVectorImpl<Type> &ArgTys) = 0;

  /// Build the signature of the given destructor variant by adding
  /// any required parameters.  For convenience, ResTy has been
  /// initialized to 'void' and ArgTys has been initialized with the
  /// type of 'this' (although this may be changed by the ABI).
  virtual void BuildDestructorSignature(const ClassDestructorDefn *Dtor,
                                        ClassDtorType T,
                                        Type &ResTy,
                               llvm::SmallVectorImpl<Type> &ArgTys) = 0;

  /// Build the ABI-specific portion of the parameter list for a
  /// function.  This generally involves a 'this' parameter and
  /// possibly some extra data for constructors and destructors.
  ///
  /// ABIs may also choose to override the return type, which has been
  /// initialized with the formal return type of the function.
  virtual void BuildInstanceFunctionParams(CodeGenFunction &CGF,
                                           Type &ResTy,
                                           FunctionArgList &Params) = 0;

  /// Emit the ABI-specific prolog for the function.
  virtual void EmitInstanceFunctionProlog(CodeGenFunction &CGF) = 0;

  virtual void EmitReturnFromThunk(CodeGenFunction &CGF,
                                   RValue RV, Type ResultType);

  /**************************** Array cookies ******************************/

  /// Returns the extra size required in order to store the array
  /// cookie for the given type.  May return 0 to indicate that no
  /// array cookie is required.
  ///
  /// Several cases are filtered out before this method is called:
  ///   - non-array allocations never need a cookie
  ///   - calls to ::operator new(size_t, void*) never need a cookie
  ///
  /// \param ElementType - the allocated type of the expression,
  ///   i.e. the pointee type of the expression result type
  virtual CharUnits GetArrayCookieSize(Type ElementType);

  /// Initialize the array cookie for the given allocation.
  ///
  /// \param NewPtr - a char* which is the presumed-non-null
  ///   return value of the allocation function
  /// \param NumElements - the computed number of elements,
  ///   potentially collapsed from the multidimensional array case
  /// \param ElementType - the base element allocated type,
  ///   i.e. the allocated type after stripping all array types
  virtual llvm::Value *InitializeArrayCookie(CodeGenFunction &CGF,
                                             llvm::Value *NewPtr,
                                             llvm::Value *NumElements,
                                             Type ElementType);

  /// Reads the array cookie associated with the given pointer,
  /// if it has one.
  ///
  /// \param Ptr - a pointer to the first element in the array
  /// \param ElementType - the base element type of elements of the array
  /// \param NumElements - an out parameter which will be initialized
  ///   with the number of elements allocated, or zero if there is no
  ///   cookie
  /// \param AllocPtr - an out parameter which will be initialized
  ///   with a char* pointing to the address returned by the allocation
  ///   function
  /// \param CookieSize - an out parameter which will be initialized
  ///   with the size of the cookie, or zero if there is no cookie
  virtual void ReadArrayCookie(CodeGenFunction &CGF, llvm::Value *Ptr,
                               Type ElementType, llvm::Value *&NumElements,
                               llvm::Value *&AllocPtr, CharUnits &CookieSize);

  /*************************** Static local guards ****************************/

  /// Emits the guarded initializer and destructor setup for the given
  /// variable, given that it couldn't be emitted as a constant.
  ///
  /// The variable may be:
  ///   - a static local variable
  ///   - a static data member of a class template instantiation
  virtual void EmitGuardedInit(CodeGenFunction &CGF, const VarDefn &D,
                               llvm::GlobalVariable *DefnPtr);

};

/// Creates an instance of a OOP ABI class.
//FIXME huyabin
CGOOPABI *CreateNVGmatABI(CodeGenModule &CGM);
CGOOPABI *CreateAMDGmatABI(CodeGenModule &CGM);
}
}

#endif /* MLANG_CODEGEN_CXXABI_H_ */
