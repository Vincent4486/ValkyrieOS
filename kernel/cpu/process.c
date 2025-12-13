// SPDX-License-Identifier: AGPL-3.0-or-later

#include "process.h"
#include <arch/i686/mem/paging.h>
#include <mem/heap.h>
#include <mem/pmm.h>
#include <mem/vmm.h>
#include <std/stdio.h>
#include <std/string.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/elf.h>
#include <fs/fat/fat.h>
#include <fs/disk/partition.h>

#define PAGE_SIZE 4096
#define HEAP_MAX 0xC0000000u // Don't allow heap above 3GB

static Process *current_process = NULL;
static uint32_t next_pid = 1;

Process *Process_Create(uint32_t entry_point)
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
   proc->priority = 10;
   proc->exit_code = 0;

   // Create page directory
   proc->page_directory = Paging_CreatePageDirectory();
   if (!proc->page_directory)
   {
      printf("[process] create: Paging_CreatePageDirectory failed\n");
      free(proc);
      return NULL;
   }

   // Initialize heap at 0x10000000 (user data segment)
   if (Heap_ProcessInitialize(proc, 0x10000000) == -1)
   {
      printf("[process] create: Heap_Initialize failed\n");
      Paging_DestroyPageDirectory(proc->page_directory);
      free(proc);
      return NULL;
   }

   // Initialize stack (grows downward from 0xBFFF0000)
   proc->stack_start = 0xBFFF0000u;
   proc->stack_end = 0xBFFF0000u;
   proc->esp = 0xBFFF0000u;

   // Initialize registers
   proc->eip = entry_point;
   proc->ebp = proc->esp;
   proc->eax = proc->ebx = proc->ecx = proc->edx = 0;
   proc->esi = proc->edi = 0;
   proc->eflags = 0x202; // IF=1 (interrupts enabled)

   // Initialize file descriptors
   for (int i = 0; i < 16; ++i) proc->fd_table[i] = NULL;

   printf("[process] created: pid=%u, entry=0x%08x\n", proc->pid, entry_point);
   return proc;
}

void Process_Destroy(Process *proc)
{
   if (!proc) return;

   // Free page directory and all mapped pages
   if (proc->page_directory)
   {
      Paging_DestroyPageDirectory(proc->page_directory);
   }

   // Free process structure
   free(proc);

   if (current_process == proc) current_process = NULL;
}

Process *Process_GetCurrent(void) { return current_process; }

void Process_SetCurrent(Process *proc)
{
   current_process = proc;
   if (proc)
   {
      switch_page_directory(proc->page_directory);
   }
}

void process_self_test(void)
{
   printf("[process] self-test: starting\n");

   // Create a test process
   Process *p = Process_Create(0x08048000);
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

   printf("[process] self-test: PASS (pid=%u, heap ok)\n", p->pid);
   Process_Destroy(p);
}