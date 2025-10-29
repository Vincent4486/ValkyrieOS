ENTRY(start)
/* Produce ELF output (default) instead of raw binary so the bootloader can
    load an ELF image. The kernel should be linked to physical address 0x00100000
    by setting the start location below. */
phys = 0x00100000;

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
