//===--- ExternalASTSource.h - Abstract External AST Interface --*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines ExternalASTSource interface, which enables
//  construction of AST nodes from some external source.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_EXTERNAL_AST_SOURCE_H_
#define MLANG_AST_EXTERNAL_AST_SOURCE_H_

#include "mlang/AST/Defn.h"
#include <cassert>
#include <stdint.h>

namespace llvm {
template<class T> class SmallVectorImpl;
}

namespace mlang {
class ASTConsumer;
class ClassBaseSpecifier;
class DefnContext;
class DefnContextLookupResult;
class DefinitionName;
class ExternalSemaSource; // layering violation required for downcasting
class NamedDefn;
class Stmt;
class TypeDefn;

/// \brief Abstract interface for external sources of AST nodes.
///
/// External AST sources provide AST nodes constructed from some
/// external source, such as a precompiled header. External AST
/// sources can resolve types and declarations from abstract IDs into
/// actual type and declaration nodes, and read parts of declaration
/// contexts.
class ExternalASTSource {
	/// \brief Whether this AST source also provides information for
	/// semantic analysis.
	bool SemaSource;

	friend class ExternalSemaSource;

public:
	ExternalASTSource() :
		SemaSource(false) {
	}

	virtual ~ExternalASTSource();

	/// \brief RAII class for safely pairing a StartedDeserializing call
	/// with FinishedDeserializing.
	class Deserializing {
		ExternalASTSource *Source;
	public:
		explicit Deserializing(ExternalASTSource *source) :
			Source(source) {
			assert(Source);
			Source->StartedDeserializing();
		}
		~Deserializing() {
			Source->FinishedDeserializing();
		}
	};

	/// \brief Resolve a definition ID into a definition, potentially
	/// building a new definition.
	///
	/// This method only needs to be implemented if the AST source ever
	/// passes back defn sets as VisibleDefinition objects.
	virtual Defn *GetExternalDefn(uint32_t ID);

	/// \brief Resolve the offset of a statement in the decl stream into
	/// a statement.
	///
	/// This operation is meant to be used via a LazyOffsetPtr.  It only
	/// needs to be implemented if the AST source uses methods like
	/// FunctionDecl::setLazyBody when building decls.
	virtual Stmt *GetExternalDefnStmt(uint64_t Offset);

	/// \brief Resolve the offset of a set of Class base specifiers in the decl
	/// stream into an array of specifiers.
	virtual ClassBaseSpecifier *GetExternalClassBaseSpecifiers(
			uint64_t Offset);

	/// \brief Finds all declarations with the given name in the
	/// given context.
	///
	/// Generally the final step of this method is either to call
	/// SetExternalVisibleDeclsForName or to recursively call lookup on
	/// the DefnContext after calling SetExternalVisibleDecls.
	virtual DefnContextLookupResult
			FindExternalVisibleDefnsByName(const DefnContext *DC,
					DefinitionName Name);

	/// \brief Deserialize all the visible declarations from external storage.
	///
	/// Name lookup deserializes visible declarations lazily, thus a DefnContext
	/// may not have a complete name lookup table. This function deserializes
	/// the rest of visible declarations from the external storage and completes
	/// the name lookup table of the DefnContext.
	virtual void MaterializeVisibleDefns(const DefnContext *DC);

	/// \brief Gives the external AST source an opportunity to complete
	/// an incomplete type.
	virtual void CompleteType(TypeDefn *Tag) {	}

	/// \brief Notify ExternalASTSource that we started deserialization of
	/// a decl or type so until FinishedDeserializing is called there may be
	/// decls that are initializing. Must be paired with FinishedDeserializing.
	///
	/// The default implementation of this method is a no-op.
	virtual void StartedDeserializing() {
	}

	/// \brief Notify ExternalASTSource that we finished the deserialization of
	/// a decl or type. Must be paired with StartedDeserializing.
	///
	/// The default implementation of this method is a no-op.
	virtual void FinishedDeserializing() {
	}

	/// \brief Function that will be invoked when we begin parsing a new
	/// translation unit involving this external AST source.
	///
	/// The default implementation of this method is a no-op.
	virtual void StartTranslationUnit(ASTConsumer *Consumer) {
	}

	/// \brief Print any statistics that have been gathered regarding
	/// the external AST source.
	///
	/// The default implementation of this method is a no-op.
	virtual void PrintStats();

