// SPDX-License-Identifier: AGPL-3.0-or-later

#include "vmm.h"
#include "pmm.h"
#include <mem/memdefs.h>
#include <arch/i686/mem/paging.h>
#include <std/stdio.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_ALIGN_DOWN(v) ((v) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(v) (((v) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

static void *kernel_page_dir = NULL;

void VMM_Initialize(void)
{
   // Get the kernel page directory from paging subsystem
   kernel_page_dir = i686_Paging_GetCurrentPageDirectory();
   if (!kernel_page_dir)
   {
      printf("[vmm] ERROR: no kernel page directory!\n");
      return;
   }
   printf("[vmm] initialized with kernel page dir at 0x%08x\n",
          (uint32_t)kernel_page_dir);
}

void *VMM_Allocate(uint32_t size, uint32_t flags)
{
   if (size == 0) return NULL;

   // Align size to page boundary
   uint32_t aligned_size = PAGE_ALIGN_UP(size);
   uint32_t num_pages = aligned_size / PAGE_SIZE;

   // Find a free virtual address range (simple: use high memory)
   // In a real system, maintain a free list or use VMA structures
   static uint32_t next_vaddr = 0x80000000u; // Start at 2 GiB
   uint32_t vaddr = next_vaddr;
   next_vaddr += aligned_size;

   // Allocate and map physical pages
   for (uint32_t i = 0; i < num_pages; ++i)
   {
      uint32_t paddr = PMM_AllocatePhysicalPage();
      if (paddr == 0)
      {
         printf("[vmm] VMM_Allocate: failed to allocate physical page %u/%u\n",
                i, num_pages);
         return NULL;
      }

      uint32_t va = vaddr + (i * PAGE_SIZE);
      if (!i686_Paging_MapPage(kernel_page_dir, va, paddr, flags | PAGE_PRESENT))
      {
         printf("[vmm] VMM_Allocate: failed to map page at 0x%08x\n", va);
         PMM_FreePhysicalPage(paddr);
         return NULL;
      }
   }

   return (void *)vaddr;
}

void VMM_Free(void *vaddr, uint32_t size)
{
   if (!vaddr || size == 0) return;

   uint32_t va = (uint32_t)vaddr;
   uint32_t aligned_size = PAGE_ALIGN_UP(size);
   uint32_t num_pages = aligned_size / PAGE_SIZE;

   // Unmap and free each page
   for (uint32_t i = 0; i < num_pages; ++i)
   {
      uint32_t page_va = va + (i * PAGE_SIZE);
      uint32_t page_pa = i686_Paging_GetPhysicalAddress(kernel_page_dir, page_va);

      if (page_pa != 0)
      {
         i686_Paging_UnmapPage(kernel_page_dir, page_va);
         PMM_FreePhysicalPage(page_pa);
      }
   }
}

bool VMM_Map(uint32_t vaddr, uint32_t paddr, uint32_t size, uint32_t flags)
{
   if (size == 0) return false;

   uint32_t aligned_size = PAGE_ALIGN_UP(size);
   uint32_t num_pages = aligned_size / PAGE_SIZE;

   for (uint32_t i = 0; i < num_pages; ++i)
   {
      uint32_t va = vaddr + (i * PAGE_SIZE);
      uint32_t pa = paddr + (i * PAGE_SIZE);

      if (!i686_Paging_MapPage(kernel_page_dir, va, pa, flags | PAGE_PRESENT))
      {
         printf("[vmm] VMM_Map: failed at offset 0x%x\n", i * PAGE_SIZE);
         return false;
      }
   }

   return true;
}

bool VMM_Unmap(uint32_t vaddr, uint32_t size)
{
   if (size == 0) return true;

   uint32_t aligned_size = PAGE_ALIGN_UP(size);
   uint32_t num_pages = aligned_size / PAGE_SIZE;

   for (uint32_t i = 0; i < num_pages; ++i)
   {
      uint32_t va = vaddr + (i * PAGE_SIZE);
      i686_Paging_UnmapPage(kernel_page_dir, va);
   }

   return true;
}

uint32_t VMM_GetPhys(uint32_t vaddr)
{
   return i686_Paging_GetPhysicalAddress(kernel_page_dir, vaddr);
}

void *VMM_GetPageDirectory(void) { return kernel_page_dir; }

void vmm_self_test(void)
{
   printf("[vmm] self-test: starting\n");

   // Allocate 3 pages via VMM
   void *v1 = VMM_Allocate(PAGE_SIZE, VMM_DEFAULT);
   void *v2 = VMM_Allocate(PAGE_SIZE * 2, VMM_DEFAULT);

   if (!v1 || !v2)
   {
      printf("[vmm] self-test: FAIL (VMM_Allocate returned NULL)\n");
      return;
   }

   // Write and read through virtual addresses
   volatile uint32_t *ptr1 = (volatile uint32_t *)v1;
   volatile uint32_t *ptr2 = (volatile uint32_t *)v2;

   *ptr1 = 0xDEADBEEFu;
   *ptr2 = 0xCAFEBABEu;

   uint32_t val1 = *ptr1;
   uint32_t val2 = *ptr2;

   if (val1 != 0xDEADBEEFu || val2 != 0xCAFEBABEu)
   {
      printf("[vmm] self-test: FAIL (write/read mismatch)\n");
      return;
   }

   // Verify physical addresses are different
   uint32_t pa1 = VMM_GetPhys((uint32_t)v1);
   uint32_t pa2 = VMM_GetPhys((uint32_t)v2);

   if (pa1 == 0 || pa2 == 0 || pa1 == pa2)
   {
      printf("[vmm] self-test: FAIL (physical address issue)\n");
      return;
   }

   // Free and verify unmapped
   VMM_Free(v1, PAGE_SIZE);
   uint32_t pa1_after = VMM_GetPhys((uint32_t)v1);

   if (pa1_after != 0)
   {
      printf("[vmm] self-test: FAIL (page not unmapped)\n");
      return;
   }

   printf("[vmm] self-test: PASS (alloc/map/write/read/free)\n");
}
