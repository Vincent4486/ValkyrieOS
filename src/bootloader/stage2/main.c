#include "stdint.h"

// BIOS video interrupt function
void print_char(char c) {
    __asm {
        mov ah, 0x0E
        mov al, c
        mov bh, 0
        int 0x10
    }
}

void print_string(const char* str) {
    while(*str) {
        print_char(*str);
        str++;
    }
}

#pragma aux _cstart_ "*"
void _cdecl _cstart_(uint16_t bootDrive) {
    print_string("Stage2 loaded!\r\n");
    
    // Infinite loop for now
    while(1) {
        __asm {
            hlt
        }
    }
}
