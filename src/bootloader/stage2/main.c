#include "stdint.h"
#include "stdio.h"

void _cdecl cstart_(uint16_t bootDrive) {
    if(bootDrive) {}
    puts("Hello world from C!\r\n");
    for (;;);
}
