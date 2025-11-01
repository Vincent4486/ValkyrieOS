#include "ata.h"
#include "arch/i686/io.h"

#include <std/stdio.h>

#define ATA_WAIT_TIMEOUT 1000000

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCOUNT0 0x02
#define ATA_REG_LBA0 0x03
#define ATA_REG_LBA1 0x04
#define ATA_REG_LBA2 0x05
#define ATA_REG_HDDEVSEL 0x06
#define ATA_REG_COMMAND 0x07
#define ATA_REG_STATUS 0x07

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF 0x20
#define ATA_SR_DSC 0x10
#define ATA_SR_DRQ 0x08
#define ATA_SR_CORR 0x04
#define ATA_SR_IDX 0x02
#define ATA_SR_ERR 0x01

static int ata_wait_bsy(void)
{
   int timeout = ATA_WAIT_TIMEOUT;
   uint8_t status;
   while (timeout-- > 0)
   {
      status = i686_inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
      if (!(status & ATA_SR_BSY))
      {
         return 0;
      }
   }
   printf("[ATA ERROR] ata_wait_bsy: TIMEOUT, status=0x%02X\n", status);
   return 1;
}

static int ata_wait_drq(void)
{
   int timeout = ATA_WAIT_TIMEOUT;
   uint8_t status;
   while (timeout-- > 0)
   {
      status = i686_inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
      if (status & ATA_SR_DRQ)
      {
         return 0;
      }
   }
   printf("[ATA ERROR] ata_wait_drq: TIMEOUT, status=0x%02X\n", status);
   return 1;
}

void ata_soft_reset(void)
{
   i686_outb(ATA_PRIMARY_CTRL, 0x04);
   i686_iowait();
   i686_outb(ATA_PRIMARY_CTRL, 0x00);
   i686_iowait();
}

int ata_read28(uint32_t lba, uint8_t *buffer, size_t count)
{
   if (lba > 0x0FFFFFFF || count == 0)
   {
      return 1;
   }
   for (size_t sector = 0; sector < count; ++sector)
   {
      if (ata_wait_bsy()) return 2;
      i686_outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
      i686_outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT0, 1);
      i686_outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
      i686_outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
      i686_outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
      i686_outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
      if (ata_wait_bsy()) return 3;
      if (ata_wait_drq()) return 4;
      for (size_t i = 0; i < ATA_SECTOR_SIZE; i += 2)
      {
         uint16_t data = i686_inb(ATA_PRIMARY_IO);
         data |= (i686_inb(ATA_PRIMARY_IO) << 8);
         buffer[sector * ATA_SECTOR_SIZE + i] = (uint8_t)(data & 0xFF);
         buffer[sector * ATA_SECTOR_SIZE + i + 1] =
             (uint8_t)((data >> 8) & 0xFF);
      }
      lba++;
   }
   return 0;
}

int ata_write28(uint32_t lba, const uint8_t *buffer, size_t count)
{
   if (lba > 0x0FFFFFFF || count == 0) return 1;
   for (size_t sector = 0; sector < count; ++sector)
   {
      ata_wait_bsy();
      i686_outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
      i686_outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT0, 1);
      i686_outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
      i686_outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
      i686_outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
      i686_outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
      ata_wait_bsy();
      ata_wait_drq();
      for (size_t i = 0; i < ATA_SECTOR_SIZE; i += 2)
      {
         uint16_t data = buffer[sector * ATA_SECTOR_SIZE + i] |
                         (buffer[sector * ATA_SECTOR_SIZE + i + 1] << 8);
         i686_outb(ATA_PRIMARY_IO, (uint8_t)(data & 0xFF));
         i686_outb(ATA_PRIMARY_IO, (uint8_t)((data >> 8) & 0xFF));
      }
      i686_outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
      ata_wait_bsy();
      lba++;
   }
   return 0;
}
