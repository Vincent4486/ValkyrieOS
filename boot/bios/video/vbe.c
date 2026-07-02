// SPDX-License-Identifier: GPL-3.0-only

#include <stddef.h>

#include "font.h"
#include "video.h"

static inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b);
static void clear_screen(uint32_t pixel);
static void draw_glyph(uint8_t c, int x, int y, uint32_t fg);
static uint32_t vga_color_to_rgb(uint8_t idx);

static int s_Initialized = 0;
static int s_HasInfo = 0;
static int s_CursorX = 0;
static int s_CursorY = 0;
static int s_TextScale = 2;
static VBE_Info s_Info = {0};

/* Cached framebuffer parameters for fast inlined writes. */
static uint8_t *s_Fb = NULL;
static uint32_t s_FbW = 0, s_FbH = 0;
static uint32_t s_FbPitch = 0;
static uint32_t s_FbBpp = 0;
static uint32_t s_FbBytesPP = 0;

/* --- Fast pixel helpers (no bounds checks — caller ensures validity) --- */

static inline void fb_write32(uint32_t *dst, uint32_t pixel) { *dst = pixel; }

static inline void fb_write24(uint8_t *dst, uint32_t pixel)
{
   dst[0] = (uint8_t)(pixel & 0xFF);
   dst[1] = (uint8_t)((pixel >> 8) & 0xFF);
   dst[2] = (uint8_t)((pixel >> 16) & 0xFF);
}

static inline void fb_write16(uint16_t *dst, uint32_t pixel)
{
   *dst = (uint16_t)(pixel & 0xFFFF);
}

static inline void fb_write8(uint8_t *dst, uint32_t pixel)
{
   *dst = (uint8_t)(pixel & 0xFF);
}

/* Write one pixel at absolute (row, col) — no clipping. */
static inline void fb_put_pixel(uint32_t x, uint32_t y, uint32_t pixel)
{
   uint8_t *row = s_Fb + y * s_FbPitch;

   switch (s_FbBytesPP)
   {
   case 4:
      fb_write32((uint32_t *)(row + x * 4), pixel);
      break;
   case 3:
      fb_write24(row + x * 3, pixel);
      break;
   case 2:
      fb_write16((uint16_t *)(row + x * 2), pixel);
      break;
   default:
      fb_write8(row + x, pixel);
      break;
   }
}

/* Fill a horizontal span [x0, x1) on row y with pixel. */
static inline void fb_fill_span(uint32_t x0, uint32_t x1, uint32_t y,
                                uint32_t pixel)
{
   uint8_t *row = s_Fb + y * s_FbPitch;
   uint32_t i;

   switch (s_FbBytesPP)
   {
   case 4: {
      uint32_t *p = (uint32_t *)(row + x0 * 4);
      for (i = x0; i < x1; i++)
         *p++ = pixel;
      break;
   }
   case 3: {
      uint8_t *p = row + x0 * 3;
      for (i = x0; i < x1; i++, p += 3)
         fb_write24(p, pixel);
      break;
   }
   case 2: {
      uint16_t *p = (uint16_t *)(row + x0 * 2);
      for (i = x0; i < x1; i++)
         *p++ = (uint16_t)pixel;
      break;
   }
   default: {
      uint8_t *p = row + x0;
      for (i = x0; i < x1; i++)
         *p++ = (uint8_t)pixel;
      break;
   }
   }
}

static inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b)
{
   uint32_t rv = 0, gv = 0, bv = 0;
   uint32_t rm = (1u << s_Info.red_mask_size) - 1;
   uint32_t gm = (1u << s_Info.green_mask_size) - 1;
   uint32_t bm = (1u << s_Info.blue_mask_size) - 1;

   if (s_Info.red_mask_size) rv = ((uint32_t)r * rm) / 255u;
   if (s_Info.green_mask_size) gv = ((uint32_t)g * gm) / 255u;
   if (s_Info.blue_mask_size) bv = ((uint32_t)b * bm) / 255u;

   return (rv << s_Info.red_field_position) |
          (gv << s_Info.green_field_position) |
          (bv << s_Info.blue_field_position);
}

