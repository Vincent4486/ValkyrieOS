#pragma once

#include "disk.h"
#include "stdint.h"
#include "stdio.h"

#pragma pack(push, 1)
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
} FAT_DirectoryEntry;

#pragma pack(pop)

typedef struct
{
	int handle;
} FAT_File;

bool FAT_Initialized(DISK *disk)
