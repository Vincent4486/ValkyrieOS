// SPDX-License-Identifier: GPL-3.0-only

#include <stddef.h>
#include <stdint.h>

#include "font.h"
#include "video.h"

static inline void seq_w(uint8_t idx, uint8_t val);
static inline void crtc_w(uint8_t idx, uint8_t val);
static inline void gc_w(uint8_t idx, uint8_t val);
static void set_mode_0x13(void);
static inline void put_pixel(int x, int y, uint8_t color);
static void draw_glyph(uint8_t c, int x, int y, uint8_t fg);
static void scroll(void);
static void clear_screen(uint8_t color);
static uint8_t vga_color_from_rgb(Video_Color color);

/* VGA Mode 0x13 constants */
#define VGA_FB ((volatile uint8_t *)0xA0000)

#define VGA_WIDTH 320
#define VGA_HEIGHT 200

/* VGA I/O ports */
#define VGA_MISC_OUT 0x3C2

#define VGA_SEQ_IDX 0x3C4
#define VGA_SEQ_DATA 0x3C5

#define VGA_CRTC_IDX 0x3D4
#define VGA_CRTC_DATA 0x3D5

#define VGA_GC_IDX 0x3CE
#define VGA_GC_DATA 0x3CF

#define VGA_AC_IDX 0x3C0
#define VGA_INSTAT_1 0x3DA

#define VGA_DAC_WRITE_IDX 0x3C8
#define VGA_DAC_DATA 0x3C9

static uint8_t s_Shadow[VGA_WIDTH * VGA_HEIGHT];
static uint8_t s_VgaPal256[256][3];

static int s_Initialized = 0;
static int s_CursorX = 0;
static int s_CursorY = 0;


static void init_palette(void)
{
   static const uint8_t dac16[16][3] = {
      {0, 0, 0},     /* 0  black        */
      {0, 0, 42},    /* 1  blue         */
      {0, 42, 0},    /* 2  green        */
      {0, 42, 42},   /* 3  cyan         */
      {42, 0, 0},    /* 4  red          */
      {42, 0, 42},   /* 5  magenta      */
      {42, 21, 0},   /* 6  brown        */
      {42, 42, 42},  /* 7  light grey   */
      {21, 21, 21},  /* 8  dark grey    */
      {21, 21, 63},  /* 9  light blue   */
      {21, 63, 21},  /* 10 light green  */
      {21, 63, 63},  /* 11 light cyan   */
      {63, 21, 21},  /* 12 light red    */
      {63, 21, 63},  /* 13 light magenta*/
      {63, 63, 21},  /* 14 yellow       */
      {63, 63, 63},  /* 15 white        */
   };

   int idx = 0;

   /* Entries 0-15: standard VGA colours */
   for (int i = 0; i < 16; i++)
   {
      for (int c = 0; c < 3; c++)
         s_VgaPal256[idx][c] = dac16[i][c];
      idx++;
   }

   /* Entries 16-231: 6x6x6 colour cube */
   for (int r = 0; r < 6; r++)
      for (int g = 0; g < 6; g++)
         for (int b = 0; b < 6; b++)
         {
            s_VgaPal256[idx][0] = r * 63u / 5;
            s_VgaPal256[idx][1] = g * 63u / 5;
            s_VgaPal256[idx][2] = b * 63u / 5;
            idx++;
         }

   /* Entries 232-255: 24-step grey ramp */
   for (int i = 0; i < 24; i++)
   {
      uint8_t grey = i * 63u / 23;
      s_VgaPal256[idx][0] = grey;
      s_VgaPal256[idx][1] = grey;
      s_VgaPal256[idx][2] = grey;
      idx++;
   }

   /* Upload to VGA DAC – index auto-increments after each write */
   outb(VGA_DAC_WRITE_IDX, 0);
   for (int i = 0; i < 256; i++)
      for (int c = 0; c < 3; c++)
         outb(VGA_DAC_DATA, s_VgaPal256[i][c]);
}

