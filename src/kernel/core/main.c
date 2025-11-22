// SPDX-License-Identifier: AGPL-3.0-or-later

#include <arch/i686/irq.h>
#include <display/buffer_text.h>
#include <drivers/ata.h>
#include <fs/disk.h>
#include <fs/fat.h>
#include <fs/partition.h>
#include <hal/hal.h>
#include <std/stdio.h>
#include <stdint.h>
#include <sys/dylib.h>
#include <sys/memdefs.h>
#include <sys/memory.h>

extern uint8_t __bss_start;
extern uint8_t __end;
extern void _init();

void crash_me();

/**
 * Set IOPL (I/O Privilege Level) to allow kernel I/O port access
 * 
 * Sets bits 12-13 of EFLAGS to level 3, enabling IN/OUT instructions
 * in kernel mode. Required for ATA driver and other I/O operations.
 */
static inline void set_iopl_level_3(void)
{
   __asm__ __volatile__ (
      "pushfl\n\t"
      "orl $0x3000, (%%esp)\n\t"  // Set IOPL to 11b (level 3)
      "popfl\n\t"
      : : : "memory"
   );
}

/**
 * Initialize disk and partition detection
 * 
 * Detects whether the boot drive is a floppy or hard disk,
 * reads the MBR for hard disks, and sets up partition information.
 * 
 * @param disk - Pointer to DISK structure to initialize
 * @param partition - Pointer to Partition structure to populate
 * @param bootDrive - BIOS drive number
 * @return true on success, false on failure
 */
