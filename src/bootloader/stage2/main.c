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

typedef void (*KernelStart)(uint16_t bootDrive);

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

   // Try to load a test library (search common paths). If found, register it
   // so the kernel can call it by looking up the registry.
   {
      const char *paths[] = { "/sys/hello.elf", "/test/hello.elf" };
      for (int p = 0; p < (int)(sizeof(paths) / sizeof(paths[0])); p++)
      {
         FAT_File *libfd = FAT_Open(&disk, paths[p]);
         if (!libfd) continue;

         void *libEntry = NULL;
         if (ELF_Load(&disk, libfd, &libEntry))
         {
            printf("Loaded %s -> entry=%p\r\n", paths[p], libEntry);

            // populate first free slot in registry
            LibRecord *reg = LIB_REGISTRY_ADDR;
            for (int i = 0; i < LIB_REGISTRY_MAX; i++)
            {
               if (reg[i].name[0] == '\0')
               {
                  // store simple name (basename without extension)
                  const char *src = paths[p];
                  const char *b = src;
                  // find last '/'
                  for (const char *q = src; *q; q++) if (*q == '/') b = q + 1;
                  // copy up to '.' or end
                  int j = 0;
                  while (b[j] && b[j] != '.' && j < LIB_NAME_MAX - 1)
                  {
                     reg[i].name[j] = b[j];
                     j++;
                  }
                  reg[i].name[j] = '\0';
                  reg[i].base = libEntry; // best-effort; precise base may be equal to entry
                  reg[i].entry = libEntry;
                  reg[i].size = 0;
                  break;
               }
            }
         }

         FAT_Close(libfd);
      }
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

   // jump to kernel entry (pass the boot drive number so kernel can access disk)
   KernelStart kernelStart = (KernelStart)entry;
   kernelStart(bootDrive);

end:
   for (;;);
}
