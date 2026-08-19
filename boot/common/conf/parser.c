// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>

#include "conf.h"

void parse_config_line(const char* line, BootProfile* current_profile) {
    if (line[0] == '#' || line[0] == '\0') return; // Skip comments & empty lines

    if (strncmp(line, "title=", 6) == 0) {
        strncpy(current_profile->title, line + 6, MAX_TITLE_LEN);
    } else if (strncmp(line, "path=", 5) == 0) {
        strncpy(current_profile->path, line + 5, MAX_PATH_LEN);
    } else if (strncmp(line, "args=", 5) == 0) {
        strncpy(current_profile->args, line + 5, MAX_ARGS_LEN);
    }
}
