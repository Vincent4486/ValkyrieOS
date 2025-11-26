// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <arch/i686/irq.h>
#include <stdint.h>

extern void i686_keyboard_init(void);
extern void i686_keyboard_irq(Registers *regs);
extern int i686_keyboard_readline_nb(char *buf, int bufsize);
extern int i686_keyboard_readline(char *buf, int bufsize);