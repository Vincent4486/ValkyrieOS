// SPDX-License-Identifier: GPL-3.0-only

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