#pragma once
#include <stddef.h>
#include <stdint.h>

#define ATA_SECTOR_SIZE 512

// Read 'count' sectors from 'lba' into 'buffer' (buffer must be at least
// count*512 bytes) Returns 0 on success, nonzero on error
int ata_read28(uint32_t lba, uint8_t *buffer, size_t count);

// Write 'count' sectors from 'buffer' to 'lba'
// Returns 0 on success, nonzero on error
int ata_write28(uint32_t lba, const uint8_t *buffer, size_t count);

void ata_soft_reset(void);
