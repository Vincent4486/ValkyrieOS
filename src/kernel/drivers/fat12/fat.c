#include "fat12.h"
#include <drivers/ata/ata.h>
#include <drivers/fdc/fdc.h>
#include <stdbool.h>
#include <stdint.h>

bool DISK_Initialize(DISK *disk, uint8_t driveNumber)
{
    disk->id = driveNumber;
    if (driveNumber == 0) {
        disk->type = DISK_TYPE_FLOPPY;
        disk->cylinders = 80;
        disk->heads = 2;
        disk->sectors = 18;
    } else {
        disk->type = DISK_TYPE_ATA;
        disk->cylinders = 0;
        disk->heads = 0;
        disk->sectors = 0;
    }
    return true;
}

bool DISK_ReadSectors(DISK *disk, uint32_t lba, uint8_t sectors, void *dataOut)
{
    if (disk->type == DISK_TYPE_FLOPPY) {
        return fdc_read_lba(lba, (uint8_t *)dataOut, sectors) == 0;
    } else {
        return ata_read28(lba, (uint8_t *)dataOut, sectors) == 0;
    }
}
