// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef VMM_H
#define VMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Virtual Memory Manager (VMM)
 *
 * High-level virtual memory operations.
 * Built on top of paging.c (page tables) and pmm.c (physical frames).
 */

/* Initialize VMM
 * Should be called after i686_Paging_Initialize() and PMM_Initialize()
 */
void VMM_Initialize(void);

/* Allocate and map virtual memory in a page directory.
 * If next_vaddr_state is NULL, uses the kernel allocator bump pointer.
 */
void *VMM_AllocateInDir(void *page_dir, uint32_t *next_vaddr_state,
					   uint32_t size, uint32_t flags);

/* Kernel convenience wrapper */
void *VMM_Allocate(uint32_t size, uint32_t flags);

/* Free previously allocated virtual memory */
void VMM_FreeInDir(void *page_dir, void *vaddr, uint32_t size);
void VMM_Free(void *vaddr, uint32_t size);

/* Map existing physical memory */
bool VMM_MapInDir(void *page_dir, uint32_t vaddr, uint32_t paddr,
				  uint32_t size, uint32_t flags);
bool VMM_Map(uint32_t vaddr, uint32_t paddr, uint32_t size, uint32_t flags);

/* Unmap virtual memory (does not free physical pages) */
bool VMM_UnmapInDir(void *page_dir, uint32_t vaddr, uint32_t size);
bool VMM_Unmap(uint32_t vaddr, uint32_t size);

/* Get physical address of a virtual address */
uint32_t VMM_GetPhysInDir(void *page_dir, uint32_t vaddr);
uint32_t VMM_GetPhys(uint32_t vaddr);

/* Get current process page directory (for context switch) */
void *VMM_GetPageDirectory(void);

/* Flags for mapping (use with PAGE_* from paging.h)
 */
#define VMM_RW 0x002         // Read/Write
#define VMM_USER 0x004       // User-accessible
#define VMM_DEFAULT (VMM_RW) // Kernel R/W only

/* Self-test helper
 */
void vmm_self_test(void);

#endif