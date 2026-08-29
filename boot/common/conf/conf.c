// SPDX-License-Identifier: GPL-3.0-only

#include <stddef.h>
#include <stdint.h>

#include <status.h>

#include <dl/bindgen.h>
#include <dl/callback.h>

#include "conf.h"

static CONF_GlobalBoot s_GlobalConf = {0};

extern int CONF_ParseFile(const char *path, CONF_GlobalBoot *global);
extern int CONF_Validate(CONF_GlobalBoot *global);

static void conf_global_reset(void)
{
   int i;

   for (i = 0; i < (int)sizeof(s_GlobalConf); i++)
      ((char *)&s_GlobalConf)[i] = 0;
   s_GlobalConf.default_profile = -1;
}

DL_INC
int CONF_ParseConf(const char *path)
{
   int rc;

   if (!path)
      return -EINVAL;

   if (!g_DlCallbackOps || !g_DlCallbackOps->Open || !g_DlCallbackOps->Read ||
       !g_DlCallbackOps->Close)
      return -ENODEV;

   conf_global_reset();

   rc = CONF_ParseFile(path, &s_GlobalConf);
   if (rc != SUCCESS)
      return rc;

   return CONF_Validate(&s_GlobalConf);
}

DL_INC
void CONF_GetGlobal(CONF_GlobalBoot **global)
{
   if (global)
      *global = &s_GlobalConf;
}

DL_INC
void CONF_GetProfile(CONF_BootProfile **profile, int profile_id)
{
   if (!profile)
      return;

   if (profile_id < 0 || profile_id >= s_GlobalConf.profile_count)
   {
      *profile = NULL;
      return;
   }

   *profile = &s_GlobalConf.profiles[profile_id];
}
