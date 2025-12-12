// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Virtual Memory Manager (VMM)
 * 
 * High-level virtual memory operations.
 * Built on top of paging.c (page tables) and pmm.c (physical frames).
 */

/* Initialize VMM
 * Should be called after paging_init() and pmm_init()
 */
void vmm_init(void);

/* Allocate and map virtual memory
 * Returns virtual address on success, NULL on failure
 * Pages are allocated from PMM and mapped with given flags
 */
void *vmm_alloc(uint32_t size, uint32_t flags);

/* Free previously allocated virtual memory
 */
void vmm_free(void *vaddr, uint32_t size);

/* Map existing physical memory to virtual address
 * (used for remapping, MMIO regions, etc.)
 */
bool vmm_map(uint32_t vaddr, uint32_t paddr, uint32_t size, uint32_t flags);

/* Unmap virtual memory (does not free physical pages)
 */
bool vmm_unmap(uint32_t vaddr, uint32_t size);

/* Get physical address of a virtual address in current page directory
 */
uint32_t vmm_get_phys(uint32_t vaddr);

/* Get current process page directory (for context switch)
 */
void *vmm_get_page_directory(void);

/* Flags for mapping (use with PAGE_* from paging.h)
 */
#define VMM_RW      0x002  // Read/Write
#define VMM_USER    0x004  // User-accessible
#define VMM_DEFAULT (VMM_RW) // Kernel R/W only

/* Self-test helper
 */
void vmm_self_test(void);
