#include <arch/i686/irq.h>
#include <fs/fat12/fat12.h>
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
end:
   for (;;);
}
