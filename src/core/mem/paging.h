// SPDX-License-Identifier: AGPL-3.0-or-later

#include <stdint.h>

// Page table initialization
void paging_init(void);
void paging_enable(void);

// Page table management
void* create_page_directory(void);
void destroy_page_directory(void* page_dir);

// Page mapping
bool map_page(void* page_dir, uint32_t vaddr, uint32_t paddr, uint32_t flags);
bool unmap_page(void* page_dir, uint32_t vaddr);

// Page lookup
uint32_t get_physical_address(void* page_dir, uint32_t vaddr);
bool is_page_mapped(void* page_dir, uint32_t vaddr);

// Page fault handling
void page_fault_handler(uint32_t fault_address, uint32_t error_code);

// TLB management
void invalidate_tlb_entry(uint32_t vaddr);
void flush_tlb(void);

// Process page directory switching
void switch_page_directory(void* page_dir);
void* get_current_page_directory(void);

// Memory region allocation
void* allocate_kernel_pages(int page_count);
void free_kernel_pages(void* addr, int page_count);