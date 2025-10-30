#!/bin/bash

set -e

TARGET=$1

STAGE1_STAGE2_LOCATION_OFFSET=480

dd if=/dev/zero of="$TARGET" bs=512 count=2880 

# portable file-size helper: GNU stat (-c), BSD stat (-f), fallback wc -c
get_size() {
    local f="$1"
    # Prefer GNU stat (-c) if available, fall back to BSD stat (-f), then wc
    if command -v stat >/dev/null 2>&1; then
        if stat -c%s "$f" >/dev/null 2>&1; then
            stat -c%s "$f"
            return
        fi
        if stat -f%z "$f" >/dev/null 2>&1; then
            stat -f%z "$f"
            return
        fi
    fi
    wc -c <"$f" | tr -d ' '
}

STAGE2_SIZE=$(get_size "${BUILD_DIR}/stage2.bin")
echo "${STAGE2_SIZE}"
# number of 512-byte sectors needed to hold stage2 (round up)
STAGE2_SECTORS=$(( (STAGE2_SIZE + 511) / 512 ))
echo "${STAGE2_SECTORS}"
# reserved sectors: stage1 (1) + stage2 sectors
RESERVED_SECTORS=$(( 1 + STAGE2_SECTORS ))
echo "${RESERVED_SECTORS}"

mkfs.fat -F 12 -R "${RESERVED_SECTORS}" -n "VALKYRIE" "$TARGET" 

# install bootloader
dd if="${BUILD_DIR}/stage1.bin" of="$TARGET" conv=notrunc bs=1 count=3 
dd if="${BUILD_DIR}/stage1.bin" of="$TARGET" conv=notrunc bs=1 seek=90 skip=90 
dd if="${BUILD_DIR}/stage2.bin" of="$TARGET" conv=notrunc bs=512 seek=1 

# write lba address of stage2 to bootloader
echo "01 00 00 00" | xxd -r -p | dd of="$TARGET" conv=notrunc bs=1 seek="$STAGE1_STAGE2_LOCATION_OFFSET" 
printf "%x" "${STAGE2_SECTORS}" | xxd -r -p | dd of="$TARGET" conv=notrunc bs=1 seek=$(( STAGE1_STAGE2_LOCATION_OFFSET + 4 )) 

mmd -i "$TARGET" "::sys"  || true
# copy kernel ELF into the image so stage2 can load /sys/kernel.elf
mcopy -i "$TARGET" "${BUILD_DIR}/kernel.elf" "::/sys/kernel.elf" 
mmd -i "$TARGET" "::test"  || true
mcopy -i "$TARGET" "test.txt" "::test.txt" 
