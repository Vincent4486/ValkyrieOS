#!/bin/bash 

# SPDX-License-Identifier: AGPL-3.0-or-later

BINUTILS_VERSION=2.37
GCC_VERSION=15.2.0
NEWLIB_VERSION=4.4.0

TARGET=i686-elf

BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz"
NEWLIB_URL="https://sourceware.org/pub/newlib/newlib-${NEWLIB_VERSION}.tar.gz"

# ---------------------------

set -e

TOOLCHAINS_DIR=toolchain
OPERATION='build'

while test $# -gt 0
do
    case "$1" in
        -c) OPERATION='clean'
            ;;
        *)  TOOLCHAINS_DIR="$1"
            ;;
    esac
    shift
done

if [ -z "$TOOLCHAINS_DIR" ]; then
    echo "Missing arg: toolchains directory"
    exit 1
fi

mkdir -p "$TOOLCHAINS_DIR"
cd "$TOOLCHAINS_DIR"
TOOLCHAIN_PREFIX="$(pwd)/${TARGET}"

if [ "$OPERATION" = "build" ]; then

    # Download and build binutils
    BINUTILS_SRC="binutils-${BINUTILS_VERSION}"
    BINUTILS_BUILD="binutils-build-${BINUTILS_VERSION}"

    wget ${BINUTILS_URL}
    tar -xf binutils-${BINUTILS_VERSION}.tar.xz

    mkdir -p ${BINUTILS_BUILD}
    cd ${BINUTILS_BUILD}
    CFLAGS= ASMFLAGS= CC= CXX= LD= ASM= LINKFLAGS= LIBS= 
    ../binutils-${BINUTILS_VERSION}/configure \
        --prefix="${TOOLCHAIN_PREFIX}"	\
        --target=${TARGET}				\
        --with-sysroot					\
        --disable-nls					\
        --disable-werror
    make -j8 
    make install

    cd ..

    # Download and build GCC
    GCC_SRC="gcc-${GCC_VERSION}"
    GCC_BUILD="gcc-build-${GCC_VERSION}"

    wget ${GCC_URL}
    tar -xf gcc-${GCC_VERSION}.tar.xz
    mkdir -p ${GCC_BUILD}
    CFLAGS= ASMFLAGS= LD= ASM= LINKFLAGS= LIBS= 
    cd ${GCC_BUILD}
    ../gcc-${GCC_VERSION}/configure \
            --prefix="${TOOLCHAIN_PREFIX}" \
            --target=${TARGET} \
            --disable-nls \
            --enable-languages=c,c++ \
            --without-headers
    make -j8 all-gcc all-target-libgcc
    make install-gcc install-target-libgcc

    cd ..

    # Download and build newlib
    NEWLIB_SRC="newlib-${NEWLIB_VERSION}"
    NEWLIB_BUILD="newlib-build-${NEWLIB_VERSION}"

    wget ${NEWLIB_URL}
    tar -xf newlib-${NEWLIB_VERSION}.tar.gz
    mkdir -p ${NEWLIB_BUILD}
    cd ${NEWLIB_BUILD}
    CFLAGS= ASMFLAGS= LD= ASM= LINKFLAGS= LIBS=
    ../newlib-${NEWLIB_VERSION}/configure \
            --prefix="${TOOLCHAIN_PREFIX}" \
            --target=${TARGET} \
            --disable-newlib-supplied-syscalls
    make -j8
    make install

elif [ "$OPERATION" = "clean" ]; then
	rm -rf *
fi
