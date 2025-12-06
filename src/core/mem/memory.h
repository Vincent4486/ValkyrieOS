// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>

/* Basic memory helpers (sizes in bytes) */
void *memcpy(void *dst, const void *src, size_t num);
void *memset(void *ptr, int value, size_t num);
int memcmp(const void *ptr1, const void *ptr2, size_t num);
void *memmove(void *dest, const void *src, size_t n);

/* Very small bump allocator for early kernel use. */
void mem_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);

/* Heap introspection (for testing) */
uintptr_t mem_heap_start(void);
uintptr_t mem_heap_end(void);

void *SegmentOffsetToLinear(void *addr);

/* Kernel bump allocator API (kmalloc family) */
void mem_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
uintptr_t mem_heap_start(void);
uintptr_t mem_heap_end(void);

/* libc-like allocation API backed by the kernel allocator.
 * Notes:
 * - `malloc`/`calloc`/`realloc` allocate from the same bump allocator as
 *   `kmalloc`/`kzalloc`.
 * - `free` is a no-op because the bump allocator does not support freeing.
 * - `realloc` performs a new allocation and copies existing data when
 *   possible; it cannot shrink the bump allocator or reclaim memory.
 */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
