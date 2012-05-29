//===- ASTDeserializationListener.h - Decl/Type PCH Read Events -*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTDeserializationListener class, which is notified
//  by the ASTReader whenever a type or declaration is deserialized.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_FRONTEND_AST_DESERIALIZATION_LISTENER_H_
#define MLANG_FRONTEND_AST_DESERIALIZATION_LISTENER_H_

#include "mlang/Serialization/ASTBitCodes.h"

namespace mlang {

class Defn;
class ASTReader;
class Type;
  
class ASTDeserializationListener {
protected:
  virtual ~ASTDeserializationListener();

public:
  /// \brief The ASTReader was initialized.
  virtual void ReaderInitialized(ASTReader *Reader) { }

  /// \brief An identifier was deserialized from the AST file.
  virtual void IdentifierRead(serialization::IdentID ID,
                              IdentifierInfo *II) { }

  /// \brief A type was deserialized from the AST file. The ID here has the
  ///        qualifier bits already removed, and T is guaranteed to be locally
  ///        unqualified.
  virtual void TypeRead(serialization::TypeIdx Idx, Type T) { }

  /// \brief A defn was deserialized from the AST file.
  virtual void DefnRead(serialization::DefnID ID, const Defn *D) { }
};

} // end namespace mlang

#endif /* MLANG_FRONTEND_AST_DESERIALIZATION_LISTENER_H_ */
