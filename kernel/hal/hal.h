// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <stdint.h>

#if defined(I686)
#include <arch/i686/cpu/gdt.h>
#include <arch/i686/cpu/idt.h>
#include <arch/i686/cpu/irq.h>
#include <arch/i686/cpu/isr.h>

#include <arch/i686/drivers/ps2.h>
#include <arch/i686/syscall/syscall_dispatch.h>
#else
#error "Unsupported architecture for HAL"
#endif

void HAL_Initialize();
