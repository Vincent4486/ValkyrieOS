// SPDX-License-Identifier: BSD-3-Clause

#define CONF_MAX_TITLE_LEN 64
#define CONF_MAX_PATH_LEN  128
#define CONF_MAX_ARGS_LEN  256

typedef struct {
    char title[MAX_TITLE_LEN];
    char root_label[MAX_TITLE_LEN];
    char path[MAX_PATH_LEN];
    char args[MAX_ARGS_LEN];
} CONF_BootProfile;
