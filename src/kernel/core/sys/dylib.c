// SPDX-License-Identifier: AGPL-3.0-or-later

/* Dynamic link helpers for the kernel. Supports true ELF dynamic linking
 * with PLT/GOT relocation. The bootloader (stage2) loads libraries and
 * populates a LibRecord table at LIB_REGISTRY_ADDR. The kernel loader
 * then applies relocations using the global symbol table.
 */

#include "dylib.h"
#include "memory.h"
#include <fs/fat.h>
#include <std/stdio.h>
#include <std/string.h>
#include <stdint.h>

// ELF32 relocation types (i686)
#define R_386_NONE 0
#define R_386_32 1
#define R_386_PC32 2
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8

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

// ELF32 Symbol table entry
typedef struct
{
   uint32_t st_name;
   uint32_t st_value;
   uint32_t st_size;
   uint8_t st_info;
   uint8_t st_other;
   uint16_t st_shndx;
} Elf32_Sym;

#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((i) & 0xff)
#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) & 0xf)

// ELF section header entry
typedef struct
{
   uint32_t sh_name;
   uint32_t sh_type;
   uint32_t sh_flags;
   uint32_t sh_addr;
   uint32_t sh_offset;
   uint32_t sh_size;
   uint32_t sh_link;
   uint32_t sh_info;
   uint32_t sh_addralign;
   uint32_t sh_entsize;
} Elf32_Shdr;

// Section types
#define SHT_SYMTAB 2
#define SHT_DYNSYM 11
#define SHT_STRTAB 3

// Extended library data (kept separately from the base LibRecord registry)
typedef struct
{
   DependencyRecord deps[DYLIB_MAX_DEPS];
   int dep_count;
   SymbolRecord symbols[DYLIB_MAX_SYMBOLS];
   int symbol_count;

   // ELF dynamic section metadata (parsed from .dynamic at load time)
   uint32_t dynsym_addr; // Address of .dynsym section in loaded memory
   uint32_t dynsym_size; // Size in bytes
   uint32_t dynstr_addr; // Address of .dynstr section in loaded memory
   uint32_t dynstr_size; // Size in bytes
   uint32_t rel_addr;    // Address of .rel.dyn relocations
   uint32_t rel_size;    // Size of .rel.dyn
   uint32_t jmprel_addr; // Address of .rel.plt (PLT relocations)
   uint32_t jmprel_size; // Size of .rel.plt
   uint32_t pltgot_addr; // Address of .got.plt (for PLT patching)

   int loaded; // 1 if loaded in memory, 0 if not
} ExtendedLibData;

// Memory allocator state
static int dylib_mem_initialized = 0;
static uint32_t dylib_mem_next_free = DYLIB_MEMORY_ADDR;
static ExtendedLibData extended_data[LIB_REGISTRY_MAX];

// Global symbol table - shared across all loaded libraries and kernel
static GlobalSymbolEntry global_symtab[DYLIB_MAX_GLOBAL_SYMBOLS];
static int global_symtab_count = 0;

// Forward declarations
static int parse_elf_symbols(ExtendedLibData *ext, uint32_t base_addr, uint32_t size);

static dylib_register_symbols_t symbol_callback = NULL;

