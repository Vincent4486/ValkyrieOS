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

bool ELF_Load(Partition *disk, FAT_File *file, void **entryOut)
{
   // read ELF header
   if (!FAT_Seek(disk, file, 0))
   {
      printf("ELF: seek header failed\r\n");
      return false;
   }

   Elf32_Ehdr ehdr;
   if (FAT_Read(disk, file, sizeof(ehdr), &ehdr) != sizeof(ehdr))
   {
      printf("ELF: read header failed\r\n");
      return false;
   }

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
      if (!FAT_Seek(disk, file, phoff))
      {
         printf("ELF: seek phdr %u failed\r\n", i);
         return false;
      }

      if (FAT_Read(disk, file, sizeof(phdr), &phdr) != sizeof(phdr))
      {
         printf("ELF: read phdr %u failed\r\n", i);
         return false;
      }

      const uint32_t PT_LOAD = 1;
      if (phdr.p_type != PT_LOAD) continue;

      // determine destination address (prefer physical p_paddr if provided)
      uint8_t *dest = (uint8_t *)(phdr.p_paddr ? phdr.p_paddr : phdr.p_vaddr);

      // read file data for this segment
      uint32_t remaining = phdr.p_filesz;
      uint32_t fileOffset = phdr.p_offset;
      const uint32_t CHUNK =
          512; // FAT sector size, read in sector-sized chunks

      if (remaining > 0)
      {
         if (!FAT_Seek(disk, file, fileOffset))
         {
            printf("ELF: seek segment data failed\r\n");
            return false;
         }

         while (remaining > 0)
         {
            uint32_t toRead = remaining > CHUNK ? CHUNK : remaining;
            uint32_t got = FAT_Read(disk, file, toRead, dest);
            if (got == 0)
            {
               printf("ELF: short read for segment\r\n");
               return false;
            }

            dest += got;
            remaining -= got;
         }
      }

      // zero the rest for bss
      if (phdr.p_memsz > phdr.p_filesz)
      {
         uint32_t zeros = phdr.p_memsz - phdr.p_filesz;
         memset(dest, 0, zeros);
      }
   }

   // return entry point
   *entryOut = (void *)ehdr.e_entry;
   return true;
}