static inline void seq_w(uint8_t idx, uint8_t val)
{
   outb(VGA_SEQ_IDX, idx);
   outb(VGA_SEQ_DATA, val);
}

static inline void crtc_w(uint8_t idx, uint8_t val)
{
   outb(VGA_CRTC_IDX, idx);
   outb(VGA_CRTC_DATA, val);
}

static inline void gc_w(uint8_t idx, uint8_t val)
{
   outb(VGA_GC_IDX, idx);
   outb(VGA_GC_DATA, val);
}

/* Proper VGA Mode 13h setup */
static void set_mode_0x13(void)
{
   static const uint8_t misc = 0x63;

   /* Standard VGA mode 13h CRTC values for 320x200x256 */
   static const uint8_t crtc[] = {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF,
                                  0x1F, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x9C, 0x8E, 0x8F, 0x28, 0x40,
                                  0x96, 0xB9, 0xA3, 0xFF};

   static const uint8_t gc[] = {0x00, 0x00, 0x00, 0x00, 0x00,
                                0x40, 0x05, 0x0F, 0xFF};

   static const uint8_t ac[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14,
                                0x07, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D,
                                0x3E, 0x3F, 0x41, 0x00, 0x0F, 0x00, 0x00};

   outb(VGA_MISC_OUT, misc);

   /* Sequencer */
   for (uint8_t i = 0; i < 5; i++)
      seq_w(i, 0x03);
   seq_w(0x01, 0x01);
   seq_w(0x02, 0x0F);
   seq_w(0x03, 0x00);
   seq_w(0x04, 0x0E);

   /* Unlock CRTC */
   crtc_w(0x11, crtc[0x11] & ~0x80);

   for (uint8_t i = 0; i < 25; i++)
      crtc_w(i, crtc[i]);

   for (uint8_t i = 0; i < 9; i++)
      gc_w(i, gc[i]);

   for (uint8_t i = 0; i < 21; i++)
   {
      inb(VGA_INSTAT_1);
      outb(VGA_AC_IDX, i);
      outb(VGA_AC_IDX, ac[i]);
   }

   inb(VGA_INSTAT_1);
   outb(VGA_AC_IDX, 0x20);
}

/* Pixel operations */
static inline void put_pixel(int x, int y, uint8_t color)
{
   if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;

   int idx = y * VGA_WIDTH + x;

   s_Shadow[idx] = color;
   VGA_FB[idx] = color;
}

/* Glyph drawing */
static void draw_glyph(uint8_t c, int x, int y, uint8_t fg)
{
   if (c < FONT_FIRST || c > FONT_LAST) c = '?';

   const uint8_t *glyph = g_Font8x16[c - FONT_FIRST];

   for (int row = 0; row < FONT_HEIGHT; row++)
   {
      uint8_t bits = glyph[row];

      for (int col = 0; col < FONT_WIDTH; col++)
      {
         if (bits & (0x80 >> col)) put_pixel(x + col, y + row, fg);
      }
   }
}

/* Scroll the pixel buffer up by one character row (FONT_HEIGHT scanlines). */
static void scroll(void)
{
   volatile uint32_t *fb32 = (volatile uint32_t *)VGA_FB;
   int pixels_per_row = VGA_WIDTH;
   int pixels_per_char_row = pixels_per_row * FONT_HEIGHT;
   int total_pixels = pixels_per_row * VGA_HEIGHT;
   int total_u32 = total_pixels / 4;
   int words_per_char_row = pixels_per_char_row / 4;

   for (int i = 0; i < total_pixels - pixels_per_char_row; i++)
      s_Shadow[i] = s_Shadow[i + pixels_per_char_row];

   for (int i = total_pixels - pixels_per_char_row; i < total_pixels; i++)
      s_Shadow[i] = 0;

   for (int i = 0; i < total_u32 - words_per_char_row; i++)
      fb32[i] = fb32[i + words_per_char_row];

   for (int i = total_u32 - words_per_char_row; i < total_u32; i++)
      fb32[i] = 0;
}

