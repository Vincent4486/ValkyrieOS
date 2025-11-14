// SPDX-License-Identifier: AGPL-3.0-or-later
#include "elf.h"
#include "memdefs.h"
#include "memory.h"
#include "stdio.h"
#include "string.h"

// ELF structures for 32-bit little-endian
typedef struct
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
} __attribute__((packed)) Elf32_Ehdr;

typedef struct
{
   uint32_t p_type;
   uint32_t p_offset;
   uint32_t p_vaddr;
   uint32_t p_paddr;
   uint32_t p_filesz;
   uint32_t p_memsz;
   uint32_t p_flags;
   uint32_t p_align;
} __attribute__((packed)) Elf32_Phdr;

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFCLASS32 1
#define ELFDATA2LSB 1
#define EM_386 3

bool ELF_Load(Partition *disk, const char *filepath, void **entryOut)
{
   // read ELF header
   FAT_File *hdr_file = FAT_Open(disk, filepath);
   if (!hdr_file)
   {
      printf("ELF: failed to open file for header\r\n");
      return false;
   }

   Elf32_Ehdr ehdr;
   if (FAT_Read(disk, hdr_file, sizeof(ehdr), &ehdr) != sizeof(ehdr))
   {
      printf("ELF: read header failed\r\n");
      FAT_Close(hdr_file);
      return false;
   }
   FAT_Close(hdr_file);

   // validate magic and class
   if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
       ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
   {
      printf("ELF: bad magic\r\n");
      return false;
   }

   if (ehdr.e_ident[4] != ELFCLASS32 || ehdr.e_ident[5] != ELFDATA2LSB)
   {
      printf("ELF: unsupported ELF class or endian\r\n");
      return false;
   }

   if (ehdr.e_machine != EM_386)
   {
      printf("ELF: unsupported machine\r\n");
      return false;
   }

   // read program headers
   if (ehdr.e_phnum == 0 || ehdr.e_phentsize != sizeof(Elf32_Phdr))
   {
      printf("ELF: no program headers or unexpected phentsize\r\n");
      return false;
   }

   // allocate temporary buffer for program headers (small count expected)
   Elf32_Phdr phdr;

   for (uint16_t i = 0; i < ehdr.e_phnum; i++)
   {
      uint32_t phoff = ehdr.e_phoff + i * ehdr.e_phentsize;

      // Open file fresh to skip to program header offset
      FAT_File *phdr_file = FAT_Open(disk, filepath);
      if (!phdr_file)
      {
         printf("ELF: failed to open file for phdr %u\r\n", i);
         return false;
      }

      // Skip to the program header by reading and discarding data
      uint32_t skip = phoff;
      uint8_t skip_buf[512];
      while (skip > 0)
      {
         uint32_t to_skip = skip > sizeof(skip_buf) ? sizeof(skip_buf) : skip;
         uint32_t got = FAT_Read(disk, phdr_file, to_skip, skip_buf);
         if (got == 0)
         {
            printf("ELF: failed to skip to phdr %u\r\n", i);
            FAT_Close(phdr_file);
            return false;
         }
         skip -= got;
      }

      if (FAT_Read(disk, phdr_file, sizeof(phdr), &phdr) != sizeof(phdr))
      {
         printf("ELF: read phdr %u failed\r\n", i);
         FAT_Close(phdr_file);
         return false;
      }
      FAT_Close(phdr_file);

      const uint32_t PT_LOAD = 1;
      if (phdr.p_type != PT_LOAD) continue;

      // determine destination address (prefer physical p_paddr if provided)
      uint8_t *dest = (uint8_t *)(phdr.p_paddr ? phdr.p_paddr : phdr.p_vaddr);

      // Open file again to read segment data
      FAT_File *seg_file = FAT_Open(disk, filepath);
      if (!seg_file)
      {
         printf("ELF: failed to open file for segment data\r\n");
         return false;
      }

      // Skip to segment offset
      skip = phdr.p_offset;
      while (skip > 0)
      {
         uint32_t to_skip = skip > sizeof(skip_buf) ? sizeof(skip_buf) : skip;
         uint32_t got = FAT_Read(disk, seg_file, to_skip, skip_buf);
         if (got == 0)
         {
            printf("ELF: failed to skip to segment data\r\n");
            FAT_Close(seg_file);
            return false;
         }
         skip -= got;
      }

      // read file data for this segment
      uint32_t remaining = phdr.p_filesz;
      const uint32_t CHUNK = 512; // FAT sector size

      while (remaining > 0)
      {
         uint32_t toRead = remaining > CHUNK ? CHUNK : remaining;
         uint32_t got = FAT_Read(disk, seg_file, toRead, dest);
         if (got == 0)
         {
            printf("ELF: short read for segment\r\n");
            FAT_Close(seg_file);
            return false;
         }

         dest += got;
         remaining -= got;
      }

      // zero the rest for bss
      if (phdr.p_memsz > phdr.p_filesz)
      {
         uint32_t zeros = phdr.p_memsz - phdr.p_filesz;
         memset(dest, 0, zeros);
      }

      FAT_Close(seg_file);
   }

   // return entry point
   *entryOut = (void *)ehdr.e_entry;
   return true;
}
