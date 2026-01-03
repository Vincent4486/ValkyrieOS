// SPDX-License-Identifier: AGPL-3.0-or-later

#include "disk.h"
#include "partition.h"
#include <drivers/ata/ata.h>
#include <drivers/fdc/fdc.h>
#include <std/stdio.h>
#include <sys/sys.h>

// Disk type constants
#define DISK_TYPE_FLOPPY 0
#define DISK_TYPE_ATA 1

// Updated: Scan all disks and populate volumes
int DISK_Initialize() {
    printf("[DISK] Starting disk initialization\n");
    DISK detectedDisks[32];  // Temp array for detected disks
    int totalDisks = 0;

    // Scan floppies
    totalDisks += FDC_Scan(detectedDisks + totalDisks, 32 - totalDisks);

    // Scan ATA
    totalDisks += ATA_Scan(detectedDisks + totalDisks, 32 - totalDisks);

    printf("[DISK] Total disks detected: %d\n", totalDisks);

    // Populate volume[] with detected disks and partitions
    for (int i = 0; i < totalDisks; i++) {
        DISK *disk = &detectedDisks[i];
        int volumeIndex = -1;
        for (int j = 0; j < 32; j++) {
            if (g_SysInfo->volume[j].disk == NULL) {
                volumeIndex = j;
                break;
            }
        }
        if (volumeIndex == -1) break;  // No slots

        g_SysInfo->volume[volumeIndex].disk = disk;
        if (disk->type == DISK_TYPE_FLOPPY) {
            g_SysInfo->volume[volumeIndex].partitionOffset = 0;
            g_SysInfo->volume[volumeIndex].partitionSize = disk->cylinders * disk->heads * disk->sectors;
        } else {
            // Detect partitions for ATA
            uint8_t mbr_buffer[512];
            if (DISK_ReadSectors(disk, 0, 1, mbr_buffer)) {
                void *partition_entry = &mbr_buffer[446];
                bool found = false;
                for (int p = 0; p < 4; p++) {
                    uint8_t *entry = (uint8_t *)partition_entry + (p * 16);
                    uint8_t type = entry[4];
                    if (type == 0x04 || type == 0x06 || type == 0x0B || type == 0x0C) {
                        g_SysInfo->volume[volumeIndex].partitionOffset = *(uint32_t *)(entry + 8);
                        g_SysInfo->volume[volumeIndex].partitionSize = *(uint32_t *)(entry + 12);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    g_SysInfo->volume[volumeIndex].partitionOffset = 16;
                    g_SysInfo->volume[volumeIndex].partitionSize = 0x100000;
                }
            } else {
                g_SysInfo->volume[volumeIndex].partitionOffset = 0;
                g_SysInfo->volume[volumeIndex].partitionSize = 0x100000;
            }
        }
        printf("[DISK] Populated volume[%d]: Offset=%u, Size=%u\n", volumeIndex, g_SysInfo->volume[volumeIndex].partitionOffset, g_SysInfo->volume[volumeIndex].partitionSize);
    }
    g_SysInfo->disk_count = totalDisks;
    printf("[DISK] Disk initialization complete, disk_count=%u\n", g_SysInfo->disk_count);
    return totalDisks > 0 ? totalDisks : -1;
}

void DISK_LBA2CHS(DISK *disk, uint32_t lba, uint16_t *cylinderOut,
                  uint16_t *sectorOut, uint16_t *headOut)
{
   // sector = (LBA % sectors per track + 1)
   *sectorOut = lba % disk->sectors + 1;

   // cylinder = (LBA / sectors per track) / heads
   *cylinderOut = (lba / disk->sectors) / disk->heads;

   // head = (LBA / sectors per track) % heads
   *headOut = (lba / disk->sectors) % disk->heads;
}

bool DISK_ReadSectors(DISK *disk, uint32_t lba, uint8_t sectors, void *dataOut)
{
   if (sectors == 0) return false;

   if (disk->type == DISK_TYPE_FLOPPY)
   {
      /* Floppy drive: use the kernel FDC driver which speaks directly to the
       * floppy controller. This avoids relying on BIOS INT13 services from
       * the kernel.
       */
      int rc = FDC_ReadLba(disk->id, lba, (uint8_t *)dataOut, sectors);
      if (rc != 0) return false;
      return true;
   }
   else if (disk->type == DISK_TYPE_ATA)
   {
      /* Hard disk (ATA): use the kernel ATA driver with primary master
       * channel/drive.
       */
      int rc = ATA_Read(ATA_CHANNEL_PRIMARY, ATA_DRIVE_MASTER, lba,
                        (uint8_t *)dataOut, sectors);
      if (rc != 0) return false;
      return true;
   }

   return false;
}

bool DISK_WriteSectors(DISK *disk, uint32_t lba, uint8_t sectors,
                       const void *dataIn)
{
   if (sectors == 0) return false;

   if (disk->type == DISK_TYPE_FLOPPY)
   {
      /* Floppy drive: use the kernel FDC driver which speaks directly to the
       * floppy controller.
       */
      int rc = FDC_WriteLba(disk->id, lba, (const uint8_t *)dataIn, sectors);
      if (rc != 0) return false;
      return true;
   }
   else if (disk->type == DISK_TYPE_ATA)
   {
      /* Hard disk (ATA): use the kernel ATA driver with primary master
       * channel/drive.
       */
      int rc = ATA_Write(ATA_CHANNEL_PRIMARY, ATA_DRIVE_MASTER, lba,
                         (const uint8_t *)dataIn, sectors);
      if (rc != 0) return false;
      return true;
   }

   return false;
}
