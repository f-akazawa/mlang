//===--- Targets.cpp - various target ---------------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
//  This file implements TargetInfo interface.
//
//===----------------------------------------------------------------------===//

#include "mlang/Basic/TargetInfo.h"
#include "mlang/Basic/LangOptions.h"
#include "mlang/Basic/MacroBuilder.h"
#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/Type.h"
#include <algorithm>

using namespace mlang;

//===----------------------------------------------------------------------===//
//  Common code shared among targets.
//===----------------------------------------------------------------------===//

/// DefineStd - Define a macro name and standard variants.  For example if
/// MacroName is "unix", then this will define "__unix", "__unix__", and "unix"
/// when in GNU mode.
static void DefineStd(MacroBuilder &Builder, llvm::StringRef MacroName,
                      const LangOptions &Opts) {
  assert(MacroName[0] != '_' && "Identifier should be in the user's namespace");

  // If in GNU mode (e.g. -std=gnu99 but not -std=c99) define the raw identifier
  // in the user's namespace.
//  if (Opts.GNUMode)
//    Builder.defineMacro(MacroName);

  // Define __unix.
  Builder.defineMacro("__" + MacroName);

  // Define __unix__.
  Builder.defineMacro("__" + MacroName + "__");
}

//===----------------------------------------------------------------------===//
// Defines specific to certain operating systems.
//===----------------------------------------------------------------------===//

namespace {
template<typename TgtInfo>
class OSTargetInfo : public TgtInfo {
protected:
  virtual void getOSDefines(const LangOptions &Opts, const llvm::Triple &Triple,
                            MacroBuilder &Builder) const=0;
public:
  OSTargetInfo(const std::string& triple) : TgtInfo(triple) {}
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const {
    TgtInfo::getTargetDefines(Opts, Builder);
    getOSDefines(Opts, TgtInfo::getTriple(), Builder);
  }

};
} // end anonymous namespace


namespace {
// Linux target
template<typename Target>
class LinuxTargetInfo : public OSTargetInfo<Target> {
protected:
  virtual void getOSDefines(const LangOptions &Opts, const llvm::Triple &Triple,
                            MacroBuilder &Builder) const {
    // Linux defines; list based off of gcc output
    DefineStd(Builder, "unix", Opts);
    DefineStd(Builder, "linux", Opts);
    Builder.defineMacro("__gnu_linux__");
    Builder.defineMacro("__ELF__");
    if (Opts.POSIXThreads)
      Builder.defineMacro("_REENTRANT");
//    if (Opts.CPlusPlus)
//      Builder.defineMacro("_GNU_SOURCE");
  }
public:
  LinuxTargetInfo(const std::string& triple)
    : OSTargetInfo<Target>(triple) {
    this->UserLabelPrefix = "";
  }
};

// Windows target
template<typename Target>
class WindowsTargetInfo : public OSTargetInfo<Target> {
protected:
  virtual void getOSDefines(const LangOptions &Opts, const llvm::Triple &Triple,
                            MacroBuilder &Builder) const {
    Builder.defineMacro("_WIN32");
  }
  void getVisualStudioDefines(const LangOptions &Opts,
                              MacroBuilder &Builder) const {
//    if (Opts.CPlusPlus) {
//      if (Opts.RTTI)
//        Builder.defineMacro("_CPPRTTI");
//
//      if (Opts.Exceptions)
//        Builder.defineMacro("_CPPUNWIND");
//    }

    if (!Opts.CharIsSigned)
      Builder.defineMacro("_CHAR_UNSIGNED");

    // FIXME: POSIXThreads isn't exactly the option this should be defined for,
    //        but it works for now.
    if (Opts.POSIXThreads)
      Builder.defineMacro("_MT");

//    if (Opts.MSCVersion != 0)
//      Builder.defineMacro("_MSC_VER", llvm::Twine(Opts.MSCVersion));
//
//    if (Opts.Microsoft) {
//      Builder.defineMacro("_MSC_EXTENSIONS");
//
//      if (Opts.CPlusPlus0x) {
//        Builder.defineMacro("_RVALUE_REFERENCES_V2_SUPPORTED");
//        Builder.defineMacro("_RVALUE_REFERENCES_SUPPORTED");
//        Builder.defineMacro("_NATIVE_NULLPTR_SUPPORTED");
//      }
//    }

    Builder.defineMacro("_INTEGRAL_MAX_BITS", "64");
  }

public:
  WindowsTargetInfo(const std::string &triple)
    : OSTargetInfo<Target>(triple) {}
};

} // end anonymous namespace.

