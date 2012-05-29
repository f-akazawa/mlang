//===--- TargetInfo.cpp - Expose information about the target ---*- C++ -*-===//
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
#include <cstdlib>
using namespace mlang;

// TargetInfo Constructor.
TargetInfo::TargetInfo(const std::string &T) : Triple(T) {
  // Set defaults.  Defaults are set for a 32-bit RISC platform, like PPC or
  // SPARC.  These should be overridden by concrete targets as needed.
  TLSSupported = true;
  NoAsmVariants = false;
  PointerWidth = PointerAlign = 32;
  IntWidth = IntAlign = 32;
  LongWidth = LongAlign = 32;
  LongLongWidth = LongLongAlign = 64;
  FloatWidth = 32;
  FloatAlign = 32;
  DoubleWidth = 64;
  DoubleAlign = 64;
  LongDoubleWidth = 64;
  LongDoubleAlign = 64;
  LargeArrayMinWidth = 0;
  LargeArrayAlign = 0;
  SizeType = Int32;
  PtrDiffType = Int32;
  IntMaxType = Int64;
  UIntMaxType = UInt64;
  IntPtrType = Int32;
  WCharType = Int16;
  WIntType = Int32;
  Char16Type = UInt16;
  Char32Type = UInt32;
  Int64Type = Int64;
  SigAtomicType = Int32;
  FloatFormat = &llvm::APFloat::IEEEsingle;
  DoubleFormat = &llvm::APFloat::IEEEdouble;
  LongDoubleFormat = &llvm::APFloat::IEEEdouble;
  DescriptionString = "E-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-"
                      "i64:64:64-f32:32:32-f64:64:64-n32";
  UserLabelPrefix = "_";
  HasAlignMac68kSupport = false;

  // Default to no types using fpret.
  RealTypeUsesObjCFPRet = 0;

  // Default to using the Itanium ABI.
  GmatABI = GmatABI_Nvidia;
}

// Out of line virtual dtor for TargetInfo.
TargetInfo::~TargetInfo() {}

/// getTypeName - Return the user string for the specified integer type enum.
/// For example, SignedShort -> "short".
const char *TargetInfo::getTypeName(IntType T) {
  switch (T) {
  default: assert(0 && "not an integer!");
  case Int8:      return "int8";
  case UInt8:     return "uint8";
  case Int16:     return "int16";
  case UInt16:    return "uint16";
  case Int32:     return "int32";
  case UInt32:    return "uint32";
  case Int64:     return "int64";
  case UInt64:    return "uint64";
  }
}

/// getTypeConstantSuffix - Return the constant suffix for the specified
/// integer type enum. For example, SignedLong -> "L".
const char *TargetInfo::getTypeConstantSuffix(IntType T) {
  switch (T) {
  default: assert(0 && "not an integer!");
  case Int8:
  case Int16:
  case Int32:       return "";
  case Int64:       return "L";
  case UInt8:
  case UInt16:
  case UInt32:      return "U";
  case UInt64:      return "UL";
  }
}

/// getTypeWidth - Return the width (in bits) of the specified integer type
/// enum. For example, SignedInt -> getIntWidth().
unsigned TargetInfo::getTypeWidth(IntType T) const {
  switch (T) {
  default: assert(0 && "not an integer!");
  case Int8:
  case UInt8:    return getShortWidth();
  case Int16:
  case UInt16:   return getIntWidth();
  case Int32:
  case UInt32:   return getLongWidth();
  case Int64:
  case UInt64:   return getLongLongWidth();
  };
}

/// getTypeAlign - Return the alignment (in bits) of the specified integer type
/// enum. For example, SignedInt -> getIntAlign().
unsigned TargetInfo::getTypeAlign(IntType T) const {
  switch (T) {
  default: assert(0 && "not an integer!");
  case Int8:
  case UInt8:    return getShortAlign();
  case Int16:
  case UInt16:      return getIntAlign();
  case Int32:
  case UInt32:     return getLongAlign();
  case Int64:
  case UInt64: return getLongLongAlign();
  };
}

