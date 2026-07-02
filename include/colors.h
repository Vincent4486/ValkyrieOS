// SPDX-License-Identifier: GPL-3.0-only
#pragma once

typedef struct Video_Color Video_Color;

#define BOOT_BG_COLOR ((Video_Color){0x23, 0x12, 0x90})

struct Video_Color
{
   uint8_t r;
   uint8_t g;
   uint8_t b;
};