/* --- Bulk framebuffer ops --- */

static void clear_screen(uint32_t pixel)
{
   uint32_t y;

   /* 32bpp fast path: fill the whole framebuffer with 32-bit stores. */
   if (s_FbBytesPP == 4)
   {
      uint32_t *fb32 = (uint32_t *)s_Fb;
      uint32_t total = (s_FbPitch / 4) * s_FbH;
      for (uint32_t i = 0; i < total; i++)
         fb32[i] = pixel;
      return;
   }

   for (y = 0; y < s_FbH; y++)
      fb_fill_span(0, s_FbW, y, pixel);
}

static void draw_glyph(uint8_t c, int x, int y, uint32_t fg)
{
   const uint8_t *glyph;
   int row, col;
   int scale = s_TextScale > 0 ? s_TextScale : 1;
   int gw = FONT_WIDTH, gh = FONT_HEIGHT;

   if (c < FONT_FIRST || c > FONT_LAST) c = '?';
   glyph = g_Font8x16[c - FONT_FIRST];

   for (row = 0; row < gh; row++)
   {
      uint8_t bits = glyph[row];
      if (!bits) continue; /* skip empty rows */

      for (col = 0; col < gw; col++)
      {
         if (!(bits & (0x80 >> col))) continue;

         int px = x + col * scale;
         int py = y + row * scale;

         /* Fast path for scale=2: write a 2x2 block with span fills. */
         if (scale == 2 && s_FbBytesPP == 4)
         {
            uint32_t *r0 =
                (uint32_t *)(s_Fb + (uint32_t)py * s_FbPitch) + (uint32_t)px;
            uint32_t *r1 = (uint32_t *)(s_Fb + (uint32_t)(py + 1) * s_FbPitch) +
                           (uint32_t)px;
            r0[0] = fg;
            r0[1] = fg;
            r1[0] = fg;
            r1[1] = fg;
         }
         else
         {
            for (int dy = 0; dy < scale; dy++)
               fb_fill_span((uint32_t)px, (uint32_t)(px + scale),
                            (uint32_t)(py + dy), fg);
         }
      }
   }
}

/* Map a VGA 4-bit colour index (0-15) to an RGB pixel. */
static uint32_t vga_color_to_rgb(uint8_t idx)
{
   static const uint8_t vga_pal[16][3] = {
      {  0,   0,   0}, /* 0  black        */
      {  0,   0, 170}, /* 1  blue         */
      {  0, 170,   0}, /* 2  green        */
      {  0, 170, 170}, /* 3  cyan         */
      {170,   0,   0}, /* 4  red          */
      {170,   0, 170}, /* 5  magenta      */
      {170,  85,   0}, /* 6  brown        */
      {170, 170, 170}, /* 7  light grey   */
      { 85,  85,  85}, /* 8  dark grey    */
      { 85,  85, 255}, /* 9  light blue   */
      { 85, 255,  85}, /* 10 light green  */
      { 85, 255, 255}, /* 11 light cyan   */
      {255,  85,  85}, /* 12 light red    */
      {255,  85, 255}, /* 13 light magenta*/
      {255, 255,  85}, /* 14 yellow       */
      {255, 255, 255}, /* 15 white        */
   };
   return pack_rgb(vga_pal[idx & 0x0F][0],
                   vga_pal[idx & 0x0F][1],
                   vga_pal[idx & 0x0F][2]);
}

/* --- Public API --- */

void VBE_SetInfo(const VBE_Info *info)
{
   if (!info) return;
   s_Info = *info;
   s_HasInfo = 1;
}

int VBE_HasInfo(void) { return s_HasInfo; }

