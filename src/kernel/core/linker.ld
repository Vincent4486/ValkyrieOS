/* SPDX-License-Identifier: AGPL-3.0-or-later */

ENTRY(start)
/* Produce ELF output (default) instead of raw binary so the bootloader can
   load an ELF image. 
   
   Memory layout:
   - 0x00100000 - 0x00900000: Dynamic libraries (dylib) - 8 MiB
   - 0x00900000 - 0x00914000: Display buffer (80 KiB)
   - 0x00A00000 onwards: Kernel code and data (10 MiB)
   
   With dynamic linking, allow undefined symbols (will be resolved at runtime
   via PLT/GOT patching after libraries are loaded).
*/

/* Reserve dylib space for dynamic libraries */
DYLIB_START = 0x00100000;
DYLIB_SIZE = 0x800000;  /* 8 MiB */

/* Reserve display buffer space at fixed location */
BUFFER_START = 0x00900000;
BUFFER_SIZE = 0x14000;  /* 80 KiB */

/* Kernel starts at 10 MiB */
phys = 0x00A00000;

/* Allow undefined symbols - they will be resolved at runtime by dylib loader */
/* This lets kernel code call library functions without explicit EXTERN declarations */

SECTIONS
{
    . = phys;

    .entry              : { __entry_start = .;      *(.entry)   }
    
    .text               : { __text_start = .;       *(.text .text.*)    }
    
    /* Procedure Linkage Table - for indirect function calls to library functions */
    .plt                : { *(.plt) }
    
    .data               : { __data_start = .;       *(.data .data.*)    }
    
    .rodata             : { __rodata_start = .;     *(.rodata .rodata.*)    }
    
    /* Global Offset Table - stores resolved addresses for indirect references */
    .got                : { *(.got) }
    .got.plt            : { *(.got.plt) }
    
    /* Dynamic symbol sections - expose so runtime can resolve kernel relocations */
    _kernel_dynsym_start = .;
    .dynsym             : { *(.dynsym) }
    _kernel_dynsym_end = .;

    _kernel_dynstr_start = .;
    .dynstr             : { *(.dynstr) }
    _kernel_dynstr_end = .;
    
    /* Relocations - kernel loader will apply these after loading libraries */
    .rel.dyn            : { _kernel_rel_dyn_start = .; *(.rel.dyn) _kernel_rel_dyn_end = .; }
    
    .rel.plt            : { _kernel_rel_plt_start = .; *(.rel.plt) _kernel_rel_plt_end = .; }
    
    .bss                : { __bss_start = .;        *(.bss .bss.*)     }
    
    __end = .;
}
