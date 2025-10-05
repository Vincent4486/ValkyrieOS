#pragma once
#include "stdint.h"
#include "color.h"

// Function declarations for standard I/O operations
void set_video_mode(void);
void clear_screen(uint8_t color);
void clear_screen_black(void);
void gotoxy(uint8_t x, uint8_t y);
void print_char_color_vga(char c, uint8_t color);
void print_char_color(char c, uint8_t color);
void print_char(char c);
void print_string(const char* str);
void print_string_color(const char* str, uint8_t color);
