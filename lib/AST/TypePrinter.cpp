//===--- TypePrinter.cpp - Pretty-Print Mlang Types -----------------------===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
//===----------------------------------------------------------------------===//
//
// This contains code to print types from Mlang's type system.
//
//===----------------------------------------------------------------------===//

#include "mlang/AST/DefnSub.h"
#include "mlang/AST/Expr.h"
#include "mlang/AST/Type.h"
#include "mlang/AST/PrettyPrinter.h"
#include "mlang/Basic/LangOptions.h"
#include "mlang/Basic/SourceManager.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"
using namespace mlang;

namespace {
  class TypePrinter {
    PrintingPolicy Policy;

  public:
    explicit TypePrinter(const PrintingPolicy &Policy) : Policy(Policy) { }

    void print(const RawType *ty, TypeInfo qs, std::string &buffer);
    void print(Type T, std::string &S);
    void AppendScope(DefnContext *DC, std::string &S);
    void printTag(TypeDefn *T, std::string &S);
#define ABSTRACT_TYPE(CLASS, PARENT)
#define TYPE(CLASS, PARENT) \
    void print##CLASS(const CLASS##Type *T, std::string &S);
#include "mlang/AST/TypeNodes.def"
  };
}

static void AppendTypeAttrsList(std::string &S, unsigned TypeQuals) {
  if (TypeQuals & TypeInfo::AT_Complex) {
    if (!S.empty()) S += ',';
    S += "complex";
  }
  if (TypeQuals & TypeInfo::AT_Global) {
    if (!S.empty()) S += ',';
    S += "global";
  }
  if (TypeQuals & TypeInfo::AT_Persistent) {
    if (!S.empty()) S += ',';
    S += "persistent";
  }
  if (TypeQuals & TypeInfo::AT_Sparse) {
	  assert(TypeQuals & TypeInfo::ArrayTy &&
			  "only ArrayType can have sparse attributes.");
     if (!S.empty()) S += ',';
     S += "sparse";
   }
  // FIXME nesting attribute should not be printed.
//  if (TypeQuals & TypeInfo::AT_Nesting) {
//     if (!S.empty()) S += ',';
//     S += "persistent";
//   }
}

void TypePrinter::print(Type t, std::string &buffer) {
  SplitQualType split = t.split();
  print(split.first, split.second, buffer);
}

void TypePrinter::print(const RawType *T, TypeInfo Quals, std::string &buffer) {
  if (!T) {
    buffer += "NULL TYPE";
    return;
  }
  
  if (Policy.SuppressSpecifiers)
    return;
  
  switch (T->getTypeClass()) {
#define ABSTRACT_TYPE(CLASS, PARENT)
#define TYPE(CLASS, PARENT) case RawType::CLASS: \
    print##CLASS(cast<CLASS##Type>(T), buffer); \
    break;
#include "mlang/AST/TypeNodes.def"
  }
  
}

void TypePrinter::printSimpleNumeric(const SimpleNumericType *T, std::string &S) {
	if (S.empty()) {
		S = T->getName(/*Policy.LangOpts*/);
	} else {
		// Prefix the basic type, e.g. 'int X'.
		S = ' ' + S;
		S = T->getName(/*Policy.LangOpts*/) + S;
	  }
}

void TypePrinter::printLValueReference(const LValueReferenceType *T, 
                                       std::string &S) { 
  S = '&' + S;
  
  // Handle things like 'int (&A)[4];' correctly.
  // FIXME: this should include vectors, but vectors use attributes I guess.
  if (isa<ArrayType>(T->getPointeeTypeAsWritten()))
    S = '(' + S + ')';
  
  print(T->getPointeeTypeAsWritten(), S);
}

void TypePrinter::printRValueReference(const RValueReferenceType *T, 
                                       std::string &S) { 
  S = "&&" + S;
  
  // Handle things like 'int (&&A)[4];' correctly.
  // FIXME: this should include vectors, but vectors use attributes I guess.
  if (isa<ArrayType>(T->getPointeeTypeAsWritten()))
    S = '(' + S + ')';
  
  print(T->getPointeeTypeAsWritten(), S);
}

void TypePrinter::printFunctionHandle(const FunctionHandleType *T,
                                      std::string &S) {
	S = '*' + S;
  print(T->getPointeeType(), S);
}

void TypePrinter::printStruct(const StructType *T,
                              std::string &S) {
}

void TypePrinter::printCell(const CellType *T,
                            std::string &S) {

}