//===----------------------------------------------------------------------===//
// Specific target implementations.
//===----------------------------------------------------------------------===//

namespace {
// Namespace for x86 abstract base class
const Builtin::Info BuiltinInfo[] = {
#define BUILTIN(ID, RETTYPE, ARGTYPE, ATTRS) \
	{ #ID, RETTYPE, ARGTYPE, ATTRS, 0, ALL_LANGUAGES, false },
#define LIBBUILTIN(ID, RETTYPE, ARGTYPE, ATTRS, HEADER) \
	{ #ID, RETTYPE, ARGTYPE, ATTRS, HEADER, ALL_LANGUAGES, false },
#include "mlang/Basic/BuiltinsX86.def"
};

static const char* const GCCRegNames[] = {
  "ax", "dx", "cx", "bx", "si", "di", "bp", "sp",
  "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)",
  "argp", "flags", "fspr", "dirflag", "frame",
  "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
  "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
};

const TargetInfo::GCCRegAlias GCCRegAliases[] = {
  { { "al", "ah", "eax", "rax" }, "ax" },
  { { "bl", "bh", "ebx", "rbx" }, "bx" },
  { { "cl", "ch", "ecx", "rcx" }, "cx" },
  { { "dl", "dh", "edx", "rdx" }, "dx" },
  { { "esi", "rsi" }, "si" },
  { { "edi", "rdi" }, "di" },
  { { "esp", "rsp" }, "sp" },
  { { "ebp", "rbp" }, "bp" },
};

// X86 target abstract base class; x86-32 and x86-64 are very close, so
// most of the implementation can be shared.
class X86TargetInfo : public TargetInfo {
  enum X86SSEEnum {
    NoMMXSSE, MMX, SSE1, SSE2, SSE3, SSSE3, SSE41, SSE42
  } SSELevel;
  enum AMD3DNowEnum {
    NoAMD3DNow, AMD3DNow, AMD3DNowAthlon
  } AMD3DNowLevel;

  bool HasAES;
  bool HasAVX;

public:
  X86TargetInfo(const std::string& triple)
    : TargetInfo(triple), SSELevel(NoMMXSSE), AMD3DNowLevel(NoAMD3DNow),
      HasAES(false), HasAVX(false) {
    LongDoubleFormat = &llvm::APFloat::x87DoubleExtended;
  }
  virtual void getTargetBuiltins(const Builtin::Info *&Records,
                                 unsigned &NumRecords) const {
    Records = BuiltinInfo;
    NumRecords = mlang::X86::LastTSBuiltin-Builtin::FirstTSBuiltin;
  }
  virtual void getGCCRegNames(const char * const *&Names,
                              unsigned &NumNames) const {
    Names = GCCRegNames;
    NumNames = llvm::array_lengthof(GCCRegNames);
  }
  virtual void getGCCRegAliases(const GCCRegAlias *&Aliases,
                                unsigned &NumAliases) const {
    Aliases = GCCRegAliases;
    NumAliases = llvm::array_lengthof(GCCRegAliases);
  }
  virtual bool validateAsmConstraint(const char *&Name,
                                     TargetInfo::ConstraintInfo &info) const;
  virtual llvm::Type* adjustInlineAsmType(std::string& Constraint,
                                     llvm::Type* Ty,
                                     llvm::LLVMContext& Context) const;
  virtual std::string convertConstraint(const char Constraint) const;
  virtual const char *getClobbers() const {
    return "~{dirflag},~{fpsr},~{flags}";
  }
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const;
  virtual bool setFeatureEnabled(llvm::StringMap<bool> &Features,
                                 const std::string &Name,
                                 bool Enabled) const;
  virtual void getDefaultFeatures(const std::string &CPU,
                                  llvm::StringMap<bool> &Features) const;
  virtual void HandleTargetFeatures(std::vector<std::string> &Features);
};

void X86TargetInfo::getDefaultFeatures(const std::string &CPU,
                                       llvm::StringMap<bool> &Features) const {
  // FIXME: This should not be here.
  Features["3dnow"] = false;
  Features["3dnowa"] = false;
  Features["mmx"] = false;
  Features["sse"] = false;
  Features["sse2"] = false;
  Features["sse3"] = false;
  Features["ssse3"] = false;
  Features["sse41"] = false;
  Features["sse42"] = false;
  Features["aes"] = false;
  Features["avx"] = false;

  // LLVM does not currently recognize this.
  // Features["sse4a"] = false;

  // FIXME: This *really* should not be here.

  // X86_64 always has SSE2.
  if (PointerWidth == 64)
    Features["sse2"] = Features["sse"] = Features["mmx"] = true;

  if (CPU == "generic" || CPU == "i386" || CPU == "i486" || CPU == "i586" ||
      CPU == "pentium" || CPU == "i686" || CPU == "pentiumpro")
    ;
  else if (CPU == "pentium-mmx" || CPU == "pentium2")
    setFeatureEnabled(Features, "mmx", true);
  else if (CPU == "pentium3")
    setFeatureEnabled(Features, "sse", true);
  else if (CPU == "pentium-m" || CPU == "pentium4" || CPU == "x86-64")
    setFeatureEnabled(Features, "sse2", true);
  else if (CPU == "yonah" || CPU == "prescott" || CPU == "nocona")
    setFeatureEnabled(Features, "sse3", true);
  else if (CPU == "core2")
    setFeatureEnabled(Features, "ssse3", true);
  else if (CPU == "penryn") {
    setFeatureEnabled(Features, "sse4", true);
    Features["sse42"] = false;
  } else if (CPU == "atom")
    setFeatureEnabled(Features, "sse3", true);
  else if (CPU == "corei7") {
    setFeatureEnabled(Features, "sse4", true);
    setFeatureEnabled(Features, "aes", true);
  }
  else if (CPU == "k6" || CPU == "winchip-c6")
    setFeatureEnabled(Features, "mmx", true);
  else if (CPU == "k6-2" || CPU == "k6-3" || CPU == "athlon" ||
           CPU == "athlon-tbird" || CPU == "winchip2" || CPU == "c3") {
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "3dnow", true);
  } else if (CPU == "athlon-4" || CPU == "athlon-xp" || CPU == "athlon-mp") {
    setFeatureEnabled(Features, "sse", true);
    setFeatureEnabled(Features, "3dnowa", true);
  } else if (CPU == "k8" || CPU == "opteron" || CPU == "athlon64" ||
           CPU == "athlon-fx") {
    setFeatureEnabled(Features, "sse2", true);
    setFeatureEnabled(Features, "3dnowa", true);
  } else if (CPU == "k8-sse3") {
    setFeatureEnabled(Features, "sse3", true);
    setFeatureEnabled(Features, "3dnowa", true);
  } else if (CPU == "c3-2")
    setFeatureEnabled(Features, "sse", true);
}

bool X86TargetInfo::setFeatureEnabled(llvm::StringMap<bool> &Features,
                                      const std::string &Name,
                                      bool Enabled) const {
  // FIXME: This *really* should not be here.  We need some way of translating
  // options into llvm subtarget features.
  if (!Features.count(Name) &&
      (Name != "sse4" && Name != "sse4.2" && Name != "sse4.1"))
    return false;

  if (Enabled) {
    if (Name == "mmx")
      Features["mmx"] = true;
    else if (Name == "sse")
      Features["mmx"] = Features["sse"] = true;
    else if (Name == "sse2")
      Features["mmx"] = Features["sse"] = Features["sse2"] = true;
    else if (Name == "sse3")
      Features["mmx"] = Features["sse"] = Features["sse2"] =
        Features["sse3"] = true;
    else if (Name == "ssse3")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = true;
    else if (Name == "sse4" || Name == "sse4.2")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = Features["sse42"] = true;
    else if (Name == "sse4.1")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = true;
    else if (Name == "3dnow")
      Features["3dnowa"] = true;
    else if (Name == "3dnowa")
      Features["3dnow"] = Features["3dnowa"] = true;
    else if (Name == "aes")
      Features["aes"] = true;
    else if (Name == "avx")
      Features["avx"] = true;
  } else {
    if (Name == "mmx")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = Features["sse42"] = false;
    else if (Name == "sse")
      Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = Features["sse42"] = false;
    else if (Name == "sse2")
      Features["sse2"] = Features["sse3"] = Features["ssse3"] =
        Features["sse41"] = Features["sse42"] = false;
    else if (Name == "sse3")
      Features["sse3"] = Features["ssse3"] = Features["sse41"] =
        Features["sse42"] = false;
    else if (Name == "ssse3")
      Features["ssse3"] = Features["sse41"] = Features["sse42"] = false;
    else if (Name == "sse4")
      Features["sse41"] = Features["sse42"] = false;
    else if (Name == "sse4.2")
      Features["sse42"] = false;
    else if (Name == "sse4.1")
      Features["sse41"] = Features["sse42"] = false;
    else if (Name == "3dnow")
      Features["3dnow"] = Features["3dnowa"] = false;
    else if (Name == "3dnowa")
      Features["3dnowa"] = false;
    else if (Name == "aes")
      Features["aes"] = false;
    else if (Name == "avx")
      Features["avx"] = false;
  }

  return true;
}

/// HandleTargetOptions - Perform initialization based on the user
/// configured set of features.
void X86TargetInfo::HandleTargetFeatures(std::vector<std::string> &Features) {
  // Remember the maximum enabled sselevel.
  for (unsigned i = 0, e = Features.size(); i !=e; ++i) {
    // Ignore disabled features.
    if (Features[i][0] == '-')
      continue;

    if (Features[i].substr(1) == "aes") {
      HasAES = true;
      continue;
    }

    // FIXME: Not sure yet how to treat AVX in regard to SSE levels.
    // For now let it be enabled together with other SSE levels.
    if (Features[i].substr(1) == "avx") {
      HasAVX = true;
      continue;
    }

    assert(Features[i][0] == '+' && "Invalid target feature!");
    X86SSEEnum Level = llvm::StringSwitch<X86SSEEnum>(Features[i].substr(1))
      .Case("sse42", SSE42)
      .Case("sse41", SSE41)
      .Case("ssse3", SSSE3)
      .Case("sse3", SSE3)
      .Case("sse2", SSE2)
      .Case("sse", SSE1)
      .Case("mmx", MMX)
      .Default(NoMMXSSE);
    SSELevel = std::max(SSELevel, Level);

    AMD3DNowEnum ThreeDNowLevel =
      llvm::StringSwitch<AMD3DNowEnum>(Features[i].substr(1))
        .Case("3dnowa", AMD3DNowAthlon)
        .Case("3dnow", AMD3DNow)
        .Default(NoAMD3DNow);

    AMD3DNowLevel = std::max(AMD3DNowLevel, ThreeDNowLevel);
  }
}

/// X86TargetInfo::getTargetDefines - Return a set of the X86-specific #defines
/// that are not tied to a specific subtarget.
void X86TargetInfo::getTargetDefines(const LangOptions &Opts,
                                     MacroBuilder &Builder) const {
  // Target identification.
  if (PointerWidth == 64) {
    Builder.defineMacro("_LP64");
    Builder.defineMacro("__LP64__");
    Builder.defineMacro("__amd64__");
    Builder.defineMacro("__amd64");
    Builder.defineMacro("__x86_64");
    Builder.defineMacro("__x86_64__");
  } else {
    DefineStd(Builder, "i386", Opts);
  }

  if (HasAES)
    Builder.defineMacro("__AES__");

  if (HasAVX)
    Builder.defineMacro("__AVX__");

  // Target properties.
  Builder.defineMacro("__LITTLE_ENDIAN__");

  // Subtarget options.
  Builder.defineMacro("__nocona");
  Builder.defineMacro("__nocona__");
  Builder.defineMacro("__tune_nocona__");
  Builder.defineMacro("__REGISTER_PREFIX__", "");

  // Define __NO_MATH_INLINES on linux/x86 so that we don't get inline
  // functions in glibc header files that use FP Stack inline asm which the
  // backend can't deal with (PR879).
  Builder.defineMacro("__NO_MATH_INLINES");

  // Each case falls through to the previous one here.
  switch (SSELevel) {
  case SSE42:
    Builder.defineMacro("__SSE4_2__");
  case SSE41:
    Builder.defineMacro("__SSE4_1__");
  case SSSE3:
    Builder.defineMacro("__SSSE3__");
  case SSE3:
    Builder.defineMacro("__SSE3__");
  case SSE2:
    Builder.defineMacro("__SSE2__");
    Builder.defineMacro("__SSE2_MATH__");  // -mfp-math=sse always implied.
  case SSE1:
    Builder.defineMacro("__SSE__");
    Builder.defineMacro("__SSE_MATH__");   // -mfp-math=sse always implied.
  case MMX:
    Builder.defineMacro("__MMX__");
  case NoMMXSSE:
    break;
  }

//  if (Opts.Microsoft && PointerWidth == 32) {
//    switch (SSELevel) {
//    case SSE42:
//    case SSE41:
//    case SSSE3:
//    case SSE3:
//    case SSE2:
//      Builder.defineMacro("_M_IX86_FP", llvm::Twine(2));
//      break;
//    case SSE1:
//      Builder.defineMacro("_M_IX86_FP", llvm::Twine(1));
//      break;
//    default:
//      Builder.defineMacro("_M_IX86_FP", llvm::Twine(0));
//    }
//  }

  // Each case falls through to the previous one here.
  switch (AMD3DNowLevel) {
  case AMD3DNowAthlon:
    Builder.defineMacro("__3dNOW_A__");
  case AMD3DNow:
    Builder.defineMacro("__3dNOW__");
  case NoAMD3DNow:
    break;
  }
}


bool
X86TargetInfo::validateAsmConstraint(const char *&Name,
                                     TargetInfo::ConstraintInfo &Info) const {
  switch (*Name) {
  default: return false;
  case 'Y': // first letter of a pair:
    switch (*(Name+1)) {
    default: return false;
    case '0':  // First SSE register.
    case 't':  // Any SSE register, when SSE2 is enabled.
    case 'i':  // Any SSE register, when SSE2 and inter-unit moves enabled.
    case 'm':  // any MMX register, when inter-unit moves enabled.
      break;   // falls through to setAllowsRegister.
  }
  case 'a': // eax.
  case 'b': // ebx.
  case 'c': // ecx.
  case 'd': // edx.
  case 'S': // esi.
  case 'D': // edi.
  case 'A': // edx:eax.
  case 'f': // any x87 floating point stack register.
  case 't': // top of floating point stack.
  case 'u': // second from top of floating point stack.
  case 'q': // Any register accessible as [r]l: a, b, c, and d.
  case 'y': // Any MMX register.
  case 'x': // Any SSE register.
  case 'Q': // Any register accessible as [r]h: a, b, c, and d.
  case 'R': // "Legacy" registers: ax, bx, cx, dx, di, si, sp, bp.
  case 'l': // "Index" registers: any general register that can be used as an
            // index in a base+index memory access.
    Info.setAllowsRegister();
    return true;
  case 'C': // SSE floating point constant.
  case 'G': // x87 floating point constant.
  case 'e': // 32-bit signed integer constant for use with zero-extending
            // x86_64 instructions.
  case 'Z': // 32-bit unsigned integer constant for use with zero-extending
            // x86_64 instructions.
    return true;
  }
  return false;
}

llvm::Type*
X86TargetInfo::adjustInlineAsmType(std::string& Constraint,
                                   llvm::Type* Ty,
                                   llvm::LLVMContext &Context) const {
  if (Constraint=="y" && Ty->isVectorTy())
    return llvm::Type::getX86_MMXTy(Context);
  return Ty;
}


std::string
X86TargetInfo::convertConstraint(const char Constraint) const {
  switch (Constraint) {
  case 'a': return std::string("{ax}");
  case 'b': return std::string("{bx}");
  case 'c': return std::string("{cx}");
  case 'd': return std::string("{dx}");
  case 'S': return std::string("{si}");
  case 'D': return std::string("{di}");
  case 'p': // address
    return std::string("im");
  case 't': // top of floating point stack.
    return std::string("{st}");
  case 'u': // second from top of floating point stack.
    return std::string("{st(1)}"); // second from top of floating point stack.
  default:
    return std::string(1, Constraint);
  }
}
} // end anonymous namespace

namespace {
// X86-32 generic target
class X86_32TargetInfo : public X86TargetInfo {
public:
  X86_32TargetInfo(const std::string& triple) : X86TargetInfo(triple) {
    DoubleAlign = LongLongAlign = 32;
    LongDoubleWidth = 96;
    LongDoubleAlign = 32;
    DescriptionString = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-"
                        "i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-"
                        "a0:0:64-f80:32:32-n8:16:32";
    SizeType = UInt32;
    PtrDiffType = Int32;
    IntPtrType = Int32;
    RegParmMax = 3;

    // Use fpret for all types.
    RealTypeUsesObjCFPRet = ((1 << TargetInfo::Float) |
                             (1 << TargetInfo::Double) |
                             (1 << TargetInfo::LongDouble));
  }
  virtual const char *getVAListDeclaration() const {
    return "typedef char* __builtin_va_list;";
  }