/// isTypeSigned - Return whether an integer types is signed. Returns true if
/// the type is signed; false otherwise.
bool TargetInfo::isTypeSigned(IntType T) const {
  switch (T) {
  default: assert(0 && "not an integer!");
  case Int8:
  case Int16:
  case Int32:
  case Int64:
    return true;
  case UInt8:
  case UInt16:
  case UInt32:
  case UInt64:
    return false;
  };
}

/// setForcedLangOptions - Set forced language options.
/// Apply changes to the target information with respect to certain
/// language options which change the target configuration.
void TargetInfo::setForcedLangOptions(LangOptions &Opts) {
  if (Opts.ShortWChar)
    WCharType = UInt8;
}

//===----------------------------------------------------------------------===//


static llvm::StringRef removeGCCRegisterPrefix(llvm::StringRef Name) {
  if (Name[0] == '%' || Name[0] == '#')
    Name = Name.substr(1);

  return Name;
}

/// isValidGCCRegisterName - Returns whether the passed in string
/// is a valid register name according to GCC. This is used by Sema for
/// inline asm statements.
bool TargetInfo::isValidGCCRegisterName(llvm::StringRef Name) const {
  if (Name.empty())
    return false;

  const char * const *Names;
  unsigned NumNames;

  // Get rid of any register prefix.
  Name = removeGCCRegisterPrefix(Name);

  if (Name == "memory" || Name == "cc")
    return true;

  getGCCRegNames(Names, NumNames);

  // If we have a number it maps to an entry in the register name array.
  if (isdigit(Name[0])) {
    int n;
    if (!Name.getAsInteger(0, n))
      return n >= 0 && (unsigned)n < NumNames;
  }

  // Check register names.
  for (unsigned i = 0; i < NumNames; i++) {
    if (Name == Names[i])
      return true;
  }

  // Now check aliases.
  const GCCRegAlias *Aliases;
  unsigned NumAliases;

  getGCCRegAliases(Aliases, NumAliases);
  for (unsigned i = 0; i < NumAliases; i++) {
    for (unsigned j = 0 ; j < llvm::array_lengthof(Aliases[i].Aliases); j++) {
      if (!Aliases[i].Aliases[j])
        break;
      if (Aliases[i].Aliases[j] == Name)
        return true;
    }
  }

  return false;
}

llvm::StringRef
TargetInfo::getNormalizedGCCRegisterName(llvm::StringRef Name) const {
  assert(isValidGCCRegisterName(Name) && "Invalid register passed in");

  // Get rid of any register prefix.
  Name = removeGCCRegisterPrefix(Name);

  const char * const *Names;
  unsigned NumNames;

  getGCCRegNames(Names, NumNames);

  // First, check if we have a number.
  if (isdigit(Name[0])) {
    int n;
    if (!Name.getAsInteger(0, n)) {
      assert(n >= 0 && (unsigned)n < NumNames &&
             "Out of bounds register number!");
      return Names[n];
    }
  }

  // Now check aliases.
  const GCCRegAlias *Aliases;
  unsigned NumAliases;

  getGCCRegAliases(Aliases, NumAliases);
  for (unsigned i = 0; i < NumAliases; i++) {
    for (unsigned j = 0 ; j < llvm::array_lengthof(Aliases[i].Aliases); j++) {
      if (!Aliases[i].Aliases[j])
        break;
      if (Aliases[i].Aliases[j] == Name)
        return Aliases[i].Register;
    }
  }

  return Name;
}