int VBE_Initialize(void)
{
   if (!s_HasInfo) return -ENODEV;

   s_CursorX = 0;
   s_CursorY = 0;
   s_Initialized = 1;

   /* Cache framebuffer parameters. */
   s_Fb = (uint8_t *)(uintptr_t)s_Info.framebuffer_addr;
   s_FbW = s_Info.width;
   s_FbH = s_Info.height;
   s_FbPitch = s_Info.pitch;
   s_FbBpp = s_Info.bpp;
   s_FbBytesPP = (s_Info.bpp + 7u) / 8u;

   clear_screen(0);
   return SUCCESS;
}

int VBE_PutChar(char c, int x, int y, char color)
{
   int scale = s_TextScale > 0 ? s_TextScale : 1;
   int glyph_w = FONT_WIDTH * scale;
   int glyph_h = FONT_HEIGHT * scale;

   if (!s_Initialized) return -ENODEV;

   if (x < 0 && y < 0)
   {
      x = s_CursorX;
      y = s_CursorY;
   }
   else if ((x < 0) != (y < 0))
      return -EINVAL;

   switch (c)
   {
   case '\n':
      s_CursorX = 0;
      s_CursorY += glyph_h;
      break;
   case '\r':
      s_CursorX = 0;
      break;
   case '\t':
      s_CursorX = (s_CursorX / (glyph_w * 4) + 1) * (glyph_w * 4);
      break;
   default:
      draw_glyph((uint8_t)c, x, y, vga_color_to_rgb((uint8_t)color));
      s_CursorX = x + glyph_w;
      s_CursorY = y;
      break;
   }

   if ((uint32_t)(s_CursorY + glyph_h) > s_FbH)
   {
      uint32_t scroll_rows = (uint32_t)glyph_h;
      uint32_t row_bytes = s_FbPitch;
      uint32_t copy_bytes = (s_FbH - scroll_rows) * row_bytes;
      uint32_t clear_off = copy_bytes;
      uint32_t clear_bytes = scroll_rows * row_bytes;

      /* memmove-down using word-sized copies. */
      {
         uint32_t *dst = (uint32_t *)s_Fb;
         uint32_t *src = (uint32_t *)(s_Fb + scroll_rows * row_bytes);
         uint32_t words = copy_bytes / 4;
         for (uint32_t i = 0; i < words; i++)
            dst[i] = src[i];
      }

      /* Zero out the cleared-in bottom region. */
      {
         uint32_t *dst = (uint32_t *)(s_Fb + clear_off);
         uint32_t words = clear_bytes / 4;
         for (uint32_t i = 0; i < words; i++)
            dst[i] = 0;
      }

      s_CursorX = 0;
      s_CursorY = (int)(s_FbH - scroll_rows);
   }

   return SUCCESS;
}

int VBE_PutPixel(uint32_t color, int x, int y)
{
   if (!s_Initialized) return -ENODEV;
   if (x < 0 || y < 0 || (uint32_t)x >= s_FbW || (uint32_t)y >= s_FbH)
      return -EINVAL;

   uint8_t r = (uint8_t)((color >> 16) & 0xFF);
   uint8_t g = (uint8_t)((color >> 8) & 0xFF);
   uint8_t b = (uint8_t)(color & 0xFF);
   fb_put_pixel((uint32_t)x, (uint32_t)y, pack_rgb(r, g, b));
   return SUCCESS;
}

uint32_t VBE_GetWidth(void) { return s_HasInfo ? s_FbW : 0; }
uint32_t VBE_GetHeight(void) { return s_HasInfo ? s_FbH : 0; }

void VBE_ClearScreen(uint32_t color)
{
   uint8_t r = (uint8_t)((color >> 16) & 0xFF);
   uint8_t g = (uint8_t)((color >> 8) & 0xFF);
   uint8_t b = (uint8_t)(color & 0xFF);
   clear_screen(pack_rgb(r, g, b));
   s_CursorX = 0;
   s_CursorY = 0;
}