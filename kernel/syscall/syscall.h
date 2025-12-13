// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Platform-independent syscall definitions and handlers
 *
 * Syscall numbers (must match arch-specific dispatcher)
 */
#define SYS_BRK 45
#define SYS_SBRK 186

/* Syscall handler prototypes
 * These are called by arch-specific dispatcher after extracting parameters
 */
intptr_t sys_brk(void *addr);
void *sys_sbrk(intptr_t increment);

/* Generic syscall dispatcher (arch code calls this)
 * syscall_num: syscall number
 * args: array of up to 6 arguments
 */
intptr_t syscall_dispatch(uint32_t syscall_num, uint32_t *args);
