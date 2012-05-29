//===--- CGVTables.h - Emit LLVM Code for C++ vtables ---------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This contains code dealing with C++ code generation of virtual tables.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_CODEGEN_CGVTABLE_H_
#define MLANG_CODEGEN_CGVTABLE_H_

#include "llvm/ADT/DenseMap.h"
#include "llvm/GlobalVariable.h"
#include "mlang/AST/GlobalDefn.h"

namespace mlang {
  class UserClassDefn;

namespace CodeGen {
  class CodeGenModule;

/// ReturnAdjustment - A return adjustment.
struct ReturnAdjustment {
  /// NonVirtual - The non-virtual adjustment from the derived object to its
  /// nearest virtual base.
  int64_t NonVirtual;
  
  /// VBaseOffsetOffset - The offset (in bytes), relative to the address point 
  /// of the virtual base class offset.
  int64_t VBaseOffsetOffset;
  
  ReturnAdjustment() : NonVirtual(0), VBaseOffsetOffset(0) { }
  
  bool isEmpty() const { return !NonVirtual && !VBaseOffsetOffset; }

  friend bool operator==(const ReturnAdjustment &LHS, 
                         const ReturnAdjustment &RHS) {
    return LHS.NonVirtual == RHS.NonVirtual && 
      LHS.VBaseOffsetOffset == RHS.VBaseOffsetOffset;
  }

  friend bool operator<(const ReturnAdjustment &LHS,
                        const ReturnAdjustment &RHS) {
    if (LHS.NonVirtual < RHS.NonVirtual)
      return true;
    
    return LHS.NonVirtual == RHS.NonVirtual && 
      LHS.VBaseOffsetOffset < RHS.VBaseOffsetOffset;
  }
};
  
/// ThisAdjustment - A 'this' pointer adjustment.
struct ThisAdjustment {
  /// NonVirtual - The non-virtual adjustment from the derived object to its
  /// nearest virtual base.
  int64_t NonVirtual;

  /// VCallOffsetOffset - The offset (in bytes), relative to the address point,
  /// of the virtual call offset.
  int64_t VCallOffsetOffset;
  
  ThisAdjustment() : NonVirtual(0), VCallOffsetOffset(0) { }

  bool isEmpty() const { return !NonVirtual && !VCallOffsetOffset; }

  friend bool operator==(const ThisAdjustment &LHS, 
                         const ThisAdjustment &RHS) {
    return LHS.NonVirtual == RHS.NonVirtual && 
      LHS.VCallOffsetOffset == RHS.VCallOffsetOffset;
  }
  
  friend bool operator<(const ThisAdjustment &LHS,
                        const ThisAdjustment &RHS) {
    if (LHS.NonVirtual < RHS.NonVirtual)
      return true;
    
    return LHS.NonVirtual == RHS.NonVirtual && 
      LHS.VCallOffsetOffset < RHS.VCallOffsetOffset;
  }
};

/// ThunkInfo - The 'this' pointer adjustment as well as an optional return
/// adjustment for a thunk.
struct ThunkInfo {
  /// This - The 'this' pointer adjustment.
  ThisAdjustment This;
    
  /// Return - The return adjustment.
  ReturnAdjustment Return;

  ThunkInfo() { }

  ThunkInfo(const ThisAdjustment &This, const ReturnAdjustment &Return)
    : This(This), Return(Return) { }

  friend bool operator==(const ThunkInfo &LHS, const ThunkInfo &RHS) {
    return LHS.This == RHS.This && LHS.Return == RHS.Return;
  }

  friend bool operator<(const ThunkInfo &LHS, const ThunkInfo &RHS) {
    if (LHS.This < RHS.This)
      return true;
      
    return LHS.This == RHS.This && LHS.Return < RHS.Return;
  }

  bool isEmpty() const { return This.isEmpty() && Return.isEmpty(); }
};  

// BaseSubobject - Uniquely identifies a direct or indirect base class. 
// Stores both the base class decl and the offset from the most derived class to
// the base class.
class BaseSubobject {
  /// Base - The base class declaration.
  const UserClassDefn *Base;
  
  /// BaseOffset - The offset from the most derived class to the base class.
  uint64_t BaseOffset;
  
public:
  BaseSubobject(const UserClassDefn *Base, uint64_t BaseOffset)
    : Base(Base), BaseOffset(BaseOffset) { }
  
  /// getBase - Returns the base class declaration.
  const UserClassDefn *getBase() const { return Base; }

  /// getBaseOffset - Returns the base class offset.
  uint64_t getBaseOffset() const { return BaseOffset; }

  friend bool operator==(const BaseSubobject &LHS, const BaseSubobject &RHS) {
    return LHS.Base == RHS.Base && LHS.BaseOffset == RHS.BaseOffset;
 }
};

} // end namespace CodeGen
} // end namespace mlang

