// SPDX-License-Identifier: GPL-3.0-only

#include <stddef.h>
#include <stdint.h>

#include <bits.h>
#include <status.h>

#include <dl/loader.h>

typedef struct Elf32Ehdr Elf32Ehdr;
typedef struct Elf32Shdr Elf32Shdr;
typedef struct Elf32Symbol Elf32Symbol;
typedef struct Elf32Rel Elf32Rel;
typedef struct Elf64Ehdr Elf64Ehdr;
typedef struct Elf64Shdr Elf64Shdr;
typedef struct Elf64Symbol Elf64Symbol;
typedef struct Elf64Rela Elf64Rela;

typedef struct DlHandle DlHandle;

/* ELF identification indices */
#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_ABIVERSION 8

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS32 1
#define ELFCLASS64 2

/* Section types */
#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_DYNSYM 11

/* Relocation types */
#if defined(BIT32)
#define R_REL_GLOB_DAT 6
#define R_REL_JMP_SLOT 7
#define R_REL_RELATIVE 8
#elif defined(BIT64)
#define R_REL_GLOB_DAT 6
#define R_REL_JMP_SLOT 7
#define R_REL_RELATIVE 8
#endif

/* Symbol binding and type helpers */
#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) & 0xf)
#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xf)
#define STB_GLOBAL 1
#define STT_FUNC 2

#if defined(BIT32)
#define ElfEhdr Elf32Ehdr
#define ElfShdr Elf32Shdr
#define ElfSymbol Elf32Symbol
#define ELF_ST_BIND(i) ELF32_ST_BIND(i)
#define ELF_ST_TYPE(i) ELF32_ST_TYPE(i)
#define ELFCLASS_EXPECTED ELFCLASS32
#elif defined(BIT64)
#define ElfEhdr Elf64Ehdr
#define ElfShdr Elf64Shdr
#define ElfSymbol Elf64Symbol
#define ELF_ST_BIND(i) ELF64_ST_BIND(i)
#define ELF_ST_TYPE(i) ELF64_ST_TYPE(i)
#define ELFCLASS_EXPECTED ELFCLASS64
#endif

/* Opaque handle buffer size (must match DlHandle layout below) */
#define DL_HANDLE_SIZE 32

struct __attribute__((packed)) Elf32Ehdr
{
   unsigned char e_ident[16];
   uint16_t e_type;
   uint16_t e_machine;
   uint32_t e_version;
   uint32_t e_entry;
   uint32_t e_phoff;
   uint32_t e_shoff;
   uint32_t e_flags;
   uint16_t e_ehsize;
   uint16_t e_phentsize;
   uint16_t e_phnum;
   uint16_t e_shentsize;
   uint16_t e_shnum;
   uint16_t e_shstrndx;
};

struct __attribute__((packed)) Elf32Shdr
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
};

struct __attribute__((packed)) Elf32Symbol
{
   uint32_t st_name;
   uint32_t st_value;
   uint32_t st_size;
   uint8_t st_info;
   uint8_t st_other;
   uint16_t st_shndx;
};

struct __attribute__((packed)) Elf32Rel
{
   uint32_t r_offset;
   uint32_t r_info;
};

struct __attribute__((packed)) Elf64Ehdr
{
   unsigned char e_ident[16];
   uint16_t e_type;
   uint16_t e_machine;
   uint32_t e_version;
   uint64_t e_entry;
   uint64_t e_phoff;
   uint64_t e_shoff;
   uint32_t e_flags;
   uint16_t e_ehsize;
   uint16_t e_phentsize;
   uint16_t e_phnum;
   uint16_t e_shentsize;
   uint16_t e_shnum;
   uint16_t e_shstrndx;
};

struct __attribute__((packed)) Elf64Shdr
{
   uint32_t sh_name;
   uint32_t sh_type;
   uint64_t sh_flags;
   uint64_t sh_addr;
   uint64_t sh_offset;
   uint64_t sh_size;
   uint32_t sh_link;
   uint32_t sh_info;
   uint64_t sh_addralign;
   uint64_t sh_entsize;
};

