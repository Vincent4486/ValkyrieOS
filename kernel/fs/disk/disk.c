// SPDX-License-Identifier: AGPL-3.0-or-later

#include "disk.h"
#include <drivers/ata.h>
#include <drivers/fdc.h>

// Disk type constants
#define DISK_TYPE_FLOPPY 0
#define DISK_TYPE_ATA 1

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
      FDC_Reset();
      cylinders = 80;
      heads = 2;
      sectors = 18;
      disk->cylinders = cylinders;
      disk->heads = heads;
      disk->sectors = sectors;
      return true;
   }
   else
   {
      // Hard disk (ATA) detected
      disk->type = DISK_TYPE_ATA;

      // Initialize ATA driver for primary master channel
      // We'll use the standard primary master (IDE0 master) for hard disk
      // access
      ATA_Init(ATA_CHANNEL_PRIMARY, ATA_DRIVE_MASTER, 0, 0x100000);

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
   if (sectors == 0) return false;

   if (disk->type == DISK_TYPE_FLOPPY)
   {
      /* Floppy drive: use the kernel FDC driver which speaks directly to the
       * floppy controller. This avoids relying on BIOS INT13 services from
       * the kernel.
       */
      int rc = FDC_ReadLba(lba, (uint8_t *)dataOut, sectors);
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
      int rc = FDC_WriteLba(lba, (const uint8_t *)dataIn, sectors);
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