namespace llvm {

template<> struct DenseMapInfo<mlang::CodeGen::BaseSubobject> {
  static mlang::CodeGen::BaseSubobject getEmptyKey() {
    return mlang::CodeGen::BaseSubobject(
      DenseMapInfo<const mlang::UserClassDefn *>::getEmptyKey(),
      DenseMapInfo<uint64_t>::getEmptyKey());
  }

  static mlang::CodeGen::BaseSubobject getTombstoneKey() {
    return mlang::CodeGen::BaseSubobject(
      DenseMapInfo<const mlang::UserClassDefn *>::getTombstoneKey(),
      DenseMapInfo<uint64_t>::getTombstoneKey());
  }

  static unsigned getHashValue(const mlang::CodeGen::BaseSubobject &Base) {
    return 
      DenseMapInfo<const mlang::UserClassDefn *>::getHashValue(Base.getBase()) ^
      DenseMapInfo<uint64_t>::getHashValue(Base.getBaseOffset());
  }

  static bool isEqual(const mlang::CodeGen::BaseSubobject &LHS,
                      const mlang::CodeGen::BaseSubobject &RHS) {
    return LHS == RHS;
  }
};

// It's OK to treat BaseSubobject as a POD type.
template <> struct isPodLike<mlang::CodeGen::BaseSubobject> {
  static const bool value = true;
};

}

namespace mlang {
namespace CodeGen {

class CodeGenVTables {
  CodeGenModule &CGM;

  /// MethodVTableIndices - Contains the index (relative to the vtable address
  /// point) where the function pointer for a virtual function is stored.
  typedef llvm::DenseMap<GlobalDefn, int64_t> MethodVTableIndicesTy;
  MethodVTableIndicesTy MethodVTableIndices;

  typedef std::pair<const UserClassDefn *,
                    const UserClassDefn *> ClassPairTy;

  /// VirtualBaseClassOffsetOffsets - Contains the vtable offset (relative to 
  /// the address point) in bytes where the offsets for virtual bases of a class
  /// are stored.
  typedef llvm::DenseMap<ClassPairTy, int64_t> 
    VirtualBaseClassOffsetOffsetsMapTy;
  VirtualBaseClassOffsetOffsetsMapTy VirtualBaseClassOffsetOffsets;

  /// VTables - All the vtables which have been defined.
  llvm::DenseMap<const UserClassDefn *, llvm::GlobalVariable *> VTables;
  
  /// NumVirtualFunctionPointers - Contains the number of virtual function 
  /// pointers in the vtable for a given record decl.
  llvm::DenseMap<const UserClassDefn *, uint64_t> NumVirtualFunctionPointers;

  typedef llvm::SmallVector<ThunkInfo, 1> ThunkInfoVectorTy;
  typedef llvm::DenseMap<const ClassMethodDefn *, ThunkInfoVectorTy> ThunksMapTy;
  
  /// Thunks - Contains all thunks that a given method decl will need.
  ThunksMapTy Thunks;

  // The layout entry and a bool indicating whether we've actually emitted
  // the vtable.
  typedef llvm::PointerIntPair<uint64_t *, 1, bool> VTableLayoutData;
  typedef llvm::DenseMap<const UserClassDefn *, VTableLayoutData>
    VTableLayoutMapTy;
  
  /// VTableLayoutMap - Stores the vtable layout for all record decls.
  /// The layout is stored as an array of 64-bit integers, where the first
  /// integer is the number of vtable entries in the layout, and the subsequent
  /// integers are the vtable components.
  VTableLayoutMapTy VTableLayoutMap;

  typedef std::pair<const UserClassDefn *, BaseSubobject> BaseSubobjectPairTy;
  typedef llvm::DenseMap<BaseSubobjectPairTy, uint64_t> AddressPointsMapTy;
  
  /// Address points - Address points for all vtables.
  AddressPointsMapTy AddressPoints;

  /// VTableAddressPointsMapTy - Address points for a single vtable.
  typedef llvm::DenseMap<BaseSubobject, uint64_t> VTableAddressPointsMapTy;

  typedef llvm::SmallVector<std::pair<uint64_t, ThunkInfo>, 1> 
    VTableThunksTy;
  
  typedef llvm::DenseMap<const UserClassDefn *, VTableThunksTy>
    VTableThunksMapTy;
  
  /// VTableThunksMap - Contains thunks needed by vtables.
  VTableThunksMapTy VTableThunksMap;
  
  uint64_t getNumVTableComponents(const UserClassDefn *RD) const {
    assert(VTableLayoutMap.count(RD) && "No vtable layout for this class!");
    
    return VTableLayoutMap.lookup(RD).getPointer()[0];
  }

  const uint64_t *getVTableComponentsData(const UserClassDefn *RD) const {
    assert(VTableLayoutMap.count(RD) && "No vtable layout for this class!");

    uint64_t *Components = VTableLayoutMap.lookup(RD).getPointer();
    return &Components[1];
  }

  typedef llvm::DenseMap<BaseSubobjectPairTy, uint64_t> SubVTTIndiciesMapTy;
  