  int getEHDataRegisterNumber(unsigned RegNo) const {
    if (RegNo == 0) return 0;
    if (RegNo == 1) return 2;
    return -1;
  }
};
} // end anonymous namespace

namespace {
// x86-32 Windows target
class WindowsX86_32TargetInfo : public WindowsTargetInfo<X86_32TargetInfo> {
public:
  WindowsX86_32TargetInfo(const std::string& triple)
    : WindowsTargetInfo<X86_32TargetInfo>(triple) {
    TLSSupported = false;
    WCharType = UInt8;
    DoubleAlign = LongLongAlign = 64;
    DescriptionString = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-"
                        "i64:64:64-f32:32:32-f64:64:64-f80:128:128-v64:64:64-"
                        "v128:128:128-a0:0:64-f80:32:32-n8:16:32";
  }
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const {
    WindowsTargetInfo<X86_32TargetInfo>::getTargetDefines(Opts, Builder);
  }
};
} // end anonymous namespace

namespace {

// x86-32 Windows Visual Studio target
class VisualStudioWindowsX86_32TargetInfo : public WindowsX86_32TargetInfo {
public:
  VisualStudioWindowsX86_32TargetInfo(const std::string& triple)
    : WindowsX86_32TargetInfo(triple) {
    LongDoubleWidth = 64;
    LongDoubleFormat = &llvm::APFloat::IEEEdouble;
  }
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const {
    WindowsX86_32TargetInfo::getTargetDefines(Opts, Builder);
    WindowsX86_32TargetInfo::getVisualStudioDefines(Opts, Builder);
    // The value of the following reflects processor type.
    // 300=386, 400=486, 500=Pentium, 600=Blend (default)
    // We lost the original triple, so we use the default.
    Builder.defineMacro("_M_IX86", "600");
  }
};
} // end anonymous namespace

namespace {
// x86-32 MinGW target
class MinGWX86_32TargetInfo : public WindowsX86_32TargetInfo {
public:
  MinGWX86_32TargetInfo(const std::string& triple)
    : WindowsX86_32TargetInfo(triple) {
  }
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const {
    WindowsX86_32TargetInfo::getTargetDefines(Opts, Builder);
    DefineStd(Builder, "WIN32", Opts);
    DefineStd(Builder, "WINNT", Opts);
    Builder.defineMacro("_X86_");
    Builder.defineMacro("__MSVCRT__");
    Builder.defineMacro("__MINGW32__");
    Builder.defineMacro("__declspec", "__declspec");
  }
};
} // end anonymous namespace

namespace {
// x86-32 Cygwin target
class CygwinX86_32TargetInfo : public X86_32TargetInfo {
public:
  CygwinX86_32TargetInfo(const std::string& triple)
    : X86_32TargetInfo(triple) {
    TLSSupported = false;
    WCharType = UInt8;
    DoubleAlign = LongLongAlign = 64;
    DescriptionString = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-"
                        "i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-"
                        "a0:0:64-f80:32:32-n8:16:32";
  }
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const {
    X86_32TargetInfo::getTargetDefines(Opts, Builder);
    Builder.defineMacro("__CYGWIN__");
    Builder.defineMacro("__CYGWIN32__");
    DefineStd(Builder, "unix", Opts);
//    if (Opts.CPlusPlus)
//      Builder.defineMacro("_GNU_SOURCE");
  }
};
} // end anonymous namespace

namespace {
// x86-64 generic target
class X86_64TargetInfo : public X86TargetInfo {
public:
  X86_64TargetInfo(const std::string &triple) : X86TargetInfo(triple) {
    LongWidth = LongAlign = PointerWidth = PointerAlign = 64;
    LongDoubleWidth = 128;
    LongDoubleAlign = 128;
    LargeArrayMinWidth = 128;
    LargeArrayAlign = 128;
    IntMaxType = Int64;
    UIntMaxType = UInt64;
    Int64Type = Int64;
    RegParmMax = 6;

    DescriptionString = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-"
                        "i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-"
                        "a0:0:64-s0:64:64-f80:128:128-n8:16:32:64";

    // Use fpret only for long double.
    RealTypeUsesObjCFPRet = (1 << TargetInfo::LongDouble);
  }
  virtual const char *getVAListDeclaration() const {
    return "typedef struct __va_list_tag {"
           "  unsigned gp_offset;"
           "  unsigned fp_offset;"
           "  void* overflow_arg_area;"
           "  void* reg_save_area;"
           "} __va_list_tag;"
           "typedef __va_list_tag __builtin_va_list[1];";
  }

