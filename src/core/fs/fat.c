// SPDX-License-Identifier: AGPL-3.0-or-later

#include "fat.h"
#include <drivers/ata.h>
#include <drivers/fdc.h>
#include <fs/partition.h>
#include <std/ctype.h>
#include <std/minmax.h>
#include <std/stdio.h>
#include <std/string.h>
#include <stddef.h>
#include <sys/memdefs.h>
#include <sys/memory.h>

#define SECTOR_SIZE 512
#define MAX_PATH_SIZE 256
#define MAX_FILE_HANDLES 10
#define ROOT_DIRECTORY_HANDLE -1
#define FAT_CACHE_SIZE 5

typedef struct
{
   // extended boot record
   uint8_t DriveNumber;
   uint8_t _Reserved;
   uint8_t Signature;
   uint32_t VolumeId;       // serial number, value doesn't matter
   uint8_t VolumeLabel[11]; // 11 bytes, padded with spaces
   uint8_t SystemId[8];
} __attribute__((packed)) FAT_ExtendedBootRecord;

typedef struct
{
   uint32_t SectorsPerFat;
   uint16_t Flags;
   uint16_t FatVersionNumber;
   uint32_t RootDirectoryCluster;
   uint16_t FSInfoSector;
   uint16_t BackupBootSector;
   uint8_t _Reserved[12];
   FAT_ExtendedBootRecord EBR;
} __attribute__((packed)) FAT32_ExtendedBootRecord;

typedef struct
{
   uint8_t BootJumpInstruction[3];
   uint8_t OemIdentifier[8];
   uint16_t BytesPerSector;
   uint8_t SectorsPerCluster;
   uint16_t ReservedSectors;
   uint8_t FatCount;
   uint16_t DirEntryCount;
   uint16_t TotalSectors;
   uint8_t MediaDescriptorType;
   uint16_t SectorsPerFat;
   uint16_t SectorsPerTrack;
   uint16_t Heads;
   uint32_t HiddenSectors;
   uint32_t LargeSectorCount;

   union
   {
      FAT_ExtendedBootRecord EBR1216;
      FAT32_ExtendedBootRecord EBR32;
   } ExtendedBootRecord;
} __attribute__((packed)) FAT_BootSector;

typedef struct
{
   uint8_t Buffer[SECTOR_SIZE];
   FAT_File Public;
   bool Opened;
   uint32_t FirstCluster;
   uint32_t CurrentCluster;
   uint32_t CurrentSectorInCluster;

} FAT_FileData;

typedef struct
{
   union
   {
      FAT_BootSector BootSector;
      uint8_t BootSectorBytes[SECTOR_SIZE];
   } BS;

   FAT_FileData RootDirectory;

   FAT_FileData OpenedFiles[MAX_FILE_HANDLES];

   uint8_t FatCache[FAT_CACHE_SIZE * SECTOR_SIZE];
   uint32_t FatCachePos;

} FAT_Data;

static FAT_Data *g_Data;
static uint32_t g_DataSectionLba;
static uint8_t g_FatType;
static uint32_t g_TotalSectors;
static uint32_t g_SectorsPerFat;

// Forward declaration
uint32_t FAT_ClusterToLba(uint32_t cluster);

bool FAT_ReadBootSector(Partition *disk)
{
   // Try reading from different offsets to find the boot sector
   for (uint32_t offset = 0; offset <= 32; offset += 16)
   {
      if (!Partition_ReadSectors(disk, offset, 1, g_Data->BS.BootSectorBytes))
         continue;

      uint16_t sig = (g_Data->BS.BootSectorBytes[511] << 8) |
                     g_Data->BS.BootSectorBytes[510];

      // Check if this looks like a boot sector (signature 0xAA55 or starts with
      // EB or E9)
      if (sig == 0xaa55 || g_Data->BS.BootSectorBytes[0] == 0xEB ||
          g_Data->BS.BootSectorBytes[0] == 0xE9)
      {
         if (!Partition_ReadSectors(disk, offset, 1,
                                    g_Data->BS.BootSectorBytes))
            return false;
         break;
      }
   }

   return true;
}

bool FAT_ReadFat(Partition *disk, size_t LBAIndex)
{
   return Partition_ReadSectors(
       disk, g_Data->BS.BootSector.ReservedSectors + LBAIndex, FAT_CACHE_SIZE,
       g_Data->FatCache);
}

void FAT_Detect(Partition *disk)
{
   uint32_t dataClusters = (g_TotalSectors - g_DataSectionLba) /
                           g_Data->BS.BootSector.SectorsPerCluster;
   if (dataClusters < 0xFF5)
      g_FatType = 12;
   else if (g_Data->BS.BootSector.SectorsPerFat != 0)
      g_FatType = 16;
   else
      g_FatType = 32;
}

