// SPDX-License-Identifier: AGPL-3.0-or-later

#include <arch/i686/irq.h>
#include <display/buffer.h>
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

void timer(Registers *regs)
{
   // printf(".");
}

void __attribute__((section(".entry"))) start(uint16_t bootDrive)
{
   memset(&__bss_start, 0, (&__end) - (&__bss_start));

   _init();
   /* initialize basic memory allocator before other subsystems */
   mem_init();

   HAL_Initialize();

   // Set IOPL to 3 (bits 12-13 of EFLAGS) to allow I/O port access from kernel mode
   __asm__ __volatile__ (
      "pushfl\n\t"
      "orl $0x3000, (%%esp)\n\t"  // Set IOPL to 11b (level 3)
      "popfl\n\t"
      : : : "memory"
   );

   i686_IRQ_RegisterHandler(0, timer);

   printf("Kernel running...\n");

   /* Print loaded modules registered by stage2 so we can see what's available.
    * Use the dylib helper which reads the shared registry populated by stage2.
    */
   dylib_list();

   DISK disk;
   if (!DISK_Initialize(&disk, bootDrive))
   {
      printf("Disk init error\r\n");
      goto end;
   }

   /* Initialize partition structure for FAT access */
   Partition partition;
   partition.disk = &disk;
   partition.partitionOffset = 0; // Using whole disk for now
   partition.partitionSize = 0;

   /* Quick FAT test: initialize FAT on the partition and list the root
    * directory entries. This helps verify the FDC driver + FAT code are
    * working together.
    */
   if (FAT_Initialize(&partition))
   {
      printf("FAT initialized\n");

      // Try to list root directory first
      FAT_File *root = FAT_Open(&partition, "/");
      if (root)
      {
         printf("Root directory entries:\n");
         FAT_DirectoryEntry entry;
         int count = 0;
         while (FAT_ReadEntry(&partition, root, &entry) && count < 10)
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

      // Test: List /test directory
      printf("\n=== /test Directory Entries ===\n");
      FAT_File *test_dir = FAT_Open(&partition, "/test");
      if (test_dir)
      {
         printf("/test directory entries:\n");
         FAT_DirectoryEntry entry;
         int count = 0;
         while (FAT_ReadEntry(&partition, test_dir, &entry) && count < 10)
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

      // Test: Read entire test.txt file from subdirectory
      printf("\n=== Testing FAT12 File Reading ===\n");
      FAT_File *tf = FAT_Open(&partition, "/test/test.txt");
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

         while ((read = FAT_Read(&partition, tf, sizeof(buf), buf)) > 0)
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
         tf = FAT_Open(&partition, "/test.txt");
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
   else
   {
      printf("FAT initialize failed\n");
   }
end:
   for (;;);
}
