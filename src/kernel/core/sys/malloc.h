// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

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
