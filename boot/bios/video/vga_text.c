// SPDX-License-Identifier: GPL-3.0-only

#include <stddef.h>
#include <stdint.h>

#include "video.h"

static void scroll(void);
static uint8_t vgatext_attr_from_rgb(Video_Color color);

#define VGATEXT_BUFFER ((volatile char *)0xB8000)

#define VGATEXT_WIDTH 80
#define VGATEXT_HEIGHT 25

#define VGA_ATTR_BOTH(idx) ((uint8_t)(((idx) << 4) | (idx)))

static int s_Initialized = 0;
static int s_CursorX = 0;
static int s_CursorY = 0;
static Video_Color s_Color = TEXT_DEFAULT_COLOR; /* light green */
static uint8_t s_BgIdx = 0;

static const uint8_t s_VgaPal16[16][3] = {
    {0, 0, 0},       /* 0  black        */
    {0, 0, 170},     /* 1  blue         */
    {0, 170, 0},     /* 2  green        */
    {0, 170, 170},   /* 3  cyan         */
    {170, 0, 0},     /* 4  red          */
    {170, 0, 170},   /* 5  magenta      */
    {170, 85, 0},    /* 6  brown        */
    {170, 170, 170}, /* 7  light grey   */
    {85, 85, 85},    /* 8  dark grey    */
    {85, 85, 255},   /* 9  light blue   */
    {85, 255, 85},   /* 10 light green  */
    {85, 255, 255},  /* 11 light cyan   */
    {255, 85, 85},   /* 12 light red    */
    {255, 85, 255},  /* 13 light magenta*/
    {255, 255, 85},  /* 14 yellow       */
    {255, 255, 255}, /* 15 white        */
};

/* Scroll the buffer up by one line. */
static void scroll(void)
{
   volatile char *buf = VGATEXT_BUFFER;
   int row, col;

   /* Copy each row up by one */
   for (row = 1; row < VGATEXT_HEIGHT; row++)
   {
      for (col = 0; col < VGATEXT_WIDTH; col++)
      {
         int src_off = (row * VGATEXT_WIDTH + col) * 2;
         int dst_off = ((row - 1) * VGATEXT_WIDTH + col) * 2;
         buf[dst_off] = buf[src_off];
         buf[dst_off + 1] = buf[src_off + 1];
      }
   }

   for (col = 0; col < VGATEXT_WIDTH; col++)
   {
      int off = ((VGATEXT_HEIGHT - 1) * VGATEXT_WIDTH + col) * 2;
      buf[off] = ' ';
      buf[off + 1] = (char)(s_BgIdx << 4);
   }

   s_CursorY = VGATEXT_HEIGHT - 1;
}

// Map RGB to the nearest VGA text attribute byte
static uint8_t vgatext_attr_from_rgb(Video_Color c)
{
   uint8_t best = 0;
   int32_t best_dist = INT32_MAX;
   int32_t dr, dg, db, d;

   for (uint8_t i = 0; i < 16; i++)
   {
      dr = (int32_t)c.r - (int32_t)s_VgaPal16[i][0];
      dg = (int32_t)c.g - (int32_t)s_VgaPal16[i][1];
      db = (int32_t)c.b - (int32_t)s_VgaPal16[i][2];
      d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
      if (d < best_dist)
      {
         best_dist = d;
         best = i;
      }
   }
   return best;
}

