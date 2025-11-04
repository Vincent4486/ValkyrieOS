// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>

#define FDC_SECTOR_SIZE 512

// Read 'count' sectors from 'lba' into 'buffer' (buffer must be at least
// count*512 bytes) Returns 0 on success, nonzero on error
int fdc_read_lba(uint32_t lba, uint8_t *buffer, size_t count);

// Write 'count' sectors from 'buffer' to 'lba'
// Returns 0 on success, nonzero on error
int fdc_write_lba(uint32_t lba, const uint8_t *buffer, size_t count);

void fdc_reset(void);
