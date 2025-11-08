// SPDX-License-Identifier: AGPL-3.0-or-later

// Simple dynamic-linking helper for kernel side to find and call modules
#pragma once

#include <sys/memdefs.h>
#include <stdint.h>
#include <fs/partition.h>

// Maximum dependencies per library
#define DYLIB_MAX_DEPS 16

// Maximum symbols per library
#define DYLIB_MAX_SYMBOLS 256

// Symbol record - exported function from a library
typedef struct {
    char name[64];      // Symbol/function name
    uint32_t address;   // Memory address of the function
} SymbolRecord;

// Dependency record - tracks which libraries a module depends on
typedef struct {
    char name[64];  // Name of the dependency
    int resolved;   // 1 if dependency is loaded, 0 if missing
} DependencyRecord;

// Find a loaded library record by name (basename without extension). Returns
// pointer into the shared registry or NULL if not found.
LibRecord *dylib_find(const char *name);

// Call the entry point of a named library if present. Returns 0 on success,
// -1 if not found or dependencies unresolved.
int dylib_call_if_exists(const char *name);

// Check if all dependencies of a library are resolved. Returns 1 if all
// resolved, 0 if any missing.
int dylib_check_dependencies(const char *name);

// Resolve all dependencies for a library. Returns 0 on success, -1 if any
// dependencies are missing.
int dylib_resolve_dependencies(const char *name);

// Print the current library registry with dependency info (for debugging)
void dylib_list(void);

// Print dependencies of a specific library
void dylib_list_deps(const char *name);

// Find a symbol (function) by name within a library. Returns the function
// address or 0 if not found.
uint32_t dylib_find_symbol(const char *libname, const char *symname);

// Call a symbol (function) within a library by name. Returns the result
// of the function call, or -1 if not found or dependencies unresolved.
int dylib_call_symbol(const char *libname, const char *symname);

// List all symbols exported by a library
void dylib_list_symbols(const char *name);

// Memory management functions

// Initialize the dylib memory allocator
int dylib_mem_init(void);

// Allocate memory for a library. Returns allocated address or 0 on failure.
uint32_t dylib_mem_alloc(const char *lib_name, uint32_t size);

// Deallocate memory for a library. Returns 0 on success, -1 on failure.
int dylib_mem_free(const char *lib_name);

// Load a library from disk into memory. Returns 0 on success, -1 on failure.
// Parameters:
//   partition: Initialized Partition structure for reading
//   name: Library name to load
//   filepath: Path to library file on disk (e.g., "/sys/graphics.so")
int dylib_load_from_disk(Partition *partition, const char *name, const char *filepath);

// Load a library from memory image. Returns 0 on success, -1 on failure.
int dylib_load(const char *name, const void *image, uint32_t size);

// Remove a library from memory. Returns 0 on success, -1 on failure.
int dylib_remove(const char *name);

// Get memory usage statistics
void dylib_mem_stats(void);
