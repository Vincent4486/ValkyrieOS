#include <stdint.h>
#include <std/stdio.h>
#include <memory/memory.h>
#include <hal/hal.h>
#include <arch/i686/irq.h>
#include <drivers/fat12/fat12.h>

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

    i686_IRQ_RegisterHandler(0, timer);

    // FAT12 test: read /test.txt
    DISK disk;
    if (DISK_Initialize(&disk, 0)) {
        if (FAT_Initialize(&disk)) {
            FAT_File *file = FAT_Open(&disk, "/test.txt");
            if (file) {
                char buf[256] = {0};
                uint32_t bytes = FAT_Read(&disk, file, sizeof(buf)-1, buf);
                buf[bytes] = '\0';
                printf("Contents of /test.txt:\n%s\n", buf);
                FAT_Close(file);
            } else {
                printf("FAT: Could not open /test.txt\n");
            }
        } else {
            printf("FAT: Filesystem init failed\n");
        }
    } else {
        printf("DISK: Init failed\n");
    }

    //crash_me();

end:
    for (;;);
}