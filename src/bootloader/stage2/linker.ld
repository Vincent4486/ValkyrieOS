ENTRY(entry)
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)

/* Link sections at low VMAs so 16-bit relocations fit. Stage1 will load the
   produced binary at physical 0x20000 when booting, and any absolute
   runtime addresses (like GDTR base) are set explicitly in the code.
*/
SECTIONS {
    . = 0x0000;

    .text : { __text_start = .; *(.text) }
    .rodata : { *(.rodata) }
    .data : { __data_start = .; *(.data) }
    .bss  : { __bss_start = .; *(.bss); __end = .; }
}