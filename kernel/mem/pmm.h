// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef PMM_H
#define PMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Physical Memory Manager (PMM)
 *
 * Tracks allocation of physical page frames.
 * Works with paging.c (which handles page table manipulation).
 */

/* Initialize PMM with available physical memory
 * Call before any PMM_AllocatePhysicalPage() calls
 * Accepts memory ranges from multiboot info or hardcoded.
 */
void PMM_Initialize(uint32_t total_mem_bytes);

/* Allocate a single 4K physical page frame
 * Returns physical address, or 0 on failure
 */
uint32_t PMM_AllocatePhysicalPage(void);

/* Free a previously allocated physical page
 * addr should be page-aligned (4K)
 */
void PMM_FreePhysicalPage(uint32_t addr);

/* Check if a physical page is free
 */
bool PMM_IsPhysicalPageFree(uint32_t addr);

/* Get total physical memory tracked
 */
uint32_t PMM_TotalMemory(void);

/* Get number of free physical pages
 */
uint32_t PMM_FreePages(void);

/* Get number of allocated physical pages
 */
uint32_t PMM_AllocatedPages(void);

/* Self-test helper
 */
void pmm_self_test(void);

#endif