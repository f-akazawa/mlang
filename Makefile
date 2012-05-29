##===- projects/mlang/Makefile ----------------------------*- Makefile -*-===##
#
# This is mlang project Makefile that uses LLVM.
#
##===----------------------------------------------------------------------===##

#
# Indicates our relative path to the top of the project's root directory.
#
LEVEL = .
DIRS = libgnu include lib tools unittests
EXTRA_DIST = unittests include

# Set common mlang build flags.
CPP.Flags += -I$(LEVEL)/include -I$(LEVEL)/../../include

#
# Include the Master Makefile that knows how to build all.
#
include $(LEVEL)/Makefile.common

test::
	@ $(MAKE) -C test

clean::
	@ $(MAKE) -C test clean

.PHONY: test clean
