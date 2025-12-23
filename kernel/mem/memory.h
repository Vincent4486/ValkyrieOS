// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>

/* Memory management information */
typedef struct {
    uint32_t total_memory;       /* Total physical memory in bytes */
    uint32_t available_memory;   /* Available physical memory in bytes */
    uint32_t used_memory;        /* Used physical memory in bytes */
    uint32_t heap_start;         /* Kernel heap start address */
    uint32_t heap_end;           /* Kernel heap end address */
    uint32_t heap_size;          /* Total heap size in bytes */
    uint32_t page_size;          /* Memory page size (typically 4096) */
    uint32_t kernel_start;       /* Kernel memory start */
    uint32_t kernel_end;         /* Kernel memory end */
    uint32_t user_start;         /* User space start */
    uint32_t user_end;           /* User space end */
    uint32_t kernel_stack_size;  /* Kernel stack size in bytes */
} __attribute__((packed)) MEM_Info;

/* Basic memory helpers (sizes in bytes) */
void *memcpy(void *dst, const void *src, size_t num);
void *memset(void *ptr, int value, size_t num);
int memcmp(const void *ptr1, const void *ptr2, size_t num);
void *memmove(void *dest, const void *src, size_t n);

void *SegmentOffsetToLinear(void *addr);

void MEM_Initialize();