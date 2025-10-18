#include "idt.h"
#include <stdint.h>

typedef struct
{
	uint16_t BaseLow;
	uint16_t SegmentSelector;
	uint8_t Reserved;
	uint8_t Flags;
	uint16_t BaseHigh;
} __attribute__((packed)) IDTEntry;

typedef struct
{
	uint16_t Limit;
	IDTEntry *Ptr;
} __attribute__((packed)) IDT_Descriptor;