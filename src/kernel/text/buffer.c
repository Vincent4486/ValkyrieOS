/* Scrollback text buffer for VGA text mode. Clean implementation. */

#include "buffer.h"
#include <std/stdio.h>
#include <stdint.h>
#include <stddef.h>

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define BUFFER_LINES 1024

static uint8_t s_color = 0x7;
static char s_buffer[BUFFER_LINES][SCREEN_WIDTH];
static uint32_t s_head = 0;
static uint32_t s_lines_used = 0;
static int s_cursor_x = 0;
static int s_cursor_y = 0;

void buffer_init(void) { buffer_clear(); }

void buffer_clear(void)
{
	for (uint32_t i = 0; i < BUFFER_LINES; i++)
		for (int j = 0; j < SCREEN_WIDTH; j++) s_buffer[i][j] = '\0';
	s_head = 0;
	s_lines_used = 0;
	s_cursor_x = 0;
	s_cursor_y = 0;
	buffer_repaint();
}

static uint32_t ensure_tail_line(void)
{
	if (s_lines_used == 0)
	{
		s_lines_used = 1;
		s_head = 0;
		for (int i = 0; i < SCREEN_WIDTH; i++) s_buffer[0][i] = '\0';
	}
	return (s_head + s_lines_used - 1) % BUFFER_LINES;
}

static void buffer_push_char(char c)
{
	if (c == '\n')
	{
		if (s_lines_used < BUFFER_LINES)
		{
			uint32_t idx = (s_head + s_lines_used) % BUFFER_LINES;
			for (int i = 0; i < SCREEN_WIDTH; i++) s_buffer[idx][i] = '\0';
			s_lines_used++;
		}
		else
		{
			s_head = (s_head + 1) % BUFFER_LINES;
			uint32_t idx = (s_head + s_lines_used - 1) % BUFFER_LINES;
			for (int i = 0; i < SCREEN_WIDTH; i++) s_buffer[idx][i] = '\0';
		}
		s_cursor_x = 0;
		if (s_cursor_y < SCREEN_HEIGHT - 1) s_cursor_y++;
		return;
	}
	uint32_t idx = ensure_tail_line();
	if (s_cursor_x >= SCREEN_WIDTH)
	{
		buffer_push_char('\n');
		idx = ensure_tail_line();
	}
	s_buffer[idx][s_cursor_x++] = c;
}

static void buffer_remove_line_at(uint32_t pos)
{
	if (s_lines_used == 0 || pos >= s_lines_used) return;
	for (uint32_t i = pos; i + 1 < s_lines_used; i++)
	{
		uint32_t dst = (s_head + i) % BUFFER_LINES;
		uint32_t src = (s_head + i + 1) % BUFFER_LINES;
		for (int c = 0; c < SCREEN_WIDTH; c++) s_buffer[dst][c] = s_buffer[src][c];
	}
	uint32_t last = (s_head + s_lines_used - 1) % BUFFER_LINES;
	for (int c = 0; c < SCREEN_WIDTH; c++) s_buffer[last][c] = '\0';
	s_lines_used--;
	if (s_lines_used == 0) s_head = 0;
}

void buffer_putc(char c)
{
	ensure_tail_line();
	switch (c)
	{
	case '\r':
		s_cursor_x = 0;
		break;
	case '\b':
		if (s_cursor_x > 0)
		{
			s_cursor_x--;
			uint32_t idx = (s_head + s_lines_used - 1) % BUFFER_LINES;
			s_buffer[idx][s_cursor_x] = '\0';
		}
		else
		{
			if (s_lines_used > 1)
			{
				uint32_t tail_pos = s_lines_used - 1;
				uint32_t tail_idx = (s_head + tail_pos) % BUFFER_LINES;
				if (s_buffer[tail_idx][0] == '\0')
				{
					buffer_remove_line_at(tail_pos);
					if (s_lines_used > 0)
					{
						uint32_t new_tail = (s_head + s_lines_used - 1) % BUFFER_LINES;
						int len = 0;
						while (len < SCREEN_WIDTH && s_buffer[new_tail][len]) len++;
						s_cursor_x = len;
						if (s_cursor_y > 0) s_cursor_y--;
					}
				}
			}
		}
		break;
	case '\t':
	{
		int n = 4 - (s_cursor_x % 4);
		for (int i = 0; i < n; i++) buffer_push_char(' ');
		break;
	}
	default:
		buffer_push_char(c);
		break;
	}
	buffer_repaint();
}

void buffer_puts(const char *s)
{
	while (*s) buffer_putc(*s++);
}

