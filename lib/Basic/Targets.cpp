//===--- Targets.cpp - Implement -arch option and targets -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements construction of a TargetInfo object from a
// target triple.
//
//===----------------------------------------------------------------------===//

#include "mlang/Basic/TargetInfo.h"
#include "mlang/Basic/Builtins.h"
#include "mlang/Basic/LangOptions.h"
#include "mlang/Basic/MacroBuilder.h"
#include "mlang/Basic/TargetBuiltins.h"
#include "mlang/Basic/TargetOptions.h"
#include "mlang/Diag/Diagnostic.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Type.h"
#include <algorithm>
using namespace mlang;

//===----------------------------------------------------------------------===//
//  Common code shared among targets.
//===----------------------------------------------------------------------===//

/// DefineStd - Define a macro name and standard variants.  For example if
/// MacroName is "unix", then this will define "__unix", "__unix__", and "unix"
/// when in GNU mode.
static void DefineStd(MacroBuilder &Builder, StringRef MacroName,
                      const LangOptions &Opts) {
  assert(MacroName[0] != '_' && "Identifier should be in the user's namespace");

  // Define __unix.
  Builder.defineMacro("__" + MacroName);

  // Define __unix__.
  Builder.defineMacro("__" + MacroName + "__");
}

static void defineCPUMacros(MacroBuilder &Builder, StringRef CPUName,
                            bool Tuning = true) {
  Builder.defineMacro("__" + CPUName);
  Builder.defineMacro("__" + CPUName + "__");
  if (Tuning)
    Builder.defineMacro("__tune_" + CPUName + "__");
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
    if (Triple.getEnvironment() == llvm::Triple::ANDROIDEABI)
      Builder.defineMacro("__ANDROID__", "1");
    if (Opts.POSIXThreads)
      Builder.defineMacro("_REENTRANT");
  }
public:
  LinuxTargetInfo(const std::string& triple)
    : OSTargetInfo<Target>(triple) {
    this->UserLabelPrefix = "";
    this->WIntType = TargetInfo::UnsignedInt;
  }

  virtual const char *getStaticInitSectionSpecifier() const {
    return ".text.startup";
  }
};

} // end anonymous namespace.
//===----------------------------------------------------------------------===//
// Specific target implementations.
//===----------------------------------------------------------------------===//

namespace {
// Namespace for x86 abstract base class
const Builtin::Info BuiltinInfo[] = {
#define BUILTIN(ID, TYPE, ATTRS) { #ID, TYPE, ATTRS, 0, ALL_LANGUAGES },
#define LIBBUILTIN(ID, TYPE, ATTRS, HEADER) { #ID, TYPE, ATTRS, HEADER,\
                                              ALL_LANGUAGES },
#include "mlang/Basic/BuiltinsX86.def"
};

static const char* const GCCRegNames[] = {
  "ax", "dx", "cx", "bx", "si", "di", "bp", "sp",
  "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)",
  "argp", "flags", "fpcr", "fpsr", "dirflag", "frame",
  "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
  "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
  "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
  "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15",
};

const TargetInfo::AddlRegName AddlRegNames[] = {
  { { "al", "ah", "eax", "rax" }, 0 },
  { { "bl", "bh", "ebx", "rbx" }, 3 },
  { { "cl", "ch", "ecx", "rcx" }, 2 },
  { { "dl", "dh", "edx", "rdx" }, 1 },
  { { "esi", "rsi" }, 4 },
  { { "edi", "rdi" }, 5 },
  { { "esp", "rsp" }, 7 },
  { { "ebp", "rbp" }, 6 },
};

// X86 target abstract base class; x86-32 and x86-64 are very close, so
// most of the implementation can be shared.
class X86TargetInfo : public TargetInfo {
  enum X86SSEEnum {
    NoSSE, SSE1, SSE2, SSE3, SSSE3, SSE41, SSE42, AVX, AVX2
  } SSELevel;
  enum MMX3DNowEnum {
    NoMMX3DNow, MMX, AMD3DNow, AMD3DNowAthlon
  } MMX3DNowLevel;

  bool HasAES;
  bool HasLZCNT;
  bool HasBMI;
  bool HasBMI2;
  bool HasPOPCNT;
  bool HasFMA4;

  /// \brief Enumeration of all of the X86 CPUs supported by Clang.
  ///
  /// Each enumeration represents a particular CPU supported by Clang. These
  /// loosely correspond to the options passed to '-march' or '-mtune' flags.
  enum CPUKind {
    CK_Generic,