struct __attribute__((packed)) Elf64Symbol
{
   uint32_t st_name;
   uint8_t st_info;
   uint8_t st_other;
   uint16_t st_shndx;
   uint64_t st_value;
   uint64_t st_size;
};

struct __attribute__((packed)) Elf64Rela
{
   uint64_t r_offset;
   uint64_t r_info;
   int64_t r_addend;
};

struct DlHandle
{
   void *symtab;     /* pointer to symbol table in file data */
   void *strtab;     /* pointer to string table in file data */
   void *image_base; /* base address where the ELF was loaded */
   void *shdr;       /* pointer to section headers */
   int sym_count;    /* number of symbol entries */
   int shnum;        /* number of section headers */
};

/* Internal handle storage — one library at a time */
static DlHandle s_Handle = {0};

/* Convert a virtual address to a file-data pointer using the section headers. */
static void *vaddr_to_ptr(void *file_data, ElfShdr *shdr, int shnum,
                           uintptr_t vaddr)
{
   for (int i = 0; i < shnum; i++)
   {
      if (shdr[i].sh_addr == 0) continue; /* skip non-loaded sections */
      uintptr_t end = shdr[i].sh_addr + shdr[i].sh_size;
      if (shdr[i].sh_addr <= vaddr && vaddr < end)
      {
         return (void *)((uintptr_t)file_data + shdr[i].sh_offset +
                         (vaddr - shdr[i].sh_addr));
      }
   }
   return NULL;
}

