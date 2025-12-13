// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Virtual Memory Manager (VMM)
 *
 * High-level virtual memory operations.
 * Built on top of paging.c (page tables) and pmm.c (physical frames).
 */

/* Initialize VMM
 * Should be called after Paging_Initialize() and PMM_Initialize()
 */
void VMM_Initialize(void);

/* Allocate and map virtual memory
 * Returns virtual address on success, NULL on failure
 * Pages are allocated from PMM and mapped with given flags
 */
void *VMM_Allocate(uint32_t size, uint32_t flags);

/* Free previously allocated virtual memory
 */
void VMM_Free(void *vaddr, uint32_t size);

/* Map existing physical memory to virtual address
 * (used for remapping, MMIO regions, etc.)
 */
bool VMM_Map(uint32_t vaddr, uint32_t paddr, uint32_t size, uint32_t flags);

/* Unmap virtual memory (does not free physical pages)
 */
bool VMM_Unmap(uint32_t vaddr, uint32_t size);

/* Get physical address of a virtual address in current page directory
 */
uint32_t VMM_GetPhys(uint32_t vaddr);

/* Get current process page directory (for context switch)
 */
void *VMM_GetPageDirectory(void);

/* Flags for mapping (use with PAGE_* from paging.h)
 */
#define VMM_RW 0x002         // Read/Write
#define VMM_USER 0x004       // User-accessible
#define VMM_DEFAULT (VMM_RW) // Kernel R/W only

/* Self-test helper
 */
void vmm_self_test(void);
