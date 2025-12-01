// SPDX-License-Identifier: AGPL-3.0-or-later

#include "init.h"
#include <fs/disk.h>
#include <fs/fat.h>
#include <fs/partition.h>
#include <stdint.h>
#include <sys/memdefs.h>

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
bool FS_Initialize(DISK *disk, Partition *partition, uint8_t bootDrive)
{
   /* Initialize DISK structure */
   if (!DISK_Initialize(disk, bootDrive))
   {
      return false;
   }

   partition->disk = disk;

   /* For hard disks, read MBR and detect partition */
   if (disk->id >= 0x80) // Hard disk
   {
      /* Read MBR (sector 0) to detect partitions */
      uint8_t mbr_buffer[512];
      if (!DISK_ReadSectors(disk, 0, 1, mbr_buffer))
      {
         return false;
      }

      /* Partition table is at offset 446 (0x1BE), each entry is 16 bytes */
      void *partition_entry = &mbr_buffer[446];

      /* Try to detect first FAT partition (types 0x04, 0x06, 0x0B, 0x0C) */
      bool found_partition = false;
      for (int i = 0; i < 4; i++)
      {
         uint8_t *entry_ptr = (uint8_t *)partition_entry + (i * 16);
         uint8_t type = entry_ptr[4];

         /* Check for FAT partition types */
         if (type == 0x04 || type == 0x06 || type == 0x0B || type == 0x0C)
         {
            /* Extract LBA start and size (little-endian) */
            partition->partitionOffset = *(uint32_t *)(entry_ptr + 8);
            partition->partitionSize = *(uint32_t *)(entry_ptr + 12);

            found_partition = true;
            break;
         }
      }

      if (!found_partition)
      {
         partition->partitionOffset = 16;
         partition->partitionSize = 0x100000;
      }
   }
   else // Floppy: use whole disk
   {
      partition->partitionOffset = 0;
      partition->partitionSize = disk->sectors;
   }

   if(!Partition_ReadSectors(partition, 0, 1, MEMORY_FAT_ADDR)){
      return -1;
   }

   /* Initialize FAT filesystem on the detected partition */
   if (!FAT_Initialize(partition))
   {
      return false;
   }

   return true;
}
