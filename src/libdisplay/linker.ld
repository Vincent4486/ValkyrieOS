/* Minimal linker script for build-time sanity of libdisplay (static archive)
 * This script is intentionally small — static libraries are archived object
 * files and do not strictly need a linker script, but having one keeps the
 * build consistent with other kernel modules.
 */
SECTIONS
{
  . = 0x08048000;
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) }
}
