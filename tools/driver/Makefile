##===- tools/driver/Makefile -------------------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##
LEVEL := ../..

TOOLNAME = mlang

# Include this here so we can get the configuration of the targets that have
# been configured for construction. We have to do this early so we can set up
# LINK_COMPONENTS before including Makefile.rules
include $(LEVEL)/../../Makefile.config

LINK_COMPONENTS := $(TARGETS_TO_BUILD) asmparser bitreader bitwriter codegen \
                   instrumentation ipo selectiondag
USEDLIBS = mlangFrontendTool.a mlangFrontend.a mlangDriver.a \
           mlangSerialization.a mlangCodeGen.a mlangParse.a mlangSema.a \
           mlangAST.a mlangLex.a mlangBasic.a mlangDiag.a

include $(LEVEL)/Makefile.common

