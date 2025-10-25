#!/bin/bash

set -e

TARGET=$1
SIZE=$2

STAGE1_STAGE2_LOCATION_OFFSET=480

DISK_SECTOR_COUNT=$(( (${SIZE} + 511 ) / 512 ))

DISK_PART1_BEGIN=2048
DISK_PART1_END=$(( ${DISK_SECTOR_COUNT} - 1 ))

# generate image file
echo "Generating disk image ${TARGET} (${DISK_SECTOR_COUNT} sectors)..."
dd if=/dev/zero of=$TARGET bs=512 count=${DISK_SECTOR_COUNT} >/dev/null

OS="$(uname -s)"

if [ "$OS" = "Darwin" ]; then
    echo "Running on macOS — using hdiutil/diskutil instead of parted/losetup."

    # create partition table + single FAT partition using diskutil
    echo "Creating partition (MBR + FAT32)..."
    # attach raw image -> get /dev/diskN
    # Try attaching as a raw disk image first (CRawDiskImage). Older hdiutil may need the flag.
    DEVICE=$(sudo hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$TARGET" 2>/dev/null | awk '/^\/dev/{print $1; exit}')
    # Fallback to plain attach if the raw image key is not supported
    if [ -z "$DEVICE" ]; then
        DEVICE=$(sudo hdiutil attach -nomount "$TARGET" 2>/dev/null | awk '/^\/dev/{print $1; exit}')
    fi
    if [ -z "$DEVICE" ]; then
        echo "Failed to attach image with hdiutil (try running with sudo)"
        exit 1
    fi
    echo "Attached as $DEVICE"

    # Partition the raw device as MBR with a single FAT32 partition named VALKYRIE
    # Note: diskutil will create and mount the partition
    sudo diskutil partitionDisk "$DEVICE" MBRFormat "MS-DOS FAT32" VALKYRIE 100% >/dev/null

    # find the partition node (e.g. /dev/disk2s1)
    # Most attachments expose the partition as "${DEVICE}s1"
    TARGET_PARTITION="${DEVICE}s1"
    if [ ! -e "$TARGET_PARTITION" ]; then
        # Fallback: pick the first slice device if present
        TARGET_PARTITION=$(ls ${DEVICE}s* 2>/dev/null | head -n1 || true)
    fi
    if [ -z "$TARGET_PARTITION" ] || [ ! -e "$TARGET_PARTITION" ]; then
        echo "Failed to locate partition device for $DEVICE"
        sudo hdiutil detach /dev/diskN || true
        hdiutil info
        sudo diskutil list
        exit 1
    fi

    echo "Partition device: ${TARGET_PARTITION}"

    # mac stat bytes:
    STAGE2_SIZE=$(stat -f%z "${BUILD_DIR}/stage2.bin")
    echo ${STAGE2_SIZE}
    STAGE2_SECTORS=$(( ( ${STAGE2_SIZE} + 511 ) / 512 ))
    echo ${STAGE2_SECTORS}

    if [ ${STAGE2_SECTORS} -gt $(( ${DISK_PART1_BEGIN} - 1 )) ]; then
        echo "Stage2 too big!!!"
        sudo hdiutil detach /dev/diskN || true
        hdiutil info
        sudo diskutil list
        exit 2
    fi

    # write stage2 at LBA 1 (same as original script)
    sudo dd if=${BUILD_DIR}/stage2.bin of="$TARGET" conv=notrunc bs=512 seek=1 >/dev/null

    echo "Formatting ${TARGET_PARTITION} (already formatted by diskutil)..."
    # diskutil already formatted/mounted; ensure volume name
    # If not mounted, mount it
    if ! mount | grep -q "${TARGET_PARTITION}"; then
        mkdir -p /tmp/valkyrie
        sudo mount -t msdos "${TARGET_PARTITION}" /tmp/valkyrie || true
    fi

    # install bootloader onto partition device (use raw partition)
    echo "Installing bootloader on ${TARGET_PARTITION}..."
    sudo dd if=${BUILD_DIR}/stage1.bin of=${TARGET_PARTITION} conv=notrunc bs=1 count=3 2>/dev/null >/dev/null || true
    sudo dd if=${BUILD_DIR}/stage1.bin of=${TARGET_PARTITION} conv=notrunc bs=1 seek=90 skip=90 2>/dev/null >/dev/null || true

    # write lba address of stage2 to bootloader (seek offset within partition device)
    echo "01 00 00 00" | xxd -r -p | sudo dd of=${TARGET_PARTITION} conv=notrunc bs=1 seek=$STAGE1_STAGE2_LOCATION_OFFSET >/dev/null 2>&1
    printf "%x" ${STAGE2_SECTORS} | xxd -r -p | sudo dd of=${TARGET_PARTITION} conv=notrunc bs=1 seek=$(( $STAGE1_STAGE2_LOCATION_OFFSET + 4 )) >/dev/null 2>&1

    # copy files (use mount point created by diskutil or ensure mounted at /tmp/valkyrie)
    MOUNTPOINT=$(mount | awk -v p="$TARGET_PARTITION" '$1==p {print $3}')
    if [ -z "$MOUNTPOINT" ]; then
        MOUNTPOINT=/tmp/valkyrie
        mkdir -p "$MOUNTPOINT"
        sudo mount -t msdos "${TARGET_PARTITION}" "$MOUNTPOINT"
    fi

    echo "Copying files to ${TARGET_PARTITION} (mounted on ${MOUNTPOINT})..."
    sudo cp ${BUILD_DIR}/kernel.bin "${MOUNTPOINT}/"
    sudo cp test.txt "${MOUNTPOINT}/"
    sudo mkdir -p "${MOUNTPOINT}/test"
    sudo cp test.txt "${MOUNTPOINT}/test/"
    sync
    sudo diskutil eject "$DEVICE" >/dev/null || hdiutil detach "$DEVICE" >/dev/null || true

else
    # Linux flow (original)
    echo "Creating partition..."
    parted -s $TARGET mklabel msdos
    parted -s $TARGET mkpart primary ${DISK_PART1_BEGIN}s ${DISK_PART1_END}s
    parted -s $TARGET set 1 boot on

    STAGE2_SIZE=$(stat -c%s ${BUILD_DIR}/stage2.bin)
    echo ${STAGE2_SIZE}
    STAGE2_SECTORS=$(( ( ${STAGE2_SIZE} + 511 ) / 512 ))
    echo ${STAGE2_SECTORS}

    if [ ${STAGE2_SECTORS} \> $(( ${DISK_PART1_BEGIN} - 1 )) ]; then
        echo "Stage2 too big!!!"
        exit 2
    fi

    dd if=${BUILD_DIR}/stage2.bin of=$TARGET conv=notrunc bs=512 seek=1 #>/dev/null

    # create loopback device
    DEVICE=$(losetup -fP --show ${TARGET})
    echo "Created loopback device ${DEVICE}"
    TARGET_PARTITION="${DEVICE}p1"

    # create file system
    echo "Formatting ${TARGET_PARTITION}..."
    mkfs.fat -n "VALKYRIE" $TARGET_PARTITION >/dev/null

    # install bootloader
    echo "Installing bootloader on ${TARGET_PARTITION}..."
    dd if=${BUILD_DIR}/stage1.bin of=$TARGET_PARTITION conv=notrunc bs=1 count=3 2>&1 >/dev/null
    dd if=${BUILD_DIR}/stage1.bin of=$TARGET_PARTITION conv=notrunc bs=1 seek=90 skip=90 2>&1 >/dev/null

    # write lba address of stage2 to bootloader
    echo "01 00 00 00" | xxd -r -p | dd of=$TARGET_PARTITION conv=notrunc bs=1 seek=$STAGE1_STAGE2_LOCATION_OFFSET
    printf "%x" ${STAGE2_SECTORS} | xxd -r -p | dd of=$TARGET_PARTITION conv=notrunc bs=1 seek=$(( $STAGE1_STAGE2_LOCATION_OFFSET + 4 ))

    # copy files
    echo "Copying files to ${TARGET_PARTITION} (mounted on /tmp/valkyrie)..."
    mkdir -p /tmp/valkyrie
    mount ${TARGET_PARTITION} /tmp/valkyrie
    cp ${BUILD_DIR}/kernel.bin /tmp/valkyrie
    cp test.txt /tmp/valkyrie
    mkdir /tmp/valkyrie/test
    cp test.txt /tmp/valkyrie/test
    umount /tmp/valkyrie

    # destroy loopback device
    losetup -d ${DEVICE}
fi