void *DL_LoadLibrary(void *file_data)
{
   unsigned char *ident = (unsigned char *)file_data;
   DlHandle *h = &s_Handle;

   /* Validate ELF magic and class */
   if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
       ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3 ||
       ident[EI_CLASS] != ELFCLASS_EXPECTED)
      return NULL;

   h->symtab = NULL;
   h->strtab = NULL;
   h->image_base = file_data;
   h->sym_count = 0;

   ElfEhdr *ehdr = (ElfEhdr *)file_data;
   ElfShdr *shdr = (ElfShdr *)((uintptr_t)file_data + ehdr->e_shoff);

   h->shdr = shdr;
   h->shnum = ehdr->e_shnum;

   /* Find symbol table (prefer .dynsym for shared libraries). */
   for (int i = 0; i < ehdr->e_shnum; i++)
   {
      if (shdr[i].sh_type == SHT_DYNSYM)
      {
         h->symtab = (void *)((uintptr_t)file_data + shdr[i].sh_offset);
         h->sym_count = shdr[i].sh_size / shdr[i].sh_entsize;

         if (shdr[i].sh_link < (uint32_t)ehdr->e_shnum)
            h->strtab = (void *)((uintptr_t)file_data +
                                 shdr[shdr[i].sh_link].sh_offset);
         break;
      }
   }

   /* If no .dynsym, fall back to .symtab. */
   if (!h->symtab)
   {
      for (int i = 0; i < ehdr->e_shnum; i++)
      {
         if (shdr[i].sh_type == SHT_SYMTAB)
         {
            h->symtab = (void *)((uintptr_t)file_data + shdr[i].sh_offset);
            h->sym_count = shdr[i].sh_size / shdr[i].sh_entsize;
            if (shdr[i].sh_link < (uint32_t)ehdr->e_shnum)
               h->strtab = (void *)((uintptr_t)file_data +
                                    shdr[shdr[i].sh_link].sh_offset);
            break;
         }
      }
   }

   if (!h->symtab || !h->strtab) return NULL;

   /* Process .rel.dyn and .rel.plt relocations to fix up GOT entries. */
   for (int i = 0; i < ehdr->e_shnum; i++)
   {
      if (shdr[i].sh_type != SHT_REL) continue;

      /* Find the associated symbol table for this relocation section. */
      ElfSymbol *dynsym = NULL;
      int dynsym_count = 0;
      if (shdr[i].sh_link < (uint32_t)ehdr->e_shnum &&
          shdr[shdr[i].sh_link].sh_type == SHT_DYNSYM)
      {
         dynsym = (ElfSymbol *)((uintptr_t)file_data +
                                shdr[shdr[i].sh_link].sh_offset);
         dynsym_count = shdr[shdr[i].sh_link].sh_size /
                        shdr[shdr[i].sh_link].sh_entsize;
      }

#if defined(BIT32)
      Elf32Rel *rel = (Elf32Rel *)((uintptr_t)file_data + shdr[i].sh_offset);
      int rel_count = shdr[i].sh_size / sizeof(Elf32Rel);

      for (int j = 0; j < rel_count; j++)
      {
         uint32_t r_type = rel[j].r_info & 0xFF;
         uint32_t r_sym = rel[j].r_info >> 8;
         void *patch_addr = vaddr_to_ptr(file_data, shdr, ehdr->e_shnum,
                                         rel[j].r_offset);
         if (!patch_addr) continue;

         if (r_type == R_REL_GLOB_DAT || r_type == R_REL_JMP_SLOT)
         {
            if (dynsym && r_sym < (uint32_t)dynsym_count)
            {
               void *sym_ptr = vaddr_to_ptr(file_data, shdr, ehdr->e_shnum,
                                             dynsym[r_sym].st_value);
               if (sym_ptr)
                  *(uint32_t *)patch_addr = (uint32_t)(uintptr_t)sym_ptr;
            }
         }
         else if (r_type == R_REL_RELATIVE)
         {
            *(uint32_t *)patch_addr += (uint32_t)(uintptr_t)file_data;
         }
      }
#elif defined(BIT64)
      Elf64Rela *rel = (Elf64Rela *)((uintptr_t)file_data + shdr[i].sh_offset);
      int rel_count = shdr[i].sh_size / sizeof(Elf64Rela);

      for (int j = 0; j < rel_count; j++)
      {
         uint32_t r_type = rel[j].r_info & 0xFF;
         uint32_t r_sym = rel[j].r_info >> 8;
         void *patch_addr = vaddr_to_ptr(file_data, shdr, ehdr->e_shnum,
                                         (uintptr_t)rel[j].r_offset);
         if (!patch_addr) continue;

         if (r_type == R_REL_GLOB_DAT || r_type == R_REL_JMP_SLOT)
         {
            if (dynsym && r_sym < (uint32_t)dynsym_count)
            {
               void *sym_ptr = vaddr_to_ptr(file_data, shdr, ehdr->e_shnum,
                                             (uintptr_t)dynsym[r_sym].st_value);
               if (sym_ptr)
                  *(uint64_t *)patch_addr = (uint64_t)(uintptr_t)sym_ptr;
            }
         }
         else if (r_type == R_REL_RELATIVE)
         {
            *(uint64_t *)patch_addr = (uint64_t)(uintptr_t)file_data +
                                      (uint64_t)rel[j].r_addend;
         }
      }
#endif
   }

   return h;
}

void *DL_LoadSymbol(void *handle, const char *symbol)
{
   DlHandle *h = (DlHandle *)handle;

   if (!h->symtab || !h->strtab || !symbol) return NULL;

   ElfSymbol *sym = (ElfSymbol *)h->symtab;
   char *strtab = (char *)h->strtab;

   for (int i = 0; i < h->sym_count; i++)
   {
      char *name = strtab + sym[i].st_name;
      if (name[0] == '\0') continue;

      int j = 0;
      while (name[j] == symbol[j] && name[j] != '\0')
         j++;
      if (name[j] == '\0' && symbol[j] == '\0')
      {
         void *ptr = vaddr_to_ptr(h->image_base, (ElfShdr *)h->shdr,
                                   h->shnum, sym[i].st_value);
         if (ptr) return ptr;
         /* Fallback: if vaddr_to_ptr fails (e.g. for .bss symbols),
            fall back to image_base + st_value. */
         return (void *)(uintptr_t)(h->image_base + sym[i].st_value);
      }
   }

   return NULL;
}