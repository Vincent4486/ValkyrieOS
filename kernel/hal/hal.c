// SPDX-License-Identifier: AGPL-3.0-or-later

#include "hal.h"

void HAL_Initialize()
{
#if defined(I686)
   i686_GDT_Initialize();
   i686_IDT_Initialize();
   i686_ISR_Initialize();
   i686_IRQ_Initialize();
   ps2_keyboard_init();

   i686_IRQ_RegisterHandler(0x80, (void *)i686_syscall_handler);
#else
#error "Unsupported architecture for HAL initialization"
#endif
}