void buffer_scroll(int lines)
{
	if (lines <= 0) return;
	uint32_t max_scroll = (s_lines_used > SCREEN_HEIGHT) ? (s_lines_used - SCREEN_HEIGHT) : 0;
	if ((uint32_t)lines > max_scroll) lines = max_scroll;
	s_head = (s_head + lines) % BUFFER_LINES;
	buffer_repaint();
}

void buffer_set_color(uint8_t color) { s_color = color; }

void buffer_set_cursor(int x, int y)
{
	if (x < 0) x = 0;
	if (x >= SCREEN_WIDTH) x = SCREEN_WIDTH - 1;
	if (y < 0) y = 0;
	if (y >= SCREEN_HEIGHT) y = SCREEN_HEIGHT - 1;
	s_cursor_x = x;
	s_cursor_y = y;
	setcursor(s_cursor_x, s_cursor_y);
}

void buffer_get_cursor(int *x, int *y)
{
	if (x) *x = s_cursor_x;
	if (y) *y = s_cursor_y;
}

void buffer_repaint(void)
{
	uint32_t start_line = 0;
	if (s_lines_used > SCREEN_HEIGHT)
		start_line = s_lines_used - SCREEN_HEIGHT;
	else
		start_line = 0;

	uint8_t *vga = (uint8_t *)0xB8000;
	const uint8_t default_color = 0x7;

	for (uint32_t row = 0; row < SCREEN_HEIGHT; row++)
	{
		uint32_t src_idx = (s_head + start_line + row) % BUFFER_LINES;
		for (uint32_t col = 0; col < SCREEN_WIDTH; col++)
		{
			char ch = s_buffer[src_idx][col];
			if (!ch) ch = '\0';
			vga[2 * (row * SCREEN_WIDTH + col)] = ch;
			vga[2 * (row * SCREEN_WIDTH + col) + 1] = s_color ? s_color : default_color;
		}
	}

	setcursor(s_cursor_x, s_cursor_y);
}
#include <arch/i686/io.h>
#include <std/stdio.h>
#include <stddef.h>
#include <stdint.h>

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define BUFFER_LINES 1024 // keep a scrollback buffer

static uint8_t s_color = 0x7;
static char s_buffer[BUFFER_LINES][SCREEN_WIDTH];
static uint32_t s_head = 0; // index of first valid line in buffer
static uint32_t s_lines_used = 0;
static int s_cursor_x = 0,
           s_cursor_y = 0; // cursor position relative to visible screen
                           // (0..width-1, 0..height-1)

void buffer_init(void) { buffer_clear(); }

void buffer_clear(void)
{
	for (uint32_t i = 0; i < BUFFER_LINES; i++)
		for (int j = 0; j < SCREEN_WIDTH; j++) s_buffer[i][j] = '\0';
	s_head = 0;
	s_lines_used = 0;
	s_cursor_x = 0;
	s_cursor_y = 0;
	buffer_repaint();
		else
		{
			/* At start of current line: if the current visible line is empty, remove it and
			   move cursor to end of previous visible line. We operate on the visible tail
			   (last line in the buffer), not arbitrary scrolled positions. */
			if (s_lines_used > 1)
			{
				/* index of visible tail line */
				uint32_t tail_pos = s_lines_used - 1;
				uint32_t tail_idx = (s_head + tail_pos) % BUFFER_LINES;
				/* if tail line is empty */
				if (s_buffer[tail_idx][0] == '\0')
				{
					/* remove that line */
					buffer_remove_line_at(tail_pos);
					/* new tail is previous line */
					uint32_t new_tail_idx = (s_head + s_lines_used - 1) % BUFFER_LINES;
					int len = 0;
					while (len < SCREEN_WIDTH && s_buffer[new_tail_idx][len]) len++;
					s_cursor_x = len;
				}
			}
		}
		// clear new current line
		uint32_t idx = (s_head + s_lines_used - 1) % BUFFER_LINES;
		for (int i = 0; i < SCREEN_WIDTH; i++) s_buffer[idx][i] = '\0';
		s_cursor_x = 0;
		// if visible area full, scroll by moving cursor_y
		if (s_cursor_y < SCREEN_HEIGHT - 1)
			s_cursor_y++;
		else
			; // top of visible window moves implicitly when painted
		return;
	}

	// printable char
	uint32_t idx = (s_head + s_lines_used - 1) % BUFFER_LINES;
	if (s_cursor_x >= SCREEN_WIDTH)
	{
		buffer_push_char('\n');
		idx = (s_head + s_lines_used - 1) % BUFFER_LINES;
	}
	s_buffer[idx][s_cursor_x++] = c;
}

