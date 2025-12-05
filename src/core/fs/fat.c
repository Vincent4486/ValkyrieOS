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
static uint32_t g_RootDirLba = 0;
static uint32_t g_RootDirSectors = 0;

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

   // Debug: print BPB values
   printf("FAT_Initialize: BPB BytesPerSector=%u, SectorsPerCluster=%u\n",
          g_Data->BS.BootSector.BytesPerSector,
          g_Data->BS.BootSector.SectorsPerCluster);

   // Validate critical BPB values to prevent divide-by-zero later
   if (g_Data->BS.BootSector.BytesPerSector == 0 ||
       g_Data->BS.BootSector.SectorsPerCluster == 0)
   {
      printf("FAT_Initialize: Invalid BPB (BytesPerSector=%u, "
             "SectorsPerCluster=%u)\n",
             g_Data->BS.BootSector.BytesPerSector,
             g_Data->BS.BootSector.SectorsPerCluster);
      return false;
   }

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
      // Data section starts after reserved + FAT areas
      g_DataSectionLba = g_Data->BS.BootSector.ReservedSectors +
                         g_SectorsPerFat * g_Data->BS.BootSector.FatCount;

      printf("FAT_Initialize (FAT32): ReservedSectors=%u, SectorsPerFat=%u, FatCount=%u\n",
             g_Data->BS.BootSector.ReservedSectors, g_SectorsPerFat, g_Data->BS.BootSector.FatCount);
      printf("FAT_Initialize: g_DataSectionLba=%u\n", g_DataSectionLba);

      // For FAT32 the root directory is a normal cluster chain starting at
      // RootDirectoryCluster. Keep cluster number in
      // RootDirectory.FirstCluster. We'll keep g_RootDirLba/g_RootDirSectors =
      // 0 to indicate "clustered" root.
      g_RootDirLba = 0;
      g_RootDirSectors = 0;
      rootDirLba =
          FAT_ClusterToLba(rootDirCluster); // temp LBA for first cluster
      rootDirSize = 0;
   }
   else
   {
      // FAT12/16: root directory stored in a fixed area (immediately after
      // FATs)
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

      g_RootDirLba = rootDirLba;
      g_RootDirSectors = rootDirSectors;
   }

   g_Data->RootDirectory.Public.Handle = ROOT_DIRECTORY_HANDLE;
   g_Data->RootDirectory.Public.IsDirectory = true;
   g_Data->RootDirectory.Public.Position = 0;
   if (isFat32)
      // For FAT32, root is a cluster chain; use a large safe size
      g_Data->RootDirectory.Public.Size = 0x1000000; // 16 MiB max
   else
      g_Data->RootDirectory.Public.Size =
          sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
   g_Data->RootDirectory.Opened = true;
   if (isFat32)
   {
      // For FAT32 we keep cluster numbers for root directory
      g_Data->RootDirectory.FirstCluster = rootDirCluster;
      g_Data->RootDirectory.CurrentCluster = rootDirCluster;
      g_Data->RootDirectory.CurrentSectorInCluster = 0;

      // Read first sector of root cluster into buffer
      Partition_ReadSectors(disk, FAT_ClusterToLba(rootDirCluster), 1,
                            g_Data->RootDirectory.Buffer);
   }
   else
   {
      // For FAT12/16 we treat FirstCluster/CurrentCluster as the starting LBA
      g_Data->RootDirectory.FirstCluster = rootDirLba;
      g_Data->RootDirectory.CurrentCluster = rootDirLba;
      g_Data->RootDirectory.CurrentSectorInCluster = 0;

      // Copy preloaded root directory (starts after 512-byte boot sector)
      uint8_t *preloaded_root = (uint8_t *)MEMORY_FAT_ADDR + SECTOR_SIZE;
      memcpy(g_Data->RootDirectory.Buffer, preloaded_root, SECTOR_SIZE);
   }

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
   printf("FAT_ClusterToLba: cluster=%u, g_DataSectionLba=%u, SectorsPerCluster=%u, result=%u\n",
   cluster, g_DataSectionLba, g_Data->BS.BootSector.SectorsPerCluster, lba);
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
   memcpy(fd->Public.Name, entry->Name, 11); // Save the name
   fd->FirstCluster =
       entry->FirstClusterLow + ((uint32_t)entry->FirstClusterHigh << 16);
   printf("FAT_OpenEntry: Size=%u, FirstClusterLow=0x%x, FirstClusterHigh=0x%x, FirstCluster=0x%x\n",
          entry->Size, entry->FirstClusterLow, entry->FirstClusterHigh, fd->FirstCluster);
   fd->CurrentCluster = fd->FirstCluster;
   fd->CurrentSectorInCluster = 0;

   printf("FAT_OpenEntry: attempting to read cluster %u for handle %d\n", fd->CurrentCluster, handle);
   uint32_t lba = FAT_ClusterToLba(fd->CurrentCluster);
   printf("FAT_OpenEntry: LBA=%u\n", lba);
   
   if (!Partition_ReadSectors(disk, lba, 1, fd->Buffer))
   {
      printf("FAT: open entry failed - read error cluster=%u lba=%u\n",
             fd->CurrentCluster, lba);
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

   printf("FAT_OpenEntry: successfully read, Handle=%d\n", handle);
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

   // For regular files (not directories), don't read empty files
   if (fd->Public.Size == 0 && !fd->Public.IsDirectory)
   {
      printf("FAT_Read: file is empty (Size=0), returning 0 bytes, IsDirectory=%u\n", fd->Public.IsDirectory);
      return 0;
   }

   // don't read past the end of the file (for non-directories)
   if (!fd->Public.IsDirectory && fd->Public.Size > 0)
      byteCount = min(byteCount, fd->Public.Size - fd->Public.Position);

   while (byteCount > 0)
   while (byteCount > 0)
   {
      uint32_t leftInBuffer = SECTOR_SIZE - (fd->Public.Position % SECTOR_SIZE);
      uint32_t take = min(byteCount, leftInBuffer);

      memcpy(u8DataOut, fd->Buffer + fd->Public.Position % SECTOR_SIZE, take);
      u8DataOut += take;
      fd->Public.Position += take;
      byteCount -= take;

      // printf("leftInBuffer=%lu take=%lu\n", leftInBuffer, take);
      // See if we need to read more data - either when buffer exhausted OR at sector boundary
      if (leftInBuffer == take || (fd->Public.Position > 0 && fd->Public.Position % SECTOR_SIZE == 0))
      {
         // Prevent infinite loops - safety check
         static uint32_t loop_counter = 0;
         if (++loop_counter > 1000)
         {
            printf("FAT_Read: infinite loop detected, breaking\n");
            loop_counter = 0;
            break;
         }
         // Special handling for root directory
         if (fd->Public.Handle == ROOT_DIRECTORY_HANDLE)
         {
            // Two cases: legacy root (FAT12/16) occupies a fixed sector range,
            // or FAT32 where root is a normal cluster chain.
            if (g_FatType == 32)
            {
               // cluster-based root directory (FAT32)
               if (++fd->CurrentSectorInCluster >=
                   g_Data->BS.BootSector.SectorsPerCluster)
               {
                  fd->CurrentSectorInCluster = 0;
                  fd->CurrentCluster =
                      FAT_NextCluster(disk, fd->CurrentCluster);
               }

               // Check for end-of-chain
               uint32_t eofMarker = 0xFFFFFFF8;
               if (fd->CurrentCluster >= eofMarker)
               {
                  fd->Public.Size = fd->Public.Position;
                  break;
               }

               if (!Partition_ReadSectors(disk,
                                          FAT_ClusterToLba(fd->CurrentCluster) +
                                              fd->CurrentSectorInCluster,
                                          1, fd->Buffer))
               {
                  printf("FAT: read error!\n");
                  break;
               }
            }
            else
            {
               // legacy root directory stored in reserved area (sector indexed)
               ++fd->CurrentCluster;

               if (fd->CurrentCluster >= g_RootDirLba + g_RootDirSectors)
               {
                  fd->Public.Size = fd->Public.Position;
                  break;
               }

               if (!Partition_ReadSectors(disk, fd->CurrentCluster, 1,
                                          fd->Buffer))
               {
                  printf("FAT: read error!\n");
                  break;
               }
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
   printf("FAT_ReadEntry: file Handle=%d, Position=%u, Size=%u\n", file->Handle, file->Position, file->Size);
   uint32_t bytes_read = FAT_Read(disk, file, sizeof(FAT_DirectoryEntry), dirEntry);
   printf("FAT_ReadEntry: read %u bytes\n", bytes_read);
   return bytes_read == sizeof(FAT_DirectoryEntry);
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
      ext = name + strlen(name); // Point to end of string if no extension

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
   printf("FAT_FindFile: file SectorPerCluster at this point=%u\n",
          g_Data->BS.BootSector.SectorsPerCluster);

   while (FAT_ReadEntry(disk, file, &entry))
   {
      printf("FAT_FindFile: read entry at Position=%u, Name='%.11s', Attributes=0x%02x\n", 
             file->Position - 32, entry.Name, entry.Attributes);
      // Skip LFN entries (attribute 0x0F)
      if ((entry.Attributes & 0x0F) == 0x0F) continue;

      if (memcmp(fatName, entry.Name, 11) == 0)
      {
         uint32_t cluster =
             entry.FirstClusterLow + ((uint32_t)entry.FirstClusterHigh << 16);
         *entryOut = entry;
         printf("FAT_FindFile: FOUND MATCH for '%s'\n", fatName);
         return true;
      }
   }

   printf("FAT_FindFile: no match found for '%s'\n", fatName);
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
      printf("FAT_Open: searching for '%s' in Handle=%d\n", name, current->Handle);
      FAT_DirectoryEntry entry;
      if (FAT_FindFile(disk, current, name, &entry))
      {
         printf("FAT_Open: found '%s', Attributes=0x%02x, isLast=%d\n", name, entry.Attributes, isLast);
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
         printf("FAT_Open: opened '%s', new Handle=%d, path next char='%c'\n", name, current->Handle, *path);
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

   printf("FAT_Open: returning current Handle=%d\n", current->Handle);
   return current;
}

bool FAT_Seek(Partition *disk, FAT_File *file, uint32_t position)
{
   FAT_FileData *fd = (file->Handle == ROOT_DIRECTORY_HANDLE)
                          ? &g_Data->RootDirectory
                          : &g_Data->OpenedFiles[file->Handle];

   printf("FAT_Seek: file Handle=%d, position=%u, Size=%u, FirstCluster=%u\n",
          file->Handle, position, fd->Public.Size, fd->FirstCluster);

   // don't seek past end
   if (position > fd->Public.Size) return false;

   fd->Public.Position = position;

   // compute cluster/sector for the position
   uint32_t bytesPerSector = g_Data->BS.BootSector.BytesPerSector;
   uint32_t sectorsPerCluster = g_Data->BS.BootSector.SectorsPerCluster;

   printf("FAT_Seek: bytesPerSector=%u, sectorsPerCluster=%u\n", bytesPerSector, sectorsPerCluster);

   // Guard against divide-by-zero from invalid FAT parameters
   if (bytesPerSector == 0 || sectorsPerCluster == 0)
   {
      printf("FAT_Seek: invalid FAT parameters (BytesPerSector=%u, "
             "SectorsPerCluster=%u)\n",
             bytesPerSector, sectorsPerCluster);
      return false;
   }

   uint32_t clusterBytes = bytesPerSector * sectorsPerCluster;

   if (fd->Public.Handle == ROOT_DIRECTORY_HANDLE)
   {
      if (g_FatType == 32)
      {
         uint32_t clusterBytes = bytesPerSector * sectorsPerCluster;
         uint32_t clusterIndex = position / clusterBytes;
         uint32_t sectorInCluster = (position % clusterBytes) / bytesPerSector;

         uint32_t cluster = fd->FirstCluster;
         for (uint32_t i = 0; i < clusterIndex; i++)
         {
            cluster = FAT_NextCluster(disk, cluster);
            uint32_t eofMarker = 0xFFFFFFF8;
            if (cluster >= eofMarker)
            {
               fd->Public.Size = fd->Public.Position;
               return false;
            }
         }

         fd->CurrentCluster = cluster;
         fd->CurrentSectorInCluster = sectorInCluster;

         if (!Partition_ReadSectors(disk,
                                    FAT_ClusterToLba(fd->CurrentCluster) +
                                        fd->CurrentSectorInCluster,
                                    1, fd->Buffer))
         {
            printf("FAT: seek read error (root)\n");
            return false;
         }
      }
      else
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
   }
   else
   {
      // Guard: don't try to seek on regular files that are empty
      if (fd->Public.Size == 0 && !fd->Public.IsDirectory)
      {
         printf("FAT_Seek: cannot seek on empty regular file\n");
         return false;
      }

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
   // Allow writing into root directory as well as opened directory files.
   if (!file) return false;

   FAT_FileData *fd;
   bool isRoot = (file->Handle == ROOT_DIRECTORY_HANDLE);
   if (isRoot)
      fd = &g_Data->RootDirectory;
   else
   {
      if (file->Handle < 0 || file->Handle >= MAX_FILE_HANDLES) return false;
      fd = &g_Data->OpenedFiles[file->Handle];
   }

   if (!file->IsDirectory)
   {
      printf("FAT: WriteEntry called on non-directory file\n");
      return false;
   }

   // Calculate which sector and offset contains the current directory entry
   uint32_t entryOffset = file->Position;
   uint32_t entryIndexInSector =
       entryOffset % (SECTOR_SIZE / sizeof(FAT_DirectoryEntry));
   uint32_t sectorIndex =
       entryOffset / (SECTOR_SIZE / sizeof(FAT_DirectoryEntry));
   uint32_t offsetInSector = entryIndexInSector * sizeof(FAT_DirectoryEntry);

   // Determine absolute LBA for this sector
   uint32_t sectorLba = 0;
   if (!isRoot)
   {
      sectorLba =
          FAT_ClusterToLba(fd->CurrentCluster) + fd->CurrentSectorInCluster;
      // If fd->CurrentSectorInCluster != expected sectorIndex, read appropriate
      // sector
   }
   else
   {
      if (g_FatType == 32)
      {
         // For FAT32, map sectorIndex to cluster/sector-in-cluster
         uint32_t sectorsPerCluster = g_Data->BS.BootSector.SectorsPerCluster;
         if (sectorsPerCluster == 0)
         {
            printf("FAT_WriteEntry: invalid SectorsPerCluster=0\n");
            return false;
         }
         uint32_t clusterIndex = sectorIndex / sectorsPerCluster;
         uint32_t sectorInCluster = sectorIndex % sectorsPerCluster;
         uint32_t cluster = g_Data->RootDirectory.FirstCluster + clusterIndex;
         sectorLba = FAT_ClusterToLba(cluster) + sectorInCluster;
      }
      else
      {
         // legacy root - contiguous area
         sectorLba = g_RootDirLba + sectorIndex;
      }
   }

   // Read the sector to modify it
   uint8_t sectorBuffer[SECTOR_SIZE];
   if (!Partition_ReadSectors(disk, sectorLba, 1, sectorBuffer))
   {
      printf("FAT: WriteEntry read error\n");
      return false;
   }

   memcpy(&sectorBuffer[offsetInSector], dirEntry, sizeof(FAT_DirectoryEntry));

   if (!Partition_WriteSectors(disk, sectorLba, 1, sectorBuffer))
   {
      printf("FAT: WriteEntry write error\n");
      return false;
   }

   // Advance position
   file->Position++;
   return true;
}

bool FAT_UpdateEntry(Partition *disk, FAT_File *file)
{
   // Search for the file in root directory to find and update its entry
   FAT_File *root = &g_Data->RootDirectory.Public;
   FAT_Seek(disk, root, 0); // Start from beginning of root

   FAT_DirectoryEntry entry;
   uint32_t entryPosition = 0;

   // Validate file handle and get file data
   if (!file) return false;
   FAT_FileData *fd = (file->Handle == ROOT_DIRECTORY_HANDLE)
                          ? &g_Data->RootDirectory
                          : &g_Data->OpenedFiles[file->Handle];
   if (file->Handle != ROOT_DIRECTORY_HANDLE)
   {
      if (file->Handle < 0 || file->Handle >= MAX_FILE_HANDLES) return false;
      if (!fd->Opened) return false;
   }

   while (FAT_ReadEntry(disk, root, &entry))
   {
      entryPosition = root->Position - sizeof(FAT_DirectoryEntry);

      // Skip LFN entries and empty entries
      if ((entry.Attributes & 0x0F) == 0x0F || entry.Name[0] == 0x00) continue;

      // Check if this is our file (match by name)
      if (memcmp(entry.Name, fd->Public.Name, 11) == 0)
      {
         // Found it! Update the entry
         entry.Size = fd->Public.Size;
         entry.FirstClusterLow = fd->FirstCluster & 0xFFFF;
         entry.FirstClusterHigh = (fd->FirstCluster >> 16) & 0xFFFF;

         // Figure out which sector contains this entry
         uint32_t sectorInDirectory = entryPosition / SECTOR_SIZE;
         uint32_t offsetInSector = entryPosition % SECTOR_SIZE;
         uint32_t targetLba = 0;

         if (g_FatType == 32)
         {
            // For FAT32, compute cluster + sector-in-cluster
            uint32_t sectorsPerCluster =
                g_Data->BS.BootSector.SectorsPerCluster;
            uint32_t clusterIndex = sectorInDirectory / sectorsPerCluster;
            uint32_t sectorInCluster = sectorInDirectory % sectorsPerCluster;
            uint32_t cluster =
                g_Data->RootDirectory.FirstCluster + clusterIndex;
            targetLba = FAT_ClusterToLba(cluster) + sectorInCluster;
         }
         else
         {
            // legacy root area
            targetLba = g_RootDirLba + sectorInDirectory;
         }

         uint8_t sectorBuffer[SECTOR_SIZE];
         if (!Partition_ReadSectors(disk, targetLba, 1, sectorBuffer))
         {
            printf("FAT: UpdateEntry read error\n");
            return false;
         }

         memcpy(&sectorBuffer[offsetInSector], &entry,
                sizeof(FAT_DirectoryEntry));

         if (!Partition_WriteSectors(disk, targetLba, 1, sectorBuffer))
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
      ext = name + strlen(name); // Point to end of string if no extension

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

   for (uint32_t testCluster = 2; testCluster < maxSearchClusters;
        testCluster++)
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
   uint32_t fatSectorLba =
       g_Data->BS.BootSector.ReservedSectors + fatSectorOffset;

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
   newEntry.Attributes = 0x20; // Archive attribute
   newEntry._Reserved = 0;
   newEntry.CreatedTimeTenths = 0;
   newEntry.CreatedTime = 0;
   newEntry.CreatedDate = 0;
   newEntry.AccessedDate = 0;
   newEntry.FirstClusterHigh = (firstFreeCluster >> 16) & 0xFFFF;
   newEntry.ModifiedTime = 0;
   newEntry.ModifiedDate = 0;
   newEntry.FirstClusterLow = firstFreeCluster & 0xFFFF;
   newEntry.Size = 0; // Start with empty file

   // Find empty slot in root directory
   FAT_File *root = &g_Data->RootDirectory.Public;
   FAT_Seek(disk, root, 0);

   FAT_DirectoryEntry dirEntry;
   uint32_t searchPosition = 0;
   int entryCount = 0;
   // For FAT32 DirEntryCount is 0; allow scanning until EOF with safety cap.
   int maxEntries = (g_Data->BS.BootSector.DirEntryCount > 0)
                        ? g_Data->BS.BootSector.DirEntryCount
                        : 65536;

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

   printf("FAT_Create: no space in root directory (checked %u entries)\n",
          entryCount);
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
   uint32_t firstCluster =
       entry.FirstClusterLow + ((uint32_t)entry.FirstClusterHigh << 16);

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
         if ((subEntry.Attributes & 0x0F) == 0x0F || subEntry.Name[0] == 0x00 ||
             (uint8_t)subEntry.Name[0] == 0xE5)
            continue;

         // Skip . and .. entries
         if ((subEntry.Name[0] == '.' && subEntry.Name[1] == ' ') ||
             (subEntry.Name[0] == '.' && subEntry.Name[1] == '.' &&
              subEntry.Name[2] == ' '))
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
                                            : 0x0FFFFFF8;
   const uint32_t largeClusterThreshold = 0x0FFFFF00;

   // Validate FAT parameters to avoid divide-by-zero
   if (g_Data->BS.BootSector.SectorsPerCluster == 0 ||
       g_Data->BS.BootSector.BytesPerSector == 0)
   {
      printf("FAT_Delete: invalid FAT parameters, skipping cluster free\n");
      goto mark_deleted;
   }

   const bool validCluster = currentCluster >= 2 &&
                             currentCluster < eofMarker &&
                             currentCluster < largeClusterThreshold;
   uint8_t fatBuffer[SECTOR_SIZE];
   int clusterCount = 0;

   if (validCluster)
   {
      while (currentCluster < eofMarker &&
             currentCluster < largeClusterThreshold &&
             clusterCount < 1000) // safety limit
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
         uint32_t fatSectorLba =
             g_Data->BS.BootSector.ReservedSectors + fatSectorOffset;

         if (!Partition_ReadSectors(disk, fatSectorLba, 1, fatBuffer))
         {
            printf("FAT_Delete: FAT read error at cluster %u\n",
                   currentCluster);
            return false;
         }

         // Get next cluster
         uint32_t nextCluster;
         if (g_FatType == 12)
         {
            if (currentCluster % 2 == 0)
               nextCluster =
                   (*(uint16_t *)(fatBuffer + fatByteOffsetInSector)) & 0x0fff;
            else
               nextCluster =
                   (*(uint16_t *)(fatBuffer + fatByteOffsetInSector)) >> 4;

            if (nextCluster >= 0xff8) nextCluster |= 0xfffff000;
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
   }

mark_deleted:
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
         if (!Partition_ReadSectors(disk,
                                    FAT_ClusterToLba(2) + sectorInDirectory, 1,
                                    sectorBuffer))
         {
            printf("FAT_Delete: read error\n");
            return false;
         }

         // Mark as deleted (0xE5 in first byte of name)
         sectorBuffer[offsetInSector] = 0xE5;

         if (!Partition_WriteSectors(disk,
                                     FAT_ClusterToLba(2) + sectorInDirectory, 1,
                                     sectorBuffer))
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

bool FAT_Truncate(Partition *disk, FAT_File *file)
{
   printf("FAT_Truncate: called, file=%p, Handle=%d\n", file, file ? file->Handle : -999);
   if (!file || file->Handle == ROOT_DIRECTORY_HANDLE) return false;

   if (file->Handle < 0 || file->Handle >= MAX_FILE_HANDLES) return false;

   FAT_FileData *fd = &g_Data->OpenedFiles[file->Handle];
   printf("FAT_Truncate: fd=%p, Opened=%d\n", fd, fd->Opened);
   if (!fd->Opened) return false;

   // Validate FAT parameters to avoid divide-by-zero
   if (g_Data->BS.BootSector.SectorsPerCluster == 0 ||
       g_Data->BS.BootSector.BytesPerSector == 0)
   {
      printf("FAT_Truncate: invalid FAT parameters (SectorsPerCluster=%u, "
             "BytesPerSector=%u)\n",
             g_Data->BS.BootSector.SectorsPerCluster,
             g_Data->BS.BootSector.BytesPerSector);
      fd->FirstCluster = 0;
      fd->CurrentCluster = 0;
      fd->CurrentSectorInCluster = 0;
      fd->Public.Position = 0;
      fd->Public.Size = 0;
      return false;
   }

   uint32_t currentCluster = fd->FirstCluster;
   uint32_t eofMarker = (g_FatType == 12)   ? 0xFF8
                        : (g_FatType == 16) ? 0xFFF8
                                            : 0x0FFFFFF8;

   if (currentCluster < 2 || currentCluster >= eofMarker)
   {
      fd->FirstCluster = 0;
      fd->CurrentCluster = 0;
      fd->CurrentSectorInCluster = 0;
      fd->Public.Position = 0;
      fd->Public.Size = 0;
      return true;
   }

   uint8_t fatBuffer[SECTOR_SIZE];
   int clusterCount = 0;

   printf("FAT_Truncate: starting cluster chain cleanup, FirstCluster=%u, g_FatType=%u\n", 
          fd->FirstCluster, g_FatType);
   printf("FAT_Truncate: eofMarker=%u (0x%x)\n", eofMarker, eofMarker);

   while (currentCluster >= 2 && currentCluster < eofMarker &&
          clusterCount < 5000)
   {
      printf("FAT_Truncate: loop iteration, currentCluster=%u\n", currentCluster);
      clusterCount++;
      printf("FAT_Truncate: clusterCount incremented to %d\n", clusterCount);

      uint32_t fatByteOffset;
      if (g_FatType == 12)
         fatByteOffset = currentCluster * 3 / 2;
      else if (g_FatType == 16)
         fatByteOffset = currentCluster * 2;
      else
         fatByteOffset = currentCluster * 4;

      uint32_t fatSectorOffset = fatByteOffset / SECTOR_SIZE;
      uint32_t fatByteOffsetInSector = fatByteOffset % SECTOR_SIZE;
      uint32_t fatSectorLba =
          g_Data->BS.BootSector.ReservedSectors + fatSectorOffset;

      if (!Partition_ReadSectors(disk, fatSectorLba, 1, fatBuffer))
      {
         printf("FAT_Truncate: FAT read error at cluster %u\n", currentCluster);
         return false;
      }

      uint32_t nextCluster = 0;
      if (g_FatType == 12)
      {
         if (currentCluster % 2 == 0)
            nextCluster =
                (*(uint16_t *)(fatBuffer + fatByteOffsetInSector)) & 0x0fff;
         else
            nextCluster =
                (*(uint16_t *)(fatBuffer + fatByteOffsetInSector)) >> 4;

         if (nextCluster >= 0xff8) nextCluster |= 0xfffff000;
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

      // Mark current cluster as free
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
      else
      {
         *(uint32_t *)(fatBuffer + fatByteOffsetInSector) = 0x00000000;
      }

      if (!Partition_WriteSectors(disk, fatSectorLba, 1, fatBuffer))
      {
         printf("FAT_Truncate: FAT write error at LBA %u\n", fatSectorLba);
         return false;
      }

      currentCluster = nextCluster;
   }

   fd->FirstCluster = 0;
   fd->CurrentCluster = 0;
   fd->CurrentSectorInCluster = 0;
   fd->Public.Position = 0;
   fd->Public.Size = 0;
   memset(fd->Buffer, 0, SECTOR_SIZE);

   g_Data->FatCachePos = 0xFFFFFFFF;
   return true;
}