bool FAT_Initialize(Partition *disk)
{
   /* Stage2 preloads the boot sector and root directory at MEMORY_FAT_ADDR
    * (0x20000). We need to allocate our own FAT_Data structure in a different
    * location, then copy the preloaded boot sector into it.
    */

   // Allocate FAT_Data structure (normally would use malloc, but we'll use a
   // static)
   static FAT_Data s_FatData;
   g_Data = &s_FatData;

   // Copy preloaded boot sector from stage2 location
   uint8_t *preloaded_boot = (uint8_t *)MEMORY_FAT_ADDR;
   memcpy(g_Data->BS.BootSectorBytes, preloaded_boot, SECTOR_SIZE);

   // read FAT
   g_Data->FatCachePos = 0xFFFFFFFF;

   g_TotalSectors = g_Data->BS.BootSector.TotalSectors;
   if (g_TotalSectors == 0)
   { // fat32
      g_TotalSectors = g_Data->BS.BootSector.LargeSectorCount;
   }

   bool isFat32 = false;
   g_SectorsPerFat = g_Data->BS.BootSector.SectorsPerFat;
   uint32_t rootDirCluster = 0;

   if (g_SectorsPerFat == 0)
   { // fat32
      isFat32 = true;
      rootDirCluster =
          g_Data->BS.BootSector.ExtendedBootRecord.EBR32.RootDirectoryCluster;
      g_SectorsPerFat =
          g_Data->BS.BootSector.ExtendedBootRecord.EBR32.SectorsPerFat;
   }

   // open root directory file
   uint32_t rootDirLba;
   uint32_t rootDirSize;
   if (isFat32)
   {
      g_DataSectionLba = g_Data->BS.BootSector.ReservedSectors +
                         g_SectorsPerFat * g_Data->BS.BootSector.FatCount;
      rootDirLba = FAT_ClusterToLba(rootDirCluster);
      rootDirSize = 0;
   }
   else
   {
      rootDirLba = g_Data->BS.BootSector.ReservedSectors +
                   g_SectorsPerFat * g_Data->BS.BootSector.FatCount;
      rootDirSize =
          sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
      uint32_t rootDirSectors =
          (rootDirSize + g_Data->BS.BootSector.BytesPerSector - 1) /
          g_Data->BS.BootSector.BytesPerSector;
      // Data section starts AFTER the root directory (which spans multiple
      // sectors)
      g_DataSectionLba = rootDirLba + rootDirSectors;
   }

   g_Data->RootDirectory.Public.Handle = ROOT_DIRECTORY_HANDLE;
   g_Data->RootDirectory.Public.IsDirectory = true;
   g_Data->RootDirectory.Public.Position = 0;
   g_Data->RootDirectory.Public.Size =
       sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
   g_Data->RootDirectory.Opened = true;
   g_Data->RootDirectory.FirstCluster = rootDirLba;
   g_Data->RootDirectory.CurrentCluster = rootDirLba;
   g_Data->RootDirectory.CurrentSectorInCluster = 0;

   // Copy preloaded root directory (starts after 512-byte boot sector)
   uint8_t *preloaded_root = (uint8_t *)MEMORY_FAT_ADDR + SECTOR_SIZE;
   memcpy(g_Data->RootDirectory.Buffer, preloaded_root, SECTOR_SIZE);

   // calculate data section
   FAT_Detect(disk);

   // reset opened files
   for (int i = 0; i < MAX_FILE_HANDLES; i++)
      g_Data->OpenedFiles[i].Opened = false;

   return true;
}

uint32_t FAT_ClusterToLba(uint32_t cluster)
{
   uint32_t lba = g_DataSectionLba +
                  (cluster - 2) * g_Data->BS.BootSector.SectorsPerCluster;
   // Uncomment for debugging:
   // printf("FAT_ClusterToLba: cluster=%u, g_DataSectionLba=%u, result=%u\n",
   // cluster, g_DataSectionLba, lba);
   return lba;
}

FAT_File *FAT_OpenEntry(Partition *disk, FAT_DirectoryEntry *entry)
{
   // find empty handle
   int handle = -1;
   for (int i = 0; i < MAX_FILE_HANDLES && handle < 0; i++)
   {
      if (!g_Data->OpenedFiles[i].Opened) handle = i;
   }

   // out of handles
   if (handle < 0)
   {
      return false;
   }

   // setup vars
   FAT_FileData *fd = &g_Data->OpenedFiles[handle];
   fd->Public.Handle = handle;
   fd->Public.IsDirectory = (entry->Attributes & FAT_ATTRIBUTE_DIRECTORY) != 0;
   fd->Public.Position = 0;
   fd->Public.Size = entry->Size;
   memcpy(fd->Public.Name, entry->Name, 11);  // Save the name
   fd->FirstCluster =
       entry->FirstClusterLow + ((uint32_t)entry->FirstClusterHigh << 16);
   fd->CurrentCluster = fd->FirstCluster;
   fd->CurrentSectorInCluster = 0;

   if (!Partition_ReadSectors(disk, FAT_ClusterToLba(fd->CurrentCluster), 1,
                              fd->Buffer))
   {
      printf("FAT: open entry failed - read error cluster=%u lba=%u\n",
             fd->CurrentCluster, FAT_ClusterToLba(fd->CurrentCluster));
      printf("     file: ");
      for (int i = 0; i < 11; i++) printf("%c", entry->Name[i]);
      printf("\n");
      printf("     Note: ATA driver may not be working in kernel mode\n");
      // For now, just mark as opened with empty buffer to avoid crash
      // Real fix: implement working ATA driver in kernel
      fd->Opened = true;
      fd->Public.Size = 0; // Mark as empty to prevent further reads
      return &fd->Public;
   }

   fd->Opened = true;
   return &fd->Public;
}

uint32_t FAT_NextCluster(Partition *disk, uint32_t currentCluster)
{
   uint32_t fatIndex;

   if (g_FatType == 12)
      fatIndex = currentCluster * 3 / 2;
   else if (g_FatType == 16)
      fatIndex = currentCluster * 2;
   else if (g_FatType == 32)
      fatIndex = currentCluster * 4;

   uint32_t fatIndexSector = fatIndex / SECTOR_SIZE;
   if (fatIndexSector < g_Data->FatCachePos ||
       fatIndexSector >= g_Data->FatCachePos + FAT_CACHE_SIZE)
   {
      FAT_ReadFat(disk, fatIndexSector);
      g_Data->FatCachePos = fatIndexSector;
   }

   fatIndex -= (g_Data->FatCachePos * SECTOR_SIZE);
   uint32_t nextCluster;
   if (g_FatType == 12)
   {
      if (currentCluster % 2 == 0)
         nextCluster = (*(uint16_t *)(g_Data->FatCache + fatIndex)) & 0x0fff;
      else
         nextCluster = (*(uint16_t *)(g_Data->FatCache + fatIndex)) >> 4;

      if (nextCluster >= 0xff8)
      {
         nextCluster |= 0xfffff000;
      }
   }
   else if (g_FatType == 16)
   {
      nextCluster = *(uint16_t *)(g_Data->FatCache + fatIndex);
      if (nextCluster >= 0xfff8) nextCluster |= 0xffff0000;
   }
   else if (g_FatType == 32)
   {
      nextCluster = *(uint32_t *)(g_Data->FatCache + fatIndex);
   }
   return nextCluster;
}

