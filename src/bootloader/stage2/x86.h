#pragma once
#include "stdint.h"

extern uint32_t cursor_x;
extern uint32_t cursor_y;

void x86_div64_32(uint64_t dividend, uint32_t divisor, uint64_t* quotientOut, uint32_t* remainderOut);

void x86_Video_WriteCharTeletype(char c, uint8_t page);

bool x86_Disk_Reset(uint8_t drive);

bool x86_Disk_Read(uint8_t drive,
                   uint16_t cylinder,
                   uint16_t sector,
                   uint16_t head,
                   uint8_t count,
                   void* dataOut);

bool x86_Disk_GetDriveParams(uint8_t drive,
                             uint8_t* driveTypeOut,
                             uint16_t* cylindersOut,
                             uint16_t* sectorsOut,
                             uint16_t* headsOut);
