#include "irq.h"
#include "pic.h"

#define PIC_REMAP_OFFSET 0x20

void i686_IRQ_Initialize()
{
    i686_PIC_Configure(PIC_REMAP_OFFSET, PIC_REMAP_OFFSET + 8);
}
void i686_IRQ_RegisterHandler(int irq, IRQHandler handler)
{

}