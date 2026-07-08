// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#if defined(I686)
#define BIT32 32
#elif defined(x86_64) || defined(AARCH64)
#define BIT64 64
#endif