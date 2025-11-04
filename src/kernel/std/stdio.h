// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

   void clrscr();
   void putc(char c);
   void puts(const char *str);
   void printf(const char *fmt, ...);
   void print_buffer(const char *msg, const void *buffer, uint32_t count);
   void setcursor(int x, int y);
   int snprintf(char *buffer, size_t buf_size, const char *format, ...);

#ifdef __cplusplus
}
#endif
