// SPDX-License-Identifier: AGPL-3.0-or-later

#include "fdc.h"
#include <arch/i686/io/io.h>
#include <arch/i686/cpu/irq.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FDC_BASE 0x3F0
#define FDC_DOR (FDC_BASE + 2)
#define FDC_MSR (FDC_BASE + 4)
#define FDC_FIFO (FDC_BASE + 5)
#define FDC_CCR (FDC_BASE + 7)

#define FDC_CMD_READ_DATA 0x46  // Read with MFM encoding (not 0xE6)
#define FDC_CMD_WRITE_DATA 0x45 // Write with MFM encoding
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

// DMA controller ports for channel 2 (FDC)
#define DMA_CHANNEL_2_ADDR 0x04
#define DMA_CHANNEL_2_COUNT 0x05
#define DMA_CHANNEL_2_PAGE 0x81
#define DMA_SINGLE_MASK 0x0A
#define DMA_MODE 0x0B
#define DMA_FLIP_FLOP_RESET 0x0C

// DMA buffer at standard location (must be below 16MB and not cross 64K
// boundary)
#define FDC_DMA_BUFFER 0x1000

// Global IRQ synchronization flag
static volatile bool g_fdc_irq_received = false;

static void fdc_dma_init(bool is_read)
{
   /* Initialize DMA channel 2 for floppy disk controller
    * Mode for channel 2:
    *   - Bits 0-1: Channel select (10 = channel 2)
    *   - Bits 2-3: Transfer type (01 = read from memory, 10 = write to memory)
    *   - Bit 4: Auto-initialize (0 = disabled)
    *   - Bit 5: Address increment (0 = increment, 1 = decrement)
    *   - Bits 6-7: Mode (01 = single mode, 10 = block mode)
    */

   // Clear any data from previous DMA buffer (for testing)
   uint8_t *dma_buffer = (uint8_t *)FDC_DMA_BUFFER;
   for (int i = 0; i < FLOPPY_SECTOR_SIZE; i++)
   {
      dma_buffer[i] = 0xAA; // Fill with test pattern
   }

   // Mask DMA channel 2
   i686_outb(DMA_SINGLE_MASK,
             0x06); // 0x06 = 0b0110 = mask set (bit 2) | channel 2

   // Reset flip-flop
   i686_outb(DMA_FLIP_FLOP_RESET, 0x0C);

   // Set DMA mode for channel 2
   // For FDC read (disk->memory): mode = 0x46
   //   0100 0110 = single transfer, address increment, autoinit disabled, write
   //   mode, channel 2
   // For FDC write (memory->disk): mode = 0x4A
   //   0100 1010 = single transfer, address increment, autoinit disabled, read
   //   mode, channel 2
   uint8_t mode = is_read ? 0x46 : 0x4A;
   i686_outb(DMA_MODE, mode);

   // Set address (must be physical address, low 16 bits)
   uint32_t addr = FDC_DMA_BUFFER;
   i686_outb(DMA_FLIP_FLOP_RESET, 0x0C);
   i686_outb(DMA_CHANNEL_2_ADDR, addr & 0xFF);
   i686_outb(DMA_CHANNEL_2_ADDR, (addr >> 8) & 0xFF);

   // Set page register (bits 16-23 of address)
   i686_outb(DMA_CHANNEL_2_PAGE, (addr >> 16) & 0xFF);

   // Set count (number of bytes - 1)
   uint16_t count = FLOPPY_SECTOR_SIZE - 1;
   i686_outb(DMA_FLIP_FLOP_RESET, 0x0C);
   i686_outb(DMA_CHANNEL_2_COUNT, count & 0xFF);
   i686_outb(DMA_CHANNEL_2_COUNT, (count >> 8) & 0xFF);

   // Unmask DMA channel 2 to allow transfers
   i686_outb(DMA_SINGLE_MASK, 0x02); // 0x02 = 0b0010 = mask clear | channel 2
}

static void fdc_motor_on(void) { i686_outb(FDC_DOR, FDC_MOTOR_ON); }

static void fdc_motor_off(void) { i686_outb(FDC_DOR, FDC_MOTOR_OFF); }