    /// \name i386
    /// i386-generation processors.
    //@{
    CK_i386,
    //@}

    /// \name i486
    /// i486-generation processors.
    //@{
    CK_i486,
    CK_WinChipC6,
    CK_WinChip2,
    CK_C3,
    //@}

    /// \name i586
    /// i586-generation processors, P5 microarchitecture based.
    //@{
    CK_i586,
    CK_Pentium,
    CK_PentiumMMX,
    //@}

    /// \name i686
    /// i686-generation processors, P6 / Pentium M microarchitecture based.
    //@{
    CK_i686,
    CK_PentiumPro,
    CK_Pentium2,
    CK_Pentium3,
    CK_Pentium3M,
    CK_PentiumM,
    CK_C3_2,

    /// This enumerator is a bit odd, as GCC no longer accepts -march=yonah.
    /// Clang however has some logic to suport this.
    // FIXME: Warn, deprecate, and potentially remove this.
    CK_Yonah,
    //@}

    /// \name Netburst
    /// Netburst microarchitecture based processors.
    //@{
    CK_Pentium4,
    CK_Pentium4M,
    CK_Prescott,
    CK_Nocona,
    //@}

    /// \name Core
    /// Core microarchitecture based processors.
    //@{
    CK_Core2,

    /// This enumerator, like \see CK_Yonah, is a bit odd. It is another
    /// codename which GCC no longer accepts as an option to -march, but Clang
    /// has some logic for recognizing it.
    // FIXME: Warn, deprecate, and potentially remove this.
    CK_Penryn,
    //@}

    /// \name Atom
    /// Atom processors
    //@{
    CK_Atom,
    //@}

    /// \name Nehalem
    /// Nehalem microarchitecture based processors.
    //@{
    CK_Corei7,
    CK_Corei7AVX,
    CK_CoreAVXi,
    CK_CoreAVX2,
    //@}

    /// \name K6
    /// K6 architecture processors.
    //@{
    CK_K6,
    CK_K6_2,
    CK_K6_3,
    //@}

    /// \name K7
    /// K7 architecture processors.
    //@{
    CK_Athlon,
    CK_AthlonThunderbird,
    CK_Athlon4,
    CK_AthlonXP,
    CK_AthlonMP,
    //@}

    /// \name K8
    /// K8 architecture processors.
    //@{
    CK_Athlon64,
    CK_Athlon64SSE3,
    CK_AthlonFX,
    CK_K8,
    CK_K8SSE3,
    CK_Opteron,
    CK_OpteronSSE3,
    CK_AMDFAM10,
    //@}

    /// \name Bobcat
    /// Bobcat architecture processors.
    //@{
    CK_BTVER1,
    //@}

    /// \name Bulldozer
    /// Bulldozer architecture processors.
    //@{
    CK_BDVER1,
    CK_BDVER2,
    //@}

    /// This specification is deprecated and will be removed in the future.
    /// Users should prefer \see CK_K8.
    // FIXME: Warn on this when the CPU is set to it.
    CK_x86_64,
    //@}

