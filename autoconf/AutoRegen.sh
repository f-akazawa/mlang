#!/bin/sh
# AutoRegen.sh
# Run this to generate all the initial makefiles, etc.

die () {
	echo "$@" 1>&2
	exit 1
}

# Print when --help is specified
usage() {
  cat <<EOF
Usage: $0 [OPTION]...
Bootstrap this package from the checked-out sources.

Options:
 --gnulib-srcdir=DIRNAME  Specify the local directory where gnulib
                          sources reside.  Use this if you already
                          have gnulib sources on your machine, and
                          do not want to waste your bandwidth downloading
                          them again.
 --copy                   Copy files instead of creating symbolic links.
 --force                  Attempt to bootstrap even if the sources seem
                          not to have been checked out.
 --skip-po                Do not download po files.

If the file $0.conf exists in the same directory as this script, its
contents are read as shell variables to configure the bootstrap.

For build prerequisites, environment variables like \$AUTOCONF and \$AMTAR
are honored.

Running without arguments will suffice in most cases.
EOF
}

# Parse options
for option
do
  case $option in
  --help)
    usage
    exit;;
  --gnulib-srcdir=*)
    GNULIB_SRCDIR=`expr "X$option" : 'X--gnulib-srcdir=\(.*\)'`;;
  --skip-po)
    SKIP_PO=t;;
  --force)
    checkout_only_file=;;
  --copy)
    copy=true;;
  *)
    echo >&2 "$0: $option: unknown option"
    exit 1;;
  esac
done

# Test autoconf dir and configure.ac in src tree
test -d autoconf && test -f autoconf/configure.ac && cd autoconf
test -f configure.ac || die "Can't find 'autoconf' dir; please cd into it first"

# Check version of autoconf tool, we need 2.6x
autoconf --version | egrep '2\.6[0-9]' > /dev/null
if test $? -ne 0 ; then
  die "Your autoconf was not detected as being 2.6x"
fi

# Get current dir where AutoRegen.sh in
cwd=`pwd`

# Remove old Gnulib build
GNULIB_DSTDIR=../libgnu
if test -d "$GNULIB_DSTDIR" ; then
  rm -fr $GNULIB_DSTDIR
fi

# Set llvm_m4 dir
if test -d ../../../autoconf/m4 ; then
  cd ../../../autoconf/m4
  llvm_m4=`pwd`
  llvm_src_root=../..
  llvm_obj_root=../..
  cd $cwd
elif test -d ../../llvm/autoconf/m4 ; then
  cd ../../llvm/autoconf/m4
  llvm_m4=`pwd`
  llvm_src_root=..
  llvm_obj_root=..
  cd $cwd
else
  while true ; do
    echo "LLVM source root not found." 
    read -p "Enter full path to LLVM source:" REPLY
    if test -d "$REPLY/autoconf/m4" ; then
      llvm_src_root="$REPLY"
      llvm_m4="$REPLY/autoconf/m4"
      read -p "Enter full path to LLVM objects (empty for same as source):" REPLY
      if test -d "$REPLY" ; then
        llvm_obj_root="$REPLY"
      else
        llvm_obj_root="$llvm_src_root"
      fi
      break
    fi
  done
fi

# Set local_m4 dir
local_m4=.
if test -d ./m4 ; then
  cd ./m4
  local_m4=`pwd`
  cd $cwd
fi

# Patch the LLVM_ROOT in configure.ac, if need
cp configure.ac configure.bak
sed -e "s#^LLVM_SRC_ROOT=.*#LLVM_SRC_ROOT=\"$llvm_src_root\"#" \
    -e "s#^LLVM_OBJ_ROOT=.*#LLVM_OBJ_ROOT=\"$llvm_obj_root\"#" configure.bak > configure.ac

# Execute autotools command to regenerate all
echo "Regenerating aclocal.m4 with aclocal"
rm -f aclocal.m4
aclocal -I $llvm_m4 -I "$llvm_m4/.." -I $local_m4|| die "aclocal failed"
echo "Regenerating configure with autoconf"
autoconf --warnings=all -o ../configure configure.ac || die "autoconf failed"
autoheader --force || die "autoheader failed"

# Copy config.h.in to src root
if test -f config.h.in ; then
  cp -f config.h.in ../config.h.in
  rm -f config.h.in
else
  echo "config.h.in not found"
  exit 1
fi

# Configure the Gnulib modules and build Makefiles for it
if test -d "$GNULIB_SRCDIR" ; then
  cd $GNULIB_SRCDIR
  GNULIB_SRCDIR_AB=`pwd`
  cd $cwd
else
  echo "$0 : gnulib-srcdir parameter is needed."
  echo "e.g. ./AutoRegen.sh --gnulib-srcdir=/PATH/TO/GNULIB_SRC"
  echo "see ./AutoRegen.sh --help for more details"
  exit 1
fi

gnulib_tool=$GNULIB_SRCDIR_AB/gnulib-tool
<$gnulib_tool || exit

# List of gnulib modules needed.
gnulib_modules=

# Set the required gnulib modules from external .conf file
if test -f "gnulib_modules.conf" ; then
  . ./gnulib_modules.conf
else
  echo "$0: gnulib_modules.conf not found"
  exit 1 
fi

# Build gnulib Makefile in SRC_ROOT/libgnu
echo "$0: gnulib_tool --create-testdir --dir=../libgnu ..."
$gnulib_tool --create-testdir --dir=$GNULIB_DSTDIR $gnulib_modules
echo "$0: gnulib configuration building ends ..."
cd $GNULIB_DSTDIR
./configure
cd $cwd

echo "$0: done.  Now you can run './configure'."
exit 0

