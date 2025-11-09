// SPDX-License-Identifier: AGPL-3.0-or-later

#include "ata.h"
#include <stdint.h>

// Driver data structure matching assembly expectations
typedef struct {
    uint32_t partition_length;   // offset 0 (dd_prtlen) - total sectors
    uint32_t start_lba;          // offset 4 (dd_stLBA) - partition start
    uint16_t dcr_port;           // offset 8 (dd_dcr) - alt status/DCR port
    uint16_t tf_port;            // offset 10 (dd_tf) - task file base port
    uint8_t  slave_bits;         // offset 12 (dd_sbits) - master/slave flags
} ata_driver_t;

// Assembly function declarations
extern int read_ata_st(uint32_t sector_count, void *buffer, void *driver_data, uint32_t lba);
extern void srst_ata_st(uint16_t dcr_port);

// Global driver instances for primary and secondary IDE
static ata_driver_t primary_master = {
    .partition_length = 0x100000,    // 1 million sectors (default)
    .start_lba = 2048,               // Typical partition start
    .dcr_port = 0x3F6,               // Primary IDE alt status
    .tf_port = 0x1F0,                // Primary IDE task file
    .slave_bits = 0xA0               // Master drive
};

static ata_driver_t primary_slave = {
    .partition_length = 0x100000,
    .start_lba = 2048,
    .dcr_port = 0x3F6,
    .tf_port = 0x1F0,
    .slave_bits = 0xB0               // Slave drive
};

static ata_driver_t secondary_master = {
    .partition_length = 0x100000,
    .start_lba = 2048,
    .dcr_port = 0x376,               // Secondary IDE alt status
    .tf_port = 0x170,                // Secondary IDE task file
    .slave_bits = 0xA0               // Master drive
};

static ata_driver_t secondary_slave = {
    .partition_length = 0x100000,
    .start_lba = 2048,
    .dcr_port = 0x376,
    .tf_port = 0x170,
    .slave_bits = 0xB0               // Slave drive
};

/**
 * Initialize ATA driver for a specific drive
 * @param channel - IDE channel (0 = primary, 1 = secondary)
 * @param drive - Drive on channel (0 = master, 1 = slave)
 * @param partition_start - Absolute LBA where partition starts
 * @param partition_size - Total sectors in partition
 */
void ata_init(int channel, int drive, uint32_t partition_start, uint32_t partition_size)
{
    ata_driver_t *drv = NULL;
    
    if (channel == 0 && drive == 0)
        drv = &primary_master;
    else if (channel == 0 && drive == 1)
        drv = &primary_slave;
    else if (channel == 1 && drive == 0)
        drv = &secondary_master;
    else if (channel == 1 && drive == 1)
        drv = &secondary_slave;
    else
        return;  // Invalid channel/drive
    
    drv->start_lba = partition_start;
    drv->partition_length = partition_size;
    
    // Perform software reset
    srst_ata_st(drv->dcr_port);
}

/**
 * Read sectors from ATA drive
 * @param channel - IDE channel (0 = primary, 1 = secondary)
 * @param drive - Drive on channel (0 = master, 1 = slave)
 * @param lba - Logical block address (relative to partition start)
 * @param buffer - Destination buffer
 * @param count - Number of sectors to read
 * @return 0 on success, -1 on failure
 */
int ata_read(int channel, int drive, uint32_t lba, uint8_t *buffer, uint32_t count)
{
    ata_driver_t *drv = NULL;
    
    if (channel == 0 && drive == 0)
        drv = &primary_master;
    else if (channel == 0 && drive == 1)
        drv = &primary_slave;
    else if (channel == 1 && drive == 0)
        drv = &secondary_master;
    else if (channel == 1 && drive == 1)
        drv = &secondary_slave;
    else
        return -1;  // Invalid channel/drive
    
    // Call assembly read function
    return read_ata_st(count, buffer, (void *)drv, lba);
}

/**
 * Write sectors to ATA drive (stub - not yet implemented)
 * @param channel - IDE channel (0 = primary, 1 = secondary)
 * @param drive - Drive on channel (0 = master, 1 = slave)
 * @param lba - Logical block address (relative to partition start)
 * @param buffer - Source buffer
 * @param count - Number of sectors to write
 * @return 0 on success, -1 on failure
 */
int ata_write(int channel, int drive, uint32_t lba, const uint8_t *buffer, uint32_t count)
{
    // TODO: Implement ATA write in assembly
    (void)channel;
    (void)drive;
    (void)lba;
    (void)buffer;
    (void)count;
    return -1;
}

/**
 * Perform software reset on ATA channel
 * @param channel - IDE channel (0 = primary, 1 = secondary)
 */
void ata_reset(int channel)
{
    uint16_t dcr_port = (channel == 0) ? 0x3F6 : 0x376;
    srst_ata_st(dcr_port);
}
