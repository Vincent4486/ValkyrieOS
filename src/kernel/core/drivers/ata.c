// SPDX-License-Identifier: AGPL-3.0-or-later

#include "ata.h"
#include <arch/i686/io.h>
#include <stdint.h>
#include <std/stdio.h>

// ATA register offsets from base port
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_NSECTOR    0x02
#define ATA_REG_LBA_LOW    0x03
#define ATA_REG_LBA_MID    0x04
#define ATA_REG_LBA_HIGH   0x05
#define ATA_REG_DEVICE     0x06
#define ATA_REG_STATUS     0x07
#define ATA_REG_COMMAND    0x07

// ATA status bits
#define ATA_STATUS_BSY     0x80  // Busy
#define ATA_STATUS_DRDY    0x40  // Device ready
#define ATA_STATUS_DRQ     0x08  // Data request
#define ATA_STATUS_ERR     0x01  // Error

// ATA commands
#define ATA_CMD_READ_PIO   0x20

// Driver data structure
typedef struct {
    uint32_t partition_length;   // Total sectors
    uint32_t start_lba;          // Partition start (should be 0 for absolute LBA)
    uint16_t dcr_port;           // Alt status/DCR port
    uint16_t tf_port;            // Task file base port
    uint8_t  slave_bits;         // Master/slave bits (0xA0 or 0xB0)
} ata_driver_t;

// Global driver instances
static ata_driver_t primary_master = {
    .partition_length = 0x100000,
    .start_lba = 0,
    .dcr_port = 0x3F6,
    .tf_port = 0x1F0,
    .slave_bits = 0xA0
};

static ata_driver_t primary_slave = {
    .partition_length = 0x100000,
    .start_lba = 0,
    .dcr_port = 0x3F6,
    .tf_port = 0x1F0,
    .slave_bits = 0xB0
};

static ata_driver_t secondary_master = {
    .partition_length = 0x100000,
    .start_lba = 0,
    .dcr_port = 0x376,
    .tf_port = 0x170,
    .slave_bits = 0xA0
};

static ata_driver_t secondary_slave = {
    .partition_length = 0x100000,
    .start_lba = 0,
    .dcr_port = 0x376,
    .tf_port = 0x170,
    .slave_bits = 0xB0
};

/**
 * Get driver for channel and drive
 */
static ata_driver_t* ata_get_driver(int channel, int drive)
{
    if (channel == 0 && drive == 0) return &primary_master;
    if (channel == 0 && drive == 1) return &primary_slave;
    if (channel == 1 && drive == 0) return &secondary_master;
    if (channel == 1 && drive == 1) return &secondary_slave;
    return NULL;
}

/**
 * Wait for drive to be ready (not busy)
 */
static int ata_wait_busy(uint16_t tf_port)
{
    int timeout = 0x1000000;  // Increased timeout
    while (timeout--) {
        uint8_t status = i686_inb(tf_port + ATA_REG_STATUS);
        if (!(status & ATA_STATUS_BSY))
            return 0;
    }
    printf("ATA: wait_busy timeout, status last read=0x%x\n", i686_inb(tf_port + ATA_REG_STATUS));
    return -1;  // Timeout
}

/**
 * Wait for data ready
 */
static int ata_wait_drq(uint16_t tf_port)
{
    int timeout = 0x1000000;  // Increased timeout
    while (timeout--) {
        uint8_t status = i686_inb(tf_port + ATA_REG_STATUS);
        if (status & ATA_STATUS_DRQ)
            return 0;
        if (status & ATA_STATUS_ERR)
            return -1;  // Error occurred
    }
    printf("ATA: wait_drq timeout, status last read=0x%x\n", i686_inb(tf_port + ATA_REG_STATUS));
    return -1;  // Timeout
}

/**
 * Perform software reset on ATA channel
 */
static void ata_soft_reset(uint16_t dcr_port)
{
    // Set SRST bit (software reset)
    i686_outb(dcr_port, 0x04);
    
    // Wait a bit
    for (volatile int i = 0; i < 100000; i++);
    
    // Clear SRST bit
    i686_outb(dcr_port, 0x00);
    
    // Wait for reset to complete
    for (volatile int i = 0; i < 100000; i++);
}

