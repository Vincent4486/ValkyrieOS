// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stdint.h>
#include <kernel/mem/memory.h>
#include <kernel/fs/fs.h>
#include <kernel/fs/disk/disk.h>
#include <kernel/arch/i686/cpu/irq.h>

/* Architecture/CPU information */
typedef struct {
    uint8_t arch;                /* Architecture (i686, x86_64, aarch64, etc) */
    uint32_t cpu_count;          /* Number of CPUs/cores */
    uint32_t cpu_frequency;      /* CPU frequency in MHz */
    uint32_t cache_line_size;    /* L1 cache line size */
    uint32_t features;           /* CPU feature flags (MMU, PAE, etc) */
    char cpu_brand[64];          /* CPU brand string */
} __attribute__((packed)) ARCH_Info;

/* Master system information structure */
typedef struct {
    /* Kernel version and identification */
    uint16_t kernel_major;       /* Kernel major version */
    uint16_t kernel_minor;       /* Kernel minor version */
    uint16_t kernel_patch;       /* Kernel patch version */
    uint32_t uptime_seconds;     /* Uptime in seconds */
    
    /* Architecture and CPU */
    ARCH_Info arch;              /* Architecture information */
    
    /* Memory */
    MEM_Info memory;             /* Memory information */
    
    /* Storage */
    DISK_Info disk;              /* Primary disk information */
    uint8_t disk_count;          /* Number of disk devices */
    
    /* Filesystem */
    FS_Info fs;                  /* Filesystem information */
    uint8_t fs_count;            /* Number of mounted filesystems */
    
    /* Interrupts */
    IRQ_Info irq;                /* Interrupt controller information */
    
    /* Bootloader and hardware */
    uint32_t boot_device;        /* Device booted from */
    uint32_t video_memory;       /* Video memory size in bytes */
    uint16_t video_width;        /* Video width in pixels/chars */
    uint16_t video_height;       /* Video height in pixels/chars */
    
    /* Status flags */
    uint8_t initialized;         /* 1 if fully initialized, 0 otherwise */
    uint8_t reserved[3];         /* Padding for alignment */
} __attribute__((packed)) SYS_Info;