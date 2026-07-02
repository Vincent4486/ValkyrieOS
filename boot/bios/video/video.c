// SPDX-License-Identifier: GPL-3.0-only
//
// Generic driver abstraction — dispatches to the active output driver
// based on g_PreferredOutput.

#include "video.h"

int Video_Initialize(void)
{
   switch (g_PreferredOutput)
   {
   case OUTPUT_VBE:
      return VBE_Initialize();
   case OUTPUT_VGA:
      return VGA_Initialize();
   case OUTPUT_VGATEXT:
      return VGATEXT_Initialize();
   case OUTPUT_UART:
      return UART_Initialize();
   default:
      return -EINVAL;
   }
}

int Video_PutChar(char c, int x, int y, Video_Color color)
{
   switch (g_PreferredOutput)
   {
   case OUTPUT_VBE:
      return VBE_PutChar(c, x, y, color);
   case OUTPUT_VGA:
      return VGA_PutChar(c, x, y, color);
   case OUTPUT_VGATEXT:
      return VGATEXT_PutChar(c, x, y, color);
   case OUTPUT_UART:
      return UART_PutChar(c, x, y, color);
   default:
      return -EINVAL;
   }
}

int Video_PutPixel(Video_Color color, int x, int y)
{
   switch (g_PreferredOutput)
   {
   case OUTPUT_VBE:
      return VBE_PutPixel(color, x, y);
   case OUTPUT_VGA:
      return VGA_PutPixel(color, x, y);
   case OUTPUT_VGATEXT:
      return VGATEXT_PutPixel(color, x, y);
   case OUTPUT_UART:
      return UART_PutPixel(color, x, y);
   default:
      return -EINVAL;
   }
}

uint32_t Video_GetWidth(void)
{
   switch (g_PreferredOutput)
   {
   case OUTPUT_VBE:
      return VBE_GetWidth();
   case OUTPUT_VGA:
      return VGA_GetWidth();
   case OUTPUT_VGATEXT:
      return VGATEXT_GetWidth();
   case OUTPUT_UART:
      return UART_GetWidth();
   default:
      return 0;
   }
}

uint32_t Video_GetHeight(void)
{
   switch (g_PreferredOutput)
   {
   case OUTPUT_VBE:
      return VBE_GetHeight();
   case OUTPUT_VGA:
      return VGA_GetHeight();
   case OUTPUT_VGATEXT:
      return VGATEXT_GetHeight();
   case OUTPUT_UART:
      return UART_GetHeight();
   default:
      return 0;
   }
}

void Video_ClearScreen(Video_Color color)
{
   switch (g_PreferredOutput)
   {
   case OUTPUT_VBE:
      VBE_ClearScreen(color);
      break;
   case OUTPUT_VGA:
      VGA_ClearScreen(color);
      break;
   case OUTPUT_VGATEXT:
      VGATEXT_ClearScreen(color);
      break;
   case OUTPUT_UART:
      UART_ClearScreen(color);
      break;
   }
}
