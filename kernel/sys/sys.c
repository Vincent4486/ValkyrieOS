// SPDX-License-Identifier: AGPL-3.0-or-later

#include "sys.h"
#include <mem/memdefs.h>
#include <std/string.h>

/* Global SYS_Info structure at fixed memory location */
SYS_Info *g_SysInfo = (SYS_Info *)SYS_INFO_ADDR;

void SYS_Initialize(){
    /* Initialize SYS_Info structure */
    g_SysInfo->kernel_major = 1;
    g_SysInfo->kernel_minor = 0;
    g_SysInfo->kernel_patch = 0;
    g_SysInfo->uptime_seconds = 0;
    g_SysInfo->initialized = 0;
    
    /* Populate architecture information */
    get_arch(&g_SysInfo->arch.arch);
    get_cpu_count(&g_SysInfo->arch.cpu_count);
    g_SysInfo->arch.cpu_frequency = 1800; /* MHz - will be detected later */
    g_SysInfo->arch.cache_line_size = 32; /* Typical for i686 */
    g_SysInfo->arch.features = 0; /* Features detection TODO */
    get_cpu_brand(g_SysInfo->arch.cpu_brand);
}

/**
 * Finalize system initialization
 * Call this after all subsystems have been initialized
 */
void SYS_Finalize(){
    g_SysInfo->initialized = 1;
}