/* Simple dynamic link helpers for the kernel. The bootloader (stage2)
 * populates a LibRecord table at LIB_REGISTRY_ADDR. These helpers let the
 * kernel find and call modules by name without hardcoding addresses.
 */

#include "dylink.h"
#include <std/stdio.h>
#include <stdint.h>

// Use kernel string helper
#include "../std/string.h"

LibRecord *dylib_find(const char *name)
{
    LibRecord *reg = LIB_REGISTRY_ADDR;
    for (int i = 0; i < LIB_REGISTRY_MAX; i++)
    {
        if (reg[i].name[0] != '\0')
        {
            if (str_eq(reg[i].name, name)) return &reg[i];
        }
    }
    return NULL;
}

int dylib_call_if_exists(const char *name)
{
    LibRecord *r = dylib_find(name);
    if (!r || !r->entry) return -1;

    typedef void (*entry_t)(void);
    entry_t fn = (entry_t)r->entry;
    fn();
    return 0;
}

void dylib_list(void)
{
    LibRecord *reg = LIB_REGISTRY_ADDR;
    printf("Loaded modules:\n");
    for (int i = 0; i < LIB_REGISTRY_MAX; i++)
    {
        if (reg[i].name[0] != '\0')
        {
            printf(" - %s @ %p\n", reg[i].name, reg[i].entry);
        }
    }
}
