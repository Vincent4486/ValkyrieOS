// SPDX-License-Identifier: AGPL-3.0-or-later

#include "memory.h"
#include <stdint.h>

extern uint8_t __end; /* linker-provided end of kernel image */

/* Basic memory helpers */
void *memcpy(void *dst, const void *src, size_t num)
{
   uint8_t *u8Dst = (uint8_t *)dst;
   const uint8_t *u8Src = (const uint8_t *)src;

   for (size_t i = 0; i < num; i++) u8Dst[i] = u8Src[i];

   return dst;
}

void *memset(void *ptr, int value, size_t num)
{
   uint8_t *u8Ptr = (uint8_t *)ptr;

   for (size_t i = 0; i < num; i++) u8Ptr[i] = (uint8_t)value;

   return ptr;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
   const uint8_t *u8Ptr1 = (const uint8_t *)ptr1;
   const uint8_t *u8Ptr2 = (const uint8_t *)ptr2;

   for (size_t i = 0; i < num; i++)
      if (u8Ptr1[i] != u8Ptr2[i]) return (int)u8Ptr1[i] - (int)u8Ptr2[i];

   return 0;
}

/* Simple bump allocator */
static uintptr_t heap_start = 0;
static uintptr_t heap_end = 0;
static uintptr_t heap_ptr = 0;

/* Align 'v' up to 'align' (align must be power of two) */
static uintptr_t align_up(uintptr_t v, size_t align)
{
   uintptr_t mask = (align - 1);
   return (v + mask) & ~mask;
}

void mem_init(void)
{
   /* place heap just after the kernel image end symbol */
   heap_start = align_up((uintptr_t)&__end, 8);

   /* cap heap to 2 GiB from heap_start (but do not exceed 32-bit max) */
   const uintptr_t two_gib = (uintptr_t)2 * 1024 * 1024 * 1024u;
   uintptr_t desired_end = heap_start + (two_gib - 1);
   if (desired_end < heap_start || desired_end > (uintptr_t)0xFFFFFFFFu)
      heap_end = (uintptr_t)0xFFFFFFFFu;
   else
      heap_end = desired_end;

   heap_ptr = heap_start;
}

void *kmalloc(size_t size)
{
   if (size == 0) return NULL;

   uintptr_t cur = align_up(heap_ptr, 8);
   if (cur > heap_end) return NULL; /* heap already exhausted */

   /* available bytes from cur to heap_end (inclusive) */
   uintptr_t avail = (heap_end - cur) + 1;
   if (size > avail) return NULL; /* not enough room */

   uintptr_t next = cur + size;
   heap_ptr = next;
   return (void *)cur;
}

void *kzalloc(size_t size)
{
   void *p = kmalloc(size);
   if (!p) return NULL;
   memset(p, 0, size);
   return p;
}

/* Heap introspection */
uintptr_t mem_heap_start(void) { return heap_start; }
uintptr_t mem_heap_end(void) { return heap_end; }