void TypePrinter::printMap(const MapType *T,
                           std::string &S) {
  
}

void TypePrinter::printClassdef(const ClassdefType *T,
                                std::string &S) {

}

void TypePrinter::printMatrix(const MatrixType *T,
                                std::string &S) {
	switch (T->getMatrixKind()) {
	default:
		S = "__matrix " + S;
		break;
	case MatrixType::Full: {
		// FIXME: We prefer to print the size directly here, but have no way
		// to get the size of the type.
		print(T->getElementType(), S);
		std::string V = "__attribute__((__matrix_size__(<";
		V += llvm::utostr_32(T->getNumRows()); // convert back to bytes.
		V += "X";
		V += llvm::utostr_32(T->getNumColumns());
		std::string ET;
		print(T->getElementType(), ET);
		V += "> * sizeof(" + ET + ")))) ";
		S = V + S;
		break;
	}
	}
}

void TypePrinter::printNDArray(const NDArrayType *T,
                                std::string &S) {

}
void TypePrinter::printVector(const VectorType *T, std::string &S) { 
  switch (T->getVectorKind()) {
  case VectorType::AltiVecPixel:
    S = "__vector __pixel " + S;
    break;
  case VectorType::AltiVecBool:
    print(T->getElementType(), S);
    S = "__vector __bool " + S;
    break;
  case VectorType::AltiVecVector:
    print(T->getElementType(), S);
    S = "__vector " + S;
    break;
  case VectorType::NeonVector:
    print(T->getElementType(), S);
    S = ("__attribute__((neon_vector_type(" +
         llvm::utostr_32(T->getNumElements()) + "))) " + S);
    break;
  case VectorType::NeonPolyVector:
    print(T->getElementType(), S);
    S = ("__attribute__((neon_polyvector_type(" +
         llvm::utostr_32(T->getNumElements()) + "))) " + S);
    break;
  case VectorType::GenericVector: {
    // FIXME: We prefer to print the size directly here, but have no way
    // to get the size of the type.
    print(T->getElementType(), S);
    std::string V = "__attribute__((__vector_size__(";
    V += llvm::utostr_32(T->getNumElements()); // convert back to bytes.
    std::string ET;
    print(T->getElementType(), ET);
    V += " * sizeof(" + ET + ")))) ";
    S = V + S;
    break;
  }
  }
}

void TypePrinter::printFunctionProto(const FunctionProtoType *T, 
                                     std::string &S) { 
  // If needed for precedence reasons, wrap the inner part in grouping parens.
  if (!S.empty())
    S = "(" + S + ")";
  
  S += "(";
  std::string Tmp;
  PrintingPolicy ParamPolicy(Policy);
  ParamPolicy.SuppressSpecifiers = false;
  for (unsigned i = 0, e = T->getNumArgs(); i != e; ++i) {
    if (i) S += ", ";
    print(T->getArgType(i), Tmp);
    S += Tmp;
    Tmp.clear();
  }
  
  if (T->isVariadic()) {
    if (T->getNumArgs())
      S += ", ";
    S += "...";
  } else if (T->getNumArgs() == 0 && !Policy.LangOpts.OOP) {
    // Do not emit int() if we have a proto, emit 'int(void)'.
    S += "void";
  }
  
  S += ")";

  FunctionType::ExtInfo Info = T->getExtInfo();
  switch(Info.getCC()) {
  case CC_Default:
  default: break;
  case CC_C:
    S += " __attribute__((cdecl))";
    break;
  case CC_X86StdCall:
    S += " __attribute__((stdcall))";
    break;
  case CC_X86FastCall:
    S += " __attribute__((fastcall))";
    break;
  case CC_X86ThisCall:
    S += " __attribute__((thiscall))";
    break;
  case CC_X86Pascal:
    S += " __attribute__((pascal))";
    break;
  }
  if (Info.getNoReturn())
    S += " __attribute__((noreturn))";
  if (Info.getRegParm())
    S += " __attribute__((regparm (" +
        llvm::utostr_32(Info.getRegParm()) + ")))";
  
  if (T->hasExceptionSpec()) {
    S += " throw(";
    if (T->hasAnyExceptionSpec())
      S += "...";
    else 
      for (unsigned I = 0, N = T->getNumExceptions(); I != N; ++I) {
        if (I)
          S += ", ";

        std::string ExceptionType;
        print(T->getExceptionType(I), ExceptionType);
        S += ExceptionType;
      }
    S += ")";
  }

  AppendTypeAttrsList(S, 0);
  
//  print(T->getResultType(), S);
}

