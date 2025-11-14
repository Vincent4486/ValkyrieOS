// SPDX-License-Identifier: AGPL-3.0-or-later

/* Dynamic link helpers for the kernel. Supports true ELF dynamic linking
 * with PLT/GOT relocation. The bootloader (stage2) loads libraries and
 * populates a LibRecord table at LIB_REGISTRY_ADDR. The kernel loader
 * then applies relocations using the global symbol table.
 */

#include "dylib.h"
#include <fs/fat.h>
#include <std/stdio.h>
#include <std/string.h>
#include <stdint.h>

// ELF32 relocation types (i686)
#define R_386_NONE        0
#define R_386_32          1
#define R_386_PC32        2
#define R_386_GLOB_DAT    6
#define R_386_JMP_SLOT    7
#define R_386_RELATIVE    8

// ELF32 structures for parsing at runtime
typedef struct
{
   uint32_t r_offset;
   uint32_t r_info;
} Elf32_Rel;

typedef struct
{
   uint32_t r_offset;
   uint32_t r_info;
   int32_t r_addend;
} Elf32_Rela;

#define ELF32_R_SYM(i)    ((i) >> 8)
#define ELF32_R_TYPE(i)   ((i) & 0xff)

// Extended library data (kept separately from the base LibRecord registry)
typedef struct
{
   DependencyRecord deps[DYLIB_MAX_DEPS];
   int dep_count;
   SymbolRecord symbols[DYLIB_MAX_SYMBOLS];
   int symbol_count;
   
   // ELF dynamic section metadata (parsed from .dynamic at load time)
   uint32_t dynsym_addr;      // Address of .dynsym section in loaded memory
   uint32_t dynsym_size;      // Size in bytes
   uint32_t dynstr_addr;      // Address of .dynstr section in loaded memory
   uint32_t dynstr_size;      // Size in bytes
   uint32_t rel_addr;         // Address of .rel.dyn relocations
   uint32_t rel_size;         // Size of .rel.dyn
   uint32_t jmprel_addr;      // Address of .rel.plt (PLT relocations)
   uint32_t jmprel_size;      // Size of .rel.plt
   uint32_t pltgot_addr;      // Address of .got.plt (for PLT patching)
   
   int loaded; // 1 if loaded in memory, 0 if not
} ExtendedLibData;

// Memory allocator state
static int dylib_mem_initialized = 0;
static uint32_t dylib_mem_next_free = DYLIB_MEMORY_ADDR;
static ExtendedLibData extended_data[LIB_REGISTRY_MAX];

// Global symbol table - shared across all loaded libraries and kernel
static GlobalSymbolEntry global_symtab[DYLIB_MAX_GLOBAL_SYMBOLS];
static int global_symtab_count = 0;

int dylib_mem_init(void)
{
   if (dylib_mem_initialized) return 0;

   // Clear the memory region
   uint8_t *mem = (uint8_t *)DYLIB_MEMORY_ADDR;
   for (uint32_t i = 0; i < DYLIB_MEMORY_SIZE; i++) mem[i] = 0;

   // Clear extended data
   for (int i = 0; i < LIB_REGISTRY_MAX; i++)
   {
      extended_data[i].dep_count = 0;
      extended_data[i].symbol_count = 0;
      extended_data[i].dynsym_addr = 0;
      extended_data[i].dynstr_addr = 0;
      extended_data[i].rel_addr = 0;
      extended_data[i].jmprel_addr = 0;
      extended_data[i].loaded = 0;
   }

   dylib_mem_next_free = DYLIB_MEMORY_ADDR;
   dylib_mem_initialized = 1;

   printf("[DYLIB] Memory allocator initialized: 0x%x - 0x%x (%d MiB)\n",
          DYLIB_MEMORY_ADDR, DYLIB_MEMORY_ADDR + DYLIB_MEMORY_SIZE,
          DYLIB_MEMORY_SIZE / 0x100000);

   return 0;
}

