dnl **************************************************************************
dnl * Initialize
dnl **************************************************************************
EXTERN_CFLAGS="$CFLAGS"
EXTERN_CXXFLAGS="$CXXFLAGS"

AC_INIT([Mlang],[0.0.9],[yabin.hwu@google.com])

dnl PACKAGE_VERSION is set by the AC_INIT VERSION arg
MLANG_VERSION="$PACKAGE_VERSION"
MLANG_API_VERSION_NUMBER="01"
MLANG_API_VERSION="api-v$MLANG_API_VERSION_NUMBER+"
MLANG_RELEASE_DATE="2010-12-31"
MLANG_COPYRIGHT="Copyright (C) 2010 Yabin Hu."
AC_SUBST(MLANG_VERSION)
AC_SUBST(MLANG_API_VERSION_NUMBER)
AC_SUBST(MLANG_API_VERSION)
AC_SUBST(MLANG_RELEASE_DATE)
AC_SUBST(MLANG_COPYRIGHT)

AC_REVISION($Revision: 1.001 $)
AC_PREREQ([2.62])

AC_CONFIG_HEADER([config.h])

AC_CONFIG_MACRO_DIR([m4])

dnl Identify where LLVM source tree is
LLVM_SRC_ROOT="../.."
LLVM_OBJ_ROOT="../.."

dnl Tell autoconf that this is an LLVM project being configured
dnl This provides the --with-llvmsrc and --with-llvmobj options
LLVM_CONFIG_PROJECT($LLVM_SRC_ROOT,$LLVM_OBJ_ROOT)

dnl Tell autoconf that the auxilliary files are actually located in
dnl the LLVM autoconf directory, not here.
AC_CONFIG_AUX_DIR($LLVM_SRC/autoconf)

dnl Verify that the source directory is valid
AC_CONFIG_SRCDIR(["Makefile.common.in"])

dnl Configure a common Makefile
AC_CONFIG_FILES(Makefile.common)

dnl Configure project makefiles
dnl List every Makefile that exists within your source tree
AC_CONFIG_MAKEFILE(Makefile)
AC_CONFIG_MAKEFILE(libgnu/Makefile)
AC_CONFIG_MAKEFILE(include/Makefile)
AC_CONFIG_MAKEFILE(lib/Makefile)
AC_CONFIG_MAKEFILE(tools/Makefile)
AC_CONFIG_MAKEFILE(unittests/Makefile)


AC_DEFINE(MLANG_SOURCE, 1, [Define if this is MLANG.])

dnl **************************************************************************
dnl * Determine which system we are building on
dnl **************************************************************************
MLANG_HOST_TYPE

dnl **************************************************************************
dnl * Check for programs.
dnl **************************************************************************

dnl **************************************************************************
dnl * Check for libraries.
dnl **************************************************************************


dnl **************************************************************************
dnl * Checks for header files.
dnl **************************************************************************

dnl **************************************************************************
dnl * Checks for typedefs, structures, and compiler characteristics.
dnl **************************************************************************

dnl **************************************************************************
dnl * Checks for library functions.
dnl **************************************************************************

dnl **************************************************************************
dnl * Enable various compile-time options
dnl **************************************************************************

dnl **************************************************************************
dnl * Set the location of various third-party software packages
dnl **************************************************************************

dnl **************************************************************************
dnl * Create the output files
dnl **************************************************************************

dnl This must be last
AC_OUTPUT
