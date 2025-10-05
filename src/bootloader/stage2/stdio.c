#include "stdio.h"
#include "x86.h"

// Set video mode to 80x25 color text mode
void set_video_mode(void) {
    _x86_VideoSetMode(0x0003);  // 80x25 16-color text mode
}

// Clear screen with specified background color using BIOS interrupt
void clear_screen(uint8_t color) {
    __asm {
        ; Clear screen using BIOS scroll function
        mov ah, 0x06    ; Scroll up function
        mov al, 0       ; Clear entire screen (0 = clear all)
        mov bh, color   ; Background/foreground attribute
        mov ch, 0       ; Top row (0)
        mov cl, 0       ; Left column (0)
        mov dh, 24      ; Bottom row (24, 0-based so this is row 25)
        mov dl, 79      ; Right column (79, 0-based so this is column 80)
        int 0x10        ; BIOS video interrupt
        
        ; Set cursor to top-left (0,0)
        mov ah, 0x02    ; Set cursor position
        mov bh, 0       ; Page number
        mov dh, 0       ; Row 0
        mov dl, 0       ; Column 0
        int 0x10        ; BIOS video interrupt
    }
}

// Clear screen with black background
void clear_screen_black(void) {
    clear_screen(MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
}

// Move cursor to specific position (x=column, y=row)
void gotoxy(uint8_t x, uint8_t y) {
    __asm {
        mov ah, 0x02    ; Set cursor position
        mov bh, 0       ; Page number
        mov dh, y       ; Row (0-24 for 25 rows)
        mov dl, x       ; Column (0-79 for 80 columns)
        int 0x10        ; BIOS video interrupt
    }
}

// Simple BIOS teletype output - most reliable method
void print_char(char c) {
    _x86_VideoWriteCharTeletype(c, 0);
}

// BIOS video interrupt function with color support using write character and attribute
void print_char_color(char c, uint8_t color) {
    __asm {
        mov ah, 0x09    ; Write character and attribute at cursor position
        mov al, c       ; Character
        mov bh, 0       ; Page number
        mov bl, color   ; Attribute
        mov cx, 1       ; Number of times to write character
        int 0x10
        
        ; Move cursor forward
        mov ah, 0x03    ; Get cursor position
        mov bh, 0       ; Page number
        int 0x10        ; Returns position in DX
        
        mov ah, 0x02    ; Set cursor position
        mov bh, 0       ; Page number
        inc dl          ; Move cursor right
        int 0x10
    }
}

// Print string using print_char function
void print_string(const char* str) {
    while(*str) {
        print_char(*str);
        str++;
    }
}

// Print colored string using print_char_color function
void print_string_color(const char* str, uint8_t color) {
    while(*str) {
        print_char_color(*str, color);
        str++;
    }
}
