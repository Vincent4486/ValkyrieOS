// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stdbool.h>
#include <stdint.h>

extern void __attribute__((cdecl)) i686_outb(uint16_t port, uint8_t value);
extern void __attribute__((cdecl)) i686_outw(uint16_t port, uint16_t value);
extern uint8_t __attribute__((cdecl)) i686_inb(uint16_t port);
extern uint16_t __attribute__((cdecl)) i686_inw(uint16_t port);

extern uint8_t __attribute__((cdecl)) i686_EnableInterrupts();
extern uint8_t __attribute__((cdecl)) i686_DisableInterrupts();

extern void i686_iowait();
extern void __attribute__((cdecl)) i686_Panic();

extern void __attribute__((cdecl)) i686_Halt();