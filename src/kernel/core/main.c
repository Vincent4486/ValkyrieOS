// SPDX-License-Identifier: AGPL-3.0-or-later

#include <arch/i686/irq.h>
#include <fs/disk.h>
#include <fs/fat.h>
#include <fs/partition.h>
#include <fs/storage.h>
#include <hal/hal.h>
#include <std/stdio.h>
#include <stdint.h>
#include <sys/dylib.h>
#include <sys/memory.h>
#include <std/string.h>

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
   __asm__ __volatile__("pushfl\n\t"
                        "orl $0x3000, (%%esp)\n\t" // Set IOPL to 11b (level 3)
                        "popfl\n\t"
                        :
                        :
                        : "memory");
}

/**
 * Test FAT filesystem operations: list directories and read files
 */
void test_fat_filesystem(Partition *partition)
{
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
            if (val < 16) printf("0"); // pad with leading zero manually
            printf("%x", val);
            if (i < 10) printf(" ");
         }
         printf("] attr=0x");
         if (entry.Attributes < 16)
            printf("0"); // pad with leading zero manually
         printf("%x size=%lu\n", entry.Attributes, (unsigned long)entry.Size);
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
         printf("%x size=%lu\n", entry.Attributes, (unsigned long)entry.Size);
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
      printf("---BEGIN FILE---\n");

      char buf[256];
      uint32_t total_read = 0;
      uint32_t read;

      while ((read = FAT_Read(partition, tf, sizeof(buf), buf)) > 0)
      {
         total_read += read;

         // Print decoded characters only
         for (uint32_t i = 0; i < read; i++)
         {
            char c = buf[i];
            if (c >= 32 && c < 127)
               printf("%c", c);
            else if (c == '\n' || c == '\r')
               printf("%c", c);
            else
               printf(".");
         }
      }

      printf("\n---END FILE---\n");
      printf("Total bytes read: %lu (expected %lu)\n",
             (unsigned long)total_read, (unsigned long)tf->Size);

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

// Global timer tick counter
static volatile uint32_t timer_ticks = 0;

void timer(Registers *regs)
{
   timer_ticks++;
   // Uncomment to show timer running:
   // if (timer_ticks % 18 == 0)  // Print every ~1 second
   //    printf(".");
}

typedef int (*math_func_t)(int, int);
int (*add)(int, int) = NULL;
int (*subtract)(int, int) = NULL;
int (*multiply)(int, int) = NULL;

// Callback function to load symbols
void load_libmath_symbols(const char *libname)
{
    if (strcmp(libname, "libmath") != 0)
        return;
    
    printf("[*] Loading libmath symbols...\n");
    add = (math_func_t)dylib_find_symbol("libmath", "add");
    subtract = (math_func_t)dylib_find_symbol("libmath", "subtract");
    multiply = (math_func_t)dylib_find_symbol("libmath", "multiply");
    
    if (!add || !subtract || !multiply)
    {
        printf("[ERROR] Failed to load libmath functions\n");
        return;
    }
    printf("[*] libmath functions loaded successfully\n");
}

void __attribute__((section(".entry"))) start(uint16_t bootDrive,
                                              void *partitionPtr)
{
   memset(&__bss_start, 0, (&__end) - (&__bss_start));
   _init();
   mem_init();
   HAL_Initialize();
   // Enable I/O port access for ATA driver
   set_iopl_level_3();
   i686_IRQ_RegisterHandler(0, timer);
   dylib_list();

   DISK disk;
   Partition partition;

   if (!Storage_Initialize(&disk, &partition, bootDrive))
   {
      printf("Storage initialization failed\n");
      goto end;
   }

   test_fat_filesystem(&partition);

   // ======================================================================
   // EXAMPLE: Load and call a dynamic library function with parameters
   // ======================================================================
   printf("\n=== Testing Dynamic Library Function Calls ===\n");

   // Register the symbol loader callback
   dylib_register_callback(load_libmath_symbols);
    
    // Now just load the library - symbols are loaded automatically!
    printf("[*] Loading libmath.so...\n");
    if (dylib_load_from_disk(&partition, "libmath", "/sys/libmath.so") != 0)
    {
        printf("[!] Failed to load libmath.so\n");
        goto end;
    }
    
    // Resolve dependencies
    dylib_resolve_dependencies("libmath");
    
    dylib_list();
    dylib_list_symbols("libmath");
    
    // Just call the functions directly - they're already loaded!
    printf("\n[*] Testing library function calls:\n");
    
    printf("\nCalling add(9, 9):\n");
    int result = add(9, 9);
    printf("  Result: %d\n", result);
    
    printf("\nCalling subtract(20, 5):\n");
    result = subtract(20, 5);
    printf("  Result: %d\n", result);
    
    printf("\nCalling multiply(7, 6):\n");
    result = multiply(7, 6);
    printf("  Result: %d\n", result);
    
    printf("\n=== Dynamic Library Test Complete ===\n");

end:
   for (;;);
}
