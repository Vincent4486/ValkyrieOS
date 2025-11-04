// SPDX-License-Identifier: AGPL-3.0-or-later

// Simple dynamic-linking helper for kernel side to find and call modules
#pragma once

#include "memory/memdefs.h"

// Find a loaded library record by name (basename without extension). Returns
// pointer into the shared registry or NULL if not found.
LibRecord *dylib_find(const char *name);

// Call the entry point of a named library if present. Returns 0 on success,
// -1 if not found.
int dylib_call_if_exists(const char *name);

// Print the current library registry (for debugging)
void dylib_list(void);
