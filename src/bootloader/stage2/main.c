#include "stdint.h"
#include "stdio.h"

void _cdecl cstart_(uint16_t bootDrive) {
    if(bootDrive) {}
    puts("================================================================================");
    puts("Stage 2 bootloader loaded successfully!\r\n");
    for (;;);
}
