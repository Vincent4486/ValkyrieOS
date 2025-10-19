#include "keyboard.h"
#include "io.h"
#include "irq.h"
#include "stdio.h"   // for putc/printf

/* Input line buffer for simple line editing */
#define KB_LINE_BUF 256
static char kb_line[KB_LINE_BUF];
static int kb_len = 0;
static int kb_ready = 0; /* 1 when a full line (\n) is available */

/* modifier state */
static int shift = 0;
static int caps = 0;

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

    /* handle key releases and modifier keys */
    if (scancode & 0x80) {
        /* key release: clear shift if shift released */
        uint8_t key = scancode & 0x7F;
        if (key == 0x2A || key == 0x36) shift = 0; /* left/right shift */
        return;
    }

    /* check for shift press */
    if (scancode == 0x2A || scancode == 0x36) { shift = 1; return; }

    /* caps lock toggle (make only) */
    if (scancode == 0x3A) { caps = !caps; return; }

    /* map scancode to character, apply shift/caps */
    if (scancode < sizeof(scancode_map)) {
        char base = scancode_map[scancode];
        if (!base) return;

        char out = base;
        /* simple alphabetic handling for caps/shift */
        if (out >= 'a' && out <= 'z') {
            if ((caps && !shift) || (shift && !caps)) {
                out = out - 'a' + 'A';
            }
        } else {
            /* rudimentary shifted symbols for digits/punctuation */
            if (shift) {
                switch (out) {
                    case '1': out = '!'; break;
                    case '2': out = '@'; break;
                    case '3': out = '#'; break;
                    case '4': out = '$'; break;
                    case '5': out = '%'; break;
                    case '6': out = '^'; break;
                    case '7': out = '&'; break;
                    case '8': out = '*'; break;
                    case '9': out = '('; break;
                    case '0': out = ')'; break;
                    case '-': out = '_'; break;
                    case '=': out = '+'; break;
                    case '\\': out = '|'; break;
                    case ';': out = ':'; break;
                    case '\'': out = '"'; break;
                    case ',': out = '<'; break;
                    case '.': out = '>'; break;
                    case '/': out = '?'; break;
                    case '`': out = '~'; break;
                    case '[': out = '{'; break;
                    case ']': out = '}'; break;
                    default: break;
                }
            }
        }

        /* handle backspace specially to update buffer */
        if (out == '\b') {
            if (kb_len > 0) {
                kb_len--;
                putc('\b');
            }
            return;
        }

        /* append to buffer if printable; on newline mark ready */
        if (out == '\n') {
            if (kb_len < KB_LINE_BUF - 1) {
                kb_line[kb_len++] = '\n';
            }
            kb_ready = 1;
            putc('\n');
        } else {
            if (kb_len < KB_LINE_BUF - 1) {
                kb_line[kb_len++] = out;
                putc(out);
            }
        }
    }
}

/* Register keyboard handler for IRQ1 */
void i686_keyboard_init(void)
{
    i686_IRQ_RegisterHandler(1, i686_keyboard_irq);
}

/* Non-blocking readline: returns number of bytes written into buf, 0 if no line ready */
int i686_keyboard_readline_nb(char *buf, int bufsize)
{
    if (!kb_ready) return 0;
    int copy = kb_len;
    if (copy > bufsize - 1) copy = bufsize - 1;
    for (int i = 0; i < copy; ++i) buf[i] = kb_line[i];
    buf[copy] = '\0';

    /* reset bufffer */
    kb_len = 0;
    kb_ready = 0;
    return copy;
}

int i686_keyboard_readline(char *buf, int bufsize)
{
    int n;
    while ((n = i686_keyboard_readline_nb(buf, bufsize)) == 0) {
        /* simple idle: execute HLT to reduce busy spin and wait for interrupts */
        __asm__ volatile ("sti; hlt; cli");
    }
    return n;
}