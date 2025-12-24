// SPDX-License-Identifier: AGPL-3.0-or-later

#include <valkyrie/valkyrie.h>

#include "sys.h"
#include <mem/memdefs.h>
#include <std/stdio.h>
#include <std/string.h>

/* Global SYS_Info structure at fixed memory location */
SYS_Info *g_SysInfo = (SYS_Info *)SYS_INFO_ADDR;

void SYS_Initialize()
{
   /* Initialize SYS_Info structure */
   g_SysInfo->kernel_major = KERNEL_MAJOR;
   g_SysInfo->kernel_minor = KERNEL_MINOR;
   g_SysInfo->uptime_seconds = 0;
   g_SysInfo->initialized = 0;

   /* Populate architecture information */
   uint8_t arch;
   uint32_t cpu_count;
   char cpu_brand[64];
   get_arch(&arch);
   get_cpu_count(&cpu_count);
   get_cpu_brand(cpu_brand);
   g_SysInfo->arch.arch = arch;
   g_SysInfo->arch.cpu_count = cpu_count;
   g_SysInfo->arch.cpu_frequency = 1800; /* MHz - will be detected later */
   g_SysInfo->arch.cache_line_size = 32; /* Typical for i686 */
   g_SysInfo->arch.features = 0;         /* Features detection TODO */
   memcpy(g_SysInfo->arch.cpu_brand, cpu_brand, 64);
}

/**
 * Finalize system initialization
 * Call this after all subsystems have been initialized
 */
void SYS_Finalize()
{
   g_SysInfo->initialized = 1;
   printf(
       "System finalized: kernel %u.%u.%u, arch=%u, cpus=%u, mem=%uMB "
       "total/%uMB avail, disks=%u, filesystems=%u, boot=0x%08x, video=%ux%u\n",
       g_SysInfo->kernel_major, g_SysInfo->kernel_minor,
      g_SysInfo->arch.arch, g_SysInfo->arch.cpu_count,
       g_SysInfo->memory.total_memory / (1024 * 1024),
       g_SysInfo->memory.available_memory / (1024 * 1024),
       g_SysInfo->disk_count, g_SysInfo->fs_count, g_SysInfo->boot_device,
       g_SysInfo->video_width, g_SysInfo->video_height);
}