static uint8_t vga_color_from_rgb(Video_Color c)
{
   uint8_t r6 = c.r >> 2;
   uint8_t g6 = c.g >> 2;
   uint8_t b6 = c.b >> 2;

   uint8_t best = 0;
   int32_t best_dist = INT32_MAX;
   int32_t dr, dg, db, d;

   for (uint16_t i = 0; i < 256; i++)
   {
      dr = (int32_t)r6 - (int32_t)s_VgaPal256[i][0];
      dg = (int32_t)g6 - (int32_t)s_VgaPal256[i][1];
      db = (int32_t)b6 - (int32_t)s_VgaPal256[i][2];
      /* Luminance-weighted distance: human eye is most sensitive to green. */
      d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
      if (d < best_dist)
      {
         best_dist = d;
         best = i;
      }
   }
   return best;
}

/* Clear screen */
static void clear_screen(uint8_t color)
{
   uint32_t val = (uint32_t)color * 0x01010101u;
   volatile uint32_t *fb32 = (volatile uint32_t *)VGA_FB;

   for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
      s_Shadow[i] = color;

   /* Write to VGA framebuffer using 32-bit accesses for reliability
    * with Cirrus chain-4 mode.  dword writes send 4 bytes at once,
    * one to each VGA plane, ensuring the full 64KB is covered. */
   for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT / 4; i++)
      fb32[i] = val;
}

int VGA_Initialize(void)
{
   set_mode_0x13();
   init_palette();
   clear_screen(0);

   s_CursorX = 0;
   s_CursorY = 0;
   s_Initialized = 1;

   return SUCCESS;
}

int VGA_PutChar(char c, int x, int y, Video_Color color)
{
   uint8_t pal_idx = vga_color_from_rgb(color);

   if (!s_Initialized) return -ENODEV;

   if ((x < 0 && y < 0) || y >= VGA_HEIGHT ||
       (s_CursorY >= (VGA_HEIGHT - FONT_HEIGHT) && y == 0))
   {
      x = s_CursorX;
      y = s_CursorY;
   }
   else if ((x < 0) != (y < 0))
   {
      return -EINVAL;
   }

   /* Control characters */
   switch (c)
   {
   case '\n':
      x = 0;
      y += FONT_HEIGHT;
      break;

   case '\r':
      x = 0;
      break;

   case '\t':
      x = (x / (FONT_WIDTH * 4) + 1) * (FONT_WIDTH * 4);
      if (x >= VGA_WIDTH)
      {
         x = 0;
         y += FONT_HEIGHT;
      }
      break;

   default:
      if (x + FONT_WIDTH > VGA_WIDTH)
      {
         x = 0;
         y += FONT_HEIGHT;
      }
      break;
   }

   /* Scroll up by one character row when the cursor goes past the bottom. */
   if (y + FONT_HEIGHT > VGA_HEIGHT)
   {
      scroll();
      y -= FONT_HEIGHT;
   }

   /* Draw glyph */
   if (c != '\n' && c != '\r' && c != '\t')
   {
      draw_glyph((uint8_t)c, x, y, pal_idx);
      x += FONT_WIDTH;
   }

   /* Always preserve coordinates internally */
   s_CursorX = x;
   s_CursorY = y;

   return SUCCESS;
}

int VGA_PutPixel(Video_Color color, int x, int y)
{
   if (!s_Initialized) return -ENODEV;

   if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return -EINVAL;

   put_pixel(x, y, vga_color_from_rgb(color));
   return SUCCESS;
}

uint32_t VGA_GetWidth(void) { return VGA_WIDTH; }

uint32_t VGA_GetHeight(void) { return VGA_HEIGHT; }

void VGA_ClearScreen(Video_Color color)
{
   clear_screen(vga_color_from_rgb(color));
   s_CursorX = 0;
   s_CursorY = 0;
}