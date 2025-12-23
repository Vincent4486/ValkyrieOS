// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef START_SCREEN_H
#define START_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

   void draw_start_screen(bool showBoot);
   void draw_outline(void);
   void draw_text(void);
   void gotoxy(int x, int y);
   void printChar(char character, uint8_t color);
   void delay_ms(unsigned int ms);

#ifdef __cplusplus
}
#endif

#endif