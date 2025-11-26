// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "isr.h"

typedef void (*IRQHandler)(Registers *regs);

extern void i686_IRQ_Initialize();
extern void i686_IRQ_RegisterHandler(int irq, IRQHandler handler);
extern void i686_IRQ_Unmask(int irq);
