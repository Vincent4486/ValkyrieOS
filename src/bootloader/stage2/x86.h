#pragma once
#include "stdint.h"

// Video BIOS functions - only the ones actually used
#pragma aux _x86_VideoSetMode "*"
void _cdecl _x86_VideoSetMode(uint16_t mode);

#pragma aux _x86_VideoWriteCharTeletype "*"
void _cdecl _x86_VideoWriteCharTeletype(char c, uint8_t page);