bool storage_initialize(DISK *disk, Partition *partition, uint8_t bootDrive)
{
   /* Initialize DISK structure */
   if (!DISK_Initialize(disk, bootDrive))
   {
      printf("DISK: Initialization failed\n");
      return false;
   }
   
   partition->disk = disk;
   
   /* For hard disks, read MBR and detect partition */
   if (disk->id >= 0x80)  // Hard disk
   {
      printf("DISK: Hard disk detected, reading MBR...\n");
      
      /* Read MBR (sector 0) to detect partitions */
      uint8_t mbr_buffer[512];
      if (!DISK_ReadSectors(disk, 0, 1, mbr_buffer))
      {
         printf("DISK: Failed to read MBR\n");
         return false;
      }
      
      printf("DISK: MBR loaded, checking partition table...\n");
      
      /* DEBUG: Print first 64 bytes of MBR to verify we read correctly */
      printf("DEBUG: MBR first 64 bytes:\n");
      for (int i = 0; i < 64; i += 16)
      {
         printf("  [%02x]: ", i);
         for (int j = 0; j < 16; j++)
            printf("%02x ", mbr_buffer[i + j]);
         printf("\n");
      }
      
      /* DEBUG: Print boot signature */
      uint16_t boot_sig = *(uint16_t *)&mbr_buffer[510];
      printf("DEBUG: Boot signature at 0x1FE: 0x%04x (valid=%s)\n", 
             boot_sig, boot_sig == 0xAA55 ? "yes" : "no");
      
      /* Partition table is at offset 446 (0x1BE), each entry is 16 bytes */
      void *partition_entry = &mbr_buffer[446];
      
      /* DEBUG: Print all 4 partition entry bytes */
      for (int p = 0; p < 4; p++)
      {
         uint8_t *entry_ptr = (uint8_t *)partition_entry + (p * 16);
         printf("DEBUG: MBR partition entry %d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                p, entry_ptr[0], entry_ptr[1], entry_ptr[2], entry_ptr[3],
                entry_ptr[4], entry_ptr[5], entry_ptr[6], entry_ptr[7],
                entry_ptr[8], entry_ptr[9], entry_ptr[10], entry_ptr[11],
                entry_ptr[12], entry_ptr[13], entry_ptr[14], entry_ptr[15]);
      }
      
      /* Try to detect first FAT partition (types 0x04, 0x06, 0x0B, 0x0C) */
      bool found_partition = false;
      for (int i = 0; i < 4; i++)
      {
         uint8_t *entry_ptr = (uint8_t *)partition_entry + (i * 16);
         uint8_t type = entry_ptr[4];
         
         /* Check for FAT partition types */
         if (type == 0x04 || type == 0x06 || type == 0x0B || type == 0x0C)
         {
            /* Extract LBA start and size (little-endian) */
            partition->partitionOffset = *(uint32_t *)(entry_ptr + 8);
            partition->partitionSize = *(uint32_t *)(entry_ptr + 12);
            
            printf("DISK: Found FAT partition %d (type=0x%02x) at LBA %u, size %u sectors\n",
                   i, type, partition->partitionOffset, partition->partitionSize);
            
            found_partition = true;
            break;
         }
      }
      
      if (!found_partition)
      {
         printf("DISK: No FAT partition found in MBR, using default offset (sector 16)\n");
         partition->partitionOffset = 16;
         partition->partitionSize = 0x100000;
      }
      
      /* DEBUG: Read and print FAT boot sector */
      printf("\nDISK: Reading FAT boot sector at LBA %u...\n", partition->partitionOffset);
      uint8_t fat_boot_sector[512];
      if (DISK_ReadSectors(disk, partition->partitionOffset, 1, fat_boot_sector))
      {
         printf("FAT Boot Sector Bytes (first 64 bytes):\n");
         for (int i = 0; i < 64; i += 16)
         {
            printf("  [%02x]: ", i);
            for (int j = 0; j < 16; j++)
               printf("%02x ", fat_boot_sector[i + j]);
            printf("\n");
         }
         
         /* Parse and print key FAT fields */
         uint16_t bytes_per_sector = *(uint16_t *)&fat_boot_sector[11];
         uint8_t sectors_per_cluster = fat_boot_sector[13];
         uint16_t reserved_sectors = *(uint16_t *)&fat_boot_sector[14];
         uint8_t fat_count = fat_boot_sector[16];
         uint16_t root_entries = *(uint16_t *)&fat_boot_sector[17];
         uint16_t total_sectors_16 = *(uint16_t *)&fat_boot_sector[19];
         uint16_t sectors_per_fat_16 = *(uint16_t *)&fat_boot_sector[22];
         uint32_t total_sectors_32 = *(uint32_t *)&fat_boot_sector[32];
         uint32_t sectors_per_fat_32 = *(uint32_t *)&fat_boot_sector[36];
         uint32_t root_dir_cluster_32 = *(uint32_t *)&fat_boot_sector[44];
         
         printf("FAT Header Fields:\n");
         printf("  BytesPerSector: %u\n", bytes_per_sector);
         printf("  SectorsPerCluster: %u\n", sectors_per_cluster);
         printf("  ReservedSectors: %u\n", reserved_sectors);
         printf("  FatCount: %u\n", fat_count);
         printf("  RootEntries (FAT12/16): %u\n", root_entries);
         printf("  TotalSectors16: %u\n", total_sectors_16);
         printf("  SectorsPerFat16: %u\n", sectors_per_fat_16);
         printf("  TotalSectors32: %u\n", total_sectors_32);
         printf("  SectorsPerFat32: %u\n", sectors_per_fat_32);
         printf("  RootDirCluster32: %u\n", root_dir_cluster_32);
      }
      else
      {
         printf("DISK: Failed to read FAT boot sector\n");
      }
   }
   else  // Floppy: use whole disk
   {
      printf("DISK: Floppy disk detected\n");
      partition->partitionOffset = 0;
      partition->partitionSize = disk->sectors;
      printf("DISK: Floppy - offset: 0x%x, size: 0x%x sectors\n", 
             partition->partitionOffset, partition->partitionSize);
   }
   
   return true;
}

/**
 * Test FAT filesystem operations: list directories and read files
 */