// FDC IRQ handler - sets flag when interrupt is received
static void fdc_irq_handler(Registers *regs) { g_fdc_irq_received = true; }

// Wait for FDC IRQ with timeout
static bool fdc_wait_irq(void)
{
   unsigned timeout = 0x100000;
   while (!g_fdc_irq_received && timeout > 0)
   {
      timeout--;
      i686_iowait();
   }

   if (!g_fdc_irq_received)
   {
      return false;
   }

   g_fdc_irq_received = false;
   return true;
}

static void fdc_send_byte(uint8_t byte)
{
   /* Wait for controller to be ready to accept a command/data byte.
    * MSR bit 7 = RQM (Request for Master). MSR bit 6 = DIO (Data Input/Output)
    * For host->controller transfers we need RQM=1 and DIO=0.
    */
   unsigned timeout = 0x10000;
   uint8_t msr;

   while (timeout--)
   {
      msr = i686_inb(FDC_MSR);
      if ((msr & 0xC0) == 0x80) // RQM=1, DIO=0
      {
         i686_outb(FDC_FIFO, byte);
         return;
      }
      i686_iowait();
   }
}

static uint8_t fdc_read_byte(void)
{
   /* Wait for controller to have data for us. Need RQM=1 and DIO=1. */
   unsigned timeout = 0x10000;
   uint8_t msr;

   while (timeout--)
   {
      msr = i686_inb(FDC_MSR);
      if ((msr & 0xC0) == 0xC0) // RQM=1, DIO=1
      {
         return i686_inb(FDC_FIFO);
      }
      i686_iowait();
   }

   return 0;
}

void FDC_Reset(void)
{
   // Register IRQ handler for FDC
   i686_IRQ_RegisterHandler(FDC_IRQ, fdc_irq_handler);

   // Unmask IRQ 6 to allow FDC interrupts
   i686_IRQ_Unmask(FDC_IRQ);

   // Reset controller
   i686_outb(FDC_DOR, 0x00);
   i686_iowait();
   i686_outb(FDC_DOR, FDC_MOTOR_ON);

   // Wait for IRQ after reset
   if (!fdc_wait_irq())
   {
   }

   // Sense interrupt status 4 times (for 4 drives)
   for (int i = 0; i < 4; i++)
   {
      fdc_send_byte(FDC_CMD_SENSE_INT);
      fdc_read_byte(); // st0
      fdc_read_byte(); // cyl
   }

   // Set data rate (500 Kbps for 1.44MB floppy)
   i686_outb(FDC_CCR, 0x00);

   // Configure controller (SPECIFY command)
   fdc_send_byte(FDC_CMD_SPECIFY);
   fdc_send_byte(0xDF); // SRT=3ms, HUT=240ms
   fdc_send_byte(0x02); // HLT=16ms, ND=0 (use DMA)
}

static bool fdc_seek(uint8_t head, uint8_t track)
{
   g_fdc_irq_received = false;

   fdc_send_byte(FDC_CMD_SEEK);
   fdc_send_byte((head << 2) | 0); // head | drive 0
   fdc_send_byte(track);

   if (!fdc_wait_irq()) return false;

   // Sense interrupt status
   fdc_send_byte(FDC_CMD_SENSE_INT);
   uint8_t st0 = fdc_read_byte();
   uint8_t cyl = fdc_read_byte();

   if (cyl != track)
   {
      return false;
   }

   return true;
}

static void lba_to_chs(uint32_t lba, uint8_t *head, uint8_t *track,
                       uint8_t *sector)
{
   *track = lba / (FLOPPY_SECTORS_PER_TRACK * FLOPPY_HEADS);
   *head = (lba / FLOPPY_SECTORS_PER_TRACK) % FLOPPY_HEADS;
   *sector = (lba % FLOPPY_SECTORS_PER_TRACK) + 1;
}

