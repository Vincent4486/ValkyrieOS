// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
   uint8_t id;
   uint8_t type; // DISK_TYPE_FLOPPY or DISK_TYPE_ATA
   uint16_t cylinders;
   uint16_t sectors;
   uint16_t heads;
} DISK;

bool DISK_Initialize(DISK *disk, uint8_t driveNumber);
bool DISK_ReadSectors(DISK *disk, uint32_t lba, uint8_t sectors,
                      void *lowerDataOut);
