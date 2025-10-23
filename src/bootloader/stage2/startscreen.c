#include "startscreen.h"
#include <stdbool.h>
#include <stdint.h>

/* Simple VGA text-mode helper for stage2 (80x25, color attributes).
   This is intentionally small and self-contained so it can be used in
   a freestanding environment during early boot. */

static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;
static int cur_x = 0;
static int cur_y = 0;
static uint8_t cur_attr = 0x07; /* light gray on black */

static inline int clamp_x(int x)
{
	if (x < 0) return 0;
	if (x > 79) return 79;
	return x;
}

static inline int clamp_y(int y)
{
	if (y < 0) return 0;
	if (y > 24) return 24;
	return y;
}

static void scroll_up_if_needed(void)
{
	if (cur_y < 25) return;
	/* move lines 1..24 to 0..23 */
	for (int row = 0; row < 24; ++row)
	{
		for (int col = 0; col < 80; ++col)
		{
			VGA[row * 80 + col] = VGA[(row + 1) * 80 + col];
		}
	}
	/* clear last row */
	for (int col = 0; col < 80; ++col)
	{
		VGA[24 * 80 + col] = (uint16_t)(' ') | ((uint16_t)cur_attr << 8);
	}
	cur_y = 24;
}

void draw_start_screen(bool showBoot)
{
	if (showBoot)
	{
		draw_outline();
		draw_text();
	}
}

void draw_outline()
{
	/* rainbow border using spaces with different background colors.
	   Draw each cell and pause after each to create a per-character animation.
	 */
	/* compute centered box dimensions */
	const int box_width = 60; /* interior including borders */
	const int box_height = 15;
	const int left = (80 - box_width) / 2;
	int top = (25 - box_height) / 2;
	/* move the whole outline up by 2 rows, clamp to screen */
	top -= 1;
	if (top < 0) top = 0;
	const int right = left + box_width - 1;
	const int bottom = top + box_height - 1;

	const uint8_t palette[] = {0x04, 0x06, 0x02, 0x03, 0x01, 0x05, 0x0E, 0x0C};
	const int pcount = sizeof(palette) / sizeof(palette[0]);

	int idx = 0;
	/* top row: draw in horizontal pairs so adjacent characters match */
	for (int x = left; x <= right; x += 2)
	{
		uint8_t bg = palette[idx % pcount];
		uint8_t attr = (bg << 4) | 0x00;
		VGA[top * 80 + x] = (uint16_t)(' ') | ((uint16_t)attr << 8);
		delay_ms(300);
		if (x + 1 <= right)
		{
			VGA[top * 80 + (x + 1)] = (uint16_t)(' ') | ((uint16_t)attr << 8);
			delay_ms(300);
		}
		++idx;
	}
	/* bottom row */
	/* bottom row: horizontal pairs as well */
	for (int x = left; x <= right; x += 2)
	{
		uint8_t bg = palette[idx % pcount];
		uint8_t attr = (bg << 4) | 0x00;
		VGA[bottom * 80 + x] = (uint16_t)(' ') | ((uint16_t)attr << 8);
		delay_ms(300);
		if (x + 1 <= right)
		{
			VGA[bottom * 80 + (x + 1)] =
			    (uint16_t)(' ') | ((uint16_t)attr << 8);
			delay_ms(300);
		}
		++idx;
	}
	/* left and right columns (two characters thick) */
	for (int y = top + 1; y < bottom; ++y)
	{
		/* left pair (same color horizontally) */
		uint8_t bg_left = palette[idx % pcount];
		uint8_t attr_left = (bg_left << 4) | 0x00;
		VGA[y * 80 + left] = (uint16_t)(' ') | ((uint16_t)attr_left << 8);
		delay_ms(300);
		VGA[y * 80 + (left + 1)] = (uint16_t)(' ') | ((uint16_t)attr_left << 8);
		delay_ms(300);
		++idx;

		/* right pair (same color horizontally) */
		uint8_t bg_right = palette[idx % pcount];
		uint8_t attr_right = (bg_right << 4) | 0x00;
		VGA[y * 80 + right] = (uint16_t)(' ') | ((uint16_t)attr_right << 8);
		delay_ms(300);
		VGA[y * 80 + (right - 1)] =
		    (uint16_t)(' ') | ((uint16_t)attr_right << 8);
		delay_ms(300);
		++idx;
	}
	/* corners are already filled by the top/bottom and left/right pairs */
}

void draw_text()
{
	const char *title = "Valkyrie OS";
	const char *line2 = "Loading...";

	/* center title on box */
	int title_len = 0;
	while (title[title_len]) ++title_len;
	int x = (80 - title_len) / 2;
	int y = 10;
	gotoxy(x, y);
	for (int i = 0; title[i]; ++i) printChar(title[i], 0x0F);

	gotoxy((80 - 10) / 2, y + 2);
	for (int i = 0; line2[i]; ++i) printChar(line2[i], 0x0F);
}

void gotoxy(int x, int y)
{
	cur_x = clamp_x(x);
	cur_y = clamp_y(y);
}

void printChar(char character, uint8_t color)
{
	cur_attr = color;
	if (character == '\n')
	{
		cur_x = 0;
		++cur_y;
		scroll_up_if_needed();
		return;
	}

	VGA[cur_y * 80 + cur_x] = (uint16_t)character | ((uint16_t)cur_attr << 8);
	++cur_x;
	if (cur_x >= 80)
	{
		cur_x = 0;
		++cur_y;
		scroll_up_if_needed();
	}

	/* Busy-wait ~300 ms. This is approximate and depends on CPU speed.
   If you need precise timing, use PIT calibration or the PIT itself. */
	extern void delay_ms(unsigned int ms);
	delay_ms(300);
}

/* Very small, approximate busy-wait delay. Calibrate or replace with PIT
   for accurate timings. This loops a simple number of cycles; value chosen
   to be reasonable for typical emulator speeds. */
void delay_ms(unsigned int ms)
{
	/* inner loop iterations per millisecond; tuned for QEMU/typical x86 speed.
	   If this is too slow/fast on your machine, reduce/increase the factor. */
	volatile unsigned long outer = ms;
	const unsigned long iters_per_ms = 40000UL; /* rough, emulator-friendly */
	for (; outer; --outer)
	{
		volatile unsigned long i = iters_per_ms;
		while (i--)
		{
			__asm__ volatile("nop");
		}
	}
}
