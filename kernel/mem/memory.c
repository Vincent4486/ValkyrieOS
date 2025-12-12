// SPDX-License-Identifier: AGPL-3.0-or-later

#include "memory.h"
#include <arch/i686/io/io.h>
#include <std/string.h>
#include <stddef.h>
#include <stdint.h>

/* Runtime-controlled memory debug flag. Set non-zero to make the handler
 * call `i686_Panic()` when a memory safety fault is detected. Default is 0.
 */
int memory_debug = 0;

/* Called from assembly on memory faults (overflow/null/unsafe).
 * Parameters (cdecl): void *addr, size_t len, int code
 * code: 1=memcpy fault, 2=memcmp fault, 3=memset fault
 */
void mem_fault_handler(void *addr, size_t len, int code)
{
   (void)addr;
   (void)len;
   (void)code;
   if (memory_debug)
   {
      i686_Panic();
   }
   /* Otherwise return and let caller continue (safe no-op behavior). */
}

/* Basic memory helpers */
/* We implement the hot paths `memcpy` and `memcmp` in assembly for
 * performance. The assembly provides `memcpy_asm` and `memcmp_asm`;
 * here we provide small C wrappers that forward to those symbols.
 */
extern void *memcpy_asm(void *dst, const void *src, size_t num);
void *memcpy(void *dst, const void *src, size_t num)
{
   return memcpy_asm(dst, src, num);
}

extern void *memset_asm(void *ptr, int value, size_t num);
void *memset(void *ptr, int value, size_t num)
{
   return memset_asm(ptr, value, num);
}

extern int memcmp_asm(const void *ptr1, const void *ptr2, size_t num);
int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
   return memcmp_asm(ptr1, ptr2, num);
}

void *SegmentOffsetToLinear(void *addr)
{
   uint32_t offset = (uint32_t)(addr) & 0xffff;
   uint32_t segment = (uint32_t)(addr) >> 16;
   return (void *)(segment * 16 + offset);
}

void *memmove(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;

    if (d == s || n == 0) {
        return dest; // No copy needed if same or zero bytes
    }

    if (d < s) {
        // Destination is before source, copy forwards
        for (size_t i = 0; i < n; ++i) {
            d[i] = s[i];
        }
    } else {
        // Destination is after source, or overlaps in a way that requires
        // copying backwards to avoid overwriting source data before it's read.
        for (size_t i = n; i > 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}