// SPDX-License-Identifier: GPL-3.0-only

#include <status.h>

// This is a local header file, and it is not allowed to directly include
// this file, so for external modules, include fs/fs.h instead.
// To interact with the filesystem, use the VFS interface defined in fs/fs.h.

#ifndef FAT_H
#define FAT_H
#include <fs/fs.h>
#include <stdbool.h>
#include <stdint.h>

// Opaque per-volume instance — defined in fat.c
typedef struct FAT_Instance FAT_Instance;

#define FAT_EDISK (-2)
#define FAT_ESTATE (-5)
#define FAT_EPERM (-6)

typedef struct
{
   uint8_t name[11];
   uint8_t attributes;
   uint8_t _reserved;
   uint8_t created_time_tenths;
   uint16_t created_time;
   uint16_t created_date;
   uint16_t accessed_date;
   uint16_t first_cluster_high;
   uint16_t modified_time;
   uint16_t modified_date;
   uint16_t first_cluster_low;
   uint32_t size;
} __attribute__((packed)) FAT_DirectoryEntry;

typedef struct
{
   int handle;
   bool is_directory;
   uint32_t position;
   uint32_t size;
   uint8_t name[11];       // FAT name (11 bytes, space-padded)
   FAT_Instance *instance; // Back-pointer to owning FAT_Instance
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

// Allocate, initialise and return the per-volume FAT_Instance.
// The caller must store the returned pointer in
// partition->fs->private_data.  Returns NULL on failure.
FAT_Instance *FAT_Initialize(Partition *disk);
FAT_File *FAT_Open(Partition *disk, const char *path);
uint32_t FAT_Read(Partition *disk, FAT_File *file, uint32_t byte_count,
                  void *data_out);
int FAT_ReadEntry(Partition *disk, FAT_File *file,
                  FAT_DirectoryEntry *dir_entry);
void FAT_Close(FAT_File *file);

// Seek to a specific byte position in an opened FAT file. Returns FAT_OK on
// success. After seeking, the internal sector buffer will contain the sector
// covering the requested position so subsequent FAT_Read calls read from the
// requested offset.
int FAT_Seek(Partition *disk, FAT_File *file, uint32_t position);

// Write a directory entry to a directory at the current file position.
// File must be opened as a directory. Advances file position to next entry.
int FAT_WriteEntry(Partition *disk, FAT_File *file,
                   const FAT_DirectoryEntry *dir_entry);

// Write data to an opened file. File position is advanced by bytes written.
// File size is updated if write extends past end. Returns bytes written.
uint32_t FAT_Write(Partition *disk, FAT_File *file, uint32_t byte_count,
                   const void *data_in);

// Truncate (shrink to 0 bytes) an opened file and free all its clusters.
// File position and size are reset to 0. Returns FAT_OK on success.
int FAT_Truncate(Partition *disk, FAT_File *file);

// Update the file's directory entry with current size and cluster info.
// Must be called after writes to persist metadata to disk.
int FAT_UpdateEntry(Partition *disk, FAT_File *file);

// Create a new file with the given name in the root directory.
// Returns a file handle opened for writing. Returns NULL on failure.
FAT_File *FAT_Create(Partition *disk, const char *name, uint16_t mode);

// Delete a file by name from the root directory.
// Frees all clusters and marks the directory entry as deleted (0xE5).
// Returns FAT_OK on success.
int FAT_Delete(Partition *disk, const char *name);

// Invalidate FAT cache and reset file handles for the given instance
void FAT_InvalidateCache(FAT_Instance *inst);

// VFS Integration
struct VFS_Operations;
typedef struct VFS_Operations VFS_Operations;

// Get VFS operations structure for FAT filesystem
const VFS_Operations *FAT_GetVFSOperations(void);

#endif