	//===--------------------------------------------------------------------===//
	// Queries for performance analysis.
	//===--------------------------------------------------------------------===//

	struct MemoryBufferSizes {
		size_t malloc_bytes;
		size_t mmap_bytes;

		MemoryBufferSizes(size_t malloc_bytes, size_t mmap_bytes) :
			malloc_bytes(malloc_bytes), mmap_bytes(mmap_bytes) {
		}
	};

	/// Return the amount of memory used by memory buffers, breaking down
	/// by heap-backed versus mmap'ed memory.
	MemoryBufferSizes getMemoryBufferSizes() const {
		MemoryBufferSizes sizes(0, 0);
		getMemoryBufferSizes(sizes);
		return sizes;
	}

	virtual void getMemoryBufferSizes(MemoryBufferSizes &sizes) const;

protected:
	static DefnContextLookupResult
	SetExternalVisibleDefnsForName(const DefnContext *DC, DefinitionName Name,
			llvm::SmallVectorImpl<NamedDefn*> &Defns);

	static DefnContextLookupResult
	SetNoExternalVisibleDefnsForName(const DefnContext *DC,
			DefinitionName Name);

	void MaterializeVisibleDefnsForName(const DefnContext *DC,
			DefinitionName Name, llvm::SmallVectorImpl<NamedDefn*> &Defns);
};

/// \brief A lazy pointer to an AST node (of base type T) that resides
/// within an external AST source.
///
/// The AST node is identified within the external AST source by a
/// 63-bit offset, and can be retrieved via an operation on the
/// external AST source itself.
template<typename T, typename OffsT, T* (ExternalASTSource::*Get)(OffsT Offset)>
struct LazyOffsetPtr {
  /// \brief Either a pointer to an AST node or the offset within the
  /// external AST source where the AST node can be found.
  ///
  /// If the low bit is clear, a pointer to the AST node. If the low
  /// bit is set, the upper 63 bits are the offset.
  mutable uint64_t Ptr;

public:
  LazyOffsetPtr() : Ptr(0) { }

  explicit LazyOffsetPtr(T *Ptr) : Ptr(reinterpret_cast<uint64_t>(Ptr)) { }
  explicit LazyOffsetPtr(uint64_t Offset) : Ptr((Offset << 1) | 0x01) {
    assert((Offset << 1 >> 1) == Offset && "Offsets must require < 63 bits");
    if (Offset == 0)
      Ptr = 0;
  }

  LazyOffsetPtr &operator=(T *Ptr) {
    this->Ptr = reinterpret_cast<uint64_t>(Ptr);
    return *this;
  }

  LazyOffsetPtr &operator=(uint64_t Offset) {
    assert((Offset << 1 >> 1) == Offset && "Offsets must require < 63 bits");
    if (Offset == 0)
      Ptr = 0;
    else
      Ptr = (Offset << 1) | 0x01;

    return *this;
  }

  /// \brief Whether this pointer is non-NULL.
  ///
  /// This operation does not require the AST node to be deserialized.
  operator bool() const { return Ptr != 0; }

  /// \brief Whether this pointer is currently stored as an offset.
  bool isOffset() const { return Ptr & 0x01; }

  /// \brief Retrieve the pointer to the AST node that this lazy pointer
  ///
  /// \param Source the external AST source.
  ///
  /// \returns a pointer to the AST node.
  T* get(ExternalASTSource *Source) const {
    if (isOffset()) {
      assert(Source &&
             "Cannot deserialize a lazy pointer without an AST source");
      Ptr = reinterpret_cast<uint64_t>((Source->*Get)(Ptr >> 1));
    }
    return reinterpret_cast<T*>(Ptr);
  }
};

/// \brief A lazy pointer to a statement.
typedef LazyOffsetPtr<Stmt, uint64_t, &ExternalASTSource::GetExternalDefnStmt>
  LazyDefnCmdPtr;

/// \brief A lazy pointer to a declaration.
typedef LazyOffsetPtr<Defn, uint32_t, &ExternalASTSource::GetExternalDefn>
  LazyDefnPtr;

/// \brief A lazy pointer to a set of ClassBaseSpecifiers.
typedef LazyOffsetPtr<ClassBaseSpecifier, uint64_t,
                      &ExternalASTSource::GetExternalClassBaseSpecifiers>
  LazyClassBaseSpecifiersPtr;
} // end namespace mlang

#endif /* MLANG_AST_EXTERNAL_AST_SOURCE_H_ */
