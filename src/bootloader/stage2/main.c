#include "disk.h"
#include "fat.h"
#include "memdefs.h"
#include "memory.h"
#include "startscreen.h"
#include "stdio.h"
#include "x86.h"
#include "elf.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

uint8_t *KernelLoadBuffer = (uint8_t *)MEMORY_LOAD_KERNEL;
uint8_t *Kernel = (uint8_t *)MEMORY_KERNEL_ADDR;

typedef void (*KernelStart)();

void __attribute__((cdecl)) start(uint16_t bootDrive)
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

   if (!FAT_Initialize(&disk))
   {
      printf("FAT init error\r\n");
      goto end;
   }

   // load ELF kernel
   FAT_File *fd = FAT_Open(&disk, "/sys/kernel.elf");
   if (!fd)
   {
      printf("FAT: failed to open /sys/kernel.elf\r\n");
      goto end;
   }

   void *entry = NULL;
   if (!ELF_Load(&disk, fd, &entry))
   {
      printf("ELF: load failed\r\n");
      FAT_Close(fd);
      goto end;
   }

   FAT_Close(fd);

   // jump to kernel entry
   KernelStart kernelStart = (KernelStart)entry;
   kernelStart();

end:
   for (;;);
}