int FDC_ReadLba(uint32_t lba, uint8_t *buffer, size_t count)
{
   if (count == 0)
   {
      return 0;
   }

   fdc_motor_on();

   // Small delay for motor spin-up
   for (volatile int i = 0; i < 100000; i++);

   for (size_t i = 0; i < count; i++)
   {
      uint8_t head, track, sector;
      lba_to_chs(lba + i, &head, &track, &sector);

      // Seek to track
      if (!fdc_seek(head, track))
      {
         fdc_motor_off();
         return 1;
      }

      // Initialize DMA for read operation (transfer from floppy to memory)
      fdc_dma_init(true);

      g_fdc_irq_received = false;

      // Issue READ DATA command
      fdc_send_byte(FDC_CMD_READ_DATA);
      fdc_send_byte((head << 2) | 0); // head | drive 0
      fdc_send_byte(track);
      fdc_send_byte(head);
      fdc_send_byte(sector);
      fdc_send_byte(2);      // 512 bytes per sector
      fdc_send_byte(sector); // Read only this sector
      fdc_send_byte(0x1B);   // GPL (gap3 length)
      fdc_send_byte(0xFF); // DTL (data length, 0xFF for specified sector size)

      // Wait for IRQ indicating data transfer complete
      if (!fdc_wait_irq())
      {
         fdc_motor_off();
         return 1;
      }

      // Read result bytes (7 bytes for READ DATA)
      uint8_t st0 = fdc_read_byte();
      uint8_t st1 = fdc_read_byte();
      uint8_t st2 = fdc_read_byte();
      uint8_t rtrack = fdc_read_byte();
      uint8_t rhead = fdc_read_byte();
      uint8_t rsector = fdc_read_byte();
      uint8_t bps = fdc_read_byte();

      // Check for errors in status
      if ((st0 & 0xC0) != 0)
      {
         fdc_motor_off();
         return 1;
      }

      // Get DMA buffer pointer
      uint8_t *dma_buffer = (uint8_t *)FDC_DMA_BUFFER;

      // Copy data from DMA buffer to destination
      for (int j = 0; j < FLOPPY_SECTOR_SIZE; j++)
      {
         buffer[i * FLOPPY_SECTOR_SIZE + j] = dma_buffer[j];
      }
   }

   fdc_motor_off();
   return 0;
}

int FDC_WriteLba(uint32_t lba, const uint8_t *buffer, size_t count)
{
   if (count == 0)
   {
      return 0;
   }

   fdc_motor_on();

   // Small delay for motor spin-up
   for (volatile int i = 0; i < 100000; i++);

   for (size_t i = 0; i < count; i++)
   {
      uint8_t head, track, sector;
      lba_to_chs(lba + i, &head, &track, &sector);

      // Seek to track
      if (!fdc_seek(head, track))
      {
         fdc_motor_off();
         return 1;
      }

      // Copy data to DMA buffer
      uint8_t *dma_buffer = (uint8_t *)FDC_DMA_BUFFER;
      for (int j = 0; j < FLOPPY_SECTOR_SIZE; j++)
      {
         dma_buffer[j] = buffer[i * FLOPPY_SECTOR_SIZE + j];
      }

      // Initialize DMA for write operation (transfer from memory to floppy)
      fdc_dma_init(false); // false = write mode

      g_fdc_irq_received = false;

      // Issue WRITE DATA command
      fdc_send_byte(FDC_CMD_WRITE_DATA);
      fdc_send_byte((head << 2) | 0); // head | drive 0
      fdc_send_byte(track);
      fdc_send_byte(head);
      fdc_send_byte(sector);
      fdc_send_byte(2);      // 512 bytes per sector
      fdc_send_byte(sector); // Write only this sector
      fdc_send_byte(0x1B);   // GPL (gap3 length)
      fdc_send_byte(0xFF); // DTL (data length, 0xFF for specified sector size)

      // Wait for IRQ indicating data transfer complete
      if (!fdc_wait_irq())
      {
         fdc_motor_off();
         return 1;
      }

      // Read result bytes (7 bytes for WRITE DATA)
      uint8_t st0 = fdc_read_byte();
      uint8_t st1 = fdc_read_byte();
      uint8_t st2 = fdc_read_byte();
      uint8_t rtrack = fdc_read_byte();
      uint8_t rhead = fdc_read_byte();
      uint8_t rsector = fdc_read_byte();
      uint8_t bps = fdc_read_byte();

      // Check for errors in status
      if ((st0 & 0xC0) != 0)
      {
         fdc_motor_off();
         return 1;
      }
   }

   fdc_motor_off();
   return 0;
}
