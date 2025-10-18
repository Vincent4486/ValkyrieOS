#pragma once
#include <stdbool.h>
#include <stdint.h>

void __attribute__((cdecl)) x86_outb(uint16_t port, uint8_t value);
uint8_t __attribute__((cdecl)) x86_inb(uint16_t port);

void i686_iowait();
void __attribute__((cdecl)) i686_Panic();