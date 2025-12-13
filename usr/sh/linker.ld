/* Linker script for sh executable */
ENTRY(main)

SECTIONS
{
    . = 0x08048000;  /* Standard x86 32-bit user code start (ELF convention) */
    
    .text :
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
