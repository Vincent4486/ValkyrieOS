// SPDX-License-Identifier: AGPL-3.0-or-later

#include "syscall.h"
#include <mem/heap.h>
#include <std/stdio.h>
#include <stddef.h>
#include <stdint.h>

/* Per-process heap management via syscalls
 *
 * These handlers implement brk/sbrk for user processes.
 * The per-process heap tracking is done via Process struct fields.
 */

/* Get current process (stub for now - will be filled when process mgmt exists)
 * TODO: replace with actual current_process() from scheduler
 */
static void *get_current_process(void)
{
   // For now, return NULL - will implement when process management is ready
   return NULL;
}

intptr_t sys_brk(void *addr)
{
   // For now, use global kernel brk (until per-process is ready)
   // When process management exists, extract from current process:
   // Process *proc = (Process*)get_current_process();
   // if (!proc) return -1;

   if (brk(addr) == 0) return (intptr_t)addr;
   return -1;
}

void *sys_sbrk(intptr_t increment)
{
   // For now, use global kernel sbrk (until per-process is ready)
   void *result = sbrk(increment);
   return result;
}

/* Generic syscall dispatcher
 *
 * Called by arch-specific handler after extracting parameters from registers.
 * Returns result in EAX (for x86).
 */
intptr_t syscall_dispatch(uint32_t syscall_num, uint32_t *args)
{
   switch (syscall_num)
   {
   case SYS_BRK:
      return sys_brk((void *)args[0]);

   case SYS_SBRK:
      return (intptr_t)sys_sbrk((intptr_t)args[0]);

   default:
      printf("[syscall] unknown syscall %u\n", syscall_num);
      return -1;
   }
}