    /// \name Geode
    /// Geode processors.
    //@{
    CK_Geode
    //@}
  } CPU;

public:
  X86TargetInfo(const std::string& triple)
    : TargetInfo(triple), SSELevel(NoSSE), MMX3DNowLevel(NoMMX3DNow),
      HasAES(false), HasLZCNT(false), HasBMI(false), HasBMI2(false),
      HasPOPCNT(false), HasFMA4(false), CPU(CK_Generic) {
    BigEndian = false;
    LongDoubleFormat = &llvm::APFloat::x87DoubleExtended;
  }
  virtual unsigned getFloatEvalMethod() const {
    // X87 evaluates with 80 bits "long double" precision.
    return SSELevel == NoSSE ? 2 : 0;
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
    Aliases = 0;
    NumAliases = 0;
  }
  virtual void getGCCAddlRegNames(const AddlRegName *&Names,
				  unsigned &NumNames) const {
    Names = AddlRegNames;
    NumNames = llvm::array_lengthof(AddlRegNames);
  }
  virtual bool validateAsmConstraint(const char *&Name,
                                     TargetInfo::ConstraintInfo &info) const;
  virtual std::string convertConstraint(const char *&Constraint) const;
  virtual const char *getClobbers() const {
    return "~{dirflag},~{fpsr},~{flags}";
  }
  virtual void getTargetDefines(const LangOptions &Opts,
                                MacroBuilder &Builder) const;
  virtual bool setFeatureEnabled(llvm::StringMap<bool> &Features,
                                 StringRef Name,
                                 bool Enabled) const;
  virtual void getDefaultFeatures(llvm::StringMap<bool> &Features) const;
  virtual bool hasFeature(StringRef Feature) const;
  virtual void HandleTargetFeatures(std::vector<std::string> &Features);
  virtual const char* getABI() const {
    if (PointerWidth == 64 && SSELevel >= AVX)
      return "avx";
    else if (PointerWidth == 32 && MMX3DNowLevel == NoMMX3DNow)
      return "no-mmx";
    return "";
  }
  virtual bool setCPU(const std::string &Name) {
    CPU = llvm::StringSwitch<CPUKind>(Name)
      .Case("i386", CK_i386)
      .Case("i486", CK_i486)
      .Case("winchip-c6", CK_WinChipC6)
      .Case("winchip2", CK_WinChip2)
      .Case("c3", CK_C3)
      .Case("i586", CK_i586)
      .Case("pentium", CK_Pentium)
      .Case("pentium-mmx", CK_PentiumMMX)
      .Case("i686", CK_i686)
      .Case("pentiumpro", CK_PentiumPro)
      .Case("pentium2", CK_Pentium2)
      .Case("pentium3", CK_Pentium3)
      .Case("pentium3m", CK_Pentium3M)
      .Case("pentium-m", CK_PentiumM)
      .Case("c3-2", CK_C3_2)
      .Case("yonah", CK_Yonah)
      .Case("pentium4", CK_Pentium4)
      .Case("pentium4m", CK_Pentium4M)
      .Case("prescott", CK_Prescott)
      .Case("nocona", CK_Nocona)
      .Case("core2", CK_Core2)
      .Case("penryn", CK_Penryn)
      .Case("atom", CK_Atom)
      .Case("corei7", CK_Corei7)
      .Case("corei7-avx", CK_Corei7AVX)
      .Case("core-avx-i", CK_CoreAVXi)
      .Case("core-avx2", CK_CoreAVX2)
      .Case("k6", CK_K6)
      .Case("k6-2", CK_K6_2)
      .Case("k6-3", CK_K6_3)
      .Case("athlon", CK_Athlon)
      .Case("athlon-tbird", CK_AthlonThunderbird)
      .Case("athlon-4", CK_Athlon4)
      .Case("athlon-xp", CK_AthlonXP)
      .Case("athlon-mp", CK_AthlonMP)
      .Case("athlon64", CK_Athlon64)
      .Case("athlon64-sse3", CK_Athlon64SSE3)
      .Case("athlon-fx", CK_AthlonFX)
      .Case("k8", CK_K8)
      .Case("k8-sse3", CK_K8SSE3)
      .Case("opteron", CK_Opteron)
      .Case("opteron-sse3", CK_OpteronSSE3)
      .Case("amdfam10", CK_AMDFAM10)
      .Case("btver1", CK_BTVER1)
      .Case("bdver1", CK_BDVER1)
      .Case("bdver2", CK_BDVER2)
      .Case("x86-64", CK_x86_64)
      .Case("geode", CK_Geode)
      .Default(CK_Generic);

    // Perform any per-CPU checks necessary to determine if this CPU is
    // acceptable.
    // FIXME: This results in terrible diagnostics. Clang just says the CPU is
    // invalid without explaining *why*.
    switch (CPU) {
    case CK_Generic:
      // No processor selected!
      return false;

    case CK_i386:
    case CK_i486:
    case CK_WinChipC6:
    case CK_WinChip2:
    case CK_C3:
    case CK_i586:
    case CK_Pentium:
    case CK_PentiumMMX:
    case CK_i686:
    case CK_PentiumPro:
    case CK_Pentium2:
    case CK_Pentium3:
    case CK_Pentium3M:
    case CK_PentiumM:
    case CK_Yonah:
    case CK_C3_2:
    case CK_Pentium4:
    case CK_Pentium4M:
    case CK_Prescott:
    case CK_K6:
    case CK_K6_2:
    case CK_K6_3:
    case CK_Athlon:
    case CK_AthlonThunderbird:
    case CK_Athlon4:
    case CK_AthlonXP:
    case CK_AthlonMP:
    case CK_Geode:
      // Only accept certain architectures when compiling in 32-bit mode.
      if (PointerWidth != 32)
        return false;

      // Fallthrough
    case CK_Nocona:
    case CK_Core2:
    case CK_Penryn:
    case CK_Atom:
    case CK_Corei7:
    case CK_Corei7AVX:
    case CK_CoreAVXi:
    case CK_CoreAVX2:
    case CK_Athlon64:
    case CK_Athlon64SSE3:
    case CK_AthlonFX:
    case CK_K8:
    case CK_K8SSE3:
    case CK_Opteron:
    case CK_OpteronSSE3:
    case CK_AMDFAM10:
    case CK_BTVER1:
    case CK_BDVER1:
    case CK_BDVER2:
    case CK_x86_64:
      return true;
    }
    llvm_unreachable("Unhandled CPU kind");
  }
};

