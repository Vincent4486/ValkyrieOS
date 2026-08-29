// SPDX-License-Identifier: GPL-3.0-only

#include "logo_gen.h"
#include <colors.h>
#include <logging.h>

#include <dl/callback.h>
#include <dl/bindgen.h>

DL_INC
void LOGO_GetValecium(uint32_t *width, uint32_t *height,
                      const uint8_t **palette, uint32_t *palette_size,
                      const uint8_t **data)
{
   *width = VALECIUM_LOGO_W;
   *height = VALECIUM_LOGO_H;
   *palette = s_ValeciumLogo_PaletteRGB;
   *palette_size = VALECIUM_LOGO_PALETTE_SIZE;
   *data = s_ValeciumLogo_Data4bpp;
}

DL_INC
void LOGO_DrawOnScreen()
{
   uint32_t scr_w = g_DlCallbackOps->Video_GetWidth();
   uint32_t scr_h = g_DlCallbackOps->Video_GetHeight();

   uint32_t logo_w, logo_h, pal_sz;
   const uint8_t *pal, *data;
   LOGO_GetValecium(&logo_w, &logo_h, &pal, &pal_sz, &data);

   uint32_t scr_shorter = (scr_w < scr_h) ? scr_w : scr_h;
   uint32_t target_size = (scr_shorter * 70) / 100;

   uint32_t logo_shorter = (logo_w < logo_h) ? logo_w : logo_h;
   
   if (logo_shorter == 0) return;

   uint32_t draw_w = (logo_w * target_size) / logo_shorter;
   uint32_t draw_h = (logo_h * target_size) / logo_shorter;

   int off_x = (int)((scr_w > draw_w) ? (scr_w - draw_w) / 2 : 0);
   int off_y = (int)((scr_h > draw_h) ? (scr_h - draw_h) / 2 : 0);

   for (uint32_t dy = 0; dy < draw_h; dy++)
   {
      /* Map screen Y back to source logo Y */
      uint32_t y = (dy * logo_h) / draw_h;

      for (uint32_t dx = 0; dx < draw_w; dx++)
      {
         /* Map screen X back to source logo X */
         uint32_t x = (dx * logo_w) / draw_w;

         /* Data is 4bpp: two pixels per byte, high nibble first. */
         uint8_t byte = data[(y * logo_w + x) / 2];
         uint8_t idx = (x & 1) ? (byte & 0x0F) : (byte >> 4);

         Video_Color pixel_color;
         pixel_color.r = pal[idx * 3];
         pixel_color.g = pal[idx * 3 + 1];
         pixel_color.b = pal[idx * 3 + 2];

         if (!(pixel_color.r == 0 && pixel_color.g == 0 && pixel_color.b == 0))
         {
            g_DlCallbackOps->Video_PutPixel(pixel_color, off_x + (int)dx, off_y + (int)dy);
         }
      }
   }
}