/* SPDX-License-Identifier: AGPL-3.0-or-later */

ENTRY(main)

SECTIONS
{
    . = 0x05000000;
    
    .text 0x05000000 :
    {
        *(.text)
        *(.text.*)
    }
    
    .got.plt ALIGN(4) :
    {
        *(.got.plt)
        *(.got)
    }
    
    .rodata ALIGN(0x1000) :
    {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data ALIGN(0x1000) :
    {
        *(.data)
        *(.data.*)
    }
    
    .bss ALIGN(0x1000) :
    {
        *(.bss)
        *(.bss.*)
    }
    
    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.gnu_debugaltlink)
    }
}