void X86TargetInfo::getDefaultFeatures(llvm::StringMap<bool> &Features) const {
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
  Features["sse4a"] = false;
  Features["aes"] = false;
  Features["avx"] = false;
  Features["avx2"] = false;
  Features["lzcnt"] = false;
  Features["bmi"] = false;
  Features["bmi2"] = false;
  Features["popcnt"] = false;
  Features["fma4"] = false;

  // FIXME: This *really* should not be here.

  // X86_64 always has SSE2.
  if (PointerWidth == 64)
    Features["sse2"] = Features["sse"] = Features["mmx"] = true;

  switch (CPU) {
  case CK_Generic:
  case CK_i386:
  case CK_i486:
  case CK_i586:
  case CK_Pentium:
  case CK_i686:
  case CK_PentiumPro:
    break;
  case CK_PentiumMMX:
  case CK_Pentium2:
    setFeatureEnabled(Features, "mmx", true);
    break;
  case CK_Pentium3:
  case CK_Pentium3M:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "sse", true);
    break;
  case CK_PentiumM:
  case CK_Pentium4:
  case CK_Pentium4M:
  case CK_x86_64:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "sse2", true);
    break;
  case CK_Yonah:
  case CK_Prescott:
  case CK_Nocona:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "sse3", true);
    break;
  case CK_Core2:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "ssse3", true);
    break;
  case CK_Penryn:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "sse4.1", true);
    break;
  case CK_Atom:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "ssse3", true);
    break;
  case CK_Corei7:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "sse4", true);
    setFeatureEnabled(Features, "aes", true);
    break;
  case CK_Corei7AVX:
  case CK_CoreAVXi:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "avx", true);
    setFeatureEnabled(Features, "aes", true);
    break;
  case CK_CoreAVX2:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "avx2", true);
    setFeatureEnabled(Features, "aes", true);
    setFeatureEnabled(Features, "lzcnt", true);
    setFeatureEnabled(Features, "bmi", true);
    setFeatureEnabled(Features, "bmi2", true);
    break;
  case CK_K6:
  case CK_WinChipC6:
    setFeatureEnabled(Features, "mmx", true);
    break;
  case CK_K6_2:
  case CK_K6_3:
  case CK_WinChip2:
  case CK_C3:
    setFeatureEnabled(Features, "3dnow", true);
    break;
  case CK_Athlon:
  case CK_AthlonThunderbird:
  case CK_Geode:
    setFeatureEnabled(Features, "3dnowa", true);
    break;
  case CK_Athlon4:
  case CK_AthlonXP:
  case CK_AthlonMP:
    setFeatureEnabled(Features, "sse", true);
    setFeatureEnabled(Features, "3dnowa", true);
    break;
  case CK_K8:
  case CK_Opteron:
  case CK_Athlon64:
  case CK_AthlonFX:
    setFeatureEnabled(Features, "sse2", true);
    setFeatureEnabled(Features, "3dnowa", true);
    break;
  case CK_K8SSE3:
  case CK_OpteronSSE3:
  case CK_Athlon64SSE3:
    setFeatureEnabled(Features, "sse3", true);
    setFeatureEnabled(Features, "3dnowa", true);
    break;
  case CK_AMDFAM10:
    setFeatureEnabled(Features, "sse3", true);
    setFeatureEnabled(Features, "sse4a", true);
    setFeatureEnabled(Features, "3dnowa", true);
    break;
  case CK_BTVER1:
    setFeatureEnabled(Features, "ssse3", true);
    setFeatureEnabled(Features, "sse4a", true);
  case CK_BDVER1:
  case CK_BDVER2:
    setFeatureEnabled(Features, "avx", true);
    setFeatureEnabled(Features, "sse4a", true);
    setFeatureEnabled(Features, "aes", true);
    break;
  case CK_C3_2:
    setFeatureEnabled(Features, "mmx", true);
    setFeatureEnabled(Features, "sse", true);
    break;
  }
}

