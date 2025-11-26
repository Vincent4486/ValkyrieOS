// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

   extern void clrscr();
   extern void putc(char c);
   extern void puts(const char *str);
   extern void printf(const char *fmt, ...);
   extern void print_buffer(const char *msg, const void *buffer, uint32_t count);
   extern void setcursor(int x, int y);
   extern int snprintf(char *buffer, size_t buf_size, const char *format, ...);

#ifdef __cplusplus
}
#endif
