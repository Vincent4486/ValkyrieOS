// SPDX-License-Identifier: AGPL-3.0-or-later

#include "jvm.h"

#include <std/stdio.h>

void java(const char *classPath) { 
    printf("hello\n"); 
    printf("Class path: %s", classPath);
}