bool TargetInfo::validateOutputConstraint(ConstraintInfo &Info) const {
  const char *Name = Info.getConstraintStr().c_str();
  // An output constraint must start with '=' or '+'
  if (*Name != '=' && *Name != '+')
    return false;

  if (*Name == '+')
    Info.setIsReadWrite();

  Name++;
  while (*Name) {
    switch (*Name) {
    default:
      if (!validateAsmConstraint(Name, Info)) {
        // FIXME: We temporarily return false
        // so we can add more constraints as we hit it.
        // Eventually, an unknown constraint should just be treated as 'g'.
        return false;
      }
    case '&': // early clobber.
      break;
    case '%': // commutative.
      // FIXME: Check that there is a another register after this one.
      break;
    case 'r': // general register.
      Info.setAllowsRegister();
      break;
    case 'm': // memory operand.
    case 'o': // offsetable memory operand.
    case 'V': // non-offsetable memory operand.
    case '<': // autodecrement memory operand.
    case '>': // autoincrement memory operand.
      Info.setAllowsMemory();
      break;
    case 'g': // general register, memory operand or immediate integer.
    case 'X': // any operand.
      Info.setAllowsRegister();
      Info.setAllowsMemory();
      break;
    case ',': // multiple alternative constraint.  Pass it.
      // Handle additional optional '=' or '+' modifiers.
      if (Name[1] == '=' || Name[1] == '+')
        Name++;
      break;
    case '?': // Disparage slightly code.
    case '!': // Disparage severely.
      break;  // Pass them.
    }

    Name++;
  }

  return true;
}

bool TargetInfo::resolveSymbolicName(const char *&Name,
                                     ConstraintInfo *OutputConstraints,
                                     unsigned NumOutputs,
                                     unsigned &Index) const {
  assert(*Name == '[' && "Symbolic name did not start with '['");
  Name++;
  const char *Start = Name;
  while (*Name && *Name != ']')
    Name++;

  if (!*Name) {
    // Missing ']'
    return false;
  }

  std::string SymbolicName(Start, Name - Start);

  for (Index = 0; Index != NumOutputs; ++Index)
    if (SymbolicName == OutputConstraints[Index].getName())
      return true;

  return false;
}

bool TargetInfo::validateInputConstraint(ConstraintInfo *OutputConstraints,
                                         unsigned NumOutputs,
                                         ConstraintInfo &Info) const {
  const char *Name = Info.ConstraintStr.c_str();

  while (*Name) {
    switch (*Name) {
    default:
      // Check if we have a matching constraint
      if (*Name >= '0' && *Name <= '9') {
        unsigned i = *Name - '0';

        // Check if matching constraint is out of bounds.
        if (i >= NumOutputs)
          return false;

        // A number must refer to an output only operand.
        if (OutputConstraints[i].isReadWrite())
          return false;

        // If the constraint is already tied, it must be tied to the
        // same operand referenced to by the number.
        if (Info.hasTiedOperand() && Info.getTiedOperand() != i)
          return false;

        // The constraint should have the same info as the respective
        // output constraint.
        Info.setTiedOperand(i, OutputConstraints[i]);
      } else if (!validateAsmConstraint(Name, Info)) {
        // FIXME: This error return is in place temporarily so we can
        // add more constraints as we hit it.  Eventually, an unknown
        // constraint should just be treated as 'g'.
        return false;
      }
      break;
    case '[': {
      unsigned Index = 0;
      if (!resolveSymbolicName(Name, OutputConstraints, NumOutputs, Index))
        return false;

      // If the constraint is already tied, it must be tied to the
      // same operand referenced to by the number.
      if (Info.hasTiedOperand() && Info.getTiedOperand() != Index)
        return false;

      Info.setTiedOperand(Index, OutputConstraints[Index]);
      break;
    }
    case '%': // commutative
      // FIXME: Fail if % is used with the last operand.
      break;
    case 'i': // immediate integer.
    case 'n': // immediate integer with a known value.
      break;
    case 'I':  // Various constant constraints with target-specific meanings.
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'P':
      break;
    case 'r': // general register.
      Info.setAllowsRegister();
      break;
    case 'm': // memory operand.
    case 'o': // offsettable memory operand.
    case 'V': // non-offsettable memory operand.
    case '<': // autodecrement memory operand.
    case '>': // autoincrement memory operand.
      Info.setAllowsMemory();
      break;
    case 'g': // general register, memory operand or immediate integer.
    case 'X': // any operand.
      Info.setAllowsRegister();
      Info.setAllowsMemory();
      break;
    case 'E': // immediate floating point.
    case 'F': // immediate floating point.
    case 'p': // address operand.
      break;
    case ',': // multiple alternative constraint.  Ignore comma.
      break;
    case '?': // Disparage slightly code.
    case '!': // Disparage severly.
      break;  // Pass them.
    }

    Name++;
  }

  return true;
}