// ============================================================================
// Global Symbol Table Management
// ============================================================================

int dylib_add_global_symbol(const char *name, uint32_t address,
                            const char *lib_name, int is_kernel)
{
   if (global_symtab_count >= DYLIB_MAX_GLOBAL_SYMBOLS)
   {
      printf("[ERROR] Global symbol table full (%d entries)\n",
             DYLIB_MAX_GLOBAL_SYMBOLS);
      return -1;
   }

   GlobalSymbolEntry *entry = &global_symtab[global_symtab_count];
   strncpy(entry->name, name, 63);
   entry->name[63] = '\0';
   entry->address = address;
   strncpy(entry->lib_name, lib_name, 63);
   entry->lib_name[63] = '\0';
   entry->is_kernel = is_kernel;

   global_symtab_count++;
   return 0;
}

uint32_t dylib_lookup_global_symbol(const char *name)
{
   for (int i = 0; i < global_symtab_count; i++)
   {
      if (strcmp(global_symtab[i].name, name) == 0)
         return global_symtab[i].address;
   }
   return 0; // Not found
}

void dylib_print_global_symtab(void)
{
   printf("\n========== Global Symbol Table ==========\n");
   printf("%-40s 0x%-8x %s\n", "Symbol", "Address", "Source");
   printf("==========================================\n");

   for (int i = 0; i < global_symtab_count; i++)
   {
      GlobalSymbolEntry *e = &global_symtab[i];
      const char *source = e->is_kernel ? "[KERNEL]" : e->lib_name;
      printf("%-40s 0x%08x %s\n", e->name, e->address, source);
   }
   printf("==========================================\n");
   printf("Total: %d symbols\n\n", global_symtab_count);
}

void dylib_clear_global_symtab(void)
{
   global_symtab_count = 0;
   printf("[DYLIB] Global symbol table cleared\n");
}

// ============================================================================
// Relocation Application
// ============================================================================

// Apply relocations to a loaded library or to the kernel
// Returns 0 on success, -1 on unresolved symbols
static int apply_relocations(uint32_t base, Elf32_Rel *rel_table,
                              uint32_t rel_count, uint32_t dynsym_addr,
                              uint32_t dynstr_addr, const char *context)
{
   if (!rel_table || rel_count == 0) return 0;

   for (uint32_t i = 0; i < rel_count; i++)
   {
      uint32_t *where = (uint32_t *)(base + rel_table[i].r_offset);
      int type = ELF32_R_TYPE(rel_table[i].r_info);
      int symidx = ELF32_R_SYM(rel_table[i].r_info);

      if (type == R_386_RELATIVE)
      {
         // Relative relocation - adjust by base address
         uint32_t addend = *where;
         *where = base + addend;
      }
      else if (type == R_386_32 || type == R_386_PC32 || type == R_386_GLOB_DAT ||
               type == R_386_JMP_SLOT)
      {
         // Symbol-based relocation - need to look up symbol in global table
         if (symidx > 0 && dynsym_addr > 0)
         {
            // Read symbol from dynsym table
            // Elf32_Sym structure: uint32_t st_name, st_value, st_size, st_info, st_other, st_shndx (16 bytes)
            uint32_t sym_ent_offset = symidx * 16;
            uint32_t st_name_offset = *(uint32_t *)(dynsym_addr + sym_ent_offset);
            uint32_t st_value = *(uint32_t *)(dynsym_addr + sym_ent_offset + 4);

            if (dynstr_addr > 0)
            {
               const char *sym_name = (const char *)(dynstr_addr + st_name_offset);

               // Look up symbol in global table
               uint32_t sym_addr = dylib_lookup_global_symbol(sym_name);
               if (sym_addr == 0)
               {
                  printf("[ERROR] Unresolved symbol in %s: %s\n", context, sym_name);
                  return -1;
               }

               uint32_t addend = *where;

               // Apply relocation based on type
               switch (type)
               {
                  case R_386_32:
                     // S + A (absolute address + addend)
                     *where = sym_addr + addend;
                     break;
                  case R_386_PC32:
                     // S + A - P (relative to position)
                     *where = sym_addr + addend - (uint32_t)where;
                     break;
                  case R_386_GLOB_DAT:
                  case R_386_JMP_SLOT:
                     // S (just the symbol address, for GOT entries)
                     *where = sym_addr;
                     break;
               }
            }
         }
      }
   }

   return 0;
}

