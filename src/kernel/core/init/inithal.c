// SPDX-License-Identifier: AGPL-3.0-or-later

#include "inithal.h"
#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <arch/i686/irq.h>
#include <arch/i686/isr.h>

void HAL_Initialize()
{
   i686_GDT_Initialize();
   i686_IDT_Initialize();
   i686_ISR_Initialize();
   i686_IRQ_Initialize();
}