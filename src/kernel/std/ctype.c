// SPDX-License-Identifier: AGPL-3.0-or-later

#include "ctype.h"

bool islower(char chr) { return chr >= 'a' && chr <= 'z'; }

char toupper(char chr) { return islower(chr) ? (chr - 'a' + 'A') : chr; }
