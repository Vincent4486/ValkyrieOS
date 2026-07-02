// SPDX-License-Identifier: GPL-3.0-only
//
// UART (serial) driver — assumes a VT100/xterm-compatible terminal
// (or emulator) is connected at the other end of the line.

#include <stddef.h>
#include <stdint.h>

#include "video.h"

/* UART (COM1) I/O registers */
#define UART_PORT 0x3F8
#define UART_DATA (UART_PORT + 0) /* R/W: data register       */
#define UART_IER (UART_PORT + 1)  /* W:   interrupt enable    */
#define UART_FCR (UART_PORT + 2)  /* W:   FIFO control        */
#define UART_LCR (UART_PORT + 3)  /* W:   line control        */
#define UART_MCR (UART_PORT + 4)  /* W:   modem control       */
#define UART_LSR (UART_PORT + 5)  /* R:   line status         */

/* Line status register bits */
#define LSR_THR_EMPTY (1 << 5) /* Transmitter hold reg empty */

/* Default fallback dimensions if terminal query fails or times out. */
#define UART_DEFAULT_WIDTH  80
#define UART_DEFAULT_HEIGHT 24

static int s_Initialized = 0;

/* Flag: does the terminal need explicit \r before \n?
 * Set by detect_newline_behavior(). */
static int s_NeedCR = 0;

static uint32_t s_TermWidth  = UART_DEFAULT_WIDTH;
static uint32_t s_TermHeight = UART_DEFAULT_HEIGHT;

/* --- static helpers ------------------------------------------------------ */

/* Write a byte to UART, waiting for THR empty. */
static void write_byte(uint8_t b)
{
   while (!(inb(UART_LSR) & LSR_THR_EMPTY))
      ;
   outb(UART_DATA, b);
}

/* Write an ANSI escape sequence: ESC [ args */
static void ansi(const char *seq)
{
   write_byte('\x1B');
   write_byte('[');
   while (*seq)
      write_byte((uint8_t)*seq++);
}

/* Write a decimal number to the UART (for ANSI escape sequences). */
static void write_dec(uint32_t n)
{
   char buf[12];
   int i = sizeof(buf) - 1;
   buf[i] = '\0';

   do {
      buf[--i] = (char)('0' + (n % 10));
      n /= 10;
   } while (n);

   while (buf[i])
      write_byte((uint8_t)buf[i++]);
}

/* Position cursor to (col, row) — ANSI 1-based. */
static void cursor_goto(int x, int y)
{
   write_byte('\x1B');
   write_byte('[');
   write_dec((uint32_t)(y + 1));
   write_byte(';');
   write_dec((uint32_t)(x + 1));
   write_byte('H');
}

/* Set ANSI 256-colour foreground (38;5;N) and background (48;5;N)
 * from a VGA text attribute byte (low nibble = fg, high nibble = bg). */
static void set_attr_from_vga(char color)
{
   /* Foreground */
   write_byte('\x1B');
   write_byte('[');
   write_byte('3'); write_byte('8'); write_byte(';');
   write_byte('5'); write_byte(';');
   write_dec((uint32_t)(uint8_t)(color & 0x0F));
   write_byte('m');

   /* Background */
   write_byte('\x1B');
   write_byte('[');
   write_byte('4'); write_byte('8'); write_byte(';');
   write_byte('5'); write_byte(';');
   write_dec((uint32_t)(uint8_t)((color >> 4) & 0x0F));
   write_byte('m');
}

/* Reset all ANSI attributes. */
static void reset_attr(void)
{
   ansi("0m");
}

/* Read a byte from UART with a simple poll-loop timeout (approx counts).
 * Returns the byte on success, -1 on timeout. */
static int read_byte(void)
{
   for (int timeout = 0; timeout < 100000; timeout++)
   {
      if (inb(UART_LSR) & 1)
         return inb(UART_DATA);
   }
   return -1;
}

/* Read and discard any pending input bytes. */
static void flush_input(void)
{
   while (inb(UART_LSR) & 1)
      (void)inb(UART_DATA);
}

/* Query the terminal for its dimensions using the VT100/xterm cursor
 * position report (CPR) method:
 *   1. Move cursor far down-right  (ESC [ 9999 ; 9999 H)
 *   2. Request cursor position     (ESC [ 6 n)
 *   3. Terminal replies with       ESC [ row ; col R
 * Falls back to UART_DEFAULT_WIDTH / UART_DEFAULT_HEIGHT on timeout. */
static void query_terminal_size(void)
{
   /* Move cursor to 9999,9999 (terminal clamps to bottom-right) */
   write_byte('\x1B'); write_byte('[');
   write_byte('9'); write_byte('9'); write_byte('9'); write_byte('9');
   write_byte(';');
   write_byte('9'); write_byte('9'); write_byte('9'); write_byte('9');
   write_byte('H');

   /* Request cursor position */
   ansi("6n");

   /* Parse response: ESC [ rows ; cols R */
   int b = read_byte();
   if (b != '\x1B') return;
   b = read_byte();
   if (b != '[')    return;

   uint32_t row = 0;
   for (;;)
   {
      b = read_byte();
      if (b < 0 || b == ';') break;
      if (b < '0' || b > '9') return;
      row = row * 10 + (uint32_t)(b - '0');
   }
   if (b != ';') return;

   uint32_t col = 0;
   for (;;)
   {
      b = read_byte();
      if (b < 0 || b == 'R') break;
      if (b < '0' || b > '9') return;
      col = col * 10 + (uint32_t)(b - '0');
   }
   if (b != 'R') return;

   if (row > 0 && col > 0)
   {
      s_TermHeight = row;
      s_TermWidth  = col;
   }
}

