//===--- ASTConsumer.h - Abstract interface for reading ASTs ----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTConsumer class.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CONSUMER_H_
#define MLANG_AST_CONSUMER_H_

namespace mlang {
class ASTContext;
class DefnGroupRef;
class HandleTagDeclDefinition;
class ASTMutationListener;
class ASTDeserializationListener; // layering violation because void* is ugly
class SemaConsumer; // layering violation required for safe SemaConsumer
class UserClassDefn;
class VarDefn;

/// ASTConsumer - This is an abstract interface that should be implemented by
/// clients that read ASTs.  This abstraction layer allows the client to be
/// independent of the AST producer (e.g. parser vs AST dump file reader, etc).
class ASTConsumer {
	/// \brief Whether this AST consumer also requires information about
	/// semantic analysis.
	bool SemaConsumer;

	friend class SemaConsumer;

public:
	ASTConsumer() :
		SemaConsumer(false) {
	}

	virtual ~ASTConsumer() {
	}

	/// Initialize - This is called to initialize the consumer, providing the
	/// ASTContext.
	virtual void Initialize(ASTContext &Context) {
	}

	/// HandleTopLevelDecl - Handle the specified top-level definition.  This is
	/// called by the parser to process every top-level Defn*. Note that D can be
	/// the head of a chain of Defns (e.g. for `int a, b` the chain will have two
	/// elements). Use Defn::getNextDeclarator() to walk the chain.
	virtual void HandleTopLevelDefn(DefnGroupRef D);

	/// HandleInterestingDefn - Handle the specified interesting declaration. This
	/// is called by the AST reader when deserializing things that might interest
	/// the consumer. The default implementation forwards to HandleTopLevelDecl.
	virtual void HandleInterestingDefn(DefnGroupRef D);

	/// HandleTranslationUnit - This method is called when the ASTs for entire
	/// translation unit have been parsed.
	virtual void HandleTranslationUnit(ASTContext &Ctx) {
	}

	/// HandleTagDeclDefinition - This callback is invoked each time a TypeDefn
	/// (e.g. struct, union, enum, class) is completed.  This allows the client to
	/// hack on the type, which can occur at any point in the file (because these
	/// can be defined in declspecs).
	virtual void HandleTagDefnDefinition(UserClassDefn *D) {
	}

	/// CompleteTentativeDefinition - Callback invoked at the end of a translation
	/// unit to notify the consumer that the given tentative definition should be
	/// completed.
	///
	/// The variable declaration itself will be a tentative
	/// definition. If it had an incomplete array type, its type will
	/// have already been changed to an array of size 1. However, the
	/// declaration remains a tentative definition and has not been
	/// modified by the introduction of an implicit zero initializer.
	virtual void CompleteTentativeDefinition(VarDefn *D) {
	}

	/// \brief Callback involved at the end of a translation unit to
	/// notify the consumer that a vtable for the given C++ class is
	/// required.
	///
	/// \param RD The class whose vtable was used.
	///
	/// \param DefinitionRequired Whether a definition of this vtable is
	/// required in this translation unit; otherwise, it is only needed if
	/// it was actually used.
	virtual void HandleVTable(UserClassDefn *RD, bool DefinitionRequired) {
	}

	/// \brief If the consumer is interested in entities getting modified after
	/// their initial creation, it should return a pointer to
	/// a GetASTMutationListener here.
	virtual ASTMutationListener *GetASTMutationListener() {
		return 0;
	}

	/// \brief If the consumer is interested in entities being deserialized from
	/// AST files, it should return a pointer to a ASTDeserializationListener here
	virtual ASTDeserializationListener *GetASTDeserializationListener() {
		return 0;
	}

	/// PrintStats - If desired, print any statistics.
	virtual void PrintStats() {
	}

	// Support isa/cast/dyn_cast
	static bool classof(const ASTConsumer *) {
		return true;
	}
};
} // end namespace mlang

#endif /* MLANG_AST_CONSUMER_H_ */
