##===- unittests/Frontend/Makefile -------------------------*- Makefile -*-===##
#
# Copyright (C) 2010 yabin @ CGCL
# HuaZhong University of Science and Technology, China
#
##===----------------------------------------------------------------------===##

LEVEL = ../..
TESTNAME = Frontend
LINK_COMPONENTS := support mc core
USEDLIBS := mlangFrontend.a mlangDriver.a mlangSerialization.a mlangCodeGen.a \
            mlangParse.a mlangSema.a mlangAST.a \
            mlangLex.a mlangBasic.a mlangDiag.a

include $(LEVEL)/Makefile.common
include $(LEVEL)/unittests/Makefile.unittest
