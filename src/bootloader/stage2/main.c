// SPDX-License-Identifier: AGPL-3.0-or-later

#include "disk.h"
#include "elf.h"
#include "fat.h"
#include "mbr.h"
#include "memdefs.h"
#include "memory.h"
#include "startscreen.h"
#include "stdio.h"
#include "x86.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint8_t *KernelLoadBuffer = (uint8_t *)MEMORY_LOAD_KERNEL;
uint8_t *Kernel = (uint8_t *)MEMORY_KERNEL_ADDR;

typedef void (*KernelStart)(uint16_t bootDrive);

void __attribute__((cdecl)) start(uint16_t bootDrive, void *partition)
{
   clrscr();

   bool drawScreen = false;
   draw_start_screen(drawScreen);
   delay_ms(1000);

   DISK disk;
   if (!DISK_Initialize(&disk, bootDrive))
   {
      printf("Disk init error\r\n");
      goto end;
   }

   Partition part;
   MBR_DetectPartition(&part, &disk, partition);

   if (!FAT_Initialize(&part))
   {
      printf("FAT init error\r\n");
      goto end;
   }

   // load ELF kernel
   void *entry = NULL;
   if (!ELF_Load(&part, "/sys/kernel.elf", &entry))
   {
      printf("ELF: load failed\r\n");
      goto end;
   }

   // jump to kernel entry (pass the boot drive number so kernel can access
   // disk)
   printf("Jumping to kernel...\n");
   KernelStart kernelStart = (KernelStart)entry;
   kernelStart(bootDrive);

end:
   for (;;);
}