bool X86TargetInfo::setFeatureEnabled(llvm::StringMap<bool> &Features,
                                      StringRef Name,
                                      bool Enabled) const {
  // FIXME: This *really* should not be here.  We need some way of translating
  // options into llvm subtarget features.
  if (!Features.count(Name) &&
      (Name != "sse4" && Name != "sse4.2" && Name != "sse4.1"))
    return false;

  // FIXME: this should probably use a switch with fall through.

  if (Enabled) {
    if (Name == "mmx")
      Features["mmx"] = true;
    else if (Name == "sse")
      Features["mmx"] = Features["sse"] = true;
    else if (Name == "sse2")
      Features["mmx"] = Features["sse"] = Features["sse2"] = true;
    else if (Name == "sse3")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        true;
    else if (Name == "ssse3")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = true;
    else if (Name == "sse4" || Name == "sse4.2")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = Features["sse42"] =
        Features["popcnt"] = true;
    else if (Name == "sse4.1")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = true;
    else if (Name == "3dnow")
      Features["mmx"] = Features["3dnow"] = true;
    else if (Name == "3dnowa")
      Features["mmx"] = Features["3dnow"] = Features["3dnowa"] = true;
    else if (Name == "aes")
      Features["aes"] = true;
    else if (Name == "avx")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = Features["sse42"] =
        Features["popcnt"] = Features["avx"] = true;
    else if (Name == "avx2")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = Features["sse42"] =
        Features["popcnt"] = Features["avx"] = Features["avx2"] = true;
    else if (Name == "fma4")
        Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = Features["sse42"] =
        Features["popcnt"] = Features["avx"] = Features["fma4"] = true;
    else if (Name == "sse4a")
      Features["mmx"] = Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["lzcnt"] = Features["popcnt"] = Features["sse4a"] = true;
    else if (Name == "lzcnt")
      Features["lzcnt"] = true;
    else if (Name == "bmi")
      Features["bmi"] = true;
    else if (Name == "bmi2")
      Features["bmi2"] = true;
    else if (Name == "popcnt")
      Features["popcnt"] = true;
  } else {
    if (Name == "mmx")
      Features["mmx"] = Features["3dnow"] = Features["3dnowa"] = false;
    else if (Name == "sse")
      Features["sse"] = Features["sse2"] = Features["sse3"] =
        Features["ssse3"] = Features["sse41"] = Features["sse42"] =
        Features["sse4a"] = false;
    else if (Name == "sse2")
      Features["sse2"] = Features["sse3"] = Features["ssse3"] =
        Features["sse41"] = Features["sse42"] = Features["sse4a"] = false;
    else if (Name == "sse3")
      Features["sse3"] = Features["ssse3"] = Features["sse41"] =
        Features["sse42"] = Features["sse4a"] = false;
    else if (Name == "ssse3")
      Features["ssse3"] = Features["sse41"] = Features["sse42"] = false;
    else if (Name == "sse4" || Name == "sse4.1")
      Features["sse41"] = Features["sse42"] = false;
    else if (Name == "sse4.2")
      Features["sse42"] = false;
    else if (Name == "3dnow")
      Features["3dnow"] = Features["3dnowa"] = false;
    else if (Name == "3dnowa")
      Features["3dnowa"] = false;
    else if (Name == "aes")
      Features["aes"] = false;
    else if (Name == "avx")
      Features["avx"] = Features["avx2"] = Features["fma4"] = false;
    else if (Name == "avx2")
      Features["avx2"] = false;
    else if (Name == "sse4a")
      Features["sse4a"] = false;
    else if (Name == "lzcnt")
      Features["lzcnt"] = false;
    else if (Name == "bmi")
      Features["bmi"] = false;
    else if (Name == "bmi2")
      Features["bmi2"] = false;
    else if (Name == "popcnt")
      Features["popcnt"] = false;
    else if (Name == "fma4")
      Features["fma4"] = false;
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

    StringRef Feature = StringRef(Features[i]).substr(1);

    if (Feature == "aes") {
      HasAES = true;
      continue;
    }

    if (Feature == "lzcnt") {
      HasLZCNT = true;
      continue;
    }

    if (Feature == "bmi") {
      HasBMI = true;
      continue;
    }

    if (Feature == "bmi2") {
      HasBMI2 = true;
      continue;
    }

    if (Feature == "popcnt") {
      HasPOPCNT = true;
      continue;
    }

    if (Feature == "fma4") {
      HasFMA4 = true;
      continue;
    }

    assert(Features[i][0] == '+' && "Invalid target feature!");
    X86SSEEnum Level = llvm::StringSwitch<X86SSEEnum>(Feature)
      .Case("avx2", AVX2)
      .Case("avx", AVX)
      .Case("sse42", SSE42)
      .Case("sse41", SSE41)
      .Case("ssse3", SSSE3)
      .Case("sse3", SSE3)
      .Case("sse2", SSE2)
      .Case("sse", SSE1)
      .Default(NoSSE);
    SSELevel = std::max(SSELevel, Level);

    MMX3DNowEnum ThreeDNowLevel =
      llvm::StringSwitch<MMX3DNowEnum>(Feature)
        .Case("3dnowa", AMD3DNowAthlon)
        .Case("3dnow", AMD3DNow)
        .Case("mmx", MMX)
        .Default(NoMMX3DNow);

    MMX3DNowLevel = std::max(MMX3DNowLevel, ThreeDNowLevel);
  }

  // Don't tell the backend if we're turning off mmx; it will end up disabling
  // SSE, which we don't want.
  std::vector<std::string>::iterator it;
  it = std::find(Features.begin(), Features.end(), "-mmx");
  if (it != Features.end())
    Features.erase(it);
}

