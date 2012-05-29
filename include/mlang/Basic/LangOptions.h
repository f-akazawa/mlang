//===--- LangOptions.h - M Language Options ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file defines the LangOptions interface.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_BASIC_LANG_OPTIONS_H_
#define MLANG_BASIC_LANG_OPTIONS_H_

#include <string>

namespace mlang {

/// LangOptions - This class keeps track of the various options that can be
/// enabled, which controls the dialect of C that is accepted.
class LangOptions {
public:
	unsigned BCPLComment :1; // BCPL-style '%' comments.
	unsigned MATLABKeywords :1; // True if MATLAB-only keywords are allowed
	unsigned OCTAVEKeywords :1; // True if OCTAVE-only keywords are allowed
	unsigned AsmPreprocessor   : 1;  // Preprocessor in asm mode.
	unsigned GPUMode :1; // Ture if in GPU accelerating mode enabled
	unsigned NoBuiltin :1; // Do not use builtin functions (-fno-builtin)
	unsigned ThreadsafeStatics :1; // Whether static initializers are protected
	// by locks.
	unsigned POSIXThreads :1; // Compiling with POSIX thread support
	// (-pthread)
	unsigned MathErrno :1; // Math functions must respect errno
	// (modulo the platform support).
	unsigned Optimize :1; // Whether __OPTIMIZE__ should be defined.
	unsigned OptimizeSize :1; // Whether __OPTIMIZE_SIZE__ should be
	// defined.
	unsigned Static :1; // Should __STATIC__ be defined (as
	// opposed to __DYNAMIC__).
	unsigned CharIsSigned :1; // Whether char is a signed or unsigned type
	unsigned ShortWChar :1; // Force wchar_t to be unsigned short int.
	unsigned ShortEnums :1; // The enum type will be equivalent to the
	// smallest integer type with enough room.
	unsigned AltiVec           : 1;  // Support AltiVec-style vector initializers.
	unsigned OpenCL :1; // OpenCL language extensions.
	  unsigned CUDA              : 1; // CUDA C++ language extensions.
	unsigned DumpRecordLayouts : 1;
	unsigned AccessControl : 2;
	unsigned LamdaCal : 1;  // Whether Functional Programming supported
	unsigned ContainerMap : 1; // Whether Container.map implemented as built-in
	unsigned OOP : 1; // Whether Object-Oriented Programming supported
	unsigned Exceptions : 1; // Support exception handling.

private:
	// We declare multibit enums as unsigned because MSVC insists on making enums
	// signed.  Set/Query these values using accessors.
	unsigned GC :2; // Objective-C Garbage Collection modes.
	unsigned SymbolVisibility :3; // Symbol's visibility.
	unsigned StackProtector :2; // Whether stack protectors are on.
	unsigned SignedOverflowBehavior :2; // How to handle signed integer overflow.

public:
	unsigned InstantiationDepth; // Maximum template instantiation depth.
	unsigned NumLargeByValueCopy; // Warn if parameter/return value is larger
	// in bytes than this setting. 0 is no check.

	enum GCMode {
		NonGC, GCOnly, HybridGC
	};
	enum StackProtectorMode {
		SSPOff, SSPOn, SSPReq
	};
	enum SignedOverflowBehaviorTy {
		SOB_Undefined, // Default C standard behavior.
		SOB_Defined, // -fwrapv
		SOB_Trapping
	// -ftrapv
	};

	/// \link Describes the different kinds of visibility that a
	/// declaration may have.  Visibility determines how a declaration
	/// interacts with the dynamic linker.  It may also affect whether the
	/// symbol can be found by runtime symbol lookup APIs.
	///
	/// Visibility is not described in any language standard and
	/// (nonetheless) sometimes has odd behavior.  Not all platforms
	/// support all visibility kinds.
	enum Visibility {
	  /// Objects with "hidden" visibility are not seen by the dynamic
	  /// linker.
	  HiddenVisibility,

	  /// Objects with "protected" visibility are seen by the dynamic
	  /// linker but always dynamically resolve to an object within this
	  /// shared object.
	  ProtectedVisibility,

	  /// Objects with "default" visibility are seen by the dynamic linker
	  /// and act like normal objects.
	  DefaultVisibility
	};

	/// The name of the handler function to be called when -ftrapv is specified.
	/// If none is specified, abort (GCC-compatible behaviour).
	std::string OverflowHandler;

	LangOptions() {
		BCPLComment = 1;
		MATLABKeywords = 0;
		OCTAVEKeywords = 0;
		AsmPreprocessor = 0;
		GPUMode = 0;
		GC = 0;
		NoBuiltin = 0;
		OpenCL = CUDA = AltiVec = 0;
		StackProtector = 0;
		SymbolVisibility = (unsigned) DefaultVisibility;
		ThreadsafeStatics = 1;
		POSIXThreads = 0;
		MathErrno = 1;
		SignedOverflowBehavior = SOB_Undefined;
		SignedOverflowBehavior = 0;
		InstantiationDepth = 1024;
		NumLargeByValueCopy = 0;
		Optimize = 0;
		OptimizeSize = 0;
		Static = 0;
		CharIsSigned = 1;
		ShortWChar = 0;
		ShortEnums = 0;
		DumpRecordLayouts = 0;
		AccessControl = 0;
		LamdaCal = 0;
		ContainerMap = 0;
		OOP = 1;
		Exceptions = 0;
	}

	GCMode getGCMode() const {
		return (GCMode) GC;
	}
	void setGCMode(GCMode m) {
		GC = (unsigned) m;
	}

	StackProtectorMode getStackProtectorMode() const {
		return static_cast<StackProtectorMode> (StackProtector);
	}
	void setStackProtectorMode(StackProtectorMode m) {
		StackProtector = static_cast<unsigned> (m);
	}

	SignedOverflowBehaviorTy getSignedOverflowBehavior() const {
		return (SignedOverflowBehaviorTy) SignedOverflowBehavior;
	}

	void setSignedOverflowBehavior(SignedOverflowBehaviorTy V) {
		SignedOverflowBehavior = (unsigned) V;
	}

	inline Visibility minVisibility(Visibility L, Visibility R) {
	  return L < R ? L : R;
	}
};
} // end namespace mlang

#endif /* MLANG_BASIC_LANG_OPTIONS_H_ */
