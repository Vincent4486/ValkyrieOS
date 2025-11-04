// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stdint.h>

void clrscr();
void putc(char c);
void puts(const char *str);
void printf(const char *fmt, ...);
void print_buffer(const char *msg, const void *buffer, uint32_t count);
