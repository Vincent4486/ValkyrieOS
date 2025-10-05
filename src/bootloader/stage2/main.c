#include "stdint.h"
#include "stdio.h"

// Simple delay function using loop
void delay_loop(uint32_t count) {
    volatile uint32_t i;
    for (i = 0; i < count; i++) {
        // Just loop to waste time
    }
}

// BIOS timer-based delay (more accurate)
void delay_seconds(uint8_t seconds) {
    uint8_t target_seconds;
    uint8_t current_seconds;
    
    // Get current seconds from BIOS
    __asm {
        mov ah, 0x02    ; Get system time
        int 0x1A        ; BIOS time services
        mov current_seconds, dh  ; DH contains seconds
    }
    
    target_seconds = current_seconds + seconds;
    if (target_seconds >= 60) {
        target_seconds -= 60;  // Handle minute rollover
    }
    
    // Wait until target time
    do {
        __asm {
            mov ah, 0x02    ; Get system time
            int 0x1A        ; BIOS time services
            mov current_seconds, dh
        }
    } while (current_seconds != target_seconds);
}

// Simple millisecond-like delay
void delay_ms(uint16_t ms) {
    uint16_t i;
    for (i = 0; i < ms; i++) {
        delay_loop(1000);  // Approximate 1ms delay
    }
}

// Animated rainbow border drawing function
void draw_animated_border(void) {
    int i;
    uint8_t color_index = 0;
    uint8_t rainbow_colors[16];
    
    // Initialize rainbow colors array with all 16 colors
    rainbow_colors[0] = COLOR_BLACK;
    rainbow_colors[1] = COLOR_BLUE;
    rainbow_colors[2] = COLOR_GREEN;
    rainbow_colors[3] = COLOR_CYAN;
    rainbow_colors[4] = COLOR_RED;
    rainbow_colors[5] = COLOR_MAGENTA;
    rainbow_colors[6] = COLOR_BROWN;
    rainbow_colors[7] = COLOR_LIGHT_GRAY;
    rainbow_colors[8] = COLOR_DARK_GRAY;
    rainbow_colors[9] = COLOR_LIGHT_BLUE;
    rainbow_colors[10] = COLOR_LIGHT_GREEN;
    rainbow_colors[11] = COLOR_LIGHT_CYAN;
    rainbow_colors[12] = COLOR_LIGHT_RED;
    rainbow_colors[13] = COLOR_LIGHT_MAGENTA;
    rainbow_colors[14] = COLOR_YELLOW;
    rainbow_colors[15] = COLOR_WHITE;
    
    // Draw top border (left to right)
    for (i = 0; i < 80; i++) {
        gotoxy(i, 0);
        print_char_color(' ', MAKE_COLOR(rainbow_colors[color_index % 16], COLOR_WHITE));
        color_index++;
        delay_ms(750);
    }
    
    // Draw right border (top to bottom)
    for (i = 1; i < 25; i++) {
        gotoxy(79, i);
        print_char_color(' ', MAKE_COLOR(rainbow_colors[color_index % 16], COLOR_WHITE));
        color_index++;
        delay_ms(1500);
    }
    
    // Draw bottom border (right to left)
    for (i = 78; i >= 0; i--) {
        gotoxy(i, 24);
        print_char_color(' ', MAKE_COLOR(rainbow_colors[color_index % 16], COLOR_WHITE));
        color_index++;
        delay_ms(750);
    }
    
    // Draw left border (bottom to top)
    for (i = 23; i > 0; i--) {
        gotoxy(0, i);
        print_char_color(' ', MAKE_COLOR(rainbow_colors[color_index % 16], COLOR_WHITE));
        color_index++;
        delay_ms(1500);
    }
}