uint32_t FAT_Read(Partition *disk, FAT_File *file, uint32_t byteCount,
                  void *dataOut)
{
   // get file data
   FAT_FileData *fd = (file->Handle == ROOT_DIRECTORY_HANDLE)
                          ? &g_Data->RootDirectory
                          : &g_Data->OpenedFiles[file->Handle];

   uint8_t *u8DataOut = (uint8_t *)dataOut;

   // don't read past the end of the file
   if (!fd->Public.IsDirectory ||
       (fd->Public.IsDirectory && fd->Public.Size != 0))
      byteCount = min(byteCount, fd->Public.Size - fd->Public.Position);

   while (byteCount > 0)
   {
      uint32_t leftInBuffer = SECTOR_SIZE - (fd->Public.Position % SECTOR_SIZE);
      uint32_t take = min(byteCount, leftInBuffer);

      memcpy(u8DataOut, fd->Buffer + fd->Public.Position % SECTOR_SIZE, take);
      u8DataOut += take;
      fd->Public.Position += take;
      byteCount -= take;

      // printf("leftInBuffer=%lu take=%lu\n", leftInBuffer, take);
      // See if we need to read more data
      if (leftInBuffer == take)
      {
         // Special handling for root directory
         if (fd->Public.Handle == ROOT_DIRECTORY_HANDLE)
         {
            ++fd->CurrentCluster;

            // Check if we've read past the end of root directory
            uint32_t rootDirSize = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
            uint32_t rootDirSectors = (rootDirSize + SECTOR_SIZE - 1) / SECTOR_SIZE;
            uint32_t rootDirLba = g_Data->BS.BootSector.ReservedSectors +
                                 g_SectorsPerFat * g_Data->BS.BootSector.FatCount;
            
            if (fd->CurrentCluster >= rootDirLba + rootDirSectors)
            {
               // End of root directory
               fd->Public.Size = fd->Public.Position;
               break;
            }

            // read next sector
            if (!Partition_ReadSectors(disk, fd->CurrentCluster, 1, fd->Buffer))
            {
               printf("FAT: read error!\n");
               break;
            }
         }
         else
         {
            // calculate next cluster & sector to read
            if (++fd->CurrentSectorInCluster >=
                g_Data->BS.BootSector.SectorsPerCluster)
            {
               fd->CurrentSectorInCluster = 0;
               fd->CurrentCluster = FAT_NextCluster(disk, fd->CurrentCluster);
            }

            // Check for end-of-chain based on FAT type
            uint32_t eofMarker = (g_FatType == 12)   ? 0xFF8
                                 : (g_FatType == 16) ? 0xFFF8
                                                     : 0xFFFFFFF8;
            if (fd->CurrentCluster >= eofMarker)
            {
               // Mark end of file
               fd->Public.Size = fd->Public.Position;
               break;
            }

            // read next sector
            if (!Partition_ReadSectors(disk,
                                       FAT_ClusterToLba(fd->CurrentCluster) +
                                           fd->CurrentSectorInCluster,
                                       1, fd->Buffer))
            {
               printf("FAT: read error!\n");
               break;
            }
         }
      }
   }

   return u8DataOut - (uint8_t *)dataOut;
}

bool FAT_ReadEntry(Partition *disk, FAT_File *file,
                   FAT_DirectoryEntry *dirEntry)
{
   return FAT_Read(disk, file, sizeof(FAT_DirectoryEntry), dirEntry) ==
          sizeof(FAT_DirectoryEntry);
}

void FAT_Close(FAT_File *file)
{
   if (file->Handle == ROOT_DIRECTORY_HANDLE)
   {
      file->Position = 0;
      g_Data->RootDirectory.CurrentCluster = g_Data->RootDirectory.FirstCluster;
   }
   else
   {
      g_Data->OpenedFiles[file->Handle].Opened = false;
   }
}

bool FAT_FindFile(Partition *disk, FAT_File *file, const char *name,
                  FAT_DirectoryEntry *entryOut)
{
   printf("FAT_FindFile: entry, name='%s'\n", name);
   
   char fatName[12];
   FAT_DirectoryEntry entry;

   // Reset directory position to start searching from the beginning
   printf("FAT_FindFile: calling FAT_Seek\n");
   FAT_Seek(disk, file, 0);
   printf("FAT_FindFile: FAT_Seek done\n");

   // convert from name to fat name
   memset(fatName, ' ', sizeof(fatName));
   fatName[11] = '\0';

   printf("FAT_FindFile: converting name\n");
   const char *ext = strchr(name, '.');
   if (ext == NULL) 
      ext = name + strlen(name);  // Point to end of string if no extension

   printf("FAT_FindFile: ext found\n");
   // Copy basename (max 8 chars before extension)
   int nameLen = (ext - name > 8) ? 8 : (ext - name);
   printf("FAT_FindFile: nameLen=%d, looping...\n", nameLen);
   for (int i = 0; i < nameLen && name[i] && name[i] != '.'; i++)
      fatName[i] = toupper(name[i]);

   printf("FAT_FindFile: basename done\n");
   // Copy extension (max 3 chars after the dot)
   if (ext != name + strlen(name) && *ext == '.')
   {
      for (int i = 0; i < 3 && ext[i + 1]; i++)
         fatName[i + 8] = toupper(ext[i + 1]);
   }

   printf("FAT_FindFile: name conversion done, fatName='%s'\n", fatName);
   printf("FAT_FindFile: about to read entries\n");

   while (FAT_ReadEntry(disk, file, &entry))
   {
      // Skip LFN entries (attribute 0x0F)
      if ((entry.Attributes & 0x0F) == 0x0F) continue;

      if (memcmp(fatName, entry.Name, 11) == 0)
      {
         uint32_t cluster =
             entry.FirstClusterLow + ((uint32_t)entry.FirstClusterHigh << 16);
         *entryOut = entry;
         return true;
      }
   }

   return false;
}

