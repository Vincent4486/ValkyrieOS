ENTRY(entry)

SECTIONS {
    . = 0x0000;
    .text : AT(0x0000) { *(.text) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}