void TypePrinter::printFunctionNoProto(const FunctionNoProtoType *T, 
                                       std::string &S) { 
  // If needed for precedence reasons, wrap the inner part in grouping parens.
  if (!S.empty())
    S = "(" + S + ")";
  
  S += "()";
  if (T->getNoReturnAttr())
    S += " __attribute__((noreturn))";
//  print(T->getResultType(), S);
}

/// Appends the given scope to the end of a string.
void TypePrinter::AppendScope(DefnContext *DC, std::string &Buffer) {
  if (DC->isTranslationUnit()) return;
  AppendScope(DC->getParent(), Buffer);

  unsigned OldSize = Buffer.size();

  if (TypeDefn *Tag = dyn_cast<TypeDefn>(DC)) {
    if (Tag->getIdentifier())
    	Buffer += Tag->getIdentifier()->getName();
  }

  if (Buffer.size() != OldSize)
    Buffer += "::";
}

void TypePrinter::printTag(TypeDefn *D, std::string &InnerString) {
  if (Policy.SuppressTag)
    return;

  std::string Buffer;
  bool HasKindDecoration = false;

  // We don't print tags unless this is an elaborated type.
  // In C, we just assume every HeteroContainerType is an elaborated type.
  if (!Policy.LangOpts.OOP ) {
    HasKindDecoration = true;
    Buffer += D->getKindName();
    Buffer += ' ';
  }

  // Compute the full nested-name-specifier for this type.
  // In C, this will always be empty except when the type
  // being printed is anonymous within other Record.
  if (!Policy.SuppressScope)
    AppendScope(D->getDefnContext(), Buffer);

  if (const IdentifierInfo *II = D->getIdentifier())
    Buffer += II->getNameStart();
  else {
    // Make an unambiguous representation for anonymous types, e.g.
    //   <anonymous enum at /usr/include/string.h:120:9>
    llvm::raw_string_ostream OS(Buffer);
    OS << "<anonymous";

    if (Policy.AnonymousTagLocations) {
      // Suppress the redundant tag keyword if we just printed one.
      // We don't have to worry about ElaboratedTypes here because you can't
      // refer to an anonymous type with one.
      if (!HasKindDecoration)
        OS << " " << D->getKindName();

      PresumedLoc PLoc = D->getASTContext().getSourceManager().getPresumedLoc(
          D->getLocation());
      if (PLoc.isValid()) {
        OS << " at " << PLoc.getFilename()
           << ':' << PLoc.getLine()
           << ':' << PLoc.getColumn();
      }
    }
    
    OS << '>';
  }

  if (!InnerString.empty()) {
    Buffer += ' ';
    Buffer += InnerString;
  }

  std::swap(Buffer, InnerString);
}

void Type::dump(const char *msg) const {
  std::string R = "identifier";
  LangOptions LO;
  getAsStringInternal(R, PrintingPolicy(LO));
  if (msg)
    llvm::errs() << msg << ": ";
  llvm::errs() << R << "\n";
}

void Type::dump() const {
  dump("");
}

void RawType::dump() const {
  Type(this, 0).dump();
}

std::string TypeInfo::getAsString() const {
  LangOptions LO;
  return getAsString(PrintingPolicy(LO));
}

// Appends qualifiers to the given string, separated by spaces.  Will
// prefix a space if the string is non-empty.  Will not append a final
// space.
void TypeInfo::getAsStringInternal(std::string &S,
                                   const PrintingPolicy&) const {
  AppendTypeAttrsList(S, getAttrs());
  if (unsigned AddressSpace = getArraySize()) {
    if (!S.empty()) S += ' ';
    S += "__attribute__((ArraySize(";
    S += llvm::utostr_32(AddressSpace);
    S += ")))";
  }
}

std::string Type::getAsString(SplitQualType T) {
	const RawType *ty = T.first;
	TypeInfo qs = T.second;
	return getAsString(ty, qs);
}

std::string Type::getAsString(const RawType *ty, TypeInfo qs) {
  std::string buffer;
  LangOptions options;
  getAsStringInternal(ty, qs, buffer, PrintingPolicy(options));
  return buffer;
}

void Type::getAsStringInternal(const RawType *ty, TypeInfo qs,
                                   std::string &buffer,
                                   const PrintingPolicy &policy) {
  TypePrinter(policy).print(ty, qs, buffer);
}

