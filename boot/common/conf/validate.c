// SPDX-License-Identifier: GPL-3.0-only

#include <stddef.h>

#include <logging.h>
#include <status.h>

#include <dl/bindgen.h>
#include <dl/callback.h>

#include "conf.h"

#define logfmt g_DlCallbackOps->logfmt

static void validate_copy_str(char *dst, size_t dst_size, const char *src)
{
   size_t i = 0;

   while (src[i] != '\0' && i + 1 < dst_size)
   {
      dst[i] = src[i];
      i++;
   }

   dst[i] = '\0';
}

_DL_FORCE_EXCLUDE
int CONF_Validate(CONF_GlobalBoot *global)
{
   int i;

   if (!global)
      return -EINVAL;

   if (global->profile_count == 0)
   {
      logfmt(LOG_ERROR, "conf: no profiles defined\n");
      return -EINVAL;
   }

   if (global->timeout < 0)
   {
      logfmt(LOG_ERROR, "conf: timeout must not be negative\n");
      return -EINVAL;
   }

   for (i = 0; i < global->profile_count; i++)
   {
      CONF_BootProfile *profile = &global->profiles[i];

      if (profile->path[0] == '\0')
      {
         logfmt(LOG_ERROR, "conf: profile '%s' is missing required 'path'\n",
                profile->name);
         return -EINVAL;
      }

      if (profile->title[0] == '\0')
      {
         validate_copy_str(profile->title, sizeof(profile->title),
                           profile->name);
         logfmt(LOG_INFO, "conf: profile '%s' title defaults to name\n",
                profile->name);
      }
   }

   if (global->default_profile < 0)
   {
      global->default_profile = 0;
      logfmt(LOG_INFO, "conf: default profile defaults to '%s'\n",
             global->profiles[0].name);
   }
   else if (global->default_profile >= global->profile_count)
   {
      logfmt(LOG_ERROR, "conf: default profile index %d is out of range\n",
             global->default_profile);
      return -EINVAL;
   }

   return SUCCESS;
}