void test_fat_filesystem(Partition *partition)
{
   if (!FAT_Initialize(partition))
   {
      printf("FAT: Failed to initialize\n");
      return;
   }

   printf("FAT initialized\n");

   // Test 1: List root directory
   FAT_File *root = FAT_Open(partition, "/");
   if (root)
   {
      printf("Root directory entries:\n");
      FAT_DirectoryEntry entry;
      int count = 0;
      while (FAT_ReadEntry(partition, root, &entry) && count < 10)
      {
         if (entry.Name[0] == 0) break;       // End of directory
         if (entry.Name[0] == 0xE5) continue; // Deleted entry
         
         // Skip volume labels and LFN entries
         if ((entry.Attributes & 0x08) || (entry.Attributes & 0x0F) == 0x0F)
            continue;

         // Format FAT filename properly: 8 chars name + '.' + 3 chars ext
         printf("  [");
         for (int i = 0; i < 11; i++)
         {
            unsigned char c = entry.Name[i];
            if (c >= 32 && c < 127)
               printf("%c", c);
            else
               printf(".");
         }
         printf("] hex=[");
         for (int i = 0; i < 11; i++)
         {
            unsigned char val = (unsigned char)entry.Name[i];
            if (val < 16) printf("0");  // pad with leading zero manually
            printf("%x", val);
            if (i < 10) printf(" ");
         }
         printf("] attr=0x");
         if (entry.Attributes < 16) printf("0");  // pad with leading zero manually
         printf("%x size=%lu\n", entry.Attributes,
                (unsigned long)entry.Size);
         count++;
      }
      FAT_Close(root);
   }

   // Test 2: List /test directory
   printf("\n=== /test Directory Entries ===\n");
   FAT_File *test_dir = FAT_Open(partition, "/test");
   if (test_dir)
   {
      printf("/test directory entries:\n");
      FAT_DirectoryEntry entry;
      int count = 0;
      while (FAT_ReadEntry(partition, test_dir, &entry) && count < 10)
      {
         if (entry.Name[0] == 0) break;       // End of directory
         if (entry.Name[0] == 0xE5) continue; // Deleted entry
         
         // Skip volume labels and LFN entries
         if ((entry.Attributes & 0x08) || (entry.Attributes & 0x0F) == 0x0F)
            continue;

         // Format FAT filename properly
         printf("  [");
         for (int i = 0; i < 11; i++)
         {
            unsigned char c = entry.Name[i];
            if (c >= 32 && c < 127)
               printf("%c", c);
            else
               printf(".");
         }
         printf("] attr=0x");
         if (entry.Attributes < 16) printf("0");
         printf("%x size=%lu\n", entry.Attributes,
                (unsigned long)entry.Size);
         count++;
      }
      FAT_Close(test_dir);
   }
   else
   {
      printf("/test directory not found\n");
   }

   // Test 3: Read entire test.txt file from subdirectory
   printf("\n=== Testing FAT File Reading ===\n");
   FAT_File *tf = FAT_Open(partition, "/test/test.txt");
   if (tf)
   {
      printf("Successfully opened /test/test.txt\n");
      printf("File size: %lu bytes\n", (unsigned long)tf->Size);
      printf("Reading entire file contents:\n");
      printf("---BEGIN FILE---\n");

      char buf[256];
      uint32_t total_read = 0;
      uint32_t read;
      int chunk_count = 0;

      while ((read = FAT_Read(partition, tf, sizeof(buf), buf)) > 0)
      {
         chunk_count++;
         total_read += read;

         // Print both ASCII and hex for debugging
         printf("Chunk %d (%lu bytes): ", chunk_count, (unsigned long)read);
         for (uint32_t i = 0; i < read && i < 64; i++)
         {
            char c = buf[i];
            if (c >= 32 && c < 127)
               printf("%c", c);
            else
               printf(".");
         }
         printf("\n");

         printf("Hex: ");
         for (uint32_t i = 0; i < read && i < 64; i++)
         {
            unsigned char byte = (unsigned char)buf[i];
            // Manually print two hex digits
            const char *hex = "0123456789abcdef";
            putc(hex[byte >> 4]);
            putc(hex[byte & 0xF]);
            putc(' ');
         }
         printf("\n");
      }

      printf("---END FILE---\n");
      printf("Total bytes read: %lu (expected %lu)\n",
             (unsigned long)total_read, (unsigned long)tf->Size);
      printf("Chunks read: %d\n", chunk_count);

      if (total_read == tf->Size)
      {
         printf("SUCCESS: Read entire file correctly!\n");
      }
      else
      {
         printf("WARNING: Byte count mismatch!\n");
      }

      FAT_Close(tf);
   }
   else
   {
      printf("FAT: /test/test.txt not found\n");

      // Try other variations
      tf = FAT_Open(partition, "/test.txt");
      if (tf)
      {
         printf("/test.txt (root) found!\n");
         FAT_Close(tf);
      }
      else
      {
         printf("FAT: /test.txt not found either\n");
      }
   }
   printf("\n=== File Reading Test Complete ===\n");
}