/**
 * Initialize ATA driver for a specific drive
 */
void ata_init(int channel, int drive, uint32_t partition_start, uint32_t partition_size)
{
    ata_driver_t *drv = ata_get_driver(channel, drive);
    if (!drv) return;
    
    drv->start_lba = 0;  // We use absolute LBA
    drv->partition_length = partition_size;
    
    // Perform software reset
    ata_soft_reset(drv->dcr_port);
    
    printf("ATA: Initialized ch=%d drv=%d (tf=0x%x, dcr=0x%x)\n",
           channel, drive, drv->tf_port, drv->dcr_port);
}

/**
 * Read sectors from ATA drive using PIO mode
 */
int ata_read(int channel, int drive, uint32_t lba, uint8_t *buffer, uint32_t count)
{
    ata_driver_t *drv = ata_get_driver(channel, drive);
    if (!drv || !buffer || count == 0)
        return -1;
    
    printf("ATA: read ch=%d drv=%d lba=0x%x count=%u\n", channel, drive, lba, count);
    
    // Wait for drive to be ready
    if (ata_wait_busy(drv->tf_port) != 0) {
        printf("ATA: Drive busy timeout\n");
        return -1;
    }
    
    // Set drive/head register with master/slave bits and upper LBA bits
    uint8_t device = drv->slave_bits | ((lba >> 24) & 0x0F);
    i686_outb(drv->tf_port + ATA_REG_DEVICE, device);
    
    // Small delay to allow drive to respond to device selection
    for (volatile int i = 0; i < 50000; i++);
    
    // Read status to ensure drive is ready
    uint8_t status = i686_inb(drv->tf_port + ATA_REG_STATUS);
    printf("ATA: Status after device select: 0x%x\n", status);
    
    // Set LBA low 24 bits and sector count
    i686_outb(drv->tf_port + ATA_REG_NSECTOR, count & 0xFF);
    i686_outb(drv->tf_port + ATA_REG_LBA_LOW,  (lba & 0xFF));
    i686_outb(drv->tf_port + ATA_REG_LBA_MID,  ((lba >> 8) & 0xFF));
    i686_outb(drv->tf_port + ATA_REG_LBA_HIGH, ((lba >> 16) & 0xFF));
    
    // Issue READ SECTORS command
    i686_outb(drv->tf_port + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    // Small delay to allow drive to process command
    for (volatile int i = 0; i < 50000; i++);
    
    status = i686_inb(drv->tf_port + ATA_REG_STATUS);
    printf("ATA: Status after command: 0x%x\n", status);
    
    // Read sectors
    for (uint32_t sec = 0; sec < count; sec++) {
        // Wait for data ready
        if (ata_wait_drq(drv->tf_port) != 0) {
            printf("ATA: DRQ timeout on sector %lu\n", (unsigned long)sec);
            return -1;
        }
        
        // Read 512 bytes (256 words) from data port
        uint8_t *dest = buffer + (sec * 512);
        for (int i = 0; i < 256; i++) {
            // Read 16-bit word from data port using two 8-bit reads
            uint8_t low = i686_inb(drv->tf_port + ATA_REG_DATA);
            uint8_t high = i686_inb(drv->tf_port + ATA_REG_DATA);
            dest[i * 2] = low;
            dest[i * 2 + 1] = high;
        }
        
        // Check for errors
        uint8_t status = i686_inb(drv->tf_port + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) {
            uint8_t error = i686_inb(drv->tf_port + ATA_REG_ERROR);
            printf("ATA: Read error at sector %lu, error=0x%x\n", (unsigned long)sec, error);
            return -1;
        }
    }
    
    printf("ATA: Successfully read %u sectors\n", count);
    return 0;
}

/**
 * Write sectors to ATA drive (stub)
 */
int ata_write(int channel, int drive, uint32_t lba, const uint8_t *buffer, uint32_t count)
{
    (void)channel;
    (void)drive;
    (void)lba;
    (void)buffer;
    (void)count;
    printf("ATA: Write not yet implemented\n");
    return -1;
}

/**
 * Perform software reset on ATA channel
 */
void ata_reset(int channel)
{
    uint16_t dcr_port = (channel == 0) ? 0x3F6 : 0x376;
    ata_soft_reset(dcr_port);
}
