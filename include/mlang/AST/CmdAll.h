//===--- CmdAll.h - "Umbrella" header for Cmd -------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the interface to the Cmd* classes.
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_AST_CMD_ALL_H_
#define MLANG_AST_CMD_ALL_H_

// This header exports all Cmd interfaces.
#include "mlang/AST/CmdBlock.h"
#include "mlang/AST/CmdBreak.h"
#include "mlang/AST/CmdContinue.h"
#include "mlang/AST/CmdDefn.h"
#include "mlang/AST/CmdFor.h"
#include "mlang/AST/CmdIF.h"
#include "mlang/AST/CmdNull.h"
#include "mlang/AST/CmdReturn.h"
#include "mlang/AST/CmdScopeDef.h"
#include "mlang/AST/CmdSwitch.h"
#include "mlang/AST/CmdTryCatch.h"
#include "mlang/AST/CmdWhile.h"

#endif /* MLANG_AST_CMD_ALL_H_ */
