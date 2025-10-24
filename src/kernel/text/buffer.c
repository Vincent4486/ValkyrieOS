/* Minimal scrollback text buffer implementation for VGA text mode.
 * Exports the functions declared in text/buffer.h.
 */
#include "buffer.h"
#include <std/stdio.h>
#include <stddef.h>
#include <stdint.h>

#define BUFFER_LINES 1024

/* declare setcursor (defined in stdio.c) to avoid implicit declaration */
extern void setcursor(int x, int y);

uint8_t s_color = 0x7;
static char s_buffer[BUFFER_LINES][SCREEN_WIDTH];
static uint32_t s_head = 0;       /* index of first valid line */
static uint32_t s_lines_used = 0; /* number of logical lines in buffer */
int s_cursor_x = 0;
int s_cursor_y = 0; /* cursor within visible area (0..SCREEN_HEIGHT-1) */
static uint32_t s_scroll =
    0; /* number of lines scrolled away from bottom (0 = at bottom) */

void buffer_init(void) { buffer_clear(); }

void buffer_clear(void)
{
   for (uint32_t i = 0; i < BUFFER_LINES; i++)
      for (int j = 0; j < SCREEN_WIDTH; j++) s_buffer[i][j] = '\0';
   s_head = 0;
   s_lines_used = 0;
   s_cursor_x = 0;
   s_cursor_y = 0;
   s_scroll = 0;
   buffer_repaint();
}

static uint32_t tail_index(void)
{
   if (s_lines_used == 0) return 0;
   return (s_head + s_lines_used - 1) % BUFFER_LINES;
}

static void ensure_line_exists(void)
{
   if (s_lines_used == 0)
   {
      s_lines_used = 1;
      s_head = 0;
      for (int i = 0; i < SCREEN_WIDTH; i++) s_buffer[0][i] = '\0';
   }
}

/* Remove logical line at relative position rel_pos (0..s_lines_used-1).
   Shifts following lines left. */
static void buffer_remove_line_at_rel(uint32_t rel_pos)
{
   if (s_lines_used == 0 || rel_pos >= s_lines_used) return;
   for (uint32_t i = rel_pos; i + 1 < s_lines_used; i++)
   {
      uint32_t dst = (s_head + i) % BUFFER_LINES;
      uint32_t src = (s_head + i + 1) % BUFFER_LINES;
      for (int c = 0; c < SCREEN_WIDTH; c++)
         s_buffer[dst][c] = s_buffer[src][c];
   }
   uint32_t last = (s_head + s_lines_used - 1) % BUFFER_LINES;
   for (int c = 0; c < SCREEN_WIDTH; c++) s_buffer[last][c] = '\0';
   s_lines_used--;
   if (s_lines_used == 0) s_head = 0;
}

/* Append an empty line at the tail. If buffer full, drop the head. */
static void push_newline_at_tail(void)
{
   if (s_lines_used < BUFFER_LINES)
   {
      uint32_t idx = (s_head + s_lines_used) % BUFFER_LINES;
      for (int i = 0; i < SCREEN_WIDTH; i++) s_buffer[idx][i] = '\0';
      s_lines_used++;
   }
   else
   {
      /* drop oldest line */
      s_head = (s_head + 1) % BUFFER_LINES;
      uint32_t idx = (s_head + s_lines_used - 1) % BUFFER_LINES;
      for (int i = 0; i < SCREEN_WIDTH; i++) s_buffer[idx][i] = '\0';
   }
}

