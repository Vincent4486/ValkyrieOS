// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "disk.h"
#include "stdint.h"

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

// Seek to a specific byte position in an opened FAT file. Returns true on
// success. After seeking, the internal sector buffer will contain the sector
// covering the requested position so subsequent FAT_Read calls read from the
// requested offset.
bool FAT_Seek(DISK *disk, FAT_File *file, uint32_t position);
