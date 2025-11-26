// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <core/arch/i686/irq.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void i686_keyboard_init(void);
void i686_keyboard_irq(Registers *regs);
int i686_keyboard_readline_nb(char *buf, int bufsize);
int i686_keyboard_readline(char *buf, int bufsize);

#ifdef __cplusplus
}
#endif