void buffer_putc(char c)
{
   ensure_line_exists();

   /* visible_start is the relative index (from head) of the first visible
      logical line. We support a simple scroll (s_scroll) that moves the visible
      window up from the bottom. Compute the base (last SCREEN_HEIGHT lines)
      then subtract s_scroll (clamped). */
   int visible_base =
       (s_lines_used > SCREEN_HEIGHT) ? (int)(s_lines_used - SCREEN_HEIGHT) : 0;
   int visible_start = visible_base - (int)s_scroll;
   if (visible_start < 0) visible_start = 0;
   /* target logical relative index */
   uint32_t rel_pos = (uint32_t)visible_start + (uint32_t)s_cursor_y;

   /* Do NOT eagerly create lines here. Creation is done lazily when a printable
      character is inserted. This prevents creating a trailing empty logical
      line when the last character printed is a '\n'. */

   uint32_t idx = (s_head + (rel_pos < s_lines_used
                                 ? rel_pos
                                 : (s_lines_used ? s_lines_used - 1 : 0))) %
                  BUFFER_LINES;

   /* Control characters */
   if (c == '\n')
   {
      /* split line: create a new empty line after rel_pos and move cursor there
       */
      /* Move cursor to the start of the next visible line. Do not create a
         new logical line here; a printable character will allocate it when
         needed. This avoids leaving an extra empty logical line after
         printing a trailing '\n'. */
      /* Move view to next visible line. When newlines are produced we want to
         show the bottom by default (reset scroll). */
      s_scroll = 0;
      if (s_cursor_y < SCREEN_HEIGHT - 1)
         s_cursor_y++;
      else
      {
         /* At bottom visible row: allocate a new logical tail line so the view
            scrolls down. push_newline_at_tail() will drop the head if the
            circular buffer is full. */
         push_newline_at_tail();
      }

      s_cursor_x = 0;
      buffer_repaint();
      return;
   }

   if (c == '\r')
   {
      s_cursor_x = 0;
      buffer_repaint();
      return;
   }

   if (c == '\t')
   {
      int n = 4 - (s_cursor_x % 4);
      for (int i = 0; i < n; i++) buffer_putc(' ');
      return;
   }

   if (c == '\b')
   {
      /* backspace */
      if (s_cursor_x > 0)
      {
         /* shift left within line */
         for (int i = s_cursor_x - 1; i < SCREEN_WIDTH - 1; i++)
            s_buffer[idx][i] = s_buffer[idx][i + 1];
         s_buffer[idx][SCREEN_WIDTH - 1] = '\0';
         s_cursor_x--;
         buffer_repaint();
         return;
      }

      /* at column 0: if previous logical line exists, merge */
      if (rel_pos > 0)
      {
         uint32_t prev_rel = rel_pos - 1;
         uint32_t prev_idx = (s_head + prev_rel) % BUFFER_LINES;
         /* compute lengths */
         int len_prev = 0, len_curr = 0;
         while (len_prev < SCREEN_WIDTH && s_buffer[prev_idx][len_prev])
            len_prev++;
         while (len_curr < SCREEN_WIDTH && s_buffer[idx][len_curr]) len_curr++;
         int orig_prev_len = len_prev;
         int can_move = SCREEN_WIDTH - len_prev;
         int move = (len_curr < can_move) ? len_curr : can_move;
         for (int i = 0; i < move; i++)
            s_buffer[prev_idx][len_prev + i] = s_buffer[idx][i];

         if (move < len_curr)
         {
            int leftover = len_curr - move;
            for (int i = 0; i < leftover; i++)
               s_buffer[idx][i] = s_buffer[idx][move + i];
            for (int i = leftover; i < SCREEN_WIDTH; i++)
               s_buffer[idx][i] = '\0';
         }
         else
         {
            for (int i = 0; i < SCREEN_WIDTH; i++) s_buffer[idx][i] = '\0';
         }

         /* if current line now empty, remove it */
         int empty = 1;
         for (int i = 0; i < SCREEN_WIDTH; i++)
            if (s_buffer[idx][i])
            {
               empty = 0;
               break;
            }
         if (empty)
         {
            buffer_remove_line_at_rel(rel_pos);
            /* set cursor to end of prev line (adjust visible start) */
            int visible_off = (int)((s_lines_used > SCREEN_HEIGHT)
                                        ? (s_lines_used - SCREEN_HEIGHT)
                                        : 0);
            s_cursor_y = (int)prev_rel - visible_off;
            if (s_cursor_y < 0) s_cursor_y = 0;
            /* Place the cursor at the start of the inserted text (the old end
               of the previous line), not at the end of the newly merged
               content. */
            int new_len = orig_prev_len;
            if (new_len > SCREEN_WIDTH - 1) new_len = SCREEN_WIDTH - 1;
            s_cursor_x = new_len;
         }
         else
         {
            s_cursor_x = 0;
         }
         buffer_repaint();
         return;
      }

      /* backspace modifies content; reset scroll to show bottom */
      s_scroll = 0;
      buffer_repaint();
      return;
   }

   /* Printable character: insert at cursor position in the target line. */
   {
      /* recompute visible_start (consider scroll) because s_lines_used might
       * have changed */
      visible_base = (s_lines_used > SCREEN_HEIGHT)
                         ? (int)(s_lines_used - SCREEN_HEIGHT)
                         : 0;
      visible_start = visible_base - (int)s_scroll;
      if (visible_start < 0) visible_start = 0;
      rel_pos = (uint32_t)visible_start + (uint32_t)s_cursor_y;
      while (rel_pos >= s_lines_used) push_newline_at_tail();
      idx = (s_head + rel_pos) % BUFFER_LINES;

      int len = 0;
      while (len < SCREEN_WIDTH && s_buffer[idx][len]) len++;
      if (s_cursor_x > len) s_cursor_x = len;

      if (len < SCREEN_WIDTH)
      {
         for (int i = len; i > s_cursor_x; i--)
            s_buffer[idx][i] = s_buffer[idx][i - 1];
         s_buffer[idx][s_cursor_x] = c;
      }
      else
      {
         /* move last char to next line (prepend) */
         char last = s_buffer[idx][SCREEN_WIDTH - 1];
         if (rel_pos + 1 >= s_lines_used)
         {
            push_newline_at_tail();
         }
         uint32_t next_idx = (s_head + rel_pos + 1) % BUFFER_LINES;
         for (int i = SCREEN_WIDTH - 1; i > 0; i--)
            s_buffer[next_idx][i] = s_buffer[next_idx][i - 1];
         s_buffer[next_idx][0] = last;
         for (int i = SCREEN_WIDTH - 1; i > s_cursor_x; i--)
            s_buffer[idx][i] = s_buffer[idx][i - 1];
         s_buffer[idx][s_cursor_x] = c;
      }

      s_cursor_x++;
      /* After inserting printable characters we want to follow the tail. */
      s_scroll = 0;
      if (s_cursor_x >= SCREEN_WIDTH)
      {
         s_cursor_x = 0;
         if (s_cursor_y < SCREEN_HEIGHT - 1)
            s_cursor_y++;
         else if (s_lines_used > SCREEN_HEIGHT)
            s_head = (s_head + 1) % BUFFER_LINES;
      }
      buffer_repaint();
      return;
   }
}

