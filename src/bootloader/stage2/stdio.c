#include "stdio.h"

#include "x86.h"

void putc(char c) { x86_Video_WriteCharTeletype(c, 0); }

void puts(const char* str) {
    while (*str) {
        putc(*str);
        str++;
    }
}

#define PRINTF_NORMAL_STATE 0

void _cdecl printf(const char* fmt, ...) {
    int state = PRINTF_NORMAL_STATE;
    while (*fmt) {
        fmt++;
    }
}
