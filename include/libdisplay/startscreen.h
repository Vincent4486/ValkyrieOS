// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern void draw_start_screen(bool showBoot);
extern void draw_outline(void);
extern void draw_text(void);
extern void gotoxy(int x, int y);
extern void printChar(char character, uint8_t color);
extern void delay_ms(unsigned int ms);

#ifdef __cplusplus
}
#endif