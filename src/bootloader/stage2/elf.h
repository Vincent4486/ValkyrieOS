// Simple ELF32 loader for stage2 bootloader
#pragma once

#include "disk.h"
#include "fat.h"
#include <stdint.h>
#include <stdbool.h>

// Load an ELF32 file from an opened FAT_File into memory. On success returns
// true and sets *entryOut to the ELF entry point (as a pointer). The loader
// will read program headers (PT_LOAD) and copy them to their p_paddr (or
// p_vaddr if p_paddr is zero), zeroing the BSS area when necessary.
bool ELF_Load(DISK *disk, FAT_File *file, void **entryOut);
