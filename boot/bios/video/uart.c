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

static int s_Initialized = 0;

/* Default fallback dimensions if terminal query fails or times out. */
#define UART_DEFAULT_WIDTH  80
#define UART_DEFAULT_HEIGHT 24

static uint32_t s_TermWidth  = UART_DEFAULT_WIDTH;
static uint32_t s_TermHeight = UART_DEFAULT_HEIGHT;

/* Read a byte from UART with a simple poll-loop timeout (approx counts).
 * Returns the byte on success, -1 on timeout. */
static int uart_read_byte(void)
{
   for (int timeout = 0; timeout < 100000; timeout++)
   {
      if (inb(UART_LSR) & 1)           /* Data ready (LSR bit 0) */
         return inb(UART_DATA);
   }
   return -1;
}

/* Read and discard any pending input bytes. */
static void uart_flush_input(void)
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
static void uart_query_terminal_size(void)
{
   /* Move cursor to 9999,9999 (terminal clamps to bottom-right) */
   outb(UART_DATA, '\x1B');
   outb(UART_DATA, '[');
   outb(UART_DATA, '9'); outb(UART_DATA, '9'); outb(UART_DATA, '9');
   outb(UART_DATA, '9');
   outb(UART_DATA, ';');
   outb(UART_DATA, '9'); outb(UART_DATA, '9'); outb(UART_DATA, '9');
   outb(UART_DATA, '9');
   outb(UART_DATA, 'H');

   /* Request cursor position */
   outb(UART_DATA, '\x1B');
   outb(UART_DATA, '[');
   outb(UART_DATA, '6');
   outb(UART_DATA, 'n');

   /* Parse response: ESC [ rows ; cols R */
   int b = uart_read_byte();
   if (b != '\x1B') return;
   b = uart_read_byte();
   if (b != '[')    return;

   uint32_t row = 0;
   for (;;)
   {
      b = uart_read_byte();
      if (b < 0 || b == ';') break;
      if (b < '0' || b > '9') return;
      row = row * 10 + (uint32_t)(b - '0');
   }
   if (b != ';') return;

   uint32_t col = 0;
   for (;;)
   {
      b = uart_read_byte();
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

   uart_flush_input();
   uart_query_terminal_size();

   s_Initialized = 1;
   return SUCCESS;
}

int UART_PutChar(char c, int x, int y, char color)
{
   (void)x;
   (void)y;
   (void)color;

   if (!s_Initialized) return -ENODEV;

   /* Wait until the transmitter holding register is empty */
   while (!(inb(UART_LSR) & LSR_THR_EMPTY))
      ;

   outb(UART_DATA, (uint8_t)c);
   return SUCCESS;
}

int UART_PutPixel(int pixel, int x, int y)
{
   (void)pixel;
   (void)x;
   (void)y;
   return -EINVAL;
}

uint32_t UART_GetWidth(void)
{
   return s_TermWidth;
}

uint32_t UART_GetHeight(void)
{
   return s_TermHeight;
}

void UART_ClearScreen(uint32_t pixel)
{
   (void)pixel;
   /* VT100 clear screen: ESC [ 2 J */
   outb(UART_DATA, '\x1B');
   outb(UART_DATA, '[');
   outb(UART_DATA, '2');
   outb(UART_DATA, 'J');
}