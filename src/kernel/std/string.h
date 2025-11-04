// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

const char *strchr(const char *str, char chr);
char *strcpy(char *dst, const char *src);
unsigned strlen(const char *str);

// Compare strings for equality. Returns 1 if equal, 0 otherwise.
int str_eq(const char *a, const char *b);