  int getEHDataRegisterNumber(unsigned RegNo) const {
    if (RegNo == 0) return 0;
    if (RegNo == 1) return 1;
    return -1;
  }
};
} // end anonymous namespace

namespace {
// x86-64 Windows target
class WindowsX86_64TargetInfo : public WindowsTargetInfo<X86_64TargetInfo> {
public:
  WindowsX86_64TargetInfo(const std::string& triple)
    : WindowsTargetInfo<X86_64TargetInfo>(triple) {
    TLSSupported = false;
    WCharType = UInt8;
    LongWidth = LongAlign = 32;
    DoubleAlign = LongLongAlign = 64;
    IntMaxType = Int64;
    UIntMaxType = Int64;
    Int64Type = Int64;
    SizeType = UInt64;
    PtrDiffType = Int64;
    IntPtrType = Int64;
  }
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const {
    WindowsTargetInfo<X86_64TargetInfo>::getTargetDefines(Opts, Builder);
    Builder.defineMacro("_WIN64");
  }
};
} // end anonymous namespace

namespace {
// x86-64 Windows Visual Studio target
class VisualStudioWindowsX86_64TargetInfo : public WindowsX86_64TargetInfo {
public:
  VisualStudioWindowsX86_64TargetInfo(const std::string& triple)
    : WindowsX86_64TargetInfo(triple) {
  }
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const {
    WindowsX86_64TargetInfo::getTargetDefines(Opts, Builder);
    WindowsX86_64TargetInfo::getVisualStudioDefines(Opts, Builder);
    Builder.defineMacro("_M_X64");
    Builder.defineMacro("_M_AMD64");
  }
  virtual const char *getVAListDeclaration() const {
    return "typedef char* __builtin_va_list;";
  }
};
} // end anonymous namespace

namespace {
// x86-64 MinGW target
class MinGWX86_64TargetInfo : public WindowsX86_64TargetInfo {
public:
  MinGWX86_64TargetInfo(const std::string& triple)
    : WindowsX86_64TargetInfo(triple) {
  }
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const {
    WindowsX86_64TargetInfo::getTargetDefines(Opts, Builder);
    DefineStd(Builder, "WIN64", Opts);
    Builder.defineMacro("__MSVCRT__");
    Builder.defineMacro("__MINGW64__");
    Builder.defineMacro("__declspec");
  }
};
} // end anonymous namespace

//=====================
// target info drivers
//=====================

static TargetInfo *AllocateTarget(const std::string &T) {
	llvm::Triple Triple(T);
	llvm::Triple::OSType os = Triple.getOS();

	switch (Triple.getArch()) {
	default:
		return NULL;

	case llvm::Triple::x86:
		switch (os) {
		case llvm::Triple::Linux:
			return new LinuxTargetInfo<X86_32TargetInfo> (T);
		case llvm::Triple::Cygwin:
			return new CygwinX86_32TargetInfo(T);
		case llvm::Triple::MinGW32:
			return new MinGWX86_32TargetInfo(T);
		case llvm::Triple::Win32:
			return new VisualStudioWindowsX86_32TargetInfo(T);
		default:
			return new X86_32TargetInfo(T);
		}

	case llvm::Triple::x86_64:
		switch (os) {
		case llvm::Triple::Linux:
			return new LinuxTargetInfo<X86_64TargetInfo> (T);
		case llvm::Triple::MinGW32:
			return new MinGWX86_64TargetInfo(T);
		case llvm::Triple::Win32: // This is what Triple.h supports now.
			return new VisualStudioWindowsX86_64TargetInfo(T);
		default:
			return new X86_64TargetInfo(T);
		}
	}
}

/// CreateTargetInfo - Return the target info object for the specified target
/// triple.
TargetInfo *TargetInfo::CreateTargetInfo(Diagnostic &Diags,
                                         TargetOptions &Opts) {
  llvm::Triple Triple(Opts.Triple);

  // Construct the target
  llvm::OwningPtr<TargetInfo> Target(AllocateTarget(Triple.str()));
  if (!Target) {
    Diags.Report(diag::err_target_unknown_triple) << Triple.str();
    return 0;
  }

  // Set the target CPU if specified.
  if (!Opts.CPU.empty() && !Target->setCPU(Opts.CPU)) {
    Diags.Report(diag::err_target_unknown_cpu) << Opts.CPU;
    return 0;
  }

  // Set the target ABI if specified.
  if (!Opts.ABI.empty() && !Target->setABI(Opts.ABI)) {
    Diags.Report(diag::err_target_unknown_abi) << Opts.ABI;
    return 0;
  }

  // Set the target C++ ABI.
  if (!Opts.GmatABI.empty() && !Target->setGmatABI(Opts.GmatABI)) {
    Diags.Report(diag::err_target_unknown_cxxabi) << Opts.GmatABI;
    return 0;
  }

  // Compute the default target features, we need the target to handle this
  // because features may have dependencies on one another.
  llvm::StringMap<bool> Features;
  Target->getDefaultFeatures(Opts.CPU, Features);

  // Apply the user specified deltas.
  for (std::vector<std::string>::const_iterator it = Opts.Features.begin(),
         ie = Opts.Features.end(); it != ie; ++it) {
    const char *Name = it->c_str();

    // Apply the feature via the target.
    if ((Name[0] != '-' && Name[0] != '+') ||
        !Target->setFeatureEnabled(Features, Name + 1, (Name[0] == '+'))) {
      Diags.Report(diag::err_target_invalid_feature) << Name;
      return 0;
    }
  }

  // Add the features to the compile options.
  //
  // FIXME: If we are completely confident that we have the right set, we only
  // need to pass the minuses.
  Opts.Features.clear();
  for (llvm::StringMap<bool>::const_iterator it = Features.begin(),
         ie = Features.end(); it != ie; ++it)
    Opts.Features.push_back(std::string(it->second ? "+" : "-") + it->first().str());
  Target->HandleTargetFeatures(Opts.Features);

  return Target.take();
}
