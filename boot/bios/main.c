// SPDX-License-Identifier: GPL-3.0-only

#include <stdint.h>

#include "video/video.h"
#include <constants.h>

#define DL_RESOLVE
#include <dl/loader.h>
#include <dl/binding_gen.h>
#include <dl/callback.h>
#undef DL_RESOLVE

typedef struct CoreFsOperations CoreFsOperations;
typedef struct MbiTagFramebuffer MbiTagFramebuffer;
typedef struct BootParams BootParams;

static void init_framebuffer_info();
static void print_bios_drive_list(void);
static void print_stage3_fs_location(void);

/* Multiboot2 tag types */
#define MBI_TAG_END 0
#define MBI_TAG_MMAP 6
#define MBI_TAG_FRAMEBUFFER 8

#if defined(RELEASE)
#define BUILD_TYPE "release"
#else
#define BUILD_TYPE "debug"
#endif

struct CoreFsOperations
{
   int (*Initialize)(const uint8_t *, uint32_t, const uint8_t *,
                     const uint8_t *);
   int (*Open)(const char *);
   int (*Read)(int, void *, int);
   int (*Close)(int);
   int (*DISK_Read)(uint8_t drive, uint16_t cylinder, uint8_t sector,
                    uint8_t head, uint8_t count, void *buffer);
   int (*DISK_ReadLBA)(uint8_t drive, uint64_t lba, uint16_t count,
                       void *buffer);
};

struct MbiTagFramebuffer
{
   uint32_t type;
   uint32_t size;
   uint64_t framebuffer_addr;
   uint32_t framebuffer_pitch;
   uint32_t framebuffer_width;
   uint32_t framebuffer_height;
   uint8_t framebuffer_bpp;
   uint8_t framebuffer_type;
   uint16_t reserved;
   uint8_t red_field_position;
   uint8_t red_mask_size;
   uint8_t green_field_position;
   uint8_t green_mask_size;
   uint8_t blue_field_position;
   uint8_t blue_mask_size;
   uint8_t rgb_reserved[2];
};

struct __attribute__((packed)) BootParams
{
   MbiTagFramebuffer *mbi_tags;
   CoreFsOperations *corefs_ops;
   uint8_t available_outputs;
   uint8_t boot_drive;
   uint8_t *bios_drive_list;
   uint8_t bios_drive_count;
   uint8_t *corefs_partition_uuid;
   uint8_t *corefs_partition_label;
};

int g_PrimaryOutputSystem = 0;
int g_PreferredOutput = OUTPUT_VGATEXT;
const char *g_Stage3Path =
    "/boot/libTheBootloader-" OS_VERSION "_" BUILD_TYPE ".so";

static BootParams s_BootParams = {0};
static DL_CallbackOperations s_DlCallbackOps = {0};

static void init_framebuffer_info()
{
   MbiTagFramebuffer *tag = s_BootParams.mbi_tags;
   for (;;)
   {
      if (tag->type == MBI_TAG_END) break;

      if (tag->type == MBI_TAG_FRAMEBUFFER && tag->size >= sizeof(MbiTagFramebuffer))
      {
         if (tag->framebuffer_type == 1)
         {
            VBE_Info info;
            info.framebuffer_addr = tag->framebuffer_addr;
            info.pitch = tag->framebuffer_pitch;
            info.width = tag->framebuffer_width;
            info.height = tag->framebuffer_height;
            info.bpp = tag->framebuffer_bpp;
            info.red_field_position = tag->red_field_position;
            info.red_mask_size = tag->red_mask_size;
            info.green_field_position = tag->green_field_position;
            info.green_mask_size = tag->green_mask_size;
            info.blue_field_position = tag->blue_field_position;
            info.blue_mask_size = tag->blue_mask_size;
            VBE_SetInfo(&info);
         }
      }

      /* Advance to next tag (8-byte aligned); size is in bytes. */
      tag = (MbiTagFramebuffer *)((uint8_t *)tag + tag->size);
      tag = (MbiTagFramebuffer *)(((uintptr_t)tag + 7) & ~(uintptr_t)7);
   }
}

