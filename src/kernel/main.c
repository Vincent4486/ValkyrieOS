#include <arch/i686/irq.h>
#include <fs/fat12/fat12.h>
#include <hal/hal.h>
#include <jvm/jvm.h>
#include <memory/memory.h>
#include <memory/memdefs.h>
#include <std/stdio.h>
#include <stdint.h>
#include <text/buffer.h>

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

end:
   for (;;);
}