int dylib_apply_kernel_relocations(void)
{
   // Kernel relocation sections are exposed by linker script
   extern char _kernel_rel_dyn_start[];
   extern char _kernel_rel_dyn_end[];
   extern char _kernel_rel_plt_start[];
   extern char _kernel_rel_plt_end[];

   uint32_t kernel_base = 0x00A00000; // Kernel load address

   // Apply .rel.dyn relocations
   {
      uint32_t rel_size = (uint32_t)_kernel_rel_dyn_end - (uint32_t)_kernel_rel_dyn_start;
      Elf32_Rel *rel = (Elf32_Rel *)_kernel_rel_dyn_start;
      int rel_count = rel_size / sizeof(Elf32_Rel);

      if (rel_count > 0)
      {
         printf("[DYLIB] Applying %d kernel .rel.dyn relocations...\n", rel_count);
         if (apply_relocations(kernel_base, rel, rel_count, 0, 0, "kernel .rel.dyn") != 0)
            return -1;
      }
   }

   // Apply .rel.plt relocations
   {
      uint32_t rel_size = (uint32_t)_kernel_rel_plt_end - (uint32_t)_kernel_rel_plt_start;
      Elf32_Rel *rel = (Elf32_Rel *)_kernel_rel_plt_start;
      int rel_count = rel_size / sizeof(Elf32_Rel);

      if (rel_count > 0)
      {
         printf("[DYLIB] Applying %d kernel .rel.plt relocations...\n", rel_count);
         if (apply_relocations(kernel_base, rel, rel_count, 0, 0, "kernel .rel.plt") != 0)
            return -1;
      }
   }

   printf("[DYLIB] Kernel relocation complete\n");
   return 0;
}

uint32_t dylib_mem_alloc(const char *lib_name, uint32_t size)
{
   if (!dylib_mem_initialized) dylib_mem_init();

   // Round up to 16-byte boundary for alignment
   uint32_t aligned_size = (size + 15) & ~15;

   // Check if we have enough space
   if (dylib_mem_next_free + aligned_size >
       DYLIB_MEMORY_ADDR + DYLIB_MEMORY_SIZE)
   {
      printf("[ERROR] Out of dylib memory! Need %d bytes, only %d available\n",
             aligned_size,
             DYLIB_MEMORY_ADDR + DYLIB_MEMORY_SIZE - dylib_mem_next_free);
      return 0;
   }

   uint32_t alloc_addr = dylib_mem_next_free;
   dylib_mem_next_free += aligned_size;

   printf("[DYLIB] Allocated 0x%x bytes at 0x%x for %s\n", aligned_size,
          alloc_addr, lib_name);

   return alloc_addr;
}

// Helper: find index of library by name
static int dylib_find_index(const char *name)
{
   LibRecord *reg = LIB_REGISTRY_ADDR;
   for (int i = 0; i < LIB_REGISTRY_MAX; i++)
   {
      if (reg[i].name[0] != '\0')
      {
         if (str_eq(reg[i].name, name)) return i;
      }
   }
   return -1;
}

LibRecord *dylib_find(const char *name)
{
   LibRecord *reg = LIB_REGISTRY_ADDR;
   for (int i = 0; i < LIB_REGISTRY_MAX; i++)
   {
      if (reg[i].name[0] != '\0')
      {
         if (str_eq(reg[i].name, name)) return &reg[i];
      }
   }
   return NULL;
}