FAT_File *FAT_Open(Partition *disk, const char *path)
{
   char name[MAX_PATH_SIZE];

   // ignore leading slash
   if (path[0] == '/') path++;

   // If path is empty or just "/", return root directory
   if (path[0] == '\0') return &g_Data->RootDirectory.Public;

   FAT_File *current = &g_Data->RootDirectory.Public;
   FAT_File *previous = NULL;

   while (*path)
   {
      // extract next file name from path
      bool isLast = false;
      const char *delim = strchr(path, '/');
      if (delim != NULL)
      {
         memcpy(name, path, delim - path);
         name[delim - path] = '\0';
         path = delim + 1;
      }
      else
      {
         unsigned len = strlen(path);
         memcpy(name, path, len);
         name[len] = '\0';
         path += len;
         isLast = true;
      }

      // find directory entry in current directory
      FAT_DirectoryEntry entry;
      if (FAT_FindFile(disk, current, name, &entry))
      {
         // Close previous directory (but not root if it's the current one)
         if (previous != NULL && previous->Handle != ROOT_DIRECTORY_HANDLE)
         {
            FAT_Close(previous);
         }

         // check if directory
         if (!isLast && (entry.Attributes & FAT_ATTRIBUTE_DIRECTORY) == 0)
         {
            printf("FAT: %s not a directory\n", name);
            return NULL;
         }

         // open new directory entry
         previous = current;
         current = FAT_OpenEntry(disk, &entry);
      }
      else
      {
         // Close previous directory (but not root)
         if (previous != NULL && previous->Handle != ROOT_DIRECTORY_HANDLE)
         {
            FAT_Close(previous);
         }

         printf("FAT: %s not found\n", name);
         return NULL;
      }
   }

   return current;
}

bool FAT_Seek(Partition *disk, FAT_File *file, uint32_t position)
{
   FAT_FileData *fd = (file->Handle == ROOT_DIRECTORY_HANDLE)
                          ? &g_Data->RootDirectory
                          : &g_Data->OpenedFiles[file->Handle];

   // don't seek past end
   if (position > fd->Public.Size) return false;

   fd->Public.Position = position;

   // compute cluster/sector for the position
   uint32_t bytesPerSector = g_Data->BS.BootSector.BytesPerSector;
   uint32_t sectorsPerCluster = g_Data->BS.BootSector.SectorsPerCluster;
   uint32_t clusterBytes = bytesPerSector * sectorsPerCluster;

   if (fd->Public.Handle == ROOT_DIRECTORY_HANDLE)
   {
      // root directory is organized by sectors (not clusters)
      uint32_t sectorIndex = position / bytesPerSector;
      fd->CurrentCluster = fd->FirstCluster + sectorIndex;
      fd->CurrentSectorInCluster = 0;

      if (!Partition_ReadSectors(disk, fd->CurrentCluster, 1, fd->Buffer))
      {
         printf("FAT: seek read error (root)\n");
         return false;
      }
   }
   else
   {
      uint32_t clusterIndex = position / clusterBytes;
      uint32_t sectorInCluster = (position % clusterBytes) / bytesPerSector;

      // walk cluster chain clusterIndex times from first cluster
      uint32_t c = fd->FirstCluster;
      for (uint32_t i = 0; i < clusterIndex; i++)
      {
         c = FAT_NextCluster(disk, c);
         uint32_t eofMarker = (g_FatType == 12)   ? 0xFF8
                              : (g_FatType == 16) ? 0xFFF8
                                                  : 0xFFFFFFF8;
         if (c >= eofMarker)
         {
            // invalid / end of chain
            fd->Public.Size = fd->Public.Position;
            return false;
         }
      }

      fd->CurrentCluster = c;
      fd->CurrentSectorInCluster = sectorInCluster;

      if (!Partition_ReadSectors(disk,
                                 FAT_ClusterToLba(fd->CurrentCluster) +
                                     fd->CurrentSectorInCluster,
                                 1, fd->Buffer))
      {
         printf("FAT: seek read error (file)\n");
         return false;
      }
   }

   return true;
}

bool FAT_WriteEntry(Partition *disk, FAT_File *file,
                    const FAT_DirectoryEntry *dirEntry)
{
   // Validate file handle
   if (!file || file->Handle < 0 || file->Handle >= MAX_FILE_HANDLES)
   {
      return false;
   }

   // Only write to directory files
   if (!file->IsDirectory)
   {
      printf("FAT: WriteEntry called on non-directory file\n");
      return false;
   }

   FAT_FileData *fd = &g_Data->OpenedFiles[file->Handle];

   // Calculate which sector contains the current directory entry
   // Each sector has 512 bytes / 32 bytes per entry = 16 entries per sector
   uint32_t entryOffset = file->Position;
   uint32_t sectorOffset = (entryOffset / 16) * SECTOR_SIZE;
   uint32_t entryOffsetInSector = (entryOffset % 16) * 32;

   // Load sector if needed
   if (fd->CurrentSectorInCluster != (sectorOffset / SECTOR_SIZE))
   {
      fd->CurrentSectorInCluster = sectorOffset / SECTOR_SIZE;

      if (!Partition_ReadSectors(disk,
                                 FAT_ClusterToLba(fd->CurrentCluster) +
                                     fd->CurrentSectorInCluster,
                                 1, fd->Buffer))
      {
         printf("FAT: WriteEntry read error\n");
         return false;
      }
   }

   // Copy directory entry to buffer
   memcpy(&fd->Buffer[entryOffsetInSector], dirEntry, sizeof(FAT_DirectoryEntry));

   // Write sector back to disk
   if (!Partition_WriteSectors(disk,
                               FAT_ClusterToLba(fd->CurrentCluster) +
                                   fd->CurrentSectorInCluster,
                               1, fd->Buffer))
   {
      printf("FAT: WriteEntry write error\n");
      return false;
   }

   // Advance position to next entry
   file->Position++;

   return true;
}

