// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <stdint.h>

/* Architecture page size (4 KiB) */
#define PAGE_SIZE 0x1000u

// 0x00000000 - 0x000003FF - interrupt vector table
// 0x00000400 - 0x000004FF - BIOS data area

#define MEMORY_MIN 0x00000500
#define MEMORY_MAX 0x00080000

// 0x00000500 - 0x00010500 - FAT driver
#define MEMORY_FAT_ADDR ((void *)0x20000)
#define MEMORY_FAT_SIZE 0x00010000

#define MEMORY_LOAD_KERNEL ((void *)0x30000)
#define MEMORY_LOAD_SIZE 0x00010000

// 0x00020000 - 0x00030000 - stage2

// 0x00030000 - 0x00080000 - free

// 0x00080000 - 0x0009FFFF - Extended BIOS data area
// 0x000A0000 - 0x000C7FFF - Video
// 0x000C8000 - 0x000FFFFF - BIOS

#define MEMORY_KERNEL_ADDR ((void *)0x00A00000)

// Dylib memory configuration (10 MiB reserved)
#define DYLIB_MEMORY_ADDR 0x1000000 // Base address for dylib memory pool
#define DYLIB_MEMORY_SIZE 0xA00000  // 10 MiB reserved for dylibs

// Library registry placed in low memory (inside FAT area). Stage2 populates
// this with loaded modules so the kernel can find them.
#define LIB_NAME_MAX 32
typedef struct
{
   char name[LIB_NAME_MAX];
   void *base;
   void *entry;
   uint32_t size;
} LibRecord;

#define LIB_REGISTRY_ADDR ((LibRecord *)0x00028000)
#define LIB_REGISTRY_MAX 16

#define BUFFER_LINES 2048
#define BUFFER_BASE_ADDR 0x00800000

#define SYS_INFO_ADDR 0x0087D000