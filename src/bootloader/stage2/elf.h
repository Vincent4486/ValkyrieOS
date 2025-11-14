// SPDX-License-Identifier: AGPL-3.0-or-later

// Simple ELF32 loader for stage2 bootloader
#pragma once

#include "disk.h"
#include "fat.h"
#include <stdbool.h>
#include <stdint.h>

// Load an ELF32 file from disk into memory. On success returns true and sets
// *entryOut to the ELF entry point (as a pointer). The loader will read
// program headers (PT_LOAD) and copy them to their p_paddr (or p_vaddr if
// p_paddr is zero), zeroing the BSS area when necessary.
// Parameters:
//   disk: Initialized Partition structure
//   filepath: Path to ELF file (e.g., "/kernel.elf")
//   entryOut: Pointer to receive entry point address
bool ELF_Load(Partition *disk, const char *filepath, void **entryOut);
