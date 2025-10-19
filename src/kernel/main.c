#include <stdint.h>
#include "stdio.h"
#include "memory.h"
#include <hal/hal.h>
#include <arch/i686/irq.h>

extern uint8_t __bss_start;
extern uint8_t __end;

void crash_me();

void timer(Registers* regs)
{
    //printf(".");
}

void __attribute__((section(".entry"))) start(uint16_t bootDrive)
{
    memset(&__bss_start, 0, (&__end) - (&__bss_start));

    /* initialize basic memory allocator before other subsystems */
    mem_init();

    HAL_Initialize();

    clrscr();

    printf("Hello from kernel!\n");

    /* allocator smoke test */
    void *a = kmalloc(64);
    void *b = kzalloc(128);
    printf("kmalloc -> %p\n", a);
    printf("kzalloc -> %p\n", b);

    /* print kernel end and actual heap range */
    printf("__end = %p\n", &__end);
    printf("heap range %p - %p\n", (void*)mem_heap_start(), (void*)mem_heap_end());

    /* large allocation test (try 1 GiB) */
    size_t one_gib = (size_t)1 * 1024 * 1024 * 1024u;
    void *large = kmalloc(one_gib);
    if (large)
    {
        printf("large alloc success: %p\n", large);
        /* try writing/reading first 16 bytes and print hex using helper */
        uint8_t *p = (uint8_t *)large;
        for (int i = 0; i < 16; ++i) p[i] = (uint8_t)(i + 1);
        print_buffer("first bytes: ", p, 16);
    }
    else
    {
        printf("large alloc failed (not enough memory)\n");
    }

    i686_IRQ_RegisterHandler(0, timer);

    //crash_me();

end:
    for (;;);
}