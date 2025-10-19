#pragma once

#include <stdint.h>
#include "irq.h"

void i686_keyboard_init(void);
void i686_keyboard_irq(Registers* regs);