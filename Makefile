##===- projects/mlang/Makefile ----------------------------*- Makefile -*-===##
#
# This is mlang project Makefile that uses LLVM.
#
##===----------------------------------------------------------------------===##

#
# Indicates our relative path to the top of the project's root directory.
#
LEVEL = .
DIRS = utils/TableGen include lib tools unittests
EXTRA_DIST = include

#
# Include the Master Makefile that knows how to build all.
#
include $(LEVEL)/Makefile.common
