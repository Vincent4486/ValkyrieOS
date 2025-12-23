// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef I686_PS2_H
#define I686_PS2_H

#include <stdint.h>

/**
 * i686-specific PS/2 keyboard driver
 * Handles port I/O, IRQ registration, and platform-specific idle
 */

/* Initialize PS/2 keyboard for i686 */
void ps2_keyboard_init(void);

/* i686-specific keyboard readline with platform idle */
int ps2_Keyboard_Readline(char *buf, int bufsize);

/* Non-blocking readline */
int ps2_Keyboard_ReadlineNb(char *buf, int bufsize);

#endif