void buffer_puts(const char *s)
{
   while (*s) buffer_putc(*s++);
}

void buffer_scroll(int lines)
{
   /* Positive lines -> scroll up (view older content); negative -> scroll down.
      We maintain s_scroll which is clamped between 0 and max_scroll. */
   if (s_lines_used <= SCREEN_HEIGHT)
   {
      /* nothing to scroll */
      return;
   }

   int max_scroll = (int)(s_lines_used - SCREEN_HEIGHT);
   int new_scroll = (int)s_scroll + lines;
   if (new_scroll < 0) new_scroll = 0;
   if (new_scroll > max_scroll) new_scroll = max_scroll;
   s_scroll = (uint32_t)new_scroll;
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
   /* determine start of visible window (last SCREEN_HEIGHT lines) then apply
    * scroll */
   int base =
       (s_lines_used > SCREEN_HEIGHT) ? (int)(s_lines_used - SCREEN_HEIGHT) : 0;
   int start = base - (int)s_scroll;
   if (start < 0) start = 0;

   uint8_t *vga = (uint8_t *)0xB8000;
   const uint8_t def_color = 0x7;

   for (uint32_t row = 0; row < SCREEN_HEIGHT; row++)
   {
      uint32_t logical_line = (uint32_t)start + row;
      if (logical_line >= s_lines_used)
      {
         /* no logical line here: clear the row with spaces */
         for (uint32_t col = 0; col < SCREEN_WIDTH; col++)
         {
            vga[2 * (row * SCREEN_WIDTH + col)] = ' ';
            vga[2 * (row * SCREEN_WIDTH + col) + 1] = def_color;
         }
      }
      else
      {
         uint32_t src = (s_head + logical_line) % BUFFER_LINES;
         for (uint32_t col = 0; col < SCREEN_WIDTH; col++)
         {
            char ch = s_buffer[src][col];
            if (!ch) ch = ' ';
            vga[2 * (row * SCREEN_WIDTH + col)] = ch;
            vga[2 * (row * SCREEN_WIDTH + col) + 1] =
                s_color ? s_color : def_color;
         }
      }
   }

   setcursor(s_cursor_x, s_cursor_y);
}