uint32_t FAT_Write(Partition *disk, FAT_File *file, uint32_t byteCount,
                   const void *dataIn)
{
   // Get file data
   FAT_FileData *fd = &g_Data->OpenedFiles[file->Handle];

   // Validate file is opened and writable (not root directory or read-only)
   if (!fd->Opened || file->Handle == ROOT_DIRECTORY_HANDLE ||
       (file->IsDirectory))
   {
      printf("FAT: Write called on invalid file\n");
      return 0;
   }

   // If file has no cluster assigned (after truncate or new file), allocate one
   if (fd->CurrentCluster == 0)
   {
      // Simple linear search for first free cluster
      uint32_t testCluster = 2;  // Cluster 0 and 1 are reserved
      uint32_t maxClusters = g_TotalSectors / g_Data->BS.BootSector.SectorsPerCluster;
      uint32_t eofMarker = (g_FatType == 12)   ? 0xFF8
                           : (g_FatType == 16) ? 0xFFF8
                                               : 0xFFFFFFF8;

      while (testCluster < maxClusters)
      {
         uint32_t nextClusterValue = FAT_NextCluster(disk, testCluster);
         if (nextClusterValue == 0)  // Found free cluster
         {
            fd->FirstCluster = testCluster;
            fd->CurrentCluster = testCluster;
            fd->CurrentSectorInCluster = 0;
            
            // Mark cluster as end-of-chain in FAT
            uint32_t fatByteOffset;
            if (g_FatType == 12)
               fatByteOffset = testCluster * 3 / 2;
            else if (g_FatType == 16)
               fatByteOffset = testCluster * 2;
            else
               fatByteOffset = testCluster * 4;

            uint32_t fatSectorOffset = fatByteOffset / SECTOR_SIZE;
            uint32_t fatByteOffsetInSector = fatByteOffset % SECTOR_SIZE;
            uint32_t fatSectorLba = g_Data->BS.BootSector.ReservedSectors + fatSectorOffset;

            uint8_t fatBuffer[SECTOR_SIZE];
            if (!Partition_ReadSectors(disk, fatSectorLba, 1, fatBuffer))
               break;

            // Mark as EOF
            if (g_FatType == 12)
            {
               if (testCluster % 2 == 0)
                  *(uint16_t *)(fatBuffer + fatByteOffsetInSector) = 0xFFF;
               else
                  *(uint16_t *)(fatBuffer + fatByteOffsetInSector) = 0xFFFF;
            }
            else if (g_FatType == 16)
               *(uint16_t *)(fatBuffer + fatByteOffsetInSector) = 0xFFFF;
            else
               *(uint32_t *)(fatBuffer + fatByteOffsetInSector) = 0xFFFFFFFF;

            Partition_WriteSectors(disk, fatSectorLba, 1, fatBuffer);
            
            // Read first sector of new cluster
            if (!Partition_ReadSectors(disk, FAT_ClusterToLba(fd->CurrentCluster), 1, fd->Buffer))
            {
               printf("FAT: Write - failed to read new cluster\n");
               return 0;
            }
            break;
         }
         testCluster++;
      }

      if (fd->CurrentCluster == 0)
      {
         printf("FAT: Write - no free clusters available\n");
         return 0;
      }
   }

   const uint8_t *u8DataIn = (const uint8_t *)dataIn;
   uint32_t bytesWritten = 0;

   while (byteCount > 0)
   {
      uint32_t offsetInSector = fd->Public.Position % SECTOR_SIZE;
      uint32_t spaceInSector = SECTOR_SIZE - offsetInSector;
      uint32_t toWrite = min(byteCount, spaceInSector);

      // Copy data into current sector buffer
      memcpy(&fd->Buffer[offsetInSector], u8DataIn, toWrite);

      // Write sector to disk if it's now full or this is the last write
      if (offsetInSector + toWrite == SECTOR_SIZE || byteCount == toWrite)
      {
         if (!Partition_WriteSectors(disk,
                                    FAT_ClusterToLba(fd->CurrentCluster) +
                                        fd->CurrentSectorInCluster,
                                    1, fd->Buffer))
         {
            printf("FAT: Write error at cluster=%u sector=%u\n",
                   fd->CurrentCluster, fd->CurrentSectorInCluster);
            return bytesWritten;
         }
      }

      // Update counters
      u8DataIn += toWrite;
      fd->Public.Position += toWrite;
      bytesWritten += toWrite;
      byteCount -= toWrite;

      // Move to next sector if needed
      if (offsetInSector + toWrite == SECTOR_SIZE)
      {
         if (++fd->CurrentSectorInCluster >=
             g_Data->BS.BootSector.SectorsPerCluster)
         {
            fd->CurrentSectorInCluster = 0;
            fd->CurrentCluster = FAT_NextCluster(disk, fd->CurrentCluster);

            // Check for end-of-chain (invalid write)
            uint32_t eofMarker = (g_FatType == 12)   ? 0xFF8
                                 : (g_FatType == 16) ? 0xFFF8
                                                     : 0xFFFFFFF8;
            if (fd->CurrentCluster >= eofMarker)
            {
               printf("FAT: Write reached end of cluster chain\n");
               return bytesWritten;
            }
         }

         // Read next sector buffer (to preserve data if partial write)
         if (byteCount > 0)
         {
            if (!Partition_ReadSectors(disk,
                                       FAT_ClusterToLba(fd->CurrentCluster) +
                                           fd->CurrentSectorInCluster,
                                       1, fd->Buffer))
            {
               printf("FAT: Write read error\n");
               return bytesWritten;
            }
         }
      }
   }

   // Flush final partial sector to disk
   if (!Partition_WriteSectors(disk,
                              FAT_ClusterToLba(fd->CurrentCluster) +
                                  fd->CurrentSectorInCluster,
                              1, fd->Buffer))
   {
      printf("FAT: Write final flush error\n");
      return bytesWritten;
   }

   // Update file size if we wrote past the end
   if (fd->Public.Position > fd->Public.Size)
   {
      fd->Public.Size = fd->Public.Position;
   }

   return bytesWritten;
}

