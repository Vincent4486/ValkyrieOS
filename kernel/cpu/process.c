// SPDX-License-Identifier: AGPL-3.0-or-later

#include "process.h"
#include <mem/memdefs.h>
#include <arch/i686/mem/paging.h>
#include <mem/heap.h>
#include <mem/stack.h>
#include <mem/pmm.h>
#include <mem/vmm.h>
#include <std/stdio.h>
#include <std/string.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/elf.h>
#include <fs/fat/fat.h>
#include <fs/disk/partition.h>
#include <fs/fd.h>

static Process *current_process = NULL;
static uint32_t next_pid = 1;

Process *Process_Create(uint32_t entry_point, bool kernel_mode)
{
   Process *proc = (Process *)kmalloc(sizeof(Process));
   if (!proc)
   {
      printf("[process] create: kmalloc failed\n");
      return NULL;
   }

   // Initialize basic fields
   proc->pid = next_pid++;
   proc->ppid = 0;
   proc->state = 0; // READY
   proc->kernel_mode = kernel_mode;
   proc->priority = 10;
   proc->exit_code = 0;

   if (kernel_mode)
   {
      // Kernel-mode: reuse current kernel page directory, no user heap/stack mapping
      proc->page_directory = i686_Paging_GetCurrentPageDirectory();
      proc->heap_start = proc->heap_end = 0;
      proc->stack_start = proc->stack_end = 0;
      proc->esp = proc->ebp = 0; // Not set here; kernel threads would set up elsewhere
   }
   else
   {
      // Create page directory
      proc->page_directory = i686_Paging_CreatePageDirectory();
      if (!proc->page_directory)
      {
         printf("[process] create: i686_Paging_CreatePageDirectory failed\n");
         free(proc);
         return NULL;
      }

      // Initialize heap at 0x10000000 (user data segment)
      if (Heap_ProcessInitialize(proc, 0x10000000) == -1)
      {
         printf("[process] create: Heap_Initialize failed\n");
         i686_i686_Paging_DestroyPageDirectory(proc->page_directory);
         free(proc);
         return NULL;
      }

      // Initialize stack (grows downward)
      const uint32_t stack_top = 0xBFFF0000u;
      const uint32_t stack_size = 64 * 1024; // 64 KiB user stack
      const uint32_t stack_bottom = stack_top - stack_size;

      // Map stack pages into the process address space
      uint32_t pages_needed = stack_size / PAGE_SIZE;
      for (uint32_t i = 0; i < pages_needed; ++i)
      {
         uint32_t va = stack_bottom + (i * PAGE_SIZE);
         uint32_t phys = PMM_AllocatePhysicalPage();
         if (phys == 0)
         {
            printf("[process] create: PMM_AllocatePhysicalPage failed for stack\n");
            // Cleanup already mapped stack pages
            for (uint32_t j = 0; j < i; ++j)
            {
               uint32_t va_cleanup = stack_bottom + (j * PAGE_SIZE);
               uint32_t phys_cleanup = i686_Paging_GetPhysicalAddress(proc->page_directory, va_cleanup);
               i686_Paging_UnmapPage(proc->page_directory, va_cleanup);
               if (phys_cleanup) PMM_FreePhysicalPage(phys_cleanup);
            }
            i686_i686_Paging_DestroyPageDirectory(proc->page_directory);
            free(proc);
            return NULL;
         }
         if (!i686_Paging_MapPage(proc->page_directory, va, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER))
         { // RW, Present, User
            printf("[process] create: map_page failed for stack at 0x%08x\n", va);
            PMM_FreePhysicalPage(phys);
            // Cleanup already mapped stack pages
            for (uint32_t j = 0; j < i; ++j)
            {
               uint32_t va_cleanup = stack_bottom + (j * PAGE_SIZE);
               uint32_t phys_cleanup = i686_Paging_GetPhysicalAddress(proc->page_directory, va_cleanup);
               i686_Paging_UnmapPage(proc->page_directory, va_cleanup);
               if (phys_cleanup) PMM_FreePhysicalPage(phys_cleanup);
            }
            i686_i686_Paging_DestroyPageDirectory(proc->page_directory);
            free(proc);
            return NULL;
         }
      }

      // Set stack bounds in PCB
      proc->stack_start = stack_bottom;
      proc->stack_end = stack_top;

      // Prepare initial stack frame using generic stack helpers
      Stack tmp_stack = {
          .base = stack_top,
          .size = stack_size,
          .current = stack_top,
          .data = (uint8_t *)stack_bottom,
      };
        // Switch to process page directory so the user stack VA is mapped while we write to it
        void *kernel_pd = VMM_GetPageDirectory();
        if (!kernel_pd) {
            printf("[process] ERROR: cannot get kernel page directory\n");
            // Cleanup: unmap already mapped stack pages
            for (uint32_t j = 0; j < pages_needed; ++j) {
                uint32_t va_cleanup = stack_bottom + (j * PAGE_SIZE);
                uint32_t phys_cleanup = i686_Paging_GetPhysicalAddress(proc->page_directory, va_cleanup);
                i686_Paging_UnmapPage(proc->page_directory, va_cleanup);
                if (phys_cleanup) PMM_FreePhysicalPage(phys_cleanup);
            }
            i686_i686_Paging_DestroyPageDirectory(proc->page_directory);
            free(proc);
            return NULL;
        }
        
        i686_Paging_SwitchPageDirectory(proc->page_directory);
        Stack_SetupProcess(&tmp_stack, entry_point);
        // Switch back to kernel page directory - critical for correctness
        i686_Paging_SwitchPageDirectory(kernel_pd);

      // Record initial ESP/EBP after setup
      proc->esp = tmp_stack.current;
      proc->ebp = tmp_stack.current;
   }

   // Initialize registers
   proc->eip = entry_point;
   proc->eax = proc->ebx = proc->ecx = proc->edx = 0;
   proc->esi = proc->edi = 0;
   proc->eflags = 0x202; // IF=1 (interrupts enabled)

   // Initialize file descriptors (all NULL, reserved FDs 0/1/2 handled by syscalls)
   for (int i = 0; i < 16; ++i) proc->fd_table[i] = NULL;

   printf("[process] created: pid=%u, entry=0x%08x\n", proc->pid, entry_point);
   return proc;
}