void draw_text(void) {
    // Large ASCII art for "VALKYRIE" - each character is 3 wide x 5 tall
    // Centered on 80x25 screen: 8 chars * 4 spacing = 32 wide, so start at (80-32)/2 = 24
    uint8_t start_x = 24;  // Starting column (centered)
    uint8_t start_y = 10;  // Starting row (centered vertically)
    uint8_t char_spacing = 4; // Space between characters (3 wide + 1 space)
    uint8_t i;

    uint8_t color_index = 0;
    uint8_t rainbow_colors[16];
    
    // Initialize rainbow colors array with all 16 colors
    rainbow_colors[0] = COLOR_BLACK;
    rainbow_colors[1] = COLOR_BLUE;
    rainbow_colors[2] = COLOR_GREEN;
    rainbow_colors[3] = COLOR_CYAN;
    rainbow_colors[4] = COLOR_RED;
    rainbow_colors[5] = COLOR_MAGENTA;
    rainbow_colors[6] = COLOR_BROWN;
    rainbow_colors[7] = COLOR_LIGHT_GRAY;
    rainbow_colors[8] = COLOR_DARK_GRAY;
    rainbow_colors[9] = COLOR_LIGHT_BLUE;
    rainbow_colors[10] = COLOR_LIGHT_GREEN;
    rainbow_colors[11] = COLOR_LIGHT_CYAN;
    rainbow_colors[12] = COLOR_LIGHT_RED;
    rainbow_colors[13] = COLOR_LIGHT_MAGENTA;
    rainbow_colors[14] = COLOR_YELLOW;
    rainbow_colors[15] = COLOR_WHITE;
    
    // Animate each character appearing one by one
    
    // V (character 0)
    for (i = 0; i < 1; i++) {
        delay_ms(1000);
        gotoxy(start_x + 0 * char_spacing + 0, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 0 * char_spacing + 2, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 0 * char_spacing + 0, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 0 * char_spacing + 2, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 0 * char_spacing + 0, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 0 * char_spacing + 2, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 0 * char_spacing + 0, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 0 * char_spacing + 2, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 0 * char_spacing + 1, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
    }
    
    // A (character 1)
    for (i = 0; i < 1; i++) {
        delay_ms(1000);
        gotoxy(start_x + 1 * char_spacing + 1, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 1 * char_spacing + 0, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 1 * char_spacing + 2, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 1 * char_spacing + 0, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 1 * char_spacing + 1, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 1 * char_spacing + 2, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 1 * char_spacing + 0, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 1 * char_spacing + 2, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 1 * char_spacing + 0, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 1 * char_spacing + 2, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
    }
    
    // L (character 2)
    for (i = 0; i < 1; i++) {
        delay_ms(1000);
        gotoxy(start_x + 2 * char_spacing + 0, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 2 * char_spacing + 0, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 2 * char_spacing + 0, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 2 * char_spacing + 0, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 2 * char_spacing + 0, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 2 * char_spacing + 1, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 2 * char_spacing + 2, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
    }
    
    // K (character 3)
    for (i = 0; i < 1; i++) {
        delay_ms(1000);
        gotoxy(start_x + 3 * char_spacing + 0, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 3 * char_spacing + 2, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 3 * char_spacing + 0, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 3 * char_spacing + 1, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 3 * char_spacing + 0, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 3 * char_spacing + 0, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 3 * char_spacing + 1, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 3 * char_spacing + 0, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 3 * char_spacing + 2, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
    }
    
    // Y (character 4)
    for (i = 0; i < 1; i++) {
        delay_ms(1000);
        gotoxy(start_x + 4 * char_spacing + 0, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 4 * char_spacing + 2, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 4 * char_spacing + 0, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 4 * char_spacing + 2, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 4 * char_spacing + 1, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 4 * char_spacing + 1, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 4 * char_spacing + 1, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
    }
    
    // R (character 5)
    for (i = 0; i < 1; i++) {
        delay_ms(1000);
        gotoxy(start_x + 5 * char_spacing + 0, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 5 * char_spacing + 1, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 5 * char_spacing + 0, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 5 * char_spacing + 2, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 5 * char_spacing + 0, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 5 * char_spacing + 1, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 5 * char_spacing + 0, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 5 * char_spacing + 1, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 5 * char_spacing + 0, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 5 * char_spacing + 2, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
    }
    
    // I (character 6)
    for (i = 0; i < 1; i++) {
        delay_ms(1000);
        gotoxy(start_x + 6 * char_spacing + 0, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 6 * char_spacing + 1, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 6 * char_spacing + 2, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 6 * char_spacing + 1, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 6 * char_spacing + 1, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 6 * char_spacing + 1, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 6 * char_spacing + 0, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 6 * char_spacing + 1, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 6 * char_spacing + 2, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
    }
    
    // E (character 7)
    for (i = 0; i < 1; i++) {
        delay_ms(1000);
        gotoxy(start_x + 7 * char_spacing + 0, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 7 * char_spacing + 1, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 7 * char_spacing + 2, start_y + 0); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 7 * char_spacing + 0, start_y + 1); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 7 * char_spacing + 0, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 7 * char_spacing + 1, start_y + 2); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 7 * char_spacing + 0, start_y + 3); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        delay_ms(1000);
        gotoxy(start_x + 7 * char_spacing + 0, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 7 * char_spacing + 1, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
        gotoxy(start_x + 7 * char_spacing + 2, start_y + 4); print_char_color(' ', MAKE_COLOR(COLOR_BLUE, COLOR_WHITE));
    }
}

#pragma aux _cstart_ "*"
void _cdecl _cstart_(uint16_t bootDrive) {
    
    // Set proper video mode FIRST (this will clear screen)
    set_video_mode();
    
    // Clear screen with black background
    clear_screen(MAKE_COLOR(COLOR_BLACK, COLOR_WHITE));
    
    // Draw animated border around the entire screen
    draw_animated_border();
    
    // Wait a moment then draw the large text
    //delay_seconds(1);
    draw_text();
    
    // Infinite loop for now
    while(1) {
        __asm {
            hlt
        }
    }
}
