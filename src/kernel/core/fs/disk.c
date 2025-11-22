// SPDX-License-Identifier: AGPL-3.0-or-later

#include "disk.h"
#include <drivers/ata.h>
#include <drivers/fdc.h>
#include <std/stdio.h>

// Disk type constants
#define DISK_TYPE_FLOPPY 0
#define DISK_TYPE_ATA    1

bool DISK_Initialize(DISK *disk, uint8_t driveNumber)
{
   uint8_t driveType;
   uint16_t cylinders, sectors, heads;

   disk->id = driveNumber;

   /* Detect disk type based on BIOS drive number convention:
    * 0x00-0x7F: Floppy drives
    * 0x80+:     Hard disks (ATA/SATA)
    */
   if (driveNumber < 0x80)
   {
      // Floppy drive detected
      disk->type = DISK_TYPE_FLOPPY;
      fdc_reset();
      cylinders = 80;
      heads = 2;
      sectors = 18;
      disk->cylinders = cylinders;
      disk->heads = heads;
      disk->sectors = sectors;
      printf("DISK: Floppy disk detected (drive 0x%x)\n", driveNumber);
      return true;
   }
   else
   {
      // Hard disk (ATA) detected
      disk->type = DISK_TYPE_ATA;
      printf("DISK: ATA hard disk detected (drive 0x%x)\n", driveNumber);
      
      // Initialize ATA driver for primary master channel
      // We'll use the standard primary master (IDE0 master) for hard disk access
      ata_init(ATA_CHANNEL_PRIMARY, ATA_DRIVE_MASTER, 0, 0x100000);
      
      printf("DISK: ATA driver initialized\n");
      return true;
   }

   return false;
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
   if (sectors == 0)
   {
      printf("DISK: ReadSectors called with sectors=0 (lba=%lu)\n",
             (unsigned long)lba);
      return false;
   }

   if (disk->type == DISK_TYPE_FLOPPY)
   {
      /* Floppy drive: use the kernel FDC driver which speaks directly to the
       * floppy controller. This avoids relying on BIOS INT13 services from
       * the kernel.
       */
      printf("DISK: FDC read from LBA %lu, sectors %u\n", (unsigned long)lba, sectors);
      int rc = fdc_read_lba(lba, (uint8_t *)dataOut, sectors);
      if (rc != 0)
      {
         printf("DISK: FDC read failed (lba=%lu, sectors=%u, error=%d)\n",
                (unsigned long)lba, sectors, rc);
         return false;
      }
      return true;
   }
   else if (disk->type == DISK_TYPE_ATA)
   {
      /* Hard disk (ATA): use the kernel ATA driver with primary master channel/drive.
       */
      printf("DISK: ATA read from LBA %lu, sectors %u\n", (unsigned long)lba, sectors);
      int rc = ata_read(ATA_CHANNEL_PRIMARY, ATA_DRIVE_MASTER, lba, (uint8_t *)dataOut, sectors);
      if (rc != 0)
      {
         printf("DISK: ATA read failed (lba=%lu, sectors=%u, error=%d)\n",
                (unsigned long)lba, sectors, rc);
         return false;
      }
      return true;
   }

   printf("DISK: Unknown disk type: %u\n", disk->type);
   return false;
}
