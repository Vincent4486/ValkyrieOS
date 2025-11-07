// SPDX-License-Identifier: AGPL-3.0-or-later

#include "jvm.hpp"

#include <std/stdio.h>

void printUsage() {}

void java(const char *classPath)
{
   printf("hello\n");
   printf("Class path: %s", classPath);
}