// SPDX-License-Identifier: BSD-3-Clause

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

int CONF_ParseConfigFile(const char *path);
CONF_BootProfile *CONF_GetProfile(int profile_id);
int CONF_GetProfileCount(void);
const char *CONF_GetDefaultProfile(void);
int CONF_GetTimeout(void);
