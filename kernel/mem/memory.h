// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <stddef.h>
#include <stdint.h>

/* Basic memory helpers (sizes in bytes) */
void *memcpy(void *dst, const void *src, size_t num);
void *memset(void *ptr, int value, size_t num);
int memcmp(const void *ptr1, const void *ptr2, size_t num);
void *memmove(void *dest, const void *src, size_t n);

void *SegmentOffsetToLinear(void *addr);
