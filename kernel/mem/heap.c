// SPDX-License-Identifier: AGPL-3.0-or-later

#include "heap.h"
#include "memory.h"
#include <std/stdio.h>
#include <stddef.h>
#include <stdint.h>

extern uint8_t __end; /* linker-provided end of kernel image */

/* Simple bump allocator state */
static uintptr_t heap_start = 0;
static uintptr_t heap_end = 0;
static uintptr_t heap_ptr = 0;

static uintptr_t align_up(uintptr_t v, size_t align)
{
    uintptr_t mask = (align - 1);
    return (v + mask) & ~mask;
}

void heap_init(void)
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

    void *n = kmalloc(size);
    if (!n) return NULL;
    memcpy(n, ptr, size);
    return n;
}

/* brk/sbrk -------------------------------------------------------------- */
int brk(void *addr)
{
    uintptr_t target = (uintptr_t)addr;
    if (target < heap_start || target > heap_end)
        return -1;
    heap_ptr = target;
    return 0;
}

void *sbrk(intptr_t inc)
{
    uintptr_t old = heap_ptr;
    if (inc == 0)
        return (void *)old;

    uintptr_t new_ptr = heap_ptr + inc;
    if ((inc > 0 && new_ptr < heap_ptr) || new_ptr > heap_end)
        return (void *)-1;
    if (new_ptr < heap_start)
        return (void *)-1;

    heap_ptr = new_ptr;
    return (void *)old;
}

/* Self-test ------------------------------------------------------------- */
void heap_self_test(void)
{
    printf("[heap] start=0x%08x end=0x%08x\n", (uint32_t)heap_start, (uint32_t)heap_end);

    char *p = (char *)kmalloc(32);
    if (!p) { printf("[heap] kmalloc failed\n"); return; }
    for (int i = 0; i < 32; ++i) p[i] = (char)(i + 1);

    char *q = (char *)realloc(p, 64);
    if (!q) { printf("[heap] realloc failed\n"); return; }
    int ok = 1;
    for (int i = 0; i < 32; ++i) if (q[i] != (char)(i + 1)) ok = 0;

    char *z = (char *)calloc(4, 8);
    int zeroed = 1;
    for (int i = 0; i < 32; ++i) if (z[i] != 0) { zeroed = 0; break; }

    void *brk0 = sbrk(0);
    void *brk1 = sbrk(4096);
    int brk_ok = (brk1 != (void *)-1);
    brk(brk0);

    printf("[heap] test kmalloc/realloc copy=%s, calloc zero=%s, sbrk=%s\n",
           ok ? "OK" : "FAIL",
           zeroed ? "OK" : "FAIL",
           brk_ok ? "OK" : "FAIL");
}