bool FAT_Truncate(Partition *disk, FAT_File *file)
{
   // Validate file is opened and not a directory or root
   if (!file || file->Handle == ROOT_DIRECTORY_HANDLE ||
       file->Handle < 0 || file->Handle >= MAX_FILE_HANDLES)
   {
      return false;
   }

   FAT_FileData *fd = &g_Data->OpenedFiles[file->Handle];

   if (!fd->Opened || file->IsDirectory)
   {
      printf("FAT: Truncate called on invalid file\n");
      return false;
   }

   // If file is already empty, nothing to do
   if (fd->Public.Size == 0)
   {
      fd->Public.Position = 0;
      fd->CurrentCluster = fd->FirstCluster;
      fd->CurrentSectorInCluster = 0;
      return true;
   }

   // Walk the cluster chain and mark all clusters as free in FAT
   uint32_t currentCluster = fd->FirstCluster;
   uint32_t eofMarker = (g_FatType == 12)   ? 0xFF8
                        : (g_FatType == 16) ? 0xFFF8
                                            : 0xFFFFFFF8;

   uint8_t fatBuffer[SECTOR_SIZE];
   int clusterCount = 0;

   while (currentCluster < eofMarker && clusterCount < 1000)  // safety limit
   {
      clusterCount++;

      // Calculate FAT entry offset
      uint32_t fatByteOffset;
      if (g_FatType == 12)
         fatByteOffset = currentCluster * 3 / 2;
      else if (g_FatType == 16)
         fatByteOffset = currentCluster * 2;
      else // FAT32
         fatByteOffset = currentCluster * 4;

      // Convert byte offset to sector offset within FAT
      uint32_t fatSectorOffset = fatByteOffset / SECTOR_SIZE;
      uint32_t fatByteOffsetInSector = fatByteOffset % SECTOR_SIZE;

      // Calculate actual LBA: ReservedSectors + SectorNumber
      uint32_t fatSectorLba = g_Data->BS.BootSector.ReservedSectors + fatSectorOffset;

      // Read the FAT sector
      if (!Partition_ReadSectors(disk, fatSectorLba, 1, fatBuffer))
      {
         printf("FAT: Truncate read error at cluster %u\n", currentCluster);
         return false;
      }

      // Get next cluster before clearing current
      uint32_t nextCluster;
      if (g_FatType == 12)
      {
         if (currentCluster % 2 == 0)
            nextCluster = (*(uint16_t *)(fatBuffer + fatByteOffsetInSector)) & 0x0fff;
         else
            nextCluster = (*(uint16_t *)(fatBuffer + fatByteOffsetInSector)) >> 4;

         if (nextCluster >= 0xff8)
            nextCluster |= 0xfffff000;
      }
      else if (g_FatType == 16)
      {
         nextCluster = *(uint16_t *)(fatBuffer + fatByteOffsetInSector);
         if (nextCluster >= 0xfff8) nextCluster |= 0xffff0000;
      }
      else // FAT32
      {
         nextCluster = *(uint32_t *)(fatBuffer + fatByteOffsetInSector);
      }

      // Validate nextCluster - if it's suspiciously large or invalid, treat as EOF
      if (nextCluster > 0x0FFFFFF || (nextCluster != 0 && nextCluster < 2))
      {
         printf("FAT_Truncate: Invalid nextCluster value 0x%x, treating as EOF\n", nextCluster);
         nextCluster = eofMarker;
      }

      // Now clear the current cluster entry
      if (g_FatType == 12)
      {
         if (currentCluster % 2 == 0)
            *(uint16_t *)(fatBuffer + fatByteOffsetInSector) &= 0xF000;
         else
            *(uint16_t *)(fatBuffer + fatByteOffsetInSector) &= 0x000F;
      }
      else if (g_FatType == 16)
      {
         *(uint16_t *)(fatBuffer + fatByteOffsetInSector) = 0x0000;
      }
      else // FAT32
      {
         *(uint32_t *)(fatBuffer + fatByteOffsetInSector) = 0x00000000;
      }

      // Write FAT sector back
      if (!Partition_WriteSectors(disk, fatSectorLba, 1, fatBuffer))
      {
         printf("FAT: Truncate write FAT error at LBA %u\n", fatSectorLba);
         return false;
      }

      currentCluster = nextCluster;
   }

   // Reset file state - don't reuse freed clusters
   fd->Public.Position = 0;
   fd->Public.Size = 0;
   fd->FirstCluster = 0;  // Mark as no cluster assigned
   fd->CurrentCluster = 0;
   fd->CurrentSectorInCluster = 0;

   // Clear buffer
   memset(fd->Buffer, 0, SECTOR_SIZE);

   return true;
}

bool FAT_UpdateEntry(Partition *disk, FAT_File *file)
{
   // Validate file
   if (!file || file->Handle == ROOT_DIRECTORY_HANDLE ||
       file->Handle < 0 || file->Handle >= MAX_FILE_HANDLES)
   {
      return false;
   }

   FAT_FileData *fd = &g_Data->OpenedFiles[file->Handle];

   if (!fd->Opened || file->IsDirectory)
   {
      return false;
   }

   // Search for the file in root directory to find and update its entry
   FAT_File *root = &g_Data->RootDirectory.Public;
   FAT_Seek(disk, root, 0);  // Start from beginning of root

   FAT_DirectoryEntry entry;
   uint32_t entryPosition = 0;
   
   while (FAT_ReadEntry(disk, root, &entry))
   {
      entryPosition = root->Position - sizeof(FAT_DirectoryEntry);
      
      // Skip LFN entries and empty entries
      if ((entry.Attributes & 0x0F) == 0x0F || entry.Name[0] == 0x00)
         continue;

      // Check if this is our file (match by name)
      if (memcmp(entry.Name, fd->Public.Name, 11) == 0)
      {
         // Found it! Update the entry
         entry.Size = fd->Public.Size;
         entry.FirstClusterLow = fd->FirstCluster & 0xFFFF;
         entry.FirstClusterHigh = (fd->FirstCluster >> 16) & 0xFFFF;

         // Calculate which sector contains this entry
         uint32_t sectorInDirectory = entryPosition / SECTOR_SIZE;
         uint32_t offsetInSector = entryPosition % SECTOR_SIZE;

         // Read the sector containing the entry
         uint8_t sectorBuffer[SECTOR_SIZE];
         if (!Partition_ReadSectors(disk, FAT_ClusterToLba(2) + sectorInDirectory, 1, sectorBuffer))
         {
            printf("FAT: UpdateEntry read error\n");
            return false;
         }

         // Update the entry in the buffer
         memcpy(&sectorBuffer[offsetInSector], &entry, sizeof(FAT_DirectoryEntry));

         // Write the sector back
         if (!Partition_WriteSectors(disk, FAT_ClusterToLba(2) + sectorInDirectory, 1, sectorBuffer))
         {
            printf("FAT: UpdateEntry write error\n");
            return false;
         }

         return true;
      }
   }

   printf("FAT: UpdateEntry - file not found in directory\n");
   return false;
}