void move_cursor(int x, int y)
{
   unsigned short position = (unsigned short)((y * 80) + x);

   /* VGA CRT Controller registers: index 0x3D4, data 0x3D5 */
   outb(0x3D4, 0x0F);
   outb(0x3D5, (uint8_t)(position & 0xFF));
   outb(0x3D4, 0x0E);
   outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

int VGATEXT_Initialize(void)
{
   volatile char *buf = VGATEXT_BUFFER;
   int i;

   s_BgIdx = 0;
   for (i = 0; i < VGATEXT_WIDTH * VGATEXT_HEIGHT * 2; i += 2)
   {
      buf[i] = ' ';
      buf[i + 1] = (char)vgatext_attr_from_rgb(s_Color);
   }

   s_CursorX = 0;
   s_CursorY = 0;
   move_cursor(s_CursorX, s_CursorY);

   s_Initialized = 1;
   return SUCCESS;
}

int VGATEXT_PutChar(char c, int x, int y, Video_Color color)
{
   volatile char *buf;
   int pos;
   uint8_t attr = vgatext_attr_from_rgb(color);

   /* Must be initialized */
   if (!s_Initialized) return -ENODEV;

   /* Exactly one of x / y negative -> invalid */
   if ((x < 0) != (y < 0)) return -EINVAL;

   /* Both negative -> write at cursor, then advance */
   if (x < 0 && y < 0)
   {
      x = s_CursorX;
      y = s_CursorY;
   }
   else
   {
      /* Clamp to screen bounds */
      if (x < 0) x = 0;
      if (x >= VGATEXT_WIDTH) x = VGATEXT_WIDTH - 1;
      if (y < 0) y = 0;
      if (y >= VGATEXT_HEIGHT) y = VGATEXT_HEIGHT - 1;
   }

   buf = VGATEXT_BUFFER;

   switch (c)
   {
   case '\n':
      /* Newline: carriage-return + line-feed */
      s_CursorX = 0;
      s_CursorY = y + 1;
      if (s_CursorY >= VGATEXT_HEIGHT) scroll();
      break;

   case '\r':
      /* Carriage return: go to column 0 on the same line */
      s_CursorX = 0;
      s_CursorY = y;
      break;

   case '\t':
      /* Tab: advance to next 8-column boundary */
      {
         int tab_stop = (x / 8 + 1) * 8;
         if (tab_stop >= VGATEXT_WIDTH) tab_stop = VGATEXT_WIDTH - 1;
         s_CursorX = tab_stop;
         s_CursorY = y;
      }
      break;

   case '\b':
      /* Backspace: move cursor left one, don't erase */
      if (x > 0)
      {
         s_CursorX = x - 1;
         s_CursorY = y;
      }
      break;

   default:
      pos = (y * VGATEXT_WIDTH + x) * 2;
      buf[pos] = c;
      buf[pos + 1] = (char)((s_BgIdx << 4) | attr);

      /* Advance cursor */
      s_CursorX = x + 1;
      s_CursorY = y;
      if (s_CursorX >= VGATEXT_WIDTH)
      {
         s_CursorX = 0;
         s_CursorY++;
         if (s_CursorY >= VGATEXT_HEIGHT) scroll();
      }
      break;
   }
   move_cursor(s_CursorX, s_CursorY);

   return SUCCESS;
}

int VGATEXT_PutPixel(Video_Color color, int x, int y)
{
   if (x < 0 || x >= VGATEXT_WIDTH / 2 || y < 0 || y >= VGATEXT_HEIGHT)
      return -EINVAL;

   uint8_t attr = VGA_ATTR_BOTH(vgatext_attr_from_rgb(color));

   volatile char *buf = VGATEXT_BUFFER;
   int off = (y * VGATEXT_WIDTH + x * 2) * 2;
   buf[off] = 0xDB; /* full block */
   buf[off + 1] = (char)attr;
   buf[off + 2] = 0xDB; /* full block */
   buf[off + 3] = (char)attr;
   return SUCCESS;
}

uint32_t VGATEXT_GetWidth(void) { return VGATEXT_WIDTH / 2; }

uint32_t VGATEXT_GetHeight(void) { return VGATEXT_HEIGHT; }

void VGATEXT_ClearScreen(Video_Color color)
{
   s_BgIdx = vgatext_attr_from_rgb(color);
   volatile char *buf = VGATEXT_BUFFER;

   for (int i = 0; i < VGATEXT_WIDTH * VGATEXT_HEIGHT * 2; i += 2)
   {
      buf[i] = ' ';
      buf[i + 1] = (char)(s_BgIdx << 4);
   }

   s_CursorX = 0;
   s_CursorY = 0;
   move_cursor(s_CursorX, s_CursorY);
}