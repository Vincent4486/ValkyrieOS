#include <arch/i686/irq.h>
#include <fs/fat12.h>
#include <hal/hal.h>
#include <jvm/jvm.h>
#include <memory/memory.h>
#include <memory/memdefs.h>
#include <std/stdio.h>
#include "memory/dylink.h"
#include <stdint.h>
#include <display/buffer.h>

extern uint8_t __bss_start;
extern uint8_t __end;

void crash_me();

void timer(Registers *regs)
{
   //printf(".");
}

void __attribute__((section(".entry"))) start(uint16_t bootDrive)
{
   memset(&__bss_start, 0, (&__end) - (&__bss_start));

   _init();
   /* initialize basic memory allocator before other subsystems */
   mem_init();

   HAL_Initialize();

   i686_IRQ_RegisterHandler(0, timer);

   printf("Kernel running...\n");
      
   /* Print loaded modules registered by stage2 so we can see what's available.
    * Use the dylink helper which reads the shared registry populated by stage2.
    */
   dylib_list();
   
   DISK disk;
   if (!DISK_Initialize(&disk, bootDrive))
   {
      printf("Disk init error\r\n");
      goto end;
   }

   /* Quick FAT test: initialize FAT on the detected disk and list the root
    * directory entries. This helps verify the FDC driver + FAT code are
    * working together.
    */
   if (FAT_Initialize(&disk))
   {
      printf("FAT initialized\n");
     
      // Try to list root directory first
      FAT_File *root = FAT_Open(&disk, "/");
      if (root)
      {
         printf("Root directory entries:\n");
         FAT_DirectoryEntry entry;
         int count = 0;
         while (FAT_ReadEntry(&disk, root, &entry) && count < 10)
         {
            if (entry.Name[0] == 0) break; // End of directory
            if (entry.Name[0] == 0xE5) continue; // Deleted entry
            
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
               printf("%02x", (unsigned char)entry.Name[i]);
               if (i < 10) printf(" ");
            }
            printf("] attr=0x%02x size=%lu\n", entry.Attributes, (unsigned long)entry.Size);
            count++;
         }
         FAT_Close(root);
      }
     
      // Test: Read entire test.txt file from subdirectory
      printf("\n=== Testing FAT12 File Reading ===\n");
      FAT_File *tf = FAT_Open(&disk, "/test/test.txt");
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
         
         while ((read = FAT_Read(&disk, tf, sizeof(buf), buf)) > 0)
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
         tf = FAT_Open(&disk, "/test.txt");
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
