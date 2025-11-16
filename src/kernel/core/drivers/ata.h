// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>

#define ATA_SECTOR_SIZE 512

// IDE Channel constants
#define ATA_CHANNEL_PRIMARY   0
#define ATA_CHANNEL_SECONDARY 1

// Drive constants
#define ATA_DRIVE_MASTER 0
#define ATA_DRIVE_SLAVE  1

/**
 * Initialize ATA driver for a specific drive
 * @param channel - IDE channel (ATA_CHANNEL_PRIMARY or ATA_CHANNEL_SECONDARY)
 * @param drive - Drive on channel (ATA_DRIVE_MASTER or ATA_DRIVE_SLAVE)
 * @param partition_start - Absolute LBA where partition starts
 * @param partition_size - Total sectors in partition
 */
void ata_init(int channel, int drive, uint32_t partition_start, uint32_t partition_size);

/**
 * Read sectors from ATA drive
 * @param channel - IDE channel (ATA_CHANNEL_PRIMARY or ATA_CHANNEL_SECONDARY)
 * @param drive - Drive on channel (ATA_DRIVE_MASTER or ATA_DRIVE_SLAVE)
 * @param lba - Logical block address (relative to partition start)
 * @param buffer - Destination buffer (must be at least count*512 bytes)
 * @param count - Number of sectors to read
 * @return 0 on success, -1 on failure
 */
int ata_read(int channel, int drive, uint32_t lba, uint8_t *buffer, uint32_t count);

/**
 * Write sectors to ATA drive
 * @param channel - IDE channel (ATA_CHANNEL_PRIMARY or ATA_CHANNEL_SECONDARY)
 * @param drive - Drive on channel (ATA_DRIVE_MASTER or ATA_DRIVE_SLAVE)
 * @param lba - Logical block address (relative to partition start)
 * @param buffer - Source buffer
 * @param count - Number of sectors to write
 * @return 0 on success, -1 on failure
 */
int ata_write(int channel, int drive, uint32_t lba, const uint8_t *buffer, uint32_t count);

/**
 * Perform software reset on ATA channel
 * @param channel - IDE channel (ATA_CHANNEL_PRIMARY or ATA_CHANNEL_SECONDARY)
 */
void ata_reset(int channel);
