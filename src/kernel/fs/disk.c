#include "disk.h"
#include <std/stdio.h>
#include <drivers/fdc/fdc.h>
#include <drivers/fdc/fdc.h>

bool DISK_Initialize(DISK *disk, uint8_t driveNumber)
{
   uint8_t driveType;
   uint16_t cylinders, sectors, heads;

   disk->id = driveNumber;

      /* Only support floppy drives in the kernel disk layer for now. If the
       * drive number indicates a floppy (BIOS convention: < 0x80) initialize
       * the FDC and return success. For all other drives return false so the
       * caller knows the disk subsystem is not available.
       */
      if (driveNumber < 0x80)
      {
         disk->id = driveNumber;
         fdc_reset();
         cylinders = 80;
         heads = 2;
         sectors = 18;
         disk->cylinders = cylinders;
         disk->heads = heads;
         disk->sectors = sectors;
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
      printf("DISK: ReadSectors called with sectors=0 (lba=%lu)\n", (unsigned long)lba);
      return false;
   }
   /* If this is a floppy drive (BIOS IDs < 0x80), use the kernel FDC driver
    * which speaks directly to the floppy controller. This avoids relying on
    * BIOS INT13 services from the kernel.
    */
   if (disk->id < 0x80)
   {
      int rc = fdc_read_lba(lba, (uint8_t *)dataOut, sectors);
      return rc == 0;
   }

   uint16_t cylinder, sector, head;

   /* For hard disks use ATA LBA API. The kernel ATA driver expects LBA and
    * will handle retries internally. For floppy devices the FDC path is
    * taken above.
    */
   return false;
}
