#pragma once
#include "stdint.h"

#pragma aux _x86_VideoWriteCharTeletype "*"
void _cdecl _x86_VideoWriteCharTeletype(char c, uint8_t page);
