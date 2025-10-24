#pragma once

#include <stdbool.h>
#include <stdint.h>

enum DISK_TYPE
{
   DISK_TYPE_FLOPPY = 0,
   DISK_TYPE_ATA = 1
};

typedef struct
{
   uint8_t id;
   uint16_t cylinders;
   uint16_t sectors;
   uint16_t heads;
   enum DISK_TYPE type;
} DISK;

bool DISK_Initialize(DISK *disk, uint8_t driveNumber);
bool DISK_ReadSectors(DISK *disk, uint32_t lba, uint8_t sectors,
                      void *lowerDataOut);

typedef struct
{
   uint8_t Name[11];
   uint8_t Attributes;
   uint8_t _Reserved;
   uint8_t CreatedTimeTenths;
   uint16_t CreatedTime;
   uint16_t CreatedDate;
   uint16_t AccessedDate;
   uint16_t FirstClusterHigh;
   uint16_t ModifiedTime;
   uint16_t ModifiedDate;
   uint16_t FirstClusterLow;
   uint32_t Size;
} __attribute__((packed)) FAT_DirectoryEntry;

typedef struct
{
   int Handle;
   bool IsDirectory;
   uint32_t Position;
   uint32_t Size;
} FAT_File;

enum FAT_Attributes
{
   FAT_ATTRIBUTE_READ_ONLY = 0x01,
   FAT_ATTRIBUTE_HIDDEN = 0x02,
   FAT_ATTRIBUTE_SYSTEM = 0x04,
   FAT_ATTRIBUTE_VOLUME_ID = 0x08,
   FAT_ATTRIBUTE_DIRECTORY = 0x10,
   FAT_ATTRIBUTE_ARCHIVE = 0x20,
   FAT_ATTRIBUTE_LFN = FAT_ATTRIBUTE_READ_ONLY | FAT_ATTRIBUTE_HIDDEN |
                       FAT_ATTRIBUTE_SYSTEM | FAT_ATTRIBUTE_VOLUME_ID
};

bool FAT_Initialize(DISK *disk);
FAT_File *FAT_Open(DISK *disk, const char *path);
uint32_t FAT_Read(DISK *disk, FAT_File *file, uint32_t byteCount,
                  void *dataOut);
bool FAT_ReadEntry(DISK *disk, FAT_File *file, FAT_DirectoryEntry *dirEntry);
void FAT_Close(FAT_File *file);
