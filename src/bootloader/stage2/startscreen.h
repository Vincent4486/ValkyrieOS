// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

void draw_start_screen(bool showBoot);
void draw_outline(void);
void draw_text(void);
void gotoxy(int x, int y);
void printChar(char character, uint8_t color);
void delay_ms(unsigned int ms);
