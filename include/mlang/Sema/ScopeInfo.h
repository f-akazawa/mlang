//===--- ScopeInfo.h - Information about a semantic context -----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines FunctionScopeInfo and BlockScopeInfo..
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_SEMA_SCOPE_INFO_H_
#define MLANG_SEMA_SCOPE_INFO_H_

#include "mlang/AST/Type.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace mlang {
class IdentifierInfo;
class ReturnCmd;
class Scope;
class ScriptDefn;
class SwitchCmd;

namespace sema {

/// \brief Retains information about a function, method, or block that is
/// currently being parsed.
class FunctionScopeInfo {
public:

  /// \brief Whether this scope information structure defined information for
  /// a block.
  bool IsBlockInfo;

	/// \brief Whether this function contains a VLA, @try, try, C++
	/// initializer, or anything else that can't be jumped past.
	bool HasBranchProtectedScope;

	/// \brief Whether this function contains any switches or direct gotos.
	bool HasBranchIntoScope;

	/// \brief Whether this function contains any indirect gotos.
	bool HasIndirectGoto;

	/// \brief Used to determine if errors occurred in this function or block.
	DiagnosticErrorTrap ErrorTrap;

	/// LabelMap - This is a mapping from label identifiers to the LabelStmt for
	/// it (which acts like the label decl in some ways).  Forward referenced
	/// labels have a LabelStmt created for them with a null location & SubStmt.
	//llvm::DenseMap<IdentifierInfo*, LabelStmt*> LabelMap;

	/// SwitchStack - This is the current set of active switch statements in the
	/// block.
	llvm::SmallVector<SwitchCmd*, 8> SwitchStack;

	/// \brief The list of return statements that occur within the function or
	/// block, if there is any chance of applying the named return value
	/// optimization.
	llvm::SmallVector<ReturnCmd *, 4> Returns;

	void setHasBranchIntoScope() {
		HasBranchIntoScope = true;
	}

	void setHasBranchProtectedScope() {
		HasBranchProtectedScope = true;
	}

	void setHasIndirectGoto() {
		HasIndirectGoto = true;
	}

	bool NeedsScopeChecking() const {
		return HasIndirectGoto || (HasBranchProtectedScope
				&& HasBranchIntoScope);
	}

  FunctionScopeInfo(DiagnosticsEngine &Diag) :
    IsBlockInfo(false), HasBranchProtectedScope(false), HasBranchIntoScope(
      false), HasIndirectGoto(false), ErrorTrap(Diag) {
  }

  virtual ~FunctionScopeInfo();

  /// \brief Clear out the information in this function scope, making it
  /// suitable for reuse.
  void Clear();

	static bool classof(const FunctionScopeInfo *FSI) {
		return true;
	}
};

/// \brief Retains information about a block that is currently being parsed.
class BlockScopeInfo: public FunctionScopeInfo {
public:
	bool hasBlockDeclRefExprs;

	ScriptDefn *TheDefn;

	/// TheScope - This is the scope for the block itself, which contains
	/// arguments etc.
	Scope *TheScope;

	/// ReturnType - The return type of the block, or null if the block
	/// signature didn't provide an explicit return type.
	Type ReturnType;

	/// BlockType - The function type of the block, if one was given.
	/// Its return type may be BuiltinType::Dependent.
	Type FunctionType;

  BlockScopeInfo(DiagnosticsEngine &Diag, Scope *BlockScope,
                 ScriptDefn *Block) :
    FunctionScopeInfo(Diag), hasBlockDeclRefExprs(false), TheDefn(Block),
    TheScope(BlockScope) {
      IsBlockInfo = true;
  }

  virtual ~BlockScopeInfo();

  static bool classof(const FunctionScopeInfo *FSI) {
    return FSI->IsBlockInfo;
  }
  static bool classof(const BlockScopeInfo *BSI) {
    return true;
  }
};

} // end of namespace sema
} // end of namespace mlang

#endif /* MLANG_SEMA_SCOPE_INFO_H_ */
