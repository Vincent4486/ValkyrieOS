// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <fs/disk.h>
#include <fs/partition.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Initialize storage system: disk detection, partition detection, and FAT
 * filesystem
 *
 * Detects whether the boot drive is a floppy or hard disk,
 * reads the MBR for hard disks, detects partition information,
 * and initializes the FAT filesystem.
 *
 * @param disk - Pointer to DISK structure to initialize
 * @param partition - Pointer to Partition structure to populate
 * @param bootDrive - BIOS drive number
 * @return true on success, false on failure
 */
bool FS_Initialize(DISK *disk, Partition *partition, uint8_t bootDrive);
