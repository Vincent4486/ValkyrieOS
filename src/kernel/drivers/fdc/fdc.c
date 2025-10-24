#include "fdc.h"
#include <arch/i686/io.h>
#include <std/stdio.h>
#include <stddef.h>
#include <stdint.h>

#define FDC_BASE 0x3F0
#define FDC_DOR (FDC_BASE + 2)
#define FDC_MSR (FDC_BASE + 4)
#define FDC_FIFO (FDC_BASE + 5)
#define FDC_CCR (FDC_BASE + 7)

#define FDC_CMD_READ_DATA 0xE6
#define FDC_CMD_RECALIBRATE 0x07
#define FDC_CMD_SENSE_INT 0x08
#define FDC_CMD_SPECIFY 0x03
#define FDC_CMD_SEEK 0x0F

#define FDC_MOTOR_ON 0x1C
#define FDC_MOTOR_OFF 0x0C

#define FDC_IRQ 6
#define FLOPPY_SECTORS_PER_TRACK 18
#define FLOPPY_HEADS 2
#define FLOPPY_TRACKS 80
#define FLOPPY_SECTOR_SIZE 512

static void fdc_motor_on(void) { i686_outb(FDC_DOR, FDC_MOTOR_ON); }

static void fdc_motor_off(void) { i686_outb(FDC_DOR, FDC_MOTOR_OFF); }

static void fdc_send_byte(uint8_t byte)
{
   while (!(i686_inb(FDC_MSR) & 0x80));
   i686_outb(FDC_FIFO, byte);
}

static uint8_t fdc_read_byte(void)
{
   while (!(i686_inb(FDC_MSR) & 0x80));
   return i686_inb(FDC_FIFO);
}

void fdc_reset(void)
{
   printf("[FDC DEBUG] fdc_reset\n");
   i686_outb(FDC_DOR, 0x00);
   i686_iowait();
   i686_outb(FDC_DOR, FDC_MOTOR_ON);
   i686_iowait();
   fdc_send_byte(FDC_CMD_SPECIFY);
   fdc_send_byte(0xDF); // SRT=13ms, HUT=240ms
   fdc_send_byte(0x02); // HLT=16ms, ND=0
}

static void fdc_recalibrate(void)
{
   fdc_send_byte(FDC_CMD_RECALIBRATE);
   fdc_send_byte(0); // drive 0
   fdc_send_byte(FDC_CMD_SENSE_INT);
   fdc_read_byte(); // st0
   fdc_read_byte(); // cyl
}

static void lba_to_chs(uint32_t lba, uint8_t *head, uint8_t *track,
                       uint8_t *sector)
{
   *track = lba / (FLOPPY_SECTORS_PER_TRACK * FLOPPY_HEADS);
   *head = (lba / FLOPPY_SECTORS_PER_TRACK) % FLOPPY_HEADS;
   *sector = (lba % FLOPPY_SECTORS_PER_TRACK) + 1;
}

int fdc_read_lba(uint32_t lba, uint8_t *buffer, size_t count)
{
   printf("[FDC DEBUG] fdc_read_lba: lba=%lu buffer=%p count=%lu\n",
          (unsigned long)lba, buffer, (unsigned long)count);
   for (size_t i = 0; i < count; i++)
   {
      uint8_t head, track, sector;
      lba_to_chs(lba + i, &head, &track, &sector);
      fdc_motor_on();
      fdc_recalibrate();
      fdc_send_byte(FDC_CMD_SEEK);
      fdc_send_byte(head << 2); // drive 0, head
      fdc_send_byte(track);
      fdc_send_byte(FDC_CMD_SENSE_INT);
      fdc_read_byte(); // st0
      fdc_read_byte(); // cyl
      fdc_send_byte(FDC_CMD_READ_DATA);
      fdc_send_byte(head << 2); // drive 0, head
      fdc_send_byte(track);
      fdc_send_byte(head);
      fdc_send_byte(sector);
      fdc_send_byte(2); // 512 bytes/sector
      fdc_send_byte(FLOPPY_SECTORS_PER_TRACK);
      fdc_send_byte(FLOPPY_HEADS);
      fdc_send_byte(0x1B); // gap3
      fdc_send_byte(0xFF); // DTL
      for (size_t b = 0; b < FLOPPY_SECTOR_SIZE; b++)
      {
         buffer[i * FLOPPY_SECTOR_SIZE + b] = fdc_read_byte();
      }
      fdc_motor_off();
   }
   return 0;
}

int fdc_write_lba(uint32_t lba, const uint8_t *buffer, size_t count)
{
   printf("[FDC DEBUG] fdc_write_lba: lba=%lu buffer=%p count=%lu\n",
          (unsigned long)lba, buffer, (unsigned long)count);
   // Not implemented: writing to floppy is more complex
   return 1;
}