/* Remove line at logical position 'pos' (0..s_lines_used-1 relative to head).
   After removal, s_lines_used is decremented. The contents after pos shift left. */
static void buffer_remove_line_at(uint32_t pos)
{
	if (s_lines_used == 0 || pos >= s_lines_used) return;

	for (uint32_t i = pos; i + 1 < s_lines_used; i++)
	{
		uint32_t dst = (s_head + i) % BUFFER_LINES;
		uint32_t src = (s_head + i + 1) % BUFFER_LINES;
		for (int c = 0; c < SCREEN_WIDTH; c++) s_buffer[dst][c] = s_buffer[src][c];
	}
	/* clear last line */
	uint32_t last = (s_head + s_lines_used - 1) % BUFFER_LINES;
	for (int c = 0; c < SCREEN_WIDTH; c++) s_buffer[last][c] = '\0';
	s_lines_used--;
}

void buffer_putc(char c)
{
	// ensure at least one line exists
	if (s_lines_used == 0)
	{
		s_lines_used = 1;
		s_head = 0;
		for (int i = 0; i < SCREEN_WIDTH; i++) s_buffer[0][i] = '\0';
	}
	switch (c)
	{
	case '\r':
		s_cursor_x = 0;
		break;
	case '\b':
		if (s_cursor_x > 0)
		{
			s_cursor_x--;
			uint32_t idx = (s_head + s_lines_used - 1) % BUFFER_LINES;
			s_buffer[idx][s_cursor_x] = '\0';
		}
		else
		{
			/* At start of current line: if there is a previous line (i.e. last
			   action was a newline), remove the empty tail line and move cursor
			   to end of previous line */
			if (s_lines_used > 1)
			{
				/* remove current (empty) tail line */
				s_lines_used--;
				/* compute new tail index */
				uint32_t idx = (s_head + s_lines_used - 1) % BUFFER_LINES;
				/* find end of previous line */
				int len = 0;
				while (len < SCREEN_WIDTH && s_buffer[idx][len]) len++;
				s_cursor_x = len;
			}
		}
		break;
	case '\t':
		for (int i = 0; i < 4 - (s_cursor_x % 4); i++) buffer_push_char(' ');
		break;
	default:
		buffer_push_char(c);
		break;
	}
	buffer_repaint();
}

void buffer_puts(const char *s)
{
	while (*s)
	{
		buffer_putc(*s++);
	}
}

void buffer_scroll(int lines)
{
	if (lines <= 0) return;
	// if we have more lines than visible, advance head by lines (up to
	// s_lines_used - SCREEN_HEIGHT)
	uint32_t max_scroll =
	    (s_lines_used > SCREEN_HEIGHT) ? (s_lines_used - SCREEN_HEIGHT) : 0;
	if ((uint32_t)lines > max_scroll) 
        lines = max_scroll;
	s_head = (s_head + lines) % BUFFER_LINES;
	buffer_repaint();
}

void buffer_set_color(uint8_t color) { s_color = color; }

void buffer_set_cursor(int x, int y)
{
	if (x < 0) x = 0;
	if (x >= SCREEN_WIDTH) x = SCREEN_WIDTH - 1;
	if (y < 0) y = 0;
	if (y >= SCREEN_HEIGHT) y = SCREEN_HEIGHT - 1;
	s_cursor_x = x;
	s_cursor_y = y;
	setcursor(s_cursor_x, s_cursor_y);
}

void buffer_get_cursor(int *x, int *y)
{
	if (x) *x = s_cursor_x;
	if (y) *y = s_cursor_y;
}

void buffer_repaint(void)
{
	// repaint visible area with last SCREEN_HEIGHT lines
	uint32_t start_line = 0;
	if (s_lines_used > SCREEN_HEIGHT)
		start_line = s_lines_used - SCREEN_HEIGHT;
	else
		start_line = 0;

	for (uint32_t row = 0; row < SCREEN_HEIGHT; row++)
	{
		uint32_t src_idx = (s_head + start_line + row) % BUFFER_LINES;
		for (uint32_t col = 0; col < SCREEN_WIDTH; col++)
		{
			char ch = s_buffer[src_idx][col];
			if (!ch) ch = '\0';
			/* write character and color to VGA buffer directly */
			uint8_t *vga = (uint8_t *)0xB8000;
			const uint8_t default_color = 0x7;
			vga[2 * (row * SCREEN_WIDTH + col)] = ch;
			vga[2 * (row * SCREEN_WIDTH + col) + 1] =
			    s_color ? s_color : default_color;
		}
	}
	setcursor(s_cursor_x, s_cursor_y);
}
