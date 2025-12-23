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
#include <cpu/cpu.h>
#include <hal/hal.h>
#include <fs/fs.h>

#include <display/startscreen.h>
#include <libmath/math.h>

extern uint8_t __bss_start;
extern uint8_t __end;
extern void _init();

void timer(Registers *regs)
{
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
