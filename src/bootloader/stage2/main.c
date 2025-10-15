#include "stdint.h"
#include "stdio.h"
#include "disk.h"
#include "x86.h"
#include "fat.h"

void* g_data = (void*)0x00500200;

void cstart_(uint32_t bootDrive)
{
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    
    // Clear screen
    for (int i = 0; i < 80 * 25; i++) {
        vga[i] = 0x0720;
    }
    
    // Show cursor position as numbers on screen
    vga[0] = 0x0730 + (cursor_x & 0xF);  // cursor_x as hex digit
    vga[1] = 0x0730 + (cursor_y & 0xF);  // cursor_y as hex digit
    
    printf("Test");
    
    // Show cursor position after printf
    vga[10] = 0x0730 + (cursor_x & 0xF);  // cursor_x after printf
    vga[11] = 0x0730 + (cursor_y & 0xF);  // cursor_y after printf
    
    for(;;);
}
