// SPDX-License-Identifier: AGPL-3.0-or-later

// Simple ELF32 loader for stage2 bootloader
#pragma once

#include <fs/disk.h>
#include <fs/fat12.h>
#include <stdbool.h>
#include <stdint.h>
#include <fs/partition.h>

// Load an ELF32 file from an opened FAT_File into memory. On success returns
// true and sets *entryOut to the ELF entry point (as a pointer). The loader
// will read program headers (PT_LOAD) and copy them to their p_paddr (or
// p_vaddr if p_paddr is zero), zeroing the BSS area when necessary.
bool ELF_Load(Partition *disk, FAT_File *file, void **entryOut);