int dylib_check_dependencies(const char *name)
{
   int idx = dylib_find_index(name);
   if (idx < 0) return 0;

   ExtendedLibData *ext = &extended_data[idx];

   // Check all dependencies
   for (int i = 0; i < ext->dep_count; i++)
   {
      if (!ext->deps[i].resolved)
      {
         printf("  [UNRESOLVED] %s requires %s\n", name, ext->deps[i].name);
         return 0;
      }
   }
   return 1;
}

int dylib_resolve_dependencies(const char *name)
{
   int idx = dylib_find_index(name);
   if (idx < 0) return -1;

   ExtendedLibData *ext = &extended_data[idx];

   printf("[*] Resolving dependencies for %s...\n", name);

   // Resolve each dependency
   for (int i = 0; i < ext->dep_count; i++)
   {
      LibRecord *dep = dylib_find(ext->deps[i].name);
      if (dep)
      {
         ext->deps[i].resolved = 1;
         printf("  [OK] Found dependency: %s\n", ext->deps[i].name);
      }
      else
      {
         ext->deps[i].resolved = 0;
         printf("  [ERROR] Missing dependency: %s\n", ext->deps[i].name);
         return -1;
      }
   }
   return 0;
}

int dylib_call_if_exists(const char *name)
{
   LibRecord *lib = dylib_find(name);
   if (!lib || !lib->entry) return -1;

   // Check dependencies before calling
   if (!dylib_check_dependencies(name))
   {
      printf("[ERROR] %s has unresolved dependencies\n", name);
      return -1;
   }

   // Call the entry point
   typedef int (*entry_t)(void);
   return ((entry_t)lib->entry)();
}

void dylib_list(void)
{
   LibRecord *reg = LIB_REGISTRY_ADDR;

   printf("\n=== Loaded Libraries ===\n");
   for (int i = 0; i < LIB_REGISTRY_MAX; i++)
   {
      if (reg[i].name[0] == '\0') break;

      ExtendedLibData *ext = &extended_data[i];

      printf("[%d] %s @ 0x%x\n", i, reg[i].name, (unsigned int)reg[i].entry);

      if (ext->dep_count > 0)
      {
         printf("    Dependencies (%d):\n", ext->dep_count);
         for (int j = 0; j < ext->dep_count; j++)
         {
            char status = ext->deps[j].resolved ? '+' : '-';
            printf("      [%c] %s\n", status, ext->deps[j].name);
         }
      }
   }
   printf("\n");
}

void dylib_list_deps(const char *name)
{
   int idx = dylib_find_index(name);
   if (idx < 0)
   {
      printf("[ERROR] Library not found: %s\n", name);
      return;
   }

   ExtendedLibData *ext = &extended_data[idx];

   printf("\nDependencies for %s:\n", name);
   if (ext->dep_count == 0)
   {
      printf("  (none)\n");
      return;
   }

   for (int i = 0; i < ext->dep_count; i++)
   {
      const char *status = ext->deps[i].resolved ? "RESOLVED" : "UNRESOLVED";
      printf("  %s: %s\n", ext->deps[i].name, status);
   }
   printf("\n");
}

uint32_t dylib_find_symbol(const char *libname, const char *symname)
{
   int idx = dylib_find_index(libname);
   if (idx < 0)
   {
      printf("[ERROR] Library not found: %s\n", libname);
      return 0;
   }

   ExtendedLibData *ext = &extended_data[idx];

   // Search for symbol in library
   for (int i = 0; i < ext->symbol_count; i++)
   {
      if (str_eq(ext->symbols[i].name, symname))
      {
         return ext->symbols[i].address;
      }
   }

   printf("[ERROR] Symbol not found: %s::%s\n", libname, symname);
   return 0;
}