/// X86TargetInfo::getTargetDefines - Return the set of the X86-specific macro
/// definitions for this particular subtarget.
void X86TargetInfo::getTargetDefines(const LangOptions &Opts,
                                     MacroBuilder &Builder) const {
  // Target identification.
  if (PointerWidth == 64) {
    if (getLongWidth() == 64) {
      Builder.defineMacro("_LP64");
      Builder.defineMacro("__LP64__");
    }
    Builder.defineMacro("__amd64__");
    Builder.defineMacro("__amd64");
    Builder.defineMacro("__x86_64");
    Builder.defineMacro("__x86_64__");
  } else {
    DefineStd(Builder, "i386", Opts);
  }

  // Subtarget options.
  // FIXME: We are hard-coding the tune parameters based on the CPU, but they
  // truly should be based on -mtune options.
  switch (CPU) {
  case CK_Generic:
    break;
  case CK_i386:
    // The rest are coming from the i386 define above.
    Builder.defineMacro("__tune_i386__");
    break;
  case CK_i486:
  case CK_WinChipC6:
  case CK_WinChip2:
  case CK_C3:
    defineCPUMacros(Builder, "i486");
    break;
  case CK_PentiumMMX:
    Builder.defineMacro("__pentium_mmx__");
    Builder.defineMacro("__tune_pentium_mmx__");
    // Fallthrough
  case CK_i586:
  case CK_Pentium:
    defineCPUMacros(Builder, "i586");
    defineCPUMacros(Builder, "pentium");
    break;
  case CK_Pentium3:
  case CK_Pentium3M:
  case CK_PentiumM:
    Builder.defineMacro("__tune_pentium3__");
    // Fallthrough
  case CK_Pentium2:
  case CK_C3_2:
    Builder.defineMacro("__tune_pentium2__");
    // Fallthrough
  case CK_PentiumPro:
    Builder.defineMacro("__tune_i686__");
    Builder.defineMacro("__tune_pentiumpro__");
    // Fallthrough
  case CK_i686:
    Builder.defineMacro("__i686");
    Builder.defineMacro("__i686__");
    // Strangely, __tune_i686__ isn't defined by GCC when CPU == i686.
    Builder.defineMacro("__pentiumpro");
    Builder.defineMacro("__pentiumpro__");
    break;
  case CK_Pentium4:
  case CK_Pentium4M:
    defineCPUMacros(Builder, "pentium4");
    break;
  case CK_Yonah:
  case CK_Prescott:
  case CK_Nocona:
    defineCPUMacros(Builder, "nocona");
    break;
  case CK_Core2:
  case CK_Penryn:
    defineCPUMacros(Builder, "core2");
    break;
  case CK_Atom:
    defineCPUMacros(Builder, "atom");
    break;
  case CK_Corei7:
  case CK_Corei7AVX:
  case CK_CoreAVXi:
  case CK_CoreAVX2:
    defineCPUMacros(Builder, "corei7");
    break;
  case CK_K6_2:
    Builder.defineMacro("__k6_2__");
    Builder.defineMacro("__tune_k6_2__");
    // Fallthrough
  case CK_K6_3:
    if (CPU != CK_K6_2) {  // In case of fallthrough
      // FIXME: GCC may be enabling these in cases where some other k6
      // architecture is specified but -m3dnow is explicitly provided. The
      // exact semantics need to be determined and emulated here.
      Builder.defineMacro("__k6_3__");
      Builder.defineMacro("__tune_k6_3__");
    }
    // Fallthrough
  case CK_K6:
    defineCPUMacros(Builder, "k6");
    break;
  case CK_Athlon:
  case CK_AthlonThunderbird:
  case CK_Athlon4:
  case CK_AthlonXP:
  case CK_AthlonMP:
    defineCPUMacros(Builder, "athlon");
    if (SSELevel != NoSSE) {
      Builder.defineMacro("__athlon_sse__");
      Builder.defineMacro("__tune_athlon_sse__");
    }
    break;
  case CK_K8:
  case CK_K8SSE3:
  case CK_x86_64:
  case CK_Opteron:
  case CK_OpteronSSE3:
  case CK_Athlon64:
  case CK_Athlon64SSE3:
  case CK_AthlonFX:
    defineCPUMacros(Builder, "k8");
    break;
  case CK_AMDFAM10:
    defineCPUMacros(Builder, "amdfam10");
    break;
  case CK_BTVER1:
    defineCPUMacros(Builder, "btver1");
    break;
  case CK_BDVER1:
    defineCPUMacros(Builder, "bdver1");
    break;
  case CK_BDVER2:
    defineCPUMacros(Builder, "bdver2");
    break;
  case CK_Geode:
    defineCPUMacros(Builder, "geode");
    break;
  }

  // Target properties.
  Builder.defineMacro("__LITTLE_ENDIAN__");
  Builder.defineMacro("__REGISTER_PREFIX__", "");

  // Define __NO_MATH_INLINES on linux/x86 so that we don't get inline
  // functions in glibc header files that use FP Stack inline asm which the
  // backend can't deal with (PR879).
  Builder.defineMacro("__NO_MATH_INLINES");

  if (HasAES)
    Builder.defineMacro("__AES__");

  if (HasLZCNT)
    Builder.defineMacro("__LZCNT__");

  if (HasBMI)
    Builder.defineMacro("__BMI__");

  if (HasBMI2)
    Builder.defineMacro("__BMI2__");

  if (HasPOPCNT)
    Builder.defineMacro("__POPCNT__");

  if (HasFMA4)
    Builder.defineMacro("__FMA4__");

  // Each case falls through to the previous one here.
  switch (SSELevel) {
  case AVX2:
    Builder.defineMacro("__AVX2__");
  case AVX:
    Builder.defineMacro("__AVX__");
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
  case NoSSE:
    break;
  }

  // Each case falls through to the previous one here.
  switch (MMX3DNowLevel) {
  case AMD3DNowAthlon:
    Builder.defineMacro("__3dNOW_A__");
  case AMD3DNow:
    Builder.defineMacro("__3dNOW__");
  case MMX:
    Builder.defineMacro("__MMX__");
  case NoMMX3DNow:
    break;
  }
}