FAT_File *FAT_Create(Partition *disk, const char *name)
{
   printf("FAT_Create: called with name='%s'\n", name);
   
   // Convert filename to FAT format (11-byte name)
   char fatName[12];
   memset(fatName, ' ', sizeof(fatName));
   fatName[11] = '\0';

   const char *ext = strchr(name, '.');
   if (ext == NULL) 
      ext = name + strlen(name);  // Point to end of string if no extension

   // Copy basename (max 8 chars before extension)
   int nameLen = (ext - name > 8) ? 8 : (ext - name);
   for (int i = 0; i < nameLen && name[i] && name[i] != '.'; i++)
      fatName[i] = toupper(name[i]);

   // Copy extension (max 3 chars after the dot)
   if (ext != name + strlen(name) && *ext == '.')
   {
      for (int i = 0; i < 3 && ext[i + 1]; i++)
         fatName[i + 8] = toupper(ext[i + 1]);
   }

   // Check if file already exists
   FAT_DirectoryEntry existingEntry;
   printf("FAT_Create: checking if '%s' exists\n", name);
   if (FAT_FindFile(disk, &g_Data->RootDirectory.Public, name, &existingEntry))
   {
      printf("FAT_Create: file '%s' already exists\n", name);
      return NULL;
   }
   printf("FAT_Create: file does not exist, proceeding\n");

   // Find first free cluster for the file
   uint32_t firstFreeCluster = 0;
   uint32_t maxClusters = (g_TotalSectors - g_DataSectionLba) /
                           g_Data->BS.BootSector.SectorsPerCluster;
   printf("FAT_Create: max clusters = %u\n", maxClusters);

   // Limit search to prevent excessive scanning
   uint32_t maxSearchClusters = (maxClusters > 1000) ? 1000 : maxClusters;
   
   for (uint32_t testCluster = 2; testCluster < maxSearchClusters; testCluster++)
   {
      uint32_t nextClusterVal = FAT_NextCluster(disk, testCluster);
      if (nextClusterVal == 0)
      {
         firstFreeCluster = testCluster;
         printf("FAT_Create: found free cluster %u\n", firstFreeCluster);
         break;
      }
   }

   if (firstFreeCluster == 0)
   {
      printf("FAT_Create: no free clusters available\n");
      return NULL;
   }

   // Mark cluster as end-of-chain in FAT
   uint32_t fatByteOffset;
   if (g_FatType == 12)
      fatByteOffset = firstFreeCluster * 3 / 2;
   else if (g_FatType == 16)
      fatByteOffset = firstFreeCluster * 2;
   else
      fatByteOffset = firstFreeCluster * 4;

   uint32_t fatSectorOffset = fatByteOffset / SECTOR_SIZE;
   uint32_t fatByteOffsetInSector = fatByteOffset % SECTOR_SIZE;
   uint32_t fatSectorLba = g_Data->BS.BootSector.ReservedSectors + fatSectorOffset;

   uint8_t fatBuffer[SECTOR_SIZE];
   if (!Partition_ReadSectors(disk, fatSectorLba, 1, fatBuffer))
   {
      printf("FAT_Create: FAT read error\n");
      return NULL;
   }

   // Mark as EOF
   if (g_FatType == 12)
   {
      if (firstFreeCluster % 2 == 0)
         *(uint16_t *)(fatBuffer + fatByteOffsetInSector) = 0xFFF;
      else
         *(uint16_t *)(fatBuffer + fatByteOffsetInSector) = 0xFFFF;
   }
   else if (g_FatType == 16)
      *(uint16_t *)(fatBuffer + fatByteOffsetInSector) = 0xFFFF;
   else
      *(uint32_t *)(fatBuffer + fatByteOffsetInSector) = 0xFFFFFFFF;

   if (!Partition_WriteSectors(disk, fatSectorLba, 1, fatBuffer))
   {
      printf("FAT_Create: FAT write error\n");
      return NULL;
   }

   // Create directory entry
   FAT_DirectoryEntry newEntry;
   memcpy(newEntry.Name, fatName, 11);
   newEntry.Attributes = 0x20;  // Archive attribute
   newEntry._Reserved = 0;
   newEntry.CreatedTimeTenths = 0;
   newEntry.CreatedTime = 0;
   newEntry.CreatedDate = 0;
   newEntry.AccessedDate = 0;
   newEntry.FirstClusterHigh = (firstFreeCluster >> 16) & 0xFFFF;
   newEntry.ModifiedTime = 0;
   newEntry.ModifiedDate = 0;
   newEntry.FirstClusterLow = firstFreeCluster & 0xFFFF;
   newEntry.Size = 0;  // Start with empty file

   // Find empty slot in root directory
   FAT_File *root = &g_Data->RootDirectory.Public;
   FAT_Seek(disk, root, 0);

   FAT_DirectoryEntry dirEntry;
   uint32_t searchPosition = 0;
   int entryCount = 0;
   int maxEntries = g_Data->BS.BootSector.DirEntryCount;

   while (FAT_ReadEntry(disk, root, &dirEntry) && entryCount < maxEntries)
   {
      entryCount++;
      // Found empty slot (first byte is 0x00) or deleted entry (0xE5)
      if (dirEntry.Name[0] == 0x00 || (uint8_t)dirEntry.Name[0] == 0xE5)
      {
         // Go back to this position
         FAT_Seek(disk, root, searchPosition);
         
         // Write the new entry
         if (!FAT_WriteEntry(disk, root, &newEntry))
         {
            printf("FAT_Create: failed to write directory entry\n");
            return NULL;
         }

         // Open the file
         FAT_File *file = FAT_OpenEntry(disk, &newEntry);
         printf("FAT_Create: created file '%s' at cluster %u, opened: %s\n", 
                name, firstFreeCluster, file ? "yes" : "no");
         if (file != NULL)
         {
            printf("FAT_Create: file handle = %d\n", file->Handle);
         }
         return file;
      }
      searchPosition = root->Position;
   }

   printf("FAT_Create: no space in root directory (checked %u entries)\n", entryCount);
   return NULL;
}

