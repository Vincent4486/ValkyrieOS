// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Physical Memory Manager (PMM)
 * 
 * Tracks allocation of physical page frames.
 * Works with paging.c (which handles page table manipulation).
 */

/* Initialize PMM with available physical memory
 * Call before any alloc_physical_page() calls
 * Accepts memory ranges from multiboot info or hardcoded.
 */
void pmm_init(uint32_t total_mem_bytes);

/* Allocate a single 4K physical page frame
 * Returns physical address, or 0 on failure
 */
uint32_t alloc_physical_page(void);

/* Free a previously allocated physical page
 * addr should be page-aligned (4K)
 */
void free_physical_page(uint32_t addr);

/* Check if a physical page is free
 */
bool is_physical_page_free(uint32_t addr);

/* Get total physical memory tracked
 */
uint32_t pmm_total_memory(void);

/* Get number of free physical pages
 */
uint32_t pmm_free_pages(void);

/* Get number of allocated physical pages
 */
uint32_t pmm_allocated_pages(void);

/* Self-test helper
 */
void pmm_self_test(void);
