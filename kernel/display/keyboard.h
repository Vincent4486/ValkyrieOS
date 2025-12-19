// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <stdint.h>

/**
 * Generic keyboard interface (platform-independent)
 * Handles scancode processing, line buffering, and editing
 */

/* Process a scancode (called by platform-specific drivers) */
void Keyboard_HandleScancode(uint8_t scancode);

/* Platform-independent line reading functions */
int Keyboard_ReadlineNb(char *buf, int bufsize);
int Keyboard_Readline(char *buf, int bufsize);