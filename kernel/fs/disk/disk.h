// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <kernel/fs/fs.h>

typedef struct
{
   uint8_t id;
   uint8_t type; // DISK_TYPE_FLOPPY or DISK_TYPE_ATA
   uint16_t cylinders;
   uint16_t sectors;
   uint16_t heads;
} DISK;

/* Disk/Storage device information */
typedef struct {
    uint8_t type;                /* Device type (ATA, SCSI, USB, etc) */
    uint8_t interface;           /* Interface type (IDE, SATA, NVMe, etc) */
    uint32_t sector_size;        /* Sector size in bytes */
    uint32_t total_sectors;      /* Total number of sectors */
    uint64_t total_size;         /* Total size in bytes */
    uint8_t removable;           /* 1 if removable, 0 if fixed */
    uint8_t status;              /* Device status (online, offline, etc) */
    char device_name[32];        /* Device name (e.g., /dev/sda) */
} __attribute__((packed)) DISK_Info;

bool DISK_Initialize(DISK *disk, uint8_t driveNumber);
bool DISK_ReadSectors(DISK *disk, uint32_t lba, uint8_t sectors,
                      void *lowerDataOut);
bool DISK_WriteSectors(DISK *disk, uint32_t lba, uint8_t sectors,
                       const void *dataIn);
