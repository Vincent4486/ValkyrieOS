#pragma once

#include <arch/i686/irq.h>
#include <stdint.h>

void i686_keyboard_init(void);
void i686_keyboard_irq(Registers *regs);
int i686_keyboard_readline_nb(char *buf, int bufsize);
int i686_keyboard_readline(char *buf, int bufsize);