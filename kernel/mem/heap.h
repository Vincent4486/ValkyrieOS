// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <cpu/process.h>
#include <stddef.h>
#include <stdint.h>

/* Per-process heap management */
int Heap_ProcessInitialize(Process *proc, uint32_t heap_start_va);
int Heap_ProcessBrk(Process *proc, void *addr);
void *Heap_ProcessSbrk(Process *proc, intptr_t inc);

/* Heap / allocator initialization */
void Heap_Initialize(void);

/* Core allocators */
void *kmalloc(size_t size);
void *kzalloc(size_t size);
uintptr_t mem_heap_start(void);
uintptr_t mem_heap_end(void);

/* libc-like API backed by the kernel allocator */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

/* brk/sbrk style heap control */
int brk(void *addr);      /* returns 0 on success, -1 on failure */
void *sbrk(intptr_t inc); /* returns previous break or (void*)-1 on failure */

/* Self-test helper */
void heap_self_test(void);
