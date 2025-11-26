// SPDX-License-Identifier: AGPL-3.0-or-later

#include "malloc.h"
#include "memory.h"
#include <std/string.h>
#include <stdint.h>

extern uint8_t __end; /* linker-provided end of kernel image */

/* Simple bump allocator state (kept private to this translation unit) */
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

uintptr_t mem_heap_start(void) { return heap_start; }
uintptr_t mem_heap_end(void) { return heap_end; }

/* libc-like wrappers ---------------------------------------------------- */
void *malloc(size_t size) { return kmalloc(size); }

void free(void *ptr)
{
   /* No-op: bump allocator does not reclaim memory. */
   (void)ptr;
}

void *calloc(size_t nmemb, size_t size)
{
   size_t total = nmemb * size;
   return kzalloc(total);
}

void *realloc(void *ptr, size_t size)
{
   if (!ptr) return kmalloc(size);
   if (size == 0)
   {
      free(ptr);
      return NULL;
   }

   /* Allocate new block and copy old contents. We can't know old size, so
    * this is a best-effort: copy 'size' bytes (may read past original if
    * the original allocation was smaller). Users should avoid realloc in
    * this simple allocator or track sizes themselves.
    */
   void *n = kmalloc(size);
   if (!n) return NULL;
   memcpy(n, ptr, size);
   return n;
}
