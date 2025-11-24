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
#include <libmath/math.h>

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

   // test_fat_filesystem(&partition);

   // ======================================================================
   // EXAMPLE: Load and call a dynamic library function with parameters
   // ======================================================================
   printf("\n=== Testing Dynamic Library Function Calls ===\n");

   // Load libmath from disk if not already registered by bootloader
   printf("[*] Loading libmath.so...\n");
   LibRecord *existing_lib = dylib_find("libmath");
   if (existing_lib && existing_lib->base)
   {
      printf("[*] libmath already registered at 0x%x (loaded by bootloader)\n",
             (unsigned int)existing_lib->base);
      // Parse symbols from the pre-loaded library
      dylib_parse_symbols(existing_lib);
   }
   else
   {
      if (dylib_load_from_disk(&partition, "libmath", "/sys/libmath.so") != 0)
      {
         printf("[!] Failed to load libmath.so\n");
         goto end;
      }
   }

   // Resolve dependencies
   dylib_resolve_dependencies("libmath");

   dylib_list();
   dylib_list_symbols("libmath");

   // Register libmath symbols in global symbol table for GOT patching
   printf("\n[*] Registering libmath symbols in global symbol table...\n");
   dylib_add_global_symbol("add", (uint32_t)dylib_find_symbol("libmath", "add"), "libmath", 0);
   dylib_add_global_symbol("subtract", (uint32_t)dylib_find_symbol("libmath", "subtract"), "libmath", 0);
   dylib_add_global_symbol("multiply", (uint32_t)dylib_find_symbol("libmath", "multiply"), "libmath", 0);
   dylib_add_global_symbol("divide", (uint32_t)dylib_find_symbol("libmath", "divide"), "libmath", 0);
   dylib_add_global_symbol("modulo", (uint32_t)dylib_find_symbol("libmath", "modulo"), "libmath", 0);
   dylib_add_global_symbol("abs_int", (uint32_t)dylib_find_symbol("libmath", "abs_int"), "libmath", 0);
   dylib_add_global_symbol("fabsf", (uint32_t)dylib_find_symbol("libmath", "fabsf"), "libmath", 0);
   dylib_add_global_symbol("fabs", (uint32_t)dylib_find_symbol("libmath", "fabs"), "libmath", 0);
   dylib_add_global_symbol("sinf", (uint32_t)dylib_find_symbol("libmath", "sinf"), "libmath", 0);
   dylib_add_global_symbol("sin", (uint32_t)dylib_find_symbol("libmath", "sin"), "libmath", 0);
   dylib_add_global_symbol("cosf", (uint32_t)dylib_find_symbol("libmath", "cosf"), "libmath", 0);
   dylib_add_global_symbol("cos", (uint32_t)dylib_find_symbol("libmath", "cos"), "libmath", 0);
   dylib_add_global_symbol("tanf", (uint32_t)dylib_find_symbol("libmath", "tanf"), "libmath", 0);
   dylib_add_global_symbol("tan", (uint32_t)dylib_find_symbol("libmath", "tan"), "libmath", 0);
   dylib_add_global_symbol("expf", (uint32_t)dylib_find_symbol("libmath", "expf"), "libmath", 0);
   dylib_add_global_symbol("exp", (uint32_t)dylib_find_symbol("libmath", "exp"), "libmath", 0);
   dylib_add_global_symbol("logf", (uint32_t)dylib_find_symbol("libmath", "logf"), "libmath", 0);
   dylib_add_global_symbol("log", (uint32_t)dylib_find_symbol("libmath", "log"), "libmath", 0);
   dylib_add_global_symbol("log10f", (uint32_t)dylib_find_symbol("libmath", "log10f"), "libmath", 0);
   dylib_add_global_symbol("log10", (uint32_t)dylib_find_symbol("libmath", "log10"), "libmath", 0);
   dylib_add_global_symbol("powf", (uint32_t)dylib_find_symbol("libmath", "powf"), "libmath", 0);
   dylib_add_global_symbol("pow", (uint32_t)dylib_find_symbol("libmath", "pow"), "libmath", 0);
   dylib_add_global_symbol("sqrtf", (uint32_t)dylib_find_symbol("libmath", "sqrtf"), "libmath", 0);
   dylib_add_global_symbol("sqrt", (uint32_t)dylib_find_symbol("libmath", "sqrt"), "libmath", 0);
   dylib_add_global_symbol("floorf", (uint32_t)dylib_find_symbol("libmath", "floorf"), "libmath", 0);
   dylib_add_global_symbol("floor", (uint32_t)dylib_find_symbol("libmath", "floor"), "libmath", 0);
   dylib_add_global_symbol("ceilf", (uint32_t)dylib_find_symbol("libmath", "ceilf"), "libmath", 0);
   dylib_add_global_symbol("ceil", (uint32_t)dylib_find_symbol("libmath", "ceil"), "libmath", 0);
   dylib_add_global_symbol("roundf", (uint32_t)dylib_find_symbol("libmath", "roundf"), "libmath", 0);
   dylib_add_global_symbol("round", (uint32_t)dylib_find_symbol("libmath", "round"), "libmath", 0);
   dylib_add_global_symbol("fminf", (uint32_t)dylib_find_symbol("libmath", "fminf"), "libmath", 0);
   dylib_add_global_symbol("fmin", (uint32_t)dylib_find_symbol("libmath", "fmin"), "libmath", 0);
   dylib_add_global_symbol("fmaxf", (uint32_t)dylib_find_symbol("libmath", "fmaxf"), "libmath", 0);
   dylib_add_global_symbol("fmax", (uint32_t)dylib_find_symbol("libmath", "fmax"), "libmath", 0);
   dylib_add_global_symbol("fmodf", (uint32_t)dylib_find_symbol("libmath", "fmodf"), "libmath", 0);
   dylib_add_global_symbol("fmod", (uint32_t)dylib_find_symbol("libmath", "fmod"), "libmath", 0);
   printf("[*] Symbols registered\n");

   // Apply kernel GOT/PLT relocations for libmath
   printf("\n[*] Applying kernel GOT/PLT relocations...\n");
   dylib_apply_kernel_relocations();
   printf("[*] Relocations applied\n");

   // Test direct calls via extern declarations (uses GOT/PLT)
   printf("\n[*] Testing library function calls (via GOT/PLT):\n");

   printf("\nCalling add(9, 9):\n");
   int result = add(9, 9);
   printf("  Result: %d\n", result);

   printf("\nCalling subtract(20, 5):\n");
   result = subtract(20, 5);
   printf("  Result: %d\n", result);

   printf("\nCalling multiply(7, 6):\n");
   result = multiply(7, 6);
   printf("  Result: %d\n", result);

   printf("\nCalling divide(42, 6):\n");
   result = divide(42, 6);
   printf("  Result: %d\n", result);

   printf("\n=== Dynamic Library Test Complete ===\n");

end:
   for (;;);
}
