// SPDX-License-Identifier: AGPL-3.0-or-later

#include "sys.h"
#include <mem/memdefs.h>
#include <std/string.h>
#include <std/stdio.h>

/* Global SYS_Info structure at fixed memory location */
SYS_Info *g_SysInfo = (SYS_Info *)SYS_INFO_ADDR;


/**
 * Get CPU brand string via CPUID
 * On i686: EBX, EDX, ECX from CPUID leaf 0
 */
static void get_cpu_brand(char *brand)
{
    uint32_t eax, ebx, ecx, edx;
    
    /* CPUID leaf 0 - Get vendor ID */
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    
    /* Copy vendor ID (12 bytes: EBX, EDX, ECX order) */
    *(uint32_t*)(brand + 0) = ebx;
    *(uint32_t*)(brand + 4) = edx;
    *(uint32_t*)(brand + 8) = ecx;
    brand[12] = '\0';
}

void SYS_Initialize(){
    /* Initialize SYS_Info structure */
    g_SysInfo->kernel_major = 1;
    g_SysInfo->kernel_minor = 0;
    g_SysInfo->kernel_patch = 0;
    g_SysInfo->uptime_seconds = 0;
    g_SysInfo->initialized = 0;
    
    /* Populate architecture information */
    g_SysInfo->arch.arch = 1; /* i686 */
    g_SysInfo->arch.cpu_count = 1; /* Single CPU for now */
    g_SysInfo->arch.cpu_frequency = 1800; /* MHz - will be detected later */
    g_SysInfo->arch.cache_line_size = 32; /* Typical for i686 */
    g_SysInfo->arch.features = 0; /* Features detection TODO */
    
    /* Get CPU brand string */
    get_cpu_brand(g_SysInfo->arch.cpu_brand);
}

/**
 * Finalize system initialization
 * Call this after all subsystems have been initialized
 */
void SYS_Finalize(){
    g_SysInfo->initialized = 1;
    printf("System finalized: kernel %u.%u.%u, arch=%u, cpus=%u, mem=%uMB total/%uMB avail, disks=%u, filesystems=%u, boot=0x%08x, video=%ux%u\n",
           g_SysInfo->kernel_major,
           g_SysInfo->kernel_minor,
           g_SysInfo->kernel_patch,
           g_SysInfo->arch.arch,
           g_SysInfo->arch.cpu_count,
           g_SysInfo->memory.total_memory / (1024 * 1024),
           g_SysInfo->memory.available_memory / (1024 * 1024),
           g_SysInfo->disk_count,
           g_SysInfo->fs_count,
           g_SysInfo->boot_device,
           g_SysInfo->video_width,
           g_SysInfo->video_height);
}