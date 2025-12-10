// SPDX-License-Identifier: AGPL-3.0-or-later

#include <fs/partition.h>
#include <std/stdio.h>
#include <stddef.h>
#include <sys/dylib.h>
#include <mem/memdefs.h>

static int load_libmath(Partition *partition)
{
   // First, ensure libmath is registered in the library registry
   LibRecord *lib_registry = LIB_REGISTRY_ADDR;

   // Check if libmath is already registered
   LibRecord *existing_lib = dylib_find("libmath");
   if (!existing_lib || existing_lib->name[0] == '\0')
   {
      // Register libmath in the registry
      lib_registry[0].name[0] = 'l';
      lib_registry[0].name[1] = 'i';
      lib_registry[0].name[2] = 'b';
      lib_registry[0].name[3] = 'm';
      lib_registry[0].name[4] = 'a';
      lib_registry[0].name[5] = 't';
      lib_registry[0].name[6] = 'h';
      lib_registry[0].name[7] = '\0';
      lib_registry[0].base = NULL;
      lib_registry[0].entry = NULL;
      lib_registry[0].size = 0;
      printf("[DYLIB] Registered libmath in library registry\n");
   }

   // Load libmath from disk using the standard loader
   if (dylib_load_from_disk(partition, "libmath", "/usr/lib/libmath.so") != 0)
   {
      printf("[ERROR] Failed to load libmath.so\n");
      return -1;
   }

   // Resolve dependencies
   dylib_resolve_dependencies("libmath");

   // dylib_list_symbols("libmath");

   // Register libmath symbols in global symbol table for GOT patching
   printf("\n[*] Registering libmath symbols in global symbol table...\n");
   dylib_add_global_symbol("add", (uint32_t)dylib_find_symbol("libmath", "add"),
                           "libmath", 0);
   dylib_add_global_symbol("subtract",
                           (uint32_t)dylib_find_symbol("libmath", "subtract"),
                           "libmath", 0);
   dylib_add_global_symbol("multiply",
                           (uint32_t)dylib_find_symbol("libmath", "multiply"),
                           "libmath", 0);
   dylib_add_global_symbol("divide",
                           (uint32_t)dylib_find_symbol("libmath", "divide"),
                           "libmath", 0);
   dylib_add_global_symbol("modulo",
                           (uint32_t)dylib_find_symbol("libmath", "modulo"),
                           "libmath", 0);
   dylib_add_global_symbol("abs_int",
                           (uint32_t)dylib_find_symbol("libmath", "abs_int"),
                           "libmath", 0);
   dylib_add_global_symbol(
       "fabsf", (uint32_t)dylib_find_symbol("libmath", "fabsf"), "libmath", 0);
   dylib_add_global_symbol(
       "fabs", (uint32_t)dylib_find_symbol("libmath", "fabs"), "libmath", 0);
   dylib_add_global_symbol(
       "sinf", (uint32_t)dylib_find_symbol("libmath", "sinf"), "libmath", 0);
   dylib_add_global_symbol("sin", (uint32_t)dylib_find_symbol("libmath", "sin"),
                           "libmath", 0);
   dylib_add_global_symbol(
       "cosf", (uint32_t)dylib_find_symbol("libmath", "cosf"), "libmath", 0);
   dylib_add_global_symbol("cos", (uint32_t)dylib_find_symbol("libmath", "cos"),
                           "libmath", 0);
   dylib_add_global_symbol(
       "tanf", (uint32_t)dylib_find_symbol("libmath", "tanf"), "libmath", 0);
   dylib_add_global_symbol("tan", (uint32_t)dylib_find_symbol("libmath", "tan"),
                           "libmath", 0);
   dylib_add_global_symbol(
       "expf", (uint32_t)dylib_find_symbol("libmath", "expf"), "libmath", 0);
   dylib_add_global_symbol("exp", (uint32_t)dylib_find_symbol("libmath", "exp"),
                           "libmath", 0);
   dylib_add_global_symbol(
       "logf", (uint32_t)dylib_find_symbol("libmath", "logf"), "libmath", 0);
   dylib_add_global_symbol("log", (uint32_t)dylib_find_symbol("libmath", "log"),
                           "libmath", 0);
   dylib_add_global_symbol("log10f",
                           (uint32_t)dylib_find_symbol("libmath", "log10f"),
                           "libmath", 0);
   dylib_add_global_symbol(
       "log10", (uint32_t)dylib_find_symbol("libmath", "log10"), "libmath", 0);
   dylib_add_global_symbol(
       "powf", (uint32_t)dylib_find_symbol("libmath", "powf"), "libmath", 0);
   dylib_add_global_symbol("pow", (uint32_t)dylib_find_symbol("libmath", "pow"),
                           "libmath", 0);
   dylib_add_global_symbol(
       "sqrtf", (uint32_t)dylib_find_symbol("libmath", "sqrtf"), "libmath", 0);
   dylib_add_global_symbol(
       "sqrt", (uint32_t)dylib_find_symbol("libmath", "sqrt"), "libmath", 0);
   dylib_add_global_symbol("floorf",
                           (uint32_t)dylib_find_symbol("libmath", "floorf"),
                           "libmath", 0);
   dylib_add_global_symbol(
       "floor", (uint32_t)dylib_find_symbol("libmath", "floor"), "libmath", 0);
   dylib_add_global_symbol(
       "ceilf", (uint32_t)dylib_find_symbol("libmath", "ceilf"), "libmath", 0);
   dylib_add_global_symbol(
       "ceil", (uint32_t)dylib_find_symbol("libmath", "ceil"), "libmath", 0);
   dylib_add_global_symbol("roundf",
                           (uint32_t)dylib_find_symbol("libmath", "roundf"),
                           "libmath", 0);
   dylib_add_global_symbol(
       "round", (uint32_t)dylib_find_symbol("libmath", "round"), "libmath", 0);
   dylib_add_global_symbol(
       "fminf", (uint32_t)dylib_find_symbol("libmath", "fminf"), "libmath", 0);
   dylib_add_global_symbol(
       "fmin", (uint32_t)dylib_find_symbol("libmath", "fmin"), "libmath", 0);
   dylib_add_global_symbol(
       "fmaxf", (uint32_t)dylib_find_symbol("libmath", "fmaxf"), "libmath", 0);
   dylib_add_global_symbol(
       "fmax", (uint32_t)dylib_find_symbol("libmath", "fmax"), "libmath", 0);
   dylib_add_global_symbol(
       "fmodf", (uint32_t)dylib_find_symbol("libmath", "fmodf"), "libmath", 0);
   dylib_add_global_symbol(
       "fmod", (uint32_t)dylib_find_symbol("libmath", "fmod"), "libmath", 0);
   printf("[*] Symbols registered\n");

   // Apply kernel GOT/PLT relocations for libmath
   printf("\n[*] Applying kernel GOT/PLT relocations...\n");
   dylib_apply_kernel_relocations();
   printf("[*] Relocations applied\n");
   return 0;
}

bool dylib_Initialize(Partition *partition)
{
   // list libraries loaded in stage2
   dylib_list();
   // Load math library
   if (load_libmath(partition) != 0) return false;
   return true;
}
