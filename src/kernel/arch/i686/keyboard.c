#include "keyboard.h"
#include "io.h"
#include "irq.h"
#include "stdio.h"   // for printf

/* Minimal set-1 scancode -> ASCII map for printable keys.
   Extend as needed (this is not full; handles letters, digits, space, enter, backspace). */
static const char scancode_map[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=','\b', /* 0x00 - 0x0F */
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,  /* 0x10 - 0x1F */
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x',/*0x20-0x2F*/
    'c','v','b','n','m',',','.','/',0,  '*',0,  ' ', /* 0x30 - 0x3B (space at 0x39) */
    /* rest zeros */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* IRQ handler for keyboard IRQ1 */
void i686_keyboard_irq(Registers* regs)
{
    (void)regs;
    uint8_t scancode = i686_inb(0x60);

    /* ignore key releases (top bit set) */
    if (scancode & 0x80) return;

    if (scancode < sizeof(scancode_map)) {
        char c = scancode_map[scancode];
        if (c) {
            /* print the character; printf exists in your kernel already */
            printf("%c", c);
        }
    }
}

/* Register keyboard handler for IRQ1 */
void i686_keyboard_init(void)
{
    i686_IRQ_RegisterHandler(1, i686_keyboard_irq);
}