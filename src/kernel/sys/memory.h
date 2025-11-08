// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>

/* Basic memory helpers (sizes in bytes) */
void *memcpy(void *dst, const void *src, size_t num);
void *memset(void *ptr, int value, size_t num);
int memcmp(const void *ptr1, const void *ptr2, size_t num);

/* Very small bump allocator for early kernel use. */
void mem_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);

/* Heap introspection (for testing) */
uintptr_t mem_heap_start(void);
uintptr_t mem_heap_end(void);

void *SegmentOffsetToLinear(void *addr);
