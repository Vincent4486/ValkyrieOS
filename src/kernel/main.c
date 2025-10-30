#include <arch/i686/irq.h>
#include <fs/fat12.h>
#include <hal/hal.h>
#include <jvm/jvm.h>
#include <memory/memory.h>
#include <memory/memdefs.h>
#include <std/stdio.h>
#include "memory/dylink.h"
#include <stdint.h>
#include <display/buffer.h>

extern uint8_t __bss_start;
extern uint8_t __end;

void crash_me();

void timer(Registers *regs)
{
   //printf(".");
}

void __attribute__((section(".entry"))) start(uint16_t bootDrive)
{
   memset(&__bss_start, 0, (&__end) - (&__bss_start));

   /* initialize basic memory allocator before other subsystems */
   mem_init();

   HAL_Initialize();

   i686_IRQ_RegisterHandler(0, timer);

   printf("Kernel running...\n");
      
   /* Print loaded modules registered by stage2 so we can see what's available.
    * Use the dylink helper which reads the shared registry populated by stage2.
    */
   dylib_list();
   
   DISK disk;
   if (!DISK_Initialize(&disk, bootDrive))
   {
      printf("Disk init error\r\n");
      goto end;
   }

   /* Quick FAT test: initialize FAT on the detected disk and list the root
    * directory entries. This helps verify the FDC driver + FAT code are
    * working together.
    */
   if (FAT_Initialize(&disk))
   {
      printf("FAT initialized\n");
     
      FAT_File *tf = FAT_Open(&disk, "/test.txt");
      if (tf)
      {
         printf("/test.txt: \n");
         char buf[256];
         uint32_t read;
         while ((read = FAT_Read(&disk, tf, sizeof(buf), buf)) > 0)
         {
            // print chunks (not NUL terminated)
            printf("%.*s", (int)read, buf);
         }
         printf("\n");
         FAT_Close(tf);
      }
      else
      {
         printf("FAT: /test.txt not found\n");
      }
   }
   else
   {
      printf("FAT initialize failed\n");
   }
end:
   for (;;);
}
