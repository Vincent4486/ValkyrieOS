#!/bin/bash

# SPDX-License-Identifier: AGPL-3.0-or-later

DEPS_DEBIAN=(
    bison
    flex
    libmpfr-dev
    libgmp-dev
    libmpc-dev
    gcc
    nasm
    mtools
    python3
    python3-scons
    python3-sh
    python3-pyparted
    libguestfs-tools
    dosfstools
)

DEPS_FEDORA=(
    bison
    flex
    mpfr-devel
    gmp-devel
    libmpc-devel
    gcc
    nasm
    mtools
    python3
    python3-scons
    python3-sh
    python3-pyparted
    libguestfs-tools
    dosfstools
)

DEPS_ARCH=(
    bison
    flex
    mpfr
    gmp
    mpc
    gcc
    nasm
    mtools
    python3
    python3-scons
    python3-sh
    python3-pyparted
    libguestfs
    dosfstools
)

DEPS_SUSE=(
    bison
    flex
    mpfr-devel
    gmp-devel
    libmpc-devel
    gcc
    nasm
    mtools
    python3
    python3-scons
    python3-sh
    python3-pyparted
    libguestfs
    dosfstools
)

OS=
PACKAGE_UPDATE=
PACKAGE_INSTALL=
DEPS=

# Detect distro
if [ -x "$(command -v apk)" ]; then
    OS='alpine'
    echo "Alpine not supported."
    exit 1
elif [ -x "$(command -v apt-get)" ]; then
    OS='debian'
    PACKAGE_UPDATE='apt-get update'
    PACKAGE_INSTALL='apt-get install'
    DEPS="${DEPS_DEBIAN[@]}"
elif [ -x "$(command -v dnf)" ]; then
    OS='fedora'
    PACKAGE_INSTALL='dnf install'
    DEPS="${DEPS_FEDORA[@]}"
elif [ -x "$(command -v yum)" ]; then
    OS='fedora'
    PACKAGE_INSTALL='yum install'
    DEPS="${DEPS_FEDORA[@]}"
elif [ -x "$(command -v zypper)" ]; then
    OS='suse'
    PACKAGE_INSTALL='zypper install'
    DEPS="${DEPS_SUSE[@]}"
elif [ -x "$(command -v pacman)" ]; then
    OS='arch'
    PACKAGE_UPDATE='pacman -Syu'
    PACKAGE_INSTALL='pacman -S'
    DEPS="${DEPS_ARCH[@]}"
else
    echo "Unknown operating system!"; 
    exit 1
fi

# Install dependencies
echo ""
echo "Will install dependencies by running the following command."
echo ""
echo " $ $PACKAGE_INSTALL ${DEPS[@]}"
echo ""

read -p "Continue (y/n)?" choice
case "$choice" in 
  y|Y ) ;;
  * ) echo "Exiting..."
        exit 0
        ;;
esac

if [ ! -z "$PACKAGE_UPDATE" ]; then
    $PACKAGE_UPDATE
fi

$PACKAGE_INSTALL ${DEPS[@]}
