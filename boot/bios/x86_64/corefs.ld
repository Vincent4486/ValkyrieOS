/* SPDX-License-Identifier: GPL-3.0-only */

SECTIONS
{
    . = 0x90000;

    .exports : AT(0)
    {
        *(.exports*)
    }

    .text :
    {
        *(.text*)
        *(.rodata*)
        *(.data*)
    }

    .bss (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)
    }
}
