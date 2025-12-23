// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef PARTITION_H
#define PARTITION_H

#include "disk.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
   DISK *disk;
   uint32_t partitionOffset;
   uint32_t partitionSize;
} Partition;

void MBR_DetectPartition(Partition *part, DISK *disk, void *partition);

bool Partition_ReadSectors(Partition *disk, uint32_t lba, uint8_t sectors,
                           void *lowDataOut);

bool Partition_WriteSectors(Partition *part, uint32_t lba, uint8_t sectors,
                            const void *lowerDataIn);

#endif