int dylib_call_symbol(const char *libname, const char *symname)
{
   LibRecord *lib = dylib_find(libname);
   if (!lib)
   {
      printf("[ERROR] Library not found: %s\n", libname);
      return -1;
   }

   // Check dependencies before calling
   if (!dylib_check_dependencies(libname))
   {
      printf("[ERROR] %s has unresolved dependencies\n", libname);
      return -1;
   }

   // Find the symbol
   uint32_t symbol_addr = dylib_find_symbol(libname, symname);
   if (!symbol_addr)
   {
      return -1;
   }

   // Call the symbol
   typedef int (*func_t)(void);
   return ((func_t)symbol_addr)();
}

void dylib_list_symbols(const char *name)
{
   int idx = dylib_find_index(name);
   if (idx < 0)
   {
      printf("[ERROR] Library not found: %s\n", name);
      return;
   }

   ExtendedLibData *ext = &extended_data[idx];

   printf("\nExported symbols from %s:\n", name);
   if (ext->symbol_count == 0)
   {
      printf("  (none)\n");
      return;
   }

   for (int i = 0; i < ext->symbol_count; i++)
   {
      printf("  [%d] %s @ 0x%x\n", i, ext->symbols[i].name,
             ext->symbols[i].address);
   }
   printf("\n");
}

int dylib_mem_free(const char *lib_name)
{
   int idx = dylib_find_index(lib_name);
   if (idx < 0)
   {
      printf("[ERROR] Library not found: %s\n", lib_name);
      return -1;
   }

   LibRecord *lib = &LIB_REGISTRY_ADDR[idx];
   ExtendedLibData *ext = &extended_data[idx];

   if (!ext->loaded)
   {
      printf("[WARNING] Library %s is not loaded\n", lib_name);
      return -1;
   }

   // Note: We don't actually free the memory in the pool since it's a linear
   // allocator Just mark as unloaded
   printf("[DYLIB] Freed 0x%x bytes for %s\n", lib->size, lib_name);

   return 0;
}

int dylib_load(const char *name, const void *image, uint32_t size)
{
   if (!dylib_mem_initialized) dylib_mem_init();

   int idx = dylib_find_index(name);
   if (idx < 0)
   {
      printf("[ERROR] Library record not found: %s\n", name);
      return -1;
   }

   LibRecord *lib = &LIB_REGISTRY_ADDR[idx];
   ExtendedLibData *ext = &extended_data[idx];

   if (ext->loaded)
   {
      printf("[WARNING] Library %s is already loaded\n", name);
      return -1;
   }

   // Allocate memory for the library
   uint32_t load_addr = dylib_mem_alloc(name, size);
   if (!load_addr)
   {
      printf("[ERROR] Failed to allocate memory for %s\n", name);
      return -1;
   }

   // Copy library image to allocated memory
   uint8_t *dest = (uint8_t *)load_addr;
   const uint8_t *src = (const uint8_t *)image;
   for (uint32_t i = 0; i < size; i++) dest[i] = src[i];

   // Update library record
   lib->base = (void *)load_addr;
   lib->size = size;
   ext->loaded = 1;

   printf("[DYLIB] Loaded %s (%d bytes) at 0x%x\n", name, size, load_addr);

   return 0;
}

