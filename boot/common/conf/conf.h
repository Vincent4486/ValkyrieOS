// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#define CONF_MAX_NAME_LEN  64
#define CONF_MAX_TITLE_LEN 64
#define CONF_MAX_PATH_LEN  128
#define CONF_MAX_ARGS_LEN  256

#define CONF_MAX_PROFILES  8

typedef struct
{
   char name[CONF_MAX_NAME_LEN];
   char title[CONF_MAX_TITLE_LEN];
   char root_label[CONF_MAX_TITLE_LEN];
   char path[CONF_MAX_PATH_LEN];
   char args[CONF_MAX_ARGS_LEN];
} CONF_BootProfile;

typedef struct
{
   int profile_count;
   int default_profile; // index into profiles[], -1 when none
   int timeout;
   CONF_BootProfile profiles[CONF_MAX_PROFILES];
} CONF_GlobalBoot;

int CONF_ParseConf(const char *path);
CONF_GlobalBoot *CONF_GetGlobal(void);
CONF_BootProfile *CONF_GetProfile(int profile_id);