static void print_bios_drive_list(void)
{
   uint32_t i;

   printf("Detected BIOS drives:\n");

   if (!s_BootParams.bios_drive_list || s_BootParams.bios_drive_count == 0)
   {
      printf("  (none)\n\n");
      return;
   }

   for (i = 0; i < s_BootParams.bios_drive_count; i++)
   {
      printf("  0x%x\n", s_BootParams.bios_drive_list[i]);
   }

   printf("\n");
}

static void print_stage3_fs_location(void)
{
   printf("Partition label: \"%s\".\n",
          s_BootParams.corefs_partition_label);

   printf("Partition UUID: ");
   {
      for (int i = 0; i < 16; i++)
      {
         printf("%x", s_BootParams.corefs_partition_uuid[i]);
      }
   }
   printf(".\n\n");
}

void print_memory_map(void)
{
   uint8_t *ptr = (uint8_t *)s_BootParams.mbi_tags;
   printf("Memory Map:\n");
   for (;;)
   {
      uint32_t type = *(uint32_t *)ptr;
      uint32_t size = *(uint32_t *)(ptr + 4);

      if (type == MBI_TAG_END) break;

      if (type == MBI_TAG_MMAP)
      {
         uint32_t entry_size = *(uint32_t *)(ptr + 8);
         /* uint32_t entry_version = *(uint32_t *)(ptr + 12); */
         uint8_t *entry = ptr + 16;
         uint32_t total_size = size - 16;
         uint32_t count = total_size / entry_size;
         uint32_t i;

         for (i = 0; i < count; i++)
         {
            uint64_t base = *(uint64_t *)entry;
            uint64_t len = *(uint64_t *)(entry + 8);
            uint32_t type2 = *(uint32_t *)(entry + 16);

            printf("  base=%x\n", base);
            printf("  len =%x\n", len);
            printf("  type=%d\n", (int)type2);
            printf("  --\n");

            entry += entry_size;
         }
      }

      /* Advance to next tag (8-byte aligned) */
      ptr += size;
      ptr = (uint8_t *)(((uintptr_t)ptr + 7) & ~(uintptr_t)7);
   }
   printf("\n");
}

/* Print which output systems are reported as available. */
void print_available_outputs(void)
{
   printf("Available outputs:\n");

   if (s_BootParams.available_outputs & (1 << OUTPUT_VBE)) printf("  VBE\n");
   if (s_BootParams.available_outputs & (1 << OUTPUT_VGA)) printf("  VGA graphics\n");
   if (s_BootParams.available_outputs & (1 << OUTPUT_VGATEXT)) printf("  VGA text\n");
   if (s_BootParams.available_outputs & (1 << OUTPUT_SERIAL)) printf("  Serial (COM1)\n");

   printf("\n");
}

void print_boot_drive_number(void)
{
   char *driveType;
   if (s_BootParams.boot_drive == 0xe0)
      driveType = "CD/DVD";
   else if (s_BootParams.boot_drive < 0x80)
      driveType = "Floppy Disk";
   else
      driveType = "Hard Disk";

   printf("Boot drive information:\n");

   printf("  Boot Drive Number: 0x%x.\n", s_BootParams.boot_drive);

   printf("  Booted from a %s.\n\n", driveType);
}

void print_corefs_memory_address(void)
{
   printf("Corefs Module location: %x.\n\n", s_BootParams.corefs_ops);
}

void print_logo(void)
{
   if (g_PreferredOutput == OUTPUT_VBE)
   {
      uint32_t logo_w, logo_h, pal_sz;
      const uint8_t *pal, *data;
      g_MainBootOperations.LOGO_GetValecium(&logo_w, &logo_h, &pal, &pal_sz,
                                            &data);

      if (g_PreferredOutput == OUTPUT_VBE)
      {
         uint32_t scr_w = VBE_GetWidth();
         uint32_t scr_h = VBE_GetHeight();

         /* Center the logo. */
         int off_x = (int)((scr_w > logo_w) ? (scr_w - logo_w) / 2 : 0);
         int off_y = (int)((scr_h > logo_h) ? (scr_h - logo_h) / 2 : 0);

         for (uint32_t y = 0; y < logo_h; y++)
         {
            for (uint32_t x = 0; x < logo_w; x++)
            {
               /* Data is 4bpp: two pixels per byte, high nibble first. */
               uint8_t byte = data[(y * logo_w + x) / 2];
               uint8_t idx = (x & 1) ? (byte & 0x0F) : (byte >> 4);
               uint32_t pixel = VBE_PackRGB(pal[idx * 3], pal[idx * 3 + 1],
                                            pal[idx * 3 + 2]);
               VBE_PutPixel(pixel, off_x + (int)x, off_y + (int)y);
            }
         }
      }
   }
}