int dylib_mem_init(void)
{
   if (dylib_mem_initialized) return 0;

   // Clear the memory region
   memset((void *)DYLIB_MEMORY_ADDR, 0, DYLIB_MEMORY_SIZE);

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

   printf("[DYLIB] Processing %d relocations for %s at base 0x%x\n", rel_count, context, base);

   for (uint32_t i = 0; i < rel_count; i++)
   {
      uint32_t r_offset = rel_table[i].r_offset; /* relocation target (usually absolute) */
      int type = ELF32_R_TYPE(rel_table[i].r_info);
      int symidx = ELF32_R_SYM(rel_table[i].r_info);

      /* Basic sanity checks before touching memory */
      if (r_offset == 0)
      {
         printf("[ERROR] Relocation[%d] has r_offset == 0 (skipping)\n", i);
         continue;
      }

      /* Verify target falls within expected area for this base. This avoids
       * writing to clearly invalid low addresses when the relocation entry
       * already contains an absolute virtual address. We allow a large
       * permitted range (1 MiB..+16 MiB) relative to base to be tolerant.
       */
      uint32_t allowed_low = base;
      uint32_t allowed_high = base + 0x0100000; /* 1 MiB window */
      if (r_offset < allowed_low || r_offset > allowed_high)
      {
         printf("[ERROR] Relocation[%d] target 0x%08x outside allowed range 0x%08x-0x%08x\n",
                i, r_offset, allowed_low, allowed_high);
         return -1;
      }

      uint32_t *where = (uint32_t *)r_offset;
      uint32_t cur_val = *where;

      /* Diagnostic: print one-line summary for the relocation */
      printf("[DYLIB]   [%d] type=%d symidx=%d where=0x%08x cur=0x%08x\n",
             i, type, symidx, r_offset, cur_val);

      if (type == R_386_RELATIVE)
      {
         /* Relative relocation - the stored value at *where may already be
          * an absolute address (already relocated) or it may be an addend
          * that must be added to the runtime base. Avoid blindly adding
          * base to an already-correct absolute address (which produced
          * incorrect results and crashes).
          */
         uint32_t addend = cur_val;

         /* If the current value already falls inside the kernel image at
          * runtime, assume it was already relocated and skip rewriting it.
          */
         if (addend >= base && addend <= base + 0x00f00000)
         {
            printf("[DYLIB]     R_386_RELATIVE at 0x%08x: already 0x%08x (skipping)\n",
                   r_offset, addend);
         }
         else if (addend < 0x01000000)
         {
            /* If addend is a small offset, treat it as an addend and
             * relocate to runtime base + addend.
             */
            uint32_t newv = base + addend;
            *where = newv;
            printf("[DYLIB]     R_386_RELATIVE at 0x%08x: addend=0x%08x -> 0x%08x\n",
                   r_offset, addend, newv);
         }
         else
         {
            printf("[WARNING] R_386_RELATIVE at 0x%08x has unexpected value 0x%08x (skipping)\n",
                   r_offset, addend);
            continue;
         }
      }
      else if (type == R_386_32 || type == R_386_PC32 ||
               type == R_386_GLOB_DAT || type == R_386_JMP_SLOT)
      {
         if (symidx > 0 && dynsym_addr > 0)
         {
            uint32_t sym_ent_offset = symidx * 16;
            uint32_t st_name_offset = *(uint32_t *)(dynsym_addr + sym_ent_offset);
            uint32_t st_value = *(uint32_t *)(dynsym_addr + sym_ent_offset + 4);

            if (dynstr_addr > 0)
            {
               const char *sym_name = (const char *)(dynstr_addr + st_name_offset);

               uint32_t sym_addr = dylib_lookup_global_symbol(sym_name);
               if (sym_addr == 0)
               {
                  printf("[WARNING] Unresolved symbol in %s: %s (skipping relocation)\n", context, sym_name);
                  /* Don't abort the whole relocation pass for an unresolved
                   * symbol - warn and continue so other relocations (in
                   * particular .rel.plt entries) can still be applied. */
                  continue;
               }

               uint32_t addend = cur_val;

               switch (type)
               {
               case R_386_32:
               {
                  uint32_t newv = sym_addr + addend;
                  *where = newv;
                  printf("[DYLIB]     R_386_32 %s at 0x%08x: addend=0x%08x -> 0x%08x\n",
                         sym_name, r_offset, addend, newv);
               }
               break;
               case R_386_PC32:
               {
                  uint32_t newv = sym_addr + addend - (uint32_t)where;
                  *where = newv;
                  printf("[DYLIB]     R_386_PC32 %s at 0x%08x: addend=0x%08x -> 0x%08x\n",
                         sym_name, r_offset, addend, newv);
               }
               break;
               case R_386_GLOB_DAT:
               case R_386_JMP_SLOT:
               {
                  uint32_t newv = sym_addr;
                  *where = newv;
                  printf("[DYLIB]     R_386_%s %s at 0x%08x: wrote 0x%08x\n",
                         (type == R_386_GLOB_DAT ? "GLOB_DAT" : "JMP_SLOT"),
                         sym_name, r_offset, newv);
               }
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
   extern char _kernel_dynsym_start[];
   extern char _kernel_dynsym_end[];
   extern char _kernel_dynstr_start[];
   extern char _kernel_dynstr_end[];

   uint32_t kernel_base = 0x00A00000; // Kernel load address

   // Apply .rel.dyn relocations
   {
      uint32_t rel_size =
          (uint32_t)_kernel_rel_dyn_end - (uint32_t)_kernel_rel_dyn_start;
      Elf32_Rel *rel = (Elf32_Rel *)_kernel_rel_dyn_start;
      int rel_count = rel_size / sizeof(Elf32_Rel);

      if (rel_count > 0)
      {
         printf("[DYLIB] Applying %d kernel .rel.dyn relocations...\n",
                rel_count);
         uint32_t dynsym_addr = (uint32_t)_kernel_dynsym_start;
         uint32_t dynstr_addr = (uint32_t)_kernel_dynstr_start;
         if (apply_relocations(kernel_base, rel, rel_count, dynsym_addr, dynstr_addr,
                               "kernel .rel.dyn") != 0)
            return -1;
      }
   }

   // Apply .rel.plt relocations
   {
      uint32_t rel_size =
          (uint32_t)_kernel_rel_plt_end - (uint32_t)_kernel_rel_plt_start;
      Elf32_Rel *rel = (Elf32_Rel *)_kernel_rel_plt_start;
      int rel_count = rel_size / sizeof(Elf32_Rel);

      if (rel_count > 0)
      {
         printf("[DYLIB] Applying %d kernel .rel.plt relocations...\n",
                rel_count);
         uint32_t dynsym_addr = (uint32_t)_kernel_dynsym_start;
         uint32_t dynstr_addr = (uint32_t)_kernel_dynstr_start;
         if (apply_relocations(kernel_base, rel, rel_count, dynsym_addr, dynstr_addr,
                               "kernel .rel.plt") != 0)
            return -1;

            /* Diagnostic: print GOT entries for JMP_SLOT relocations so we can
             * verify they point to the expected symbol addresses. */
            printf("[DYLIB] Inspecting GOT entries for kernel .rel.plt...\n");
            for (int ri = 0; ri < rel_count; ri++)
            {
               int rtype = ELF32_R_TYPE(rel[ri].r_info);
               int rsym = ELF32_R_SYM(rel[ri].r_info);
               if (rtype != R_386_JMP_SLOT) continue;

               uint32_t where_addr = rel[ri].r_offset; /* already absolute */
               uint32_t got_val = *(uint32_t *)where_addr;

               const char *sym_name = "(unknown)";
               uint32_t sym_addr = 0;
               if (dynsym_addr && dynstr_addr && rsym > 0)
               {
                  uint32_t sym_ent_offset = rsym * 16;
                  uint32_t st_name = *(uint32_t *)(dynsym_addr + sym_ent_offset);
                  sym_name = (const char *)(dynstr_addr + st_name);
                  sym_addr = *(uint32_t *)(dynsym_addr + sym_ent_offset + 4);
               }

               printf("[DYLIB]  .rel.plt[%d] -> GOT@0x%x = 0x%08x (sym='%s' dynsym_val=0x%08x)\n",
                      ri, where_addr, got_val, sym_name, sym_addr);
            }
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

int dylib_parse_symbols(LibRecord *lib)
{
   if (!lib || !lib->base)
   {
      printf("[ERROR] Invalid library record\n");
      return -1;
   }

   int idx = dylib_find_index(lib->name);
   if (idx < 0)
   {
      printf("[ERROR] Library not found in registry: %s\n", lib->name);
      return -1;
   }

   ExtendedLibData *ext = &extended_data[idx];

   // Parse ELF symbols from the pre-loaded library at its base address
   printf("[DYLIB] Parsing symbols for pre-loaded library: %s at 0x%x\n", 
          lib->name, (unsigned int)lib->base);
   
   parse_elf_symbols(ext, (uint32_t)lib->base, lib->size);
   
   ext->loaded = 1;  // Mark as loaded so symbol table is available

   return 0;
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
   void *dest = (void *)load_addr;
   const void *src = (const void *)image;
   memcpy(dest, src, size);

   // Update library record
   lib->base = (void *)load_addr;
   lib->size = size;
   ext->loaded = 1;

   printf("[DYLIB] Loaded %s (%d bytes) at 0x%x\n", name, size, load_addr);

   // Parse ELF symbols from the loaded library
   parse_elf_symbols(ext, load_addr, size);

   return 0;
}

// Parse ELF header and extract dynamic symbols from a loaded library
static int parse_elf_symbols(ExtendedLibData *ext, uint32_t base_addr, uint32_t size)
{
   // ELF header at the beginning of the loaded binary
   uint8_t *elf_data = (uint8_t *)base_addr;
   
   // Check ELF magic number
   if (elf_data[0] != 0x7f || elf_data[1] != 'E' || elf_data[2] != 'L' || elf_data[3] != 'F')
   {
      printf("[ERROR] Not a valid ELF file\n");
      return -1;
   }
   
   // Parse ELF32 header (little-endian)
   uint32_t e_shoff = *(uint32_t *)(elf_data + 32);  // Section header offset (in file)
   uint16_t e_shnum = *(uint16_t *)(elf_data + 48);  // Number of sections
   uint16_t e_shentsize = *(uint16_t *)(elf_data + 46); // Section header entry size
   
   printf("[DYLIB] ELF: e_shoff=0x%x, e_shnum=%d, e_shentsize=%d\n", e_shoff, e_shnum, e_shentsize);
   
   if (e_shoff == 0 || e_shnum == 0 || e_shentsize == 0)
   {
      printf("[DYLIB] Invalid section headers\n");
      return 0;
   }
   
   // Find the first PROGBITS section to determine the offset adjustment
   // When we load the ELF file, all sections keep their relative offsets
   // But the sections that need to be in memory are those with SHF_ALLOC flag
   // We need to find where .text actually starts in the file
   uint32_t text_section_file_offset = 0;
   for (int i = 0; i < e_shnum; i++)
   {
      Elf32_Shdr *sh = (Elf32_Shdr *)(elf_data + e_shoff + (i * e_shentsize));
      // Find the first allocable section - this is where code actually starts
      if (sh->sh_type == 1 && (sh->sh_flags & 0x2))  // PROGBITS with ALLOC
      {
         text_section_file_offset = sh->sh_offset;
         printf("[DYLIB] First loadable section (.text) at file offset 0x%x\n", text_section_file_offset);
         break;
      }
   }
   
   // File offsets from base_addr need to be adjusted by the .text section offset
   // Memory layout: base_addr points to start of loaded file (including ELF header)
   // But symbols are addresses in the code section
   // So: symbol_memory_address = base_addr + file_offset_of_section + offset_within_section
   //                           = base_addr + st_value_offset
   // where st_value_offset = st_value - original_base (offset from link address)
   
   // Find the first loadable section to determine the original base address
   // For simplicity, we'll use a default of 0x08000000 (from typical shared lib link address)
   uint32_t original_base = 0x08000000;
   
   // Find .symtab and .strtab sections
   uint32_t symtab_addr = 0, symtab_size = 0, symtab_entsize = 0;
   uint32_t strtab_addr = 0, strtab_size = 0;
   int strtab_link = -1;
   
   for (int i = 0; i < e_shnum; i++)
   {
      Elf32_Shdr *sh = (Elf32_Shdr *)(elf_data + e_shoff + (i * e_shentsize));
      
      printf("[DYLIB] Section %d: type=%d, offset=0x%x, size=%d, link=%d, entsize=%d\n", 
             i, sh->sh_type, sh->sh_offset, sh->sh_size, sh->sh_link, sh->sh_entsize);
      
      if (sh->sh_type == SHT_SYMTAB)
      {
         // Found symbol table - address is file offset + base (since we loaded full file)
         symtab_addr = base_addr + sh->sh_offset;
         symtab_size = sh->sh_size;
         symtab_entsize = sh->sh_entsize;
         strtab_link = sh->sh_link;  // Index of associated string table
         printf("[DYLIB] Found .symtab at file offset 0x%x, memory 0x%x, size=%d, entsize=%d, strtab_link=%d\n", 
                sh->sh_offset, symtab_addr, symtab_size, symtab_entsize, strtab_link);
      }
   }
   
   // Now find the associated string table
   if (strtab_link >= 0 && strtab_link < e_shnum)
   {
      Elf32_Shdr *sh = (Elf32_Shdr *)(elf_data + e_shoff + (strtab_link * e_shentsize));
      if (sh->sh_type == SHT_STRTAB)
      {
         strtab_addr = base_addr + sh->sh_offset;
         strtab_size = sh->sh_size;
         printf("[DYLIB] Found associated .strtab at file offset 0x%x, memory 0x%x, size=%d\n", 
                sh->sh_offset, strtab_addr, strtab_size);
      }
   }
   
   if (symtab_addr == 0 || strtab_addr == 0 || symtab_entsize == 0)
   {
      printf("[DYLIB] Symbol table, string table, or entsize not found/invalid\n");
      return 0;
   }
   
   // Parse symbol entries
   uint32_t num_symbols = symtab_size / symtab_entsize;
   ext->symbol_count = 0;
   
   printf("[DYLIB] Parsing %d symbols (entsize=%d, base_addr=0x%x, original_base=0x%x)...\n", 
          num_symbols, symtab_entsize, base_addr, original_base);
   
   for (uint32_t i = 0; i < num_symbols && ext->symbol_count < DYLIB_MAX_SYMBOLS; i++)
   {
      Elf32_Sym *sym = (Elf32_Sym *)(symtab_addr + (i * symtab_entsize));
      
      // Skip undefined and local symbols
      uint8_t st_bind = ELF32_ST_BIND(sym->st_info);
      uint8_t st_type = ELF32_ST_TYPE(sym->st_info);
      
      if (st_bind == 0 || sym->st_shndx == 0) continue;  // Skip local or undefined
      
      // Get symbol name from string table
      if (sym->st_name < strtab_size)
      {
         const char *sym_name = (const char *)(strtab_addr + sym->st_name);
         if (sym_name[0] != '\0')
         {
            // Add to symbol table
            strncpy(ext->symbols[ext->symbol_count].name, sym_name, 63);
            ext->symbols[ext->symbol_count].name[63] = '\0';
            
            // Symbol address calculation:
            // st_value is the absolute address in the linked image (e.g., 0x08000000 + offset)
            // Offset from link base: st_value - original_base
            // Actual address: base_addr (ELF file start in memory) + file_offset_of_section + offset_within_section
            // But st_value is already relative to 0x08000000, which is 0x1000 bytes into the file (where .text starts)
            // So: memory_addr = base_addr + text_section_file_offset + (st_value - original_base)
            //               = base_addr + 0x1000 + (st_value - 0x08000000)
            uint32_t symbol_offset_in_code = sym->st_value - original_base;
            uint32_t symbol_addr = base_addr + text_section_file_offset + symbol_offset_in_code;
            ext->symbols[ext->symbol_count].address = symbol_addr;
            printf("[DYLIB]   Symbol[%d]: %s @ 0x%x (st_value=0x%x, shndx=%d)\n", 
                   ext->symbol_count, sym_name, symbol_addr, sym->st_value, sym->st_shndx);
            ext->symbol_count++;
         }
      }
   }
   
   printf("[DYLIB] Extracted %d symbols\n", ext->symbol_count);
   
   // Apply simple relocation: scan loadable sections for embedded original_base addresses and patch them
   printf("[DYLIB] Applying address relocations...\n");
   
   for (int i = 0; i < e_shnum; i++)
   {
      Elf32_Shdr *sh = (Elf32_Shdr *)(elf_data + e_shoff + (i * e_shentsize));
      
      if (sh->sh_type == 1 && (sh->sh_flags & 0x2))  // PROGBITS with ALLOC flag
      {
         uint8_t *section_start = elf_data + sh->sh_offset;
         uint32_t section_size = sh->sh_size;
         
         printf("[DYLIB]   Scanning section at file offset 0x%x (size=%d) for embedded addresses...\n", 
                sh->sh_offset, section_size);
         
         // Scan for 32-bit addresses matching the original base
         for (uint32_t j = 0; j < section_size - 3; j++)
         {
            uint32_t *addr_ptr = (uint32_t *)(section_start + j);
            uint32_t value = *addr_ptr;
            
            // Check if this looks like an address in the original library range
            if (value >= original_base && value < original_base + 0x10000)
            {
               // This might be a relative address - compute the offset
               uint32_t offset = value - original_base;
               // Patch with new base
               uint32_t new_value = base_addr + offset;
               *addr_ptr = new_value;
               printf("[DYLIB]     Patched at file offset 0x%x (memory 0x%x): 0x%x -> 0x%x\n", 
                      sh->sh_offset + j, (uint32_t)addr_ptr, value, new_value);
            }
         }
      }
   }
   
   printf("[DYLIB] Relocation patching complete\n");
   
   // Now look for formal relocation sections (if they exist)
   printf("[DYLIB] Looking for formal relocation sections...\n");
   for (int i = 0; i < e_shnum; i++)
   {
      Elf32_Shdr *sh = (Elf32_Shdr *)(elf_data + e_shoff + (i * e_shentsize));
      
      // Look for .rel.dyn or .rel.text sections (type 9 = SHT_REL)
      if (sh->sh_type == 9)  // SHT_REL
      {
         uint32_t rel_addr = base_addr + sh->sh_offset;
         uint32_t rel_size = sh->sh_size;
         uint32_t rel_entsize = sh->sh_entsize;
         int rel_count = rel_size / rel_entsize;
         
         printf("[DYLIB]   Applying %d relocations from section %d\n", rel_count, i);
         
         for (int j = 0; j < rel_count; j++)
         {
            Elf32_Rel *rel = (Elf32_Rel *)(rel_addr + (j * rel_entsize));
            uint32_t *patch_addr = (uint32_t *)(base_addr + rel->r_offset);
            uint32_t type = ELF32_R_TYPE(rel->r_info);
            
            // For R_386_RELATIVE, just add the difference between load and original base
            if (type == R_386_RELATIVE)
            {
               uint32_t adjustment = base_addr - original_base;
               *patch_addr += adjustment;
               printf("[DYLIB]     Reloc at 0x%x: R_386_RELATIVE, patching with +0x%x\n", 
                      rel->r_offset, adjustment);
            }
         }
      }
   }
   
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
   FAT_Seek(partition, file, 0);
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

   // Parse ELF symbols from the loaded library
   parse_elf_symbols(ext, load_addr, file_size);

   if (symbol_callback)
   {
       symbol_callback(name);
   }

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

void dylib_register_callback(dylib_register_symbols_t callback)
{
    symbol_callback = callback;
}

static int load_libmath(Partition *partition)
{
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
      if (dylib_load_from_disk(partition, "libmath", "/sys/libmath.so") != 0)
      {
         printf("[!] Failed to load libmath.so\n");
         return -1;
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
   return 0;
}

bool dylib_Initialize(Partition *partition)
{
   // Load math library
   if(load_libmath(partition) != 0)
      return false;
   return true;
}