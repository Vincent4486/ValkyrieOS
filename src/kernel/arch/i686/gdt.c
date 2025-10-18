#include "gdt.h"
#include <stdint.h>

typedef struct{
    uint16_t LimitLow;
    uint16_t BaseLow;
    uint8_t BaeMiddle;
    uint8_t access;
    uint8_t FlagLimitHigh;
    uint8_t BaseHigh;
}__attribute__((packed)) GDTEntry;


typedef struct{
    uint16_t Limit;
    GDTEntry* Ptr;
}__attribute__((packed)) GDTDescriptor;


typedef enum{
    GDT_ACCESS_CODE_READABLE = 0x02,
    GDT_ACCESS_DATA_WRITEABLE = 0x02,

    GDT_ACCESS_CODE_COMFORMING = 0x04,
    GDT_ACCESS_DATA_DIRECTION_NORMAL = 0x00,
    GDT_ACCESS_DATA_DIRECTION_DOWN = 0x04,

    GDT_ACCESS_DATA_SEGMENT = 0x10,
    GDT_ACCESS_CODE_SEGMENT = 0x18,

    GDT_ACCESS_DESCRIPTOR_TSS = 0x00,

    GDT_ACCESS_RING0 = 0x00,
    GDT_ACCESS_RING1 = 0x20,
    GDT_ACCESS_RING2 = 0x40,
    GDT_ACCESS_RING3 = 0x60,

    GDT_ACCESS_PRESENT = 0x80,
    
}GDT_ACCESS;