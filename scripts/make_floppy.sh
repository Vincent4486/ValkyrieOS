#!/bin/bash

TARGET=$1

STAGE1_STAGE2_LOCATION_OFFSET=480

dd if=/dev/zero of="$TARGET" bs=512 count=2880 >/dev/null 2>&1

# portable file-size helper: GNU stat (-c), BSD stat (-f), fallback wc -c
get_size() {
    local f="$1"
    if stat -c%s "$f" >/dev/null 2>&1; then
        stat -c%s "$f"
    elif stat -f%z "$f" >/dev/null 2>&1; then
        stat -f%z "$f"
    else
        wc -c <"$f" | tr -d ' '
    fi
}

STAGE2_SIZE=$(get_size "$BUILD_DIR/stage2.bin")
echo "--> Stage2 size: ${STAGE2_SIZE}"
STAGE2_SECTORS=$(( 1 + ( ( ${STAGE2_SIZE} + 511 ) / 512 ) ))
echo "--> Stage2 sectors: ${STAGE2_SECTORS}"
RESERVED_SECTORS=$(( 1 + ${STAGE2_SECTORS} ))
echo "--> Reserved sectors: ${RESERVED_SECTORS}"

mkfs.fat -F 12 -R "${RESERVED_SECTORS}" -n "VALKYRIE" "$TARGET" >/dev/null 2>&1

# install bootloader
dd if="${BUILD_DIR}/stage1.bin" of="$TARGET" conv=notrunc bs=1 count=3 >/dev/null 2>&1
dd if="${BUILD_DIR}/stage1.bin" of="$TARGET" conv=notrunc bs=1 seek=90 skip=90 >/dev/null 2>&1
dd if="${BUILD_DIR}/stage2.bin" of="$TARGET" conv=notrunc bs=512 seek=1 >/dev/null 2>&1

# write lba address of stage2 to bootloader
echo "01 00 00 00" | xxd -r -p | dd of="$TARGET" conv=notrunc bs=1 seek="$STAGE1_STAGE2_LOCATION_OFFSET" >/dev/null 2>&1
printf "%x" "${STAGE2_SECTORS}" | xxd -r -p | dd of="$TARGET" conv=notrunc bs=1 seek=$(( STAGE1_STAGE2_LOCATION_OFFSET + 4 )) >/dev/null 2>&1

mmd -i "$TARGET" "::sys" >/dev/null 2>&1 || true
# copy kernel ELF into the image so stage2 can load /sys/kernel.elf
mcopy -i "$TARGET" "${BUILD_DIR}/kernel.elf" "::/sys/kernel.elf" >/dev/null 2>&1
mmd -i "$TARGET" "::test" >/dev/null 2>&1 || true
mcopy -i "$TARGET" test.txt "::test.txt" >/dev/null 2>&1