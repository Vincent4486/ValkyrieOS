// SPDX-License-Identifier: AGPL-3.0-or-later

#include <fs/disk/disk.h>
#include <fs/disk/partition.h>
#include <fs/fat/fat.h>
#include <mem/memdefs.h>
#include <stdint.h>
#include <sys/sys.h>
#include <std/stdio.h>

/**
 * Initialize storage system: scan and initialize all disks
 *
 * @return true on success, false on failure
 */
bool FS_Initialize()
{
    printf("[FS] Initializing filesystem\n");
    // Call DISK_Initialize to scan and populate all volumes
    int disksDetected = DISK_Initialize();
    if (disksDetected < 0)
    {
        printf("[FS] Disk initialization failed\n");
        return false;
    }
    printf("[FS] Filesystem initialization complete, disks detected: %d\n", disksDetected);
    return true;
}

/**
 * Mount a filesystem on a specific volume
 *
 * @param volume - Pointer to the volume to mount
 * @return filesystem index on success, -1 on failure
 */
int FS_Mount(Partition *volume)
{
    printf("[FS] Mounting filesystem on volume at %s\n", volume->disk->brand);
    if (!volume || !volume->disk)
    {
        printf("[FS] Invalid volume\n");
        return -1;
    }

    // Find available filesystem slot
    int fsIndex = -1;
    for (int i = 0; i < 32; i++)
    {
        if (g_SysInfo->fs[i].mounted == 0)
        {
            fsIndex = i;
            break;
        }
    }
    if (fsIndex == -1) {
        printf("[FS] No available filesystem slots\n");
        return -1;
    }

    // Detect filesystem type (read boot sector from partition)
    uint8_t bootSector[512];
    if (!Partition_ReadSectors(volume, 0, 1, bootSector))
    {
        printf("[FS] Failed to read boot sector\n");
        return -1;
    }
    // Simple detection: Check for FAT signature (e.g., bytes 510-511 == 0xAA55)
    // For now, assume FAT; extend for other FS types
    bool isFat = (bootSector[510] == 0x55 && bootSector[511] == 0xAA);
    printf("[FS] Detected filesystem type: %s\n", isFat ? "FAT" : "Unknown");
    if (!isFat)
    {
        // Handle other FS types here (e.g., ext2)
        return -1;
    }

    // Initialize FAT
    if (!FAT_Initialize(volume))
    {
        printf("[FS] FAT initialization failed\n");
        return -1;
    }

    // Populate filesystem info
    g_SysInfo->fs[fsIndex].mounted = 1;
    g_SysInfo->fs[fsIndex].read_only = 0;
    g_SysInfo->fs[fsIndex].block_size = 512;
    g_SysInfo->fs[fsIndex].type = FAT32;
    g_SysInfo->fs[fsIndex].partition = volume;  // Assuming added to Filesystem struct
    g_SysInfo->fs_count++;

    printf("[FS] Mounted filesystem at index %d\n", fsIndex);
    return fsIndex;
}

/**
 * Open a file by path, routing to the correct mounted FS
 *
 * @param path - File path (e.g., "/fs0/file.txt")
 * @return FAT_File pointer on success, NULL on failure
 */
FAT_File *FS_Open(const char *path)
{
    printf("[FS] Opening file: %s\n", path);
    // Parse path to find mount point (e.g., "/fs0/file.txt" -> fsIndex 0)
    // This is simplistic; you'll need a mount table for better routing
    if (path[0] != '/' || path[1] != 'f' || path[2] != 's')
    {
        printf("[FS] Invalid path format\n");
        return NULL;
    }
    int fsIndex = path[3] - '0';  // e.g., "/fs0/file.txt" -> 0
    const char *strippedPath = path + 4;  // "/file.txt"

    printf("[FS] Routing to filesystem index %d, stripped path: %s\n", fsIndex, strippedPath);
    if (fsIndex < 0 || fsIndex >= 32 || !g_SysInfo->fs[fsIndex].mounted)
    {
        printf("[FS] Filesystem not mounted or invalid index\n");
        return NULL;
    }

    Partition *partition = g_SysInfo->fs[fsIndex].partition;  // Assuming added to Filesystem struct
    FAT_File *file = FAT_Open(partition, strippedPath);
    printf("[FS] File open result: %p\n", file);
    return file;
}

/**
 * Mount the volume corresponding to the boot device
 *
 * @return filesystem index on success, -1 on failure
 */
int FS_MountBootVolume()
{
    printf("[FS] Attempting to mount boot volume (Drive ID: 0x%x)\n", g_SysInfo->boot_device);
    
    // Iterate through detected volumes
    for (int i = 0; i < 32; i++)
    {
        if (g_SysInfo->volume[i].disk != NULL)
        {
            // Match volume disk ID with boot device ID
            // BIOS drive numbers: 0x00-0x03 (floppy), 0x80-0x83 (HDD)
            if (g_SysInfo->volume[i].disk->id == g_SysInfo->boot_device)
            {
                printf("[FS] Found boot volume at index %d\n", i);
                return FS_Mount(&g_SysInfo->volume[i]);
            }
        }
    }
    
    printf("[FS] Boot volume not found among detected disks\n");
    return -1;
}