/* Probe whether \n alone moves the cursor to column 0, or whether
 * an explicit \r is needed.  Uses the same CPR method as
 * query_terminal_size().
 *
 *   1. Move cursor to column 10 on the current row.
 *   2. Send just \n.
 *   3. Query cursor position (CPR).
 *   4. If column > 10 → LF is LF-only → need \r.
 *      If column == 1  → LF implies CR → no extra \r needed.
 * Falls back to assuming \r IS needed on timeout (conservative). */
static void detect_newline_behavior(void)
{
   /* Set cursor to column 10. */
   cursor_goto(10, 0);
   flush_input();

   /* Send a bare line-feed. */
   write_byte('\n');

   /* Request cursor position. */
   ansi("6n");

   /* Parse response: ESC [ rows ; cols R */
   int b = read_byte();
   if (b != '\x1B') { s_NeedCR = 1; return; }
   b = read_byte();
   if (b != '[')    { s_NeedCR = 1; return; }

   uint32_t row = 0;
   for (;;)
   {
      b = read_byte();
      if (b < 0 || b == ';') break;
      if (b < '0' || b > '9') { s_NeedCR = 1; return; }
      row = row * 10 + (uint32_t)(b - '0');
   }
   if (b != ';') { s_NeedCR = 1; return; }

   uint32_t col = 0;
   for (;;)
   {
      b = read_byte();
      if (b < 0 || b == 'R') break;
      if (b < '0' || b > '9') { s_NeedCR = 1; return; }
      col = col * 10 + (uint32_t)(b - '0');
   }
   if (b != 'R') { s_NeedCR = 1; return; }

   /* ANSI is 1-based.  If col > 10 the \n didn't return to column 0 → need \r. */
   s_NeedCR = (col > 10) ? 1 : 0;
}

/* --- public interface ---------------------------------------------------- */

int UART_Initialize(void)
{
   /* Baud rate divisor = 1  (115200 baud with 1.8432 MHz crystal) */
   outb(UART_LCR, 0x80);  /* DLAB = 1 (enable divisor access) */
   outb(UART_DATA, 0x01); /* divisor low  byte                */
   outb(UART_IER, 0x00);  /* divisor high byte                */

   outb(UART_LCR, 0x03); /* 8 bits, no parity, 1 stop bit    */
   outb(UART_FCR, 0xC7); /* enable FIFO, clear, 14-byte thr  */
   outb(UART_MCR, 0x0B); /* DTR + RTS + OUT2 (enable IRQ)    */
   outb(UART_IER, 0x00); /* disable all interrupts           */

   flush_input();
   query_terminal_size();

   /* Probe whether the terminal needs \r before \n. */
   detect_newline_behavior();

   s_Initialized = 1;
   return SUCCESS;
}

int UART_PutChar(char c, int x, int y, char color)
{
   if (!s_Initialized) return -ENODEV;

   /* Both coordinates non-negative → absolute position with colour. */
   if (x >= 0 && y >= 0)
   {
      if ((uint32_t)x >= s_TermWidth || (uint32_t)y >= s_TermHeight)
         return -EINVAL;

      set_attr_from_vga(color);
      cursor_goto(x, y);
   }

   /* Send \r before \n if the terminal doesn't do CR on LF alone. */
   if (c == '\n' && s_NeedCR)
      write_byte('\r');

   /* Write the character. */
   write_byte((uint8_t)c);

   /* If we set a colour, reset so subsequent stream output is uncoloured. */
   if (x >= 0 && y >= 0)
      reset_attr();

   return SUCCESS;
}

int UART_PutPixel(uint32_t color, int x, int y)
{
   if (!s_Initialized) return -ENODEV;
   if (x < 0 || y < 0) return -EINVAL;

   if ((uint32_t)x >= s_TermWidth || (uint32_t)y >= s_TermHeight)
      return -EINVAL;

   cursor_goto(x, y);

   /* Map RGB to ANSI 256-colour (6x6x6 cube: 16-231). */
   uint8_t r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
   uint8_t ansi = 16 + 36 * (r / 51) + 6 * (g / 51) + (b / 51);

   /* 256-colour background: ESC[48;5;Nm, then space, then reset. */
   write_byte('\x1B'); write_byte('[');
   write_byte('4'); write_byte('8'); write_byte(';');
   write_byte('5'); write_byte(';');
   write_dec(ansi);
   write_byte('m');

   write_byte(' ');
   reset_attr();

   return SUCCESS;
}

uint32_t UART_GetWidth(void)
{
   return s_TermWidth;
}

uint32_t UART_GetHeight(void)
{
   return s_TermHeight;
}

void UART_ClearScreen(uint32_t color)
{
   /* Set 256-colour background then clear. */
   write_byte('\x1B'); write_byte('[');
   write_byte('4'); write_byte('8'); write_byte(';');
   write_byte('5'); write_byte(';');
   write_dec(color & 0xFF);
   write_byte('m');

   ansi("2J");
   ansi("H");  /* cursor home */
}