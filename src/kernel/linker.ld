/* SPDX-License-Identifier: AGPL-3.0-or-later */

ENTRY(start)
/* Produce ELF output (default) instead of raw binary so the bootloader can
   load an ELF image. The kernel is linked to physical address 0x00A00000
   (10 MiB) so stage2 may place libraries below this region. */
phys = 0x00A00000;

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
