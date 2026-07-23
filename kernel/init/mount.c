// SPDX-License-Identifier: GPL-3.0-only

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <fs/fs.h>
#include <std/stdio.h>
#include <sys/sys.h>
#include <sys/system.h>
#include <sys/valecium.h>

// Root filesystem mount for ValeciumOS.
// Walks g_SysInfo->volume[] for the partition tagged as root by DISK_Scan,
// mounts it to "/", and verifies /boot/init.sys exists.

// Init_MountRoot — scans volume[] for root partition, mounts to "/",
// verifies /boot/init.sys exists. Returns 0 on success.
int Init_MountRoot(void)
{
   for (int i = 0; i < MAX_DISKS; i++)
   {
      /* Skip entries that have not been tagged as root */
      if (!g_SysInfo->volume[i].disk || !g_SysInfo->volume[i].is_root_partition)
      {
         continue;
      }

      Partition *part = &g_SysInfo->volume[i];

      logfmt(LOG_INFO, "[MOUNT] Root found: LABEL=\"%s\"\n",
             part->label[0] ? part->label : "VALECIUM");

      int rc = FS_Mount(part, "/");
      if (rc != 0)
      {
         logfmt(LOG_WARNING,
                "[MOUNT] FS_Mount failed for volume[%d] (rc=%d) — trying "
                "next candidate\n",
                i, rc);
         continue;
      }

      logfmt(LOG_INFO,
             "[MOUNT] Root filesystem mounted from volume[%d] at \"/\"\n", i);

      // Post-mount probe: verify /boot/init.sys exists
      struct VFS_File *init_sys = VFS_Open("/boot/init.sys");
      if (init_sys)
      {
         logfmt(LOG_INFO, "[MOUNT] Found /boot/init.sys — userspace transition "
                          "ready\n");
         VFS_Close(init_sys);
      }
      else
      {
         logfmt(LOG_WARNING, "[MOUNT] /boot/init.sys not found on root "
                             "filesystem\n");
      }

      return 0;
   }

   /* No tagged partition was mountable */
   logfmt(LOG_FATAL,
          "No root partition found or all FS_Mount attempts failed.\n");

   return -1; /* No root partition found */
}
