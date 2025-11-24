#!/bin/bash

# SPDX-License-Identifier: AGPL-3.0-or-later

QEMU_ARGS='-debugcon stdio -m 32 -machine pc'

if [ "$#" -le 1 ]; then
    echo "Usage: $0 <image_type> <image>"
    exit 1
fi

case "$1" in
    "floppy")   QEMU_ARGS="${QEMU_ARGS} -fda $2"
    ;;
    "disk")     QEMU_ARGS="${QEMU_ARGS} -drive file=$2,format=raw,if=ide,index=0,media=disk"
    ;;
    *)          echo "Unknown image type $1."
                exit 2
esac

qemu-system-x86_64 $QEMU_ARGS