int dylib_load_from_disk(Partition *partition, const char *name,
                         const char *filepath)
{
   if (!dylib_mem_initialized) dylib_mem_init();

   if (!partition)
   {
      printf("[ERROR] Partition pointer is NULL\n");
      return -1;
   }

   int idx = dylib_find_index(name);
   if (idx < 0)
   {
      printf("[ERROR] Library record not found: %s\n", name);
      return -1;
   }

   LibRecord *lib = &LIB_REGISTRY_ADDR[idx];
   ExtendedLibData *ext = &extended_data[idx];

   if (ext->loaded)
   {
      printf("[WARNING] Library %s is already loaded\n", name);
      return -1;
   }

   // Open the library file from disk
   printf("[DYLIB] Opening %s from disk...\n", filepath);
   FAT_File *file = FAT_Open(partition, filepath);
   if (!file)
   {
      printf("[ERROR] Failed to open file: %s\n", filepath);
      return -1;
   }

   // Get file size
   uint32_t file_size = file->Size;
   if (file_size == 0)
   {
      printf("[ERROR] Library file is empty: %s\n", filepath);
      FAT_Close(file);
      return -1;
   }

   // Allocate memory for the library
   uint32_t load_addr = dylib_mem_alloc(name, file_size);
   if (!load_addr)
   {
      printf("[ERROR] Failed to allocate memory for %s (need %d bytes)\n", name,
             file_size);
      FAT_Close(file);
      return -1;
   }

   // Read library data from disk
   printf("[DYLIB] Reading %d bytes into memory...\n", file_size);
   uint32_t bytes_read =
       FAT_Read(partition, file, file_size, (void *)load_addr);
   if (bytes_read != file_size)
   {
      printf("[ERROR] Failed to read library: expected %d bytes, got %d\n",
             file_size, bytes_read);
      FAT_Close(file);
      dylib_mem_free(name);
      return -1;
   }

   // Close the file
   FAT_Close(file);

   // Update library record
   lib->base = (void *)load_addr;
   lib->size = file_size;
   ext->loaded = 1;

   printf("[DYLIB] Loaded %s (%d bytes) from disk at 0x%x\n", name, file_size,
          load_addr);

   return 0;
}

int dylib_remove(const char *name)
{
   int idx = dylib_find_index(name);
   if (idx < 0)
   {
      printf("[ERROR] Library not found: %s\n", name);
      return -1;
   }

   LibRecord *lib = &LIB_REGISTRY_ADDR[idx];
   ExtendedLibData *ext = &extended_data[idx];

   if (!ext->loaded)
   {
      printf("[WARNING] Library %s is not loaded\n", name);
      return -1;
   }

   // Free memory
   if (dylib_mem_free(name) != 0) return -1;

   // Mark as unloaded
   ext->loaded = 0;
   lib->base = NULL;
   lib->size = 0;

   // Clear dependency resolution
   for (int i = 0; i < ext->dep_count; i++)
   {
      ext->deps[i].resolved = 0;
   }

   printf("[DYLIB] Removed %s from memory\n", name);

   return 0;
}

void dylib_mem_stats(void)
{
   if (!dylib_mem_initialized)
   {
      printf("[DYLIB] Memory allocator not initialized\n");
      return;
   }

   uint32_t total_allocated = dylib_mem_next_free - DYLIB_MEMORY_ADDR;
   uint32_t total_available = DYLIB_MEMORY_SIZE;
   uint32_t remaining = total_available - total_allocated;
   int percent_used = (total_allocated * 100) / total_available;

   printf("\n=== Dylib Memory Statistics ===\n");
   printf("Total Memory:     %d MiB (0x%x - 0x%x)\n",
          total_available / 0x100000, DYLIB_MEMORY_ADDR,
          DYLIB_MEMORY_ADDR + DYLIB_MEMORY_SIZE);
   printf("Allocated:        %d KiB (%d%%)\n", total_allocated / 1024,
          percent_used);
   printf("Available:        %d KiB\n", remaining / 1024);

   // List loaded libraries
   printf("\nLoaded Libraries:\n");
   LibRecord *reg = (LibRecord *)LIB_REGISTRY_ADDR;
   for (int i = 0; i < LIB_REGISTRY_MAX; i++)
   {
      if (reg[i].name[0] == '\0') break;

      ExtendedLibData *ext = &extended_data[i];
      if (ext->loaded)
      {
         printf("  %s: 0x%x bytes at 0x%x\n", reg[i].name, reg[i].size,
                (uint32_t)reg[i].base);
      }
   }
   printf("\n");
}
