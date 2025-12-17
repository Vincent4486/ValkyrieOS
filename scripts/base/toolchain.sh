#!/bin/bash 

# SPDX-License-Identifier: AGPL-3.0-or-later

BINUTILS_VERSION=2.37
GCC_VERSION=15.2.0
MUSL_VERSION=1.2.5

TARGET=i686-elf

BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz"
MUSL_URL="https://musl.libc.org/releases/musl-${MUSL_VERSION}.tar.gz"

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

    # Download and build musl
    MUSL_SRC="musl-${MUSL_VERSION}"
    MUSL_BUILD="musl-build-${MUSL_VERSION}"

    wget ${MUSL_URL}
    tar -xf musl-${MUSL_VERSION}.tar.gz
    mkdir -p ${MUSL_BUILD}
    cd ${MUSL_BUILD}
    CFLAGS= ASMFLAGS= LD= ASM= LINKFLAGS= LIBS=
    ../musl-${MUSL_VERSION}/configure \
            --prefix="${TOOLCHAIN_PREFIX}" \
            --host=${TARGET} \
            --enable-static \
            --disable-shared
    make -j8
    make install

elif [ "$OPERATION" = "clean" ]; then
	rm -rf *
fi
