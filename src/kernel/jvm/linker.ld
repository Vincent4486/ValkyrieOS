/* Linker script for the JVM binary
 * Entry point: void start() placed into section .entry via
 *   void __attribute__((section(".entry"))) start();
 * Produces a flat binary image linked at physical address 0x00100000.
 */

ENTRY(start)
OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386)

/* physical load address for the JVM binary */
PHYS = 0x00100000;

SECTIONS
{
    /* start linking at the physical load address */
    . = PHYS;

    .entry :
    {
        __entry_start = .;
        KEEP(*(.entry))
        __entry_end = .;
    }

    .text :
    {
        __text_start = .;
        *(.text .text.*)
        __text_end = .;
    }

    .rodata :
    {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }

    .data :
    {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    .bss :
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* final end symbol */
    __end = .;
}

/* provide convenient symbols for C code */
PROVIDE(__entry_start = __entry_start);
PROVIDE(__entry_end = __entry_end);
PROVIDE(__text_start = __text_start);
PROVIDE(__text_end = __text_end);
PROVIDE(__data_start = __data_start);
PROVIDE(__data_end = __data_end);
PROVIDE(__bss_start = __bss_start);
PROVIDE(__bss_end = __bss_end);
PROVIDE(__end = __end);
ENTRY(start)
OUTPUT_FORMAT("binary")
phys = 0x00200000;

SECTIONS
{
    . = phys;

    .entry              : { __entry_start = .;      *(.entry)   }
    .text               : { __text_start = .;       *(.text)    }
    .data               : { __data_start = .;       *(.data)    }
    .rodata             : { __rodata_start = .;     *(.rodata)  }
    .bss                : { __bss_start = .;        *(.bss)     }
    
    __end = .;
}