void init_fs(void)
{
   printf("Entering filesystem setup.\n");

   int rc = s_BootParams.corefs_ops->Initialize(s_BootParams.bios_drive_list, s_BootParams.bios_drive_count,
                               s_BootParams.corefs_partition_uuid, s_BootParams.corefs_partition_label);
   if (rc != SUCCESS)
   {
      printf("  FS_Initialize failed: %d.\n", rc);
   }
   else
   {
      printf("  FS initialized successful.\n\n");
   }
}

void init_main_boot(void)
{
   printf("Loading libTheBootloader.\n");

   int fd = s_BootParams.corefs_ops->Open(g_Stage3Path);
   if (fd < 0)
   {
      printf("  Failed to open %s: %d\n", g_Stage3Path, fd);
      return;
   }

   static uint8_t stage3_buf[512 * 1024];
   int total = 0;
   int rc;

   while (total < (int)sizeof(stage3_buf))
   {
      rc =
          s_BootParams.corefs_ops->Read(fd, stage3_buf + total, (int)sizeof(stage3_buf) - total);
      if (rc <= 0) break;
      total += rc;
   }

   s_BootParams.corefs_ops->Close(fd);

   if (rc < 0 || total == 0)
   {
      printf("  Failed to read %s\n", g_Stage3Path);
      return;
   }

   printf("  Read %d bytes from %s\n", total, g_Stage3Path);

   // Populate s_DlCallbackOps
   s_DlCallbackOps.DISK_Read = s_BootParams.corefs_ops->DISK_Read;
   s_DlCallbackOps.DISK_ReadLBA = s_BootParams.corefs_ops->DISK_ReadLBA;

   // Check for VLSO to patch callback address
   for (int i = 0; i + 4 <= total;
        i++) /* only iterate over bytes that were actually read */
   {
      if (stage3_buf[i] == 'V' && stage3_buf[i + 1] == 'L' &&
          stage3_buf[i + 2] == 'S' && stage3_buf[i + 3] == 'O')
      {
#if defined(BIT32)
         *(uint32_t *)(stage3_buf + i + 4) =
             (uint32_t)(uintptr_t)&s_DlCallbackOps;
#elif defined(BIT64)
         *(uint64_t *)(stage3_buf + i + 4) =
             (uint64_t)(uintptr_t)&s_DlCallbackOps;
#endif
         break;
      }
   }

   void *handle = DL_LoadLibrary(stage3_buf);
   if (!handle)
   {
      printf("  DL_LoadLibrary failed\n");
      return;
   }

   if (dl_resolve_all(handle) != 0)
   {
      printf("  dl_resolve_all failed\n");
      return;
   }

   printf("  Stage3 loaded and resolved successfully.\n");
}

int main(const BootParams *boot_params)
{
   s_BootParams = *boot_params;
   
   init_framebuffer_info();

   g_PreferredOutput = OUTPUT_SERIAL; /* fallback  */
   if (s_BootParams.available_outputs & (1 << OUTPUT_VGATEXT))
      g_PreferredOutput = OUTPUT_VGATEXT;
   if (s_BootParams.available_outputs & (1 << OUTPUT_VGA)) g_PreferredOutput = OUTPUT_VGA;
   if ((s_BootParams.available_outputs & (1 << OUTPUT_VBE)) && VBE_HasInfo())
      g_PreferredOutput = OUTPUT_VBE;

   switch (g_PreferredOutput)
   {
   case OUTPUT_SERIAL:
      Serial_Initialize();
      break;
   case OUTPUT_VGATEXT:
      VGATEXT_Initialize();
      break;
   case OUTPUT_VGA:
      VGA_Initialize();
      break;
   case OUTPUT_VBE:
      VBE_Initialize();
      break;
   }

   g_PrimaryOutputSystem = s_BootParams.available_outputs;

   print_available_outputs();
   print_memory_map();
   print_boot_drive_number();
   print_bios_drive_list();
   print_corefs_memory_address();
   print_stage3_fs_location();

   init_fs();
   init_main_boot();

   print_logo();
   for (;;)
      ;

   return 0;
}
