// SPDX-License-Identifier: AGPL-3.0-or-later

#include "disk.h"
#include "elf.h"
#include "fat.h"
#include "mbr.h"
#include "memdefs.h"
#include "memory.h"
#include "stdio.h"
#include "x86.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint8_t *KernelLoadBuffer = (uint8_t *)MEMORY_LOAD_KERNEL;
uint8_t *Kernel = (uint8_t *)MEMORY_KERNEL_ADDR;

typedef void (*KernelStart)(uint16_t bootDrive, void *partitionPtr);

void __attribute__((cdecl)) start(uint16_t bootDrive, void *partition)
{
   clrscr();

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
   if (!ELF_Load(&part, "/sys/core.elf", &entry))
   {
      printf("ELF: load failed\r\n");
      goto end;
   }

   // Register libraries in the library registry
   LibRecord *lib_registry = LIB_REGISTRY_ADDR;

   // Load and register libmath library
   printf("Loading libmath.so...\r\n");
   FAT_File *libmath_file = FAT_Open(&part, "/sys/libmath.so");
   if (libmath_file)
   {
      // Allocate memory for libmath (use a fixed address for simplicity)
      void *libmath_base = (void *)0x500000; // Load at 5 MiB
      uint32_t bytes_read =
          FAT_Read(&part, libmath_file, libmath_file->Size, libmath_base);

      if (bytes_read == libmath_file->Size)
      {
         // Register in library registry
         lib_registry[0].name[0] = 'l';
         lib_registry[0].name[1] = 'i';
         lib_registry[0].name[2] = 'b';
         lib_registry[0].name[3] = 'm';
         lib_registry[0].name[4] = 'a';
         lib_registry[0].name[5] = 't';
         lib_registry[0].name[6] = 'h';
         lib_registry[0].name[7] = '\0';
         lib_registry[0].base = libmath_base;
         lib_registry[0].entry =
             (void *)((uint32_t)libmath_base +
                      0x100); // Entry point offset (will be fixed by kernel)
         lib_registry[0].size = libmath_file->Size;
         printf("libmath.so registered at 0x%x\r\n",
                (unsigned int)libmath_base);
      }
      else
      {
         printf("Failed to read libmath.so\r\n");
      }
      FAT_Close(libmath_file);
   }
   else
   {
      printf("libmath.so not found\r\n");
   }

   // jump to kernel entry (pass the boot drive number and partition pointer)
   printf("Jumping to kernel...\n");
   KernelStart kernelStart = (KernelStart)entry;
   kernelStart(bootDrive, partition);

end:
   for (;;);
}
