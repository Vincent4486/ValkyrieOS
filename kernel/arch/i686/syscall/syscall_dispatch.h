// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <arch/i686/cpu/irq.h>
#include <stdint.h>

/* x86 syscall dispatcher entry point
 *
 * Called from syscall_entry_asm.asm when user executes int 0x80
 * Extracts parameters from registers and dispatches to generic handler
 */
void i686_syscall_handler(Registers *regs);
