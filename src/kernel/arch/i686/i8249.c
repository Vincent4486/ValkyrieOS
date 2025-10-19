#include "i8249.h"
#include "io.h"
#include <stdint.h>
#include <stdbool.h>

#define PIC1_COMMAND_PORT           0x20
#define PIC1_DATA_PORT              0x21
#define PIC2_COMMAND_PORT           0xA0
#define PIC2_DATA_PORT              0xA1

enum{
    PIC_ICW1_ICW4 = 0x1,
    PIC_ICW1_SINGLE = 0x2,
    PIC_ICW1_INTERVAL4 = 0x4,
    PIC_ICW1_LEVEL = 0x8,
    PIC_ICW1_INITIALIZE = 0x10
} PIC_ICW1;

enum {
    PIC_ICW4_8086           = 0x1,
    PIC_ICW4_AUTO_EOI       = 0x2,
    PIC_ICW4_BUFFER_MASTER  = 0x4,
    PIC_ICW4_BUFFER_SLAVE   = 0x0,
    PIC_ICW4_BUFFERRED      = 0x8,
    PIC_ICW4_SFNM           = 0x10,
} PIC_ICW4;


enum {
    PIC_CMD_END_OF_INTERRUPT    = 0x20,
    PIC_CMD_READ_IRR            = 0x0A,
    PIC_CMD_READ_ISR            = 0x0B,
} PIC_CMD;

static uint16_t g_PicMask = 0xffff;
static bool g_AutoEoi = false;

void i8249_Configure(uint8_t OffsetPic1, uint8_t OffsetPic2, bool autoEoi){
    i8249_SetMask(0xffff);

    i686_outb(PIC1_COMMAND_PORT, PIC_ICW1_ICW4 | PIC_ICW1_INITIALIZE);
    i686_iowait();
    i686_outb(PIC2_COMMAND_PORT, PIC_ICW1_ICW4 | PIC_ICW1_INITIALIZE);
    i686_iowait();

    // initialization control word 2 - the offsets
    i686_outb(PIC1_DATA_PORT, OffsetPic1);
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, OffsetPic2);
    i686_iowait();

    // initialization control word 3
    i686_outb(PIC1_DATA_PORT, 0x4);             // tell PIC1 that it has a slave at IRQ2 (0000 0100)
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, 0x2);             // tell PIC2 its cascade identity (0000 0010)
    i686_iowait();

    // initialization control word 4
    uint8_t icw4 = PIC_ICW4_8086;
    if(autoEoi){
        icw4 |= PIC_ICW4_AUTO_EOI;
    }

    i686_outb(PIC1_DATA_PORT, icw4);
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, icw4);
    i686_iowait();

    // clear data registers
    i8249_SetMask(0xffff);
}

void i8249_SendEndOfInterrupt(int irq)
{
    if (irq >= 8)
        i686_outb(PIC2_COMMAND_PORT, PIC_CMD_END_OF_INTERRUPT);
    i686_outb(PIC1_COMMAND_PORT, PIC_CMD_END_OF_INTERRUPT);
}

void i8249_SetMask(uint16_t Mask){
    g_PicMask = Mask;

    i686_outb(PIC1_DATA_PORT, g_PicMask & 0xff);        // mask all
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, g_PicMask & 0xff);        // mask all
    i686_iowait();
}
void i8249_Disable()
{
    i8249_SetMask(0xffff);
}

void i8249_Mask(int irq)
{
    uint8_t port;
    uint8_t mask;
    if (irq < 8) 
    {
        port = PIC1_DATA_PORT;
        mask = g_PicMask & 0xff;
    }
    else
    {
        irq -= 8;
        port = PIC2_DATA_PORT;
        mask = g_PicMask >> 8;
    }
    i686_outb(port,  mask | (1 << irq));
}

void i8249_Unmask(int irq)
{
    uint8_t port;
    uint8_t mask;
    if (irq < 8) 
    {
        port = PIC1_DATA_PORT;
        mask = g_PicMask & 0xff;
    }
    else
    {
        irq -= 8;
        port = PIC2_DATA_PORT;
        mask = g_PicMask >> 8;
    }

    mask = i686_inb(PIC1_DATA_PORT);
    i686_outb(PIC1_DATA_PORT,  mask & ~(1 << irq));
}

uint16_t i8249_ReadIrqRequestRegister()
{
    i686_outb(PIC1_COMMAND_PORT, PIC_CMD_READ_IRR);
    i686_outb(PIC2_COMMAND_PORT, PIC_CMD_READ_IRR);
    return ((uint16_t)i686_inb(PIC2_COMMAND_PORT)) | (((uint16_t)i686_inb(PIC2_COMMAND_PORT)) << 8);
}

uint16_t i8249_ReadInServiceRegister()
{
    i686_outb(PIC1_COMMAND_PORT, PIC_CMD_READ_ISR);
    i686_outb(PIC2_COMMAND_PORT, PIC_CMD_READ_ISR);
    return ((uint16_t)i686_inb(PIC2_COMMAND_PORT)) | (((uint16_t)i686_inb(PIC2_COMMAND_PORT)) << 8);
}