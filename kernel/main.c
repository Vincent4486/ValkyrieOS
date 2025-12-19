// SPDX-License-Identifier: AGPL-3.0-or-later

#include <hal/irq.h>
#include <cpu/process.h>
#include <drivers/ata/ata.h>
#include <fs/disk/disk.h>
#include <fs/disk/partition.h>
#include <fs/fat/fat.h>
#include <std/stdio.h>
#include <std/string.h>
#include <stdint.h>
#include <sys/dylib.h>
#include <sys/elf.h>
#include <sys/sys.h>
#include <mem/memory.h>
#include <cpu.h>
#include <hal/hal.h>
#include <fs/fs.h>

#include <display/startscreen.h>
#include <libmath/math.h>

extern uint8_t __bss_start;
extern uint8_t __end;
extern void _init();

void crash_me();

/**
 * Set IOPL (I/O Privilege Level) to allow kernel I/O port access
 *
 * Sets bits 12-13 of EFLAGS to level 3, enabling IN/OUT instructions
 * in kernel mode. Required for ATA driver and other I/O operations.
 */
static inline void set_iopl_level_3(void)
{
   __asm__ __volatile__("pushfl\n\t"
                        "orl $0x3000, (%%esp)\n\t" // Set IOPL to 11b (level 3)
                        "popfl\n\t"
                        :
                        :
                        : "memory");
}

// Global timer tick counter
static volatile uint32_t timer_ticks = 0;

void timer(Registers *regs)
{
   timer_ticks++;
   // Uncomment to show timer running:
   // if (timer_ticks % 18 == 0)  // Print every ~1 second
   //    printf(".");
}

void __attribute__((section(".entry"))) start(uint16_t bootDrive,
                                              void *partitionPtr)
{
   // Init system
   memset(&__bss_start, 0, (&__end) - (&__bss_start));
   _init();

   SYS_Initialize();
   MEM_Initialize();
   CPU_Initialize();
   HAL_Initialize();
   set_iopl_level_3();

   HAL_IRQ_RegisterHandler(0, timer);

   DISK disk;
   Partition partition;

   if (!FS_Initialize(&disk, &partition, bootDrive))
   {
      printf("FS initialization failed\n");
      goto end;
   }

   if (!Dylib_Initialize(&partition))
   {
      printf("Failed to load dynamic libraries...");
      goto end;
   }

   /* Mark system as fully initialized */
   SYS_Finalize();

   ELF_LoadProcess(&partition, "/usr/bin/sh", false);

end:
   for (;;);
}