void timer(Registers *regs)
{
   // printf(".");
}

void __attribute__((section(".entry"))) start(uint16_t bootDrive, void *partitionPtr)
{
   memset(&__bss_start, 0, (&__end) - (&__bss_start));
   _init();
   mem_init();
   HAL_Initialize();
   // Enable I/O port access for ATA driver
   set_iopl_level_3();
   i686_IRQ_RegisterHandler(0, timer);
   dylib_list();
   
   /* Read test.txt from its actual location on disk (LBA 8645) */
   uint8_t buffer[512];
   uint32_t lba = 8645;  // LBA where "Hello world from a file!!!" is located
   printf("\n=== Reading test.txt from LBA %u ===\n", lba);
   if (ata_read(0, 0, lba, buffer, 1) == 0)
   {
      printf("Successfully read LBA %u\n", lba);
      printf("Content:\n");
      for (int i = 0; i < 512; i += 16)
      {
         printf("  [%03x]: ", i);
         for (int j = 0; j < 16; j++)
         {
            unsigned char c = buffer[i + j];
            if (c >= 32 && c < 127)
               printf("%c", c);
            else
               printf(".");
         }
         printf("  ");
         for (int j = 0; j < 16; j++)
            printf("%02x ", buffer[i + j]);
         printf("\n");
      }
   }
   else
   {
      printf("Failed to read LBA %u\n", lba);
   }
   printf("=== Read Complete ===\n\n");
   
   DISK disk;
   Partition partition;
   
   /* Stage1 bootloader copies the MBR partition entry to 0x20000.
    * Try to use that directly. If it's invalid, fall back to reading MBR.
    */
   
   // Validate if partition data at 0x20000 looks like an MBR entry
   // (16 bytes, partition type at offset +4 should be FAT)
   void *stage1_partition_ptr = (void *)0x20000;
   uint8_t *part_data = (uint8_t *)stage1_partition_ptr;
   uint8_t part_type = part_data[4];
   
   printf("KERNEL: Checking partition at 0x20000, type=0x%02x\n", part_type);
   
   if ((part_type == 0x04 || part_type == 0x06 || part_type == 0x0B || part_type == 0x0C) &&
       partitionPtr != NULL)
   {
      // Use partition data from stage1
      printf("KERNEL: Using partition data from stage1 at 0x20000\n");
      if (!DISK_Initialize(&disk, bootDrive))
      {
         printf("DISK: Initialization failed\n");
         goto end;
      }
      partition.disk = &disk;
      MBR_DetectPartition(&partition, &disk, stage1_partition_ptr);
      printf("KERNEL: Partition detected: offset=%u, size=%u\n",
             partition.partitionOffset, partition.partitionSize);
   }
   else
   {
      printf("KERNEL: Invalid partition at 0x20000, reading MBR from disk\n");
      if (!DISK_Initialize(&disk, bootDrive))
      {
         printf("DISK: Initialization failed\n");
         goto end;
      }
      
      if (!storage_initialize(&disk, &partition, bootDrive))
      {
         printf("DISK: Initialization failed\n");
         goto end;
      }
   }
   
   /* Run FAT filesystem tests */
   test_fat_filesystem(&partition);

end:
   for (;;);
}
