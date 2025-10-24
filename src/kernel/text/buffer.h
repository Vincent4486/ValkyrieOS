#pragma once
#include <stdint.h>

/* Export screen dimensions so other modules (keyboard, etc.) can reference
   the VGA text-mode dimensions. Keep these in the header to avoid duplicate
   magic numbers across files. */
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

void buffer_init(void);
void buffer_clear(void);
void buffer_putc(char c);
void buffer_puts(const char *s);
void buffer_repaint(void);
void buffer_scroll(int lines);
void buffer_set_color(uint8_t color);
void buffer_set_cursor(int x, int y);
void buffer_get_cursor(int *x, int *y);
