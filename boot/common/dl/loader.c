// SPDX-License-Identifier: GPL-3.0-only

#include <stddef.h>
#include <stdint.h>

#include <bits.h>
#include <status.h>

#include <dl/loader.h>

typedef struct Elf32Ehdr Elf32Ehdr;
typedef struct Elf32Shdr Elf32Shdr;
typedef struct Elf32Symbol Elf32Symbol;
typedef struct Elf64Ehdr Elf64Ehdr;
typedef struct Elf64Shdr Elf64Shdr;
typedef struct Elf64Symbol Elf64Symbol;

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
#define SHT_DYNSYM 11

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

struct DlHandle
{
   void *symtab;     /* pointer to symbol table in file data */
   void *strtab;     /* pointer to string table in file data */
   void *image_base; /* base address where the ELF was loaded */
   int sym_count;    /* number of symbol entries */
};

/* Internal handle storage — one library at a time */
static DlHandle s_Handle = {0};

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

   for (int i = 0; i < ehdr->e_shnum; i++)
   {
      if (shdr[i].sh_type == SHT_SYMTAB || shdr[i].sh_type == SHT_DYNSYM)
      {
         h->symtab = (void *)((uintptr_t)file_data + shdr[i].sh_offset);
         h->sym_count = shdr[i].sh_size / shdr[i].sh_entsize;

         if (shdr[i].sh_link < (uint32_t)ehdr->e_shnum)
            h->strtab = (void *)((uintptr_t)file_data +
                                 shdr[shdr[i].sh_link].sh_offset);
         return h;
      }
   }

   return NULL;
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
         return (void *)(uintptr_t)(h->image_base + sym[i].st_value);
   }

   return NULL;
}