bool FAT_Delete(Partition *disk, const char *name)
{
   // Find the file to delete
   FAT_DirectoryEntry entry;
   if (!FAT_FindFile(disk, &g_Data->RootDirectory.Public, name, &entry))
   {
      printf("FAT_Delete: file '%s' not found\n", name);
      return false;
   }

   // Get cluster info from entry
   uint32_t firstCluster = entry.FirstClusterLow + ((uint32_t)entry.FirstClusterHigh << 16);

   // If it's a directory, recursively delete its contents first
   if (entry.Attributes & FAT_ATTRIBUTE_DIRECTORY)
   {
      // Open the directory for reading
      FAT_DirectoryEntry dirEntry;
      dirEntry.FirstClusterLow = firstCluster & 0xFFFF;
      dirEntry.FirstClusterHigh = (firstCluster >> 16) & 0xFFFF;
      dirEntry.Size = 0;
      dirEntry.Attributes = FAT_ATTRIBUTE_DIRECTORY;
      memcpy(dirEntry.Name, entry.Name, 11);
      
      FAT_File *dir = FAT_OpenEntry(disk, &dirEntry);
      if (!dir)
      {
         printf("FAT_Delete: failed to open directory\n");
         return false;
      }
      
      FAT_DirectoryEntry subEntry;
      while (FAT_ReadEntry(disk, dir, &subEntry))
      {
         // Skip empty entries and LFN entries
         if ((subEntry.Attributes & 0x0F) == 0x0F || subEntry.Name[0] == 0x00 || (uint8_t)subEntry.Name[0] == 0xE5)
            continue;
         
         // Skip . and .. entries
         if ((subEntry.Name[0] == '.' && subEntry.Name[1] == ' ') ||
             (subEntry.Name[0] == '.' && subEntry.Name[1] == '.' && subEntry.Name[2] == ' '))
            continue;
         
         // Recursively delete subdirectory/file
         if (subEntry.Attributes & FAT_ATTRIBUTE_DIRECTORY)
         {
            if (!FAT_Delete(disk, (const char *)subEntry.Name))
            {
               printf("FAT_Delete: failed to delete subdirectory\n");
               FAT_Close(dir);
               return false;
            }
         }
      }
      
      FAT_Close(dir);
   }

   // Free all clusters in the chain
   uint32_t currentCluster = firstCluster;
   uint32_t eofMarker = (g_FatType == 12)   ? 0xFF8
                        : (g_FatType == 16) ? 0xFFF8
                                            : 0xFFFFFFF8;

   uint8_t fatBuffer[SECTOR_SIZE];
   int clusterCount = 0;

   while (currentCluster < eofMarker && clusterCount < 1000)  // safety limit
   {
      clusterCount++;

      // Calculate FAT entry offset
      uint32_t fatByteOffset;
      if (g_FatType == 12)
         fatByteOffset = currentCluster * 3 / 2;
      else if (g_FatType == 16)
         fatByteOffset = currentCluster * 2;
      else
         fatByteOffset = currentCluster * 4;

      uint32_t fatSectorOffset = fatByteOffset / SECTOR_SIZE;
      uint32_t fatByteOffsetInSector = fatByteOffset % SECTOR_SIZE;
      uint32_t fatSectorLba = g_Data->BS.BootSector.ReservedSectors + fatSectorOffset;

      if (!Partition_ReadSectors(disk, fatSectorLba, 1, fatBuffer))
      {
         printf("FAT_Delete: FAT read error at cluster %u\n", currentCluster);
         return false;
      }

      // Get next cluster
      uint32_t nextCluster;
      if (g_FatType == 12)
      {
         if (currentCluster % 2 == 0)
            nextCluster = (*(uint16_t *)(fatBuffer + fatByteOffsetInSector)) & 0x0fff;
         else
            nextCluster = (*(uint16_t *)(fatBuffer + fatByteOffsetInSector)) >> 4;

         if (nextCluster >= 0xff8)
            nextCluster |= 0xfffff000;
      }
      else if (g_FatType == 16)
      {
         nextCluster = *(uint16_t *)(fatBuffer + fatByteOffsetInSector);
         if (nextCluster >= 0xfff8) nextCluster |= 0xffff0000;
      }
      else
      {
         nextCluster = *(uint32_t *)(fatBuffer + fatByteOffsetInSector);
      }

      // Mark cluster as free
      if (g_FatType == 12)
      {
         if (currentCluster % 2 == 0)
            *(uint16_t *)(fatBuffer + fatByteOffsetInSector) &= 0xF000;
         else
            *(uint16_t *)(fatBuffer + fatByteOffsetInSector) &= 0x000F;
      }
      else if (g_FatType == 16)
         *(uint16_t *)(fatBuffer + fatByteOffsetInSector) = 0x0000;
      else
         *(uint32_t *)(fatBuffer + fatByteOffsetInSector) = 0x00000000;

      if (!Partition_WriteSectors(disk, fatSectorLba, 1, fatBuffer))
      {
         printf("FAT_Delete: FAT write error at LBA %u\n", fatSectorLba);
         return false;
      }

      currentCluster = nextCluster;
   }

   // Mark directory entry as deleted (set first byte to 0xE5)
   FAT_File *root = &g_Data->RootDirectory.Public;
   FAT_Seek(disk, root, 0);

   FAT_DirectoryEntry dirEntry;
   uint32_t entryPosition = 0;

   while (FAT_ReadEntry(disk, root, &dirEntry))
   {
      entryPosition = root->Position - sizeof(FAT_DirectoryEntry);

      if ((dirEntry.Attributes & 0x0F) == 0x0F || dirEntry.Name[0] == 0x00)
         continue;

      if (memcmp(dirEntry.Name, entry.Name, 11) == 0)
      {
         // Found the entry. Read its sector, mark as deleted, write back
         uint32_t sectorInDirectory = entryPosition / SECTOR_SIZE;
         uint32_t offsetInSector = entryPosition % SECTOR_SIZE;

         uint8_t sectorBuffer[SECTOR_SIZE];
         if (!Partition_ReadSectors(disk, FAT_ClusterToLba(2) + sectorInDirectory, 1, sectorBuffer))
         {
            printf("FAT_Delete: read error\n");
            return false;
         }

         // Mark as deleted (0xE5 in first byte of name)
         sectorBuffer[offsetInSector] = 0xE5;

         if (!Partition_WriteSectors(disk, FAT_ClusterToLba(2) + sectorInDirectory, 1, sectorBuffer))
         {
            printf("FAT_Delete: write error\n");
            return false;
         }

         printf("FAT_Delete: deleted file '%s'\n", name);
         return true;
      }
   }

   printf("FAT_Delete: entry not found in directory\n");
   return false;
}