void Process_Destroy(Process *proc)
{
   if (!proc) return;

   // Only unmap/free resources if it's a user-mode process
   if (!proc->kernel_mode)
   {
      // Unmap and free stack pages
      if (proc->page_directory && proc->stack_start && proc->stack_end)
      {
         uint32_t pages = (proc->stack_end - proc->stack_start) / PAGE_SIZE;
         for (uint32_t i = 0; i < pages; ++i)
         {
            uint32_t va = proc->stack_start + (i * PAGE_SIZE);
            uint32_t phys = i686_Paging_GetPhysicalAddress(proc->page_directory, va);
            i686_Paging_UnmapPage(proc->page_directory, va);
            if (phys) PMM_FreePhysicalPage(phys);
         }
      }

      // Unmap and free heap pages
      if (proc->page_directory && proc->heap_start && proc->heap_end)
      {
         uint32_t heap_pages = (proc->heap_end - proc->heap_start + PAGE_SIZE - 1) / PAGE_SIZE;
         for (uint32_t i = 0; i < heap_pages; ++i)
         {
            uint32_t va = proc->heap_start + (i * PAGE_SIZE);
            uint32_t phys = i686_Paging_GetPhysicalAddress(proc->page_directory, va);
            i686_Paging_UnmapPage(proc->page_directory, va);
            if (phys) PMM_FreePhysicalPage(phys);
         }
      }

      // Only destroy page directory for user-mode processes
      if (proc->page_directory)
      {
         i686_i686_Paging_DestroyPageDirectory(proc->page_directory);
      }
   }
   // Kernel-mode: just free the PCB, don't touch page directory

   // Close all open file descriptors
   FD_CloseAll(proc);

   free(proc);

   if (current_process == proc)
   {
      current_process = NULL;
      i686_Paging_SwitchPageDirectory(VMM_GetPageDirectory());
   }
}

Process *Process_GetCurrent(void) { return current_process; }

void Process_SetCurrent(Process *proc)
{
   current_process = proc;
   if (proc)
   {
      i686_Paging_SwitchPageDirectory(proc->page_directory);
   }
   else
   {
       // Restore kernel page directory when no process is current
       i686_Paging_SwitchPageDirectory(VMM_GetPageDirectory());
   }
}

void process_self_test(void)
{
   printf("[process] self-test: starting\n");

   // Create a test process
   Process *p = Process_Create(0x08048000, false);
   if (!p)
   {
      printf("[process] self-test: FAIL (Process_Create returned NULL)\n");
      return;
   }

   // Test per-process heap
   if (Heap_ProcessSbrk(p, 4096) == (void *)-1)
   {
      printf("[process] self-test: FAIL (sbrk failed)\n");
      Process_Destroy(p);
      return;
   }

   // Set as current and test heap write/read
   Process_SetCurrent(p);
   volatile uint32_t *heap_test = (volatile uint32_t *)p->heap_start;
   *heap_test = 0xCAFEBABEu;
   uint32_t val = *heap_test;

   if (val != 0xCAFEBABEu)
   {
      printf("[process] self-test: FAIL (heap write/read)\n");
      Process_Destroy(p);
      return;
   }

    // Test user stack mapping and write/read near top
    volatile uint32_t *stack_test = (volatile uint32_t *)(p->stack_end - sizeof(uint32_t));
    *stack_test = 0x11223344u;
    uint32_t sval = *stack_test;
    if (sval != 0x11223344u)
    {
       printf("[process] self-test: FAIL (stack write/read)\n");
       Process_Destroy(p);
       return;
    }

   printf("[process] self-test: PASS (pid=%u, heap+stack ok)\n", p->pid);
   Process_Destroy(p);
}