bool X86TargetInfo::hasFeature(StringRef Feature) const {
  return llvm::StringSwitch<bool>(Feature)
      .Case("aes", HasAES)
      .Case("avx", SSELevel >= AVX)
      .Case("avx2", SSELevel >= AVX2)
      .Case("bmi", HasBMI)
      .Case("bmi2", HasBMI2)
      .Case("fma4", HasFMA4)
      .Case("lzcnt", HasLZCNT)
      .Case("mm3dnow", MMX3DNowLevel >= AMD3DNow)
      .Case("mm3dnowa", MMX3DNowLevel >= AMD3DNowAthlon)
      .Case("mmx", MMX3DNowLevel >= MMX)
      .Case("popcnt", HasPOPCNT)
      .Case("sse", SSELevel >= SSE1)
      .Case("sse2", SSELevel >= SSE2)
      .Case("sse3", SSELevel >= SSE3)
      .Case("ssse3", SSELevel >= SSSE3)
      .Case("sse41", SSELevel >= SSE41)
      .Case("sse42", SSELevel >= SSE42)
      .Case("x86", true)
      .Case("x86_32", PointerWidth == 32)
      .Case("x86_64", PointerWidth == 64)
      .Default(false);
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
}


std::string
X86TargetInfo::convertConstraint(const char *&Constraint) const {
  switch (*Constraint) {
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
    return std::string(1, *Constraint);
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
    SuitableAlign = 128;
    DescriptionString = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-"
                        "i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-"
                        "a0:0:64-f80:32:32-n8:16:32-S128";
    SizeType = UnsignedInt;
    PtrDiffType = SignedInt;
    IntPtrType = SignedInt;
    RegParmMax = 3;

    // Use fpret for all types.
    RealTypeUsesObjCFPRet = ((1 << TargetInfo::Float) |
                             (1 << TargetInfo::Double) |
                             (1 << TargetInfo::LongDouble));

    // x86-32 has atomics up to 8 bytes
    // FIXME: Check that we actually have cmpxchg8b before setting
    // MaxAtomicInlineWidth. (cmpxchg8b is an i586 instruction.)
    MaxAtomicPromoteWidth = MaxAtomicInlineWidth = 64;
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
// x86-64 generic target
class X86_64TargetInfo : public X86TargetInfo {
public:
  X86_64TargetInfo(const std::string &triple) : X86TargetInfo(triple) {
    LongWidth = LongAlign = PointerWidth = PointerAlign = 64;
    LongDoubleWidth = 128;
    LongDoubleAlign = 128;
    LargeArrayMinWidth = 128;
    LargeArrayAlign = 128;
    SuitableAlign = 128;
    IntMaxType = SignedLong;
    UIntMaxType = UnsignedLong;
    Int64Type = SignedLong;
    RegParmMax = 6;

    DescriptionString = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-"
                        "i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-"
                        "a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128";

    // Use fpret only for long double.
    RealTypeUsesObjCFPRet = (1 << TargetInfo::LongDouble);

    // Use fp2ret for _Complex long double.
    ComplexLongDoubleUsesFP2Ret = true;

    // x86-64 has atomics up to 16 bytes.
    // FIXME: Once the backend is fixed, increase MaxAtomicInlineWidth to 128
    // on CPUs with cmpxchg16b
    MaxAtomicPromoteWidth = 128;
    MaxAtomicInlineWidth = 64;
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

//===----------------------------------------------------------------------===//
// Driver code
//===----------------------------------------------------------------------===//

static TargetInfo *AllocateTarget(const std::string &T) {
  llvm::Triple Triple(T);
  llvm::Triple::OSType os = Triple.getOS();

  switch (Triple.getArch()) {
  default:
    return NULL;

  case llvm::Triple::x86:
    switch (os) {
    case llvm::Triple::Linux:
      return new LinuxTargetInfo<X86_32TargetInfo>(T);
    default:
      return new X86_32TargetInfo(T);
    }

  case llvm::Triple::x86_64:
    switch (os) {
    case llvm::Triple::Linux:
      return new LinuxTargetInfo<X86_64TargetInfo>(T);
    default:
      return new X86_64TargetInfo(T);
    }
  }
}

/// CreateTargetInfo - Return the target info object for the specified target
/// triple.
TargetInfo *TargetInfo::CreateTargetInfo(DiagnosticsEngine &Diags,
                                         TargetOptions &Opts) {
  llvm::Triple Triple(Opts.Triple);

  // Construct the target
  OwningPtr<TargetInfo> Target(AllocateTarget(Triple.str()));
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
  if (!Opts.CXXABI.empty() && !Target->setCXXABI(Opts.CXXABI)) {
    Diags.Report(diag::err_target_unknown_cxxabi) << Opts.CXXABI;
    return 0;
  }

  // Compute the default target features, we need the target to handle this
  // because features may have dependencies on one another.
  llvm::StringMap<bool> Features;
  Target->getDefaultFeatures(Features);

  // Apply the user specified deltas.
  // First the enables.
  for (std::vector<std::string>::const_iterator it = Opts.Features.begin(),
         ie = Opts.Features.end(); it != ie; ++it) {
    const char *Name = it->c_str();

    if (Name[0] != '+')
      continue;

    // Apply the feature via the target.
    if (!Target->setFeatureEnabled(Features, Name + 1, true)) {
      Diags.Report(diag::err_target_invalid_feature) << Name;
      return 0;
    }
  }

  // Then the disables.
  for (std::vector<std::string>::const_iterator it = Opts.Features.begin(),
         ie = Opts.Features.end(); it != ie; ++it) {
    const char *Name = it->c_str();

    if (Name[0] == '+')
      continue;

    // Apply the feature via the target.
    if (Name[0] != '-' ||
        !Target->setFeatureEnabled(Features, Name + 1, false)) {
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
    Opts.Features.push_back((it->second ? "+" : "-") + it->first().str());
  Target->HandleTargetFeatures(Opts.Features);

  return Target.take();
}