  /// SubVTTIndicies - Contains indices into the various sub-VTTs.
  SubVTTIndiciesMapTy SubVTTIndicies;

  typedef llvm::DenseMap<BaseSubobjectPairTy, uint64_t>
    SecondaryVirtualPointerIndicesMapTy;

  /// SecondaryVirtualPointerIndices - Contains the secondary virtual pointer
  /// indices.
  SecondaryVirtualPointerIndicesMapTy SecondaryVirtualPointerIndices;

  /// getNumVirtualFunctionPointers - Return the number of virtual function
  /// pointers in the vtable for a given record decl.
  uint64_t getNumVirtualFunctionPointers(const UserClassDefn *RD);
  
  void ComputeMethodVTableIndices(const UserClassDefn *RD);

  llvm::GlobalVariable *GenerateVTT(llvm::GlobalVariable::LinkageTypes Linkage,
                                    bool GenerateDefinition,
                                    const UserClassDefn *RD);

  /// EmitThunk - Emit a single thunk.
  void EmitThunk(GlobalDefn GD, const ThunkInfo &Thunk);
  
  /// ComputeVTableRelatedInformation - Compute and store all vtable related
  /// information (vtable layout, vbase offset offsets, thunks etc) for the
  /// given record decl.
  void ComputeVTableRelatedInformation(const UserClassDefn *RD,
                                       bool VTableRequired);

  /// CreateVTableInitializer - Create a vtable initializer for the given record
  /// decl.
  /// \param Components - The vtable components; this is really an array of
  /// VTableComponents.
  llvm::Constant *CreateVTableInitializer(const UserClassDefn *RD,
                                          const uint64_t *Components, 
                                          unsigned NumComponents,
                                          const VTableThunksTy &VTableThunks);

public:
  CodeGenVTables(CodeGenModule &CGM)
    : CGM(CGM) { }

  /// \brief True if the VTable of this record must be emitted in the
  /// translation unit.
  bool ShouldEmitVTableInThisTU(const UserClassDefn *RD);

  /// needsVTTParameter - Return whether the given global decl needs a VTT
  /// parameter, which it does if it's a base constructor or destructor with
  /// virtual bases.
  static bool needsVTTParameter(GlobalDefn GD);

  /// getSubVTTIndex - Return the index of the sub-VTT for the base class of the
  /// given record decl.
  uint64_t getSubVTTIndex(const UserClassDefn *RD, BaseSubobject Base);
  
  /// getSecondaryVirtualPointerIndex - Return the index in the VTT where the
  /// virtual pointer for the given subobject is located.
  uint64_t getSecondaryVirtualPointerIndex(const UserClassDefn *RD,
                                           BaseSubobject Base);

  /// getMethodVTableIndex - Return the index (relative to the vtable address
  /// point) where the function pointer for the given virtual function is
  /// stored.
  uint64_t getMethodVTableIndex(GlobalDefn GD);

  /// getVirtualBaseOffsetOffset - Return the offset in bytes (relative to the
  /// vtable address point) where the offset of the virtual base that contains 
  /// the given base is stored, otherwise, if no virtual base contains the given
  /// class, return 0.  Base must be a virtual base class or an unambigious
  /// base.
  CharUnits getVirtualBaseOffsetOffset(const UserClassDefn *RD,
                                     const UserClassDefn *VBase);

  /// getAddressPoint - Get the address point of the given subobject in the
  /// class decl.
  uint64_t getAddressPoint(BaseSubobject Base, const UserClassDefn *RD);
  
  /// GetAddrOfVTable - Get the address of the vtable for the given record decl.
  llvm::GlobalVariable *GetAddrOfVTable(const UserClassDefn *RD);

  /// EmitVTableDefinition - Emit the definition of the given vtable.
  void EmitVTableDefinition(llvm::GlobalVariable *VTable,
                            llvm::GlobalVariable::LinkageTypes Linkage,
                            const UserClassDefn *RD);
  
  /// GenerateConstructionVTable - Generate a construction vtable for the given 
  /// base subobject.
  llvm::GlobalVariable *
  GenerateConstructionVTable(const UserClassDefn *RD, const BaseSubobject &Base, 
                             bool BaseIsVirtual, 
                             VTableAddressPointsMapTy& AddressPoints);
  
  llvm::GlobalVariable *getVTT(const UserClassDefn *RD);

  /// EmitThunks - Emit the associated thunks for the given global decl.
  void EmitThunks(GlobalDefn GD);
    
  /// GenerateClassData - Generate all the class data required to be generated
  /// upon definition of a KeyFunction.  This includes the vtable, the
  /// rtti data structure and the VTT.
  ///
  /// \param Linkage - The desired linkage of the vtable, the RTTI and the VTT.
  void GenerateClassData(llvm::GlobalVariable::LinkageTypes Linkage,
                         const UserClassDefn *RD);
};

} // end namespace CodeGen
} // end namespace mlang
#endif /* MLANG_CODEGEN_CGVTABLE_H_ */
