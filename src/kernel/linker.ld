/* SPDX-License-Identifier: AGPL-3.0-or-later */

ENTRY(start)
/* Produce ELF output (default) instead of raw binary so the bootloader can
   load an ELF image. The kernel is linked to physical address 0x00A00000
   (10 MiB) so stage2 may place libraries below this region.
   
   With dynamic linking, allow undefined symbols (will be resolved at runtime
   via PLT/GOT patching after libraries are loaded).
*/
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
    
    /* Relocations - kernel loader will apply these after loading libraries */
    _kernel_rel_dyn_start = .;
    .rel.dyn            : { *(.rel.dyn) }
    _kernel_rel_dyn_end = .;
    
    _kernel_rel_plt_start = .;
    .rel.plt            : { *(.rel.plt) }
    _kernel_rel_plt_end = .;
    
    .bss                : { __bss_start = .;        *(.bss .bss.*)     }
    
    __end = .;
}
