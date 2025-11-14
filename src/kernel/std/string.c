// SPDX-License-Identifier: AGPL-3.0-or-later

#include "string.h"
#include <stddef.h>
#include <stdint.h>

const char *strchr(const char *str, char chr)
{
   if (str == NULL) return NULL;

   while (*str)
   {
      if (*str == chr) return str;

      ++str;
   }

   return NULL;
}

char *strcpy(char *dst, const char *src)
{
   char *origDst = dst;

   if (dst == NULL) return NULL;

   if (src == NULL)
   {
      *dst = '\0';
      return dst;
   }

   while (*src)
   {
      *dst = *src;
      ++src;
      ++dst;
   }

   *dst = '\0';
   return origDst;
}

unsigned strlen(const char *str)
{
   unsigned len = 0;
   while (*str)
   {
      ++len;
      ++str;
   }

   return len;
}

int str_eq(const char *a, const char *b)
{
   if (a == NULL || b == NULL) return 0;
   while (*a && *b)
   {
      if (*a != *b) return 0;
      ++a;
      ++b;
   }
   return *a == *b;
}

char *strncpy(char *dst, const char *src, unsigned n)
{
   char *origDst = dst;

   if (dst == NULL) return NULL;

   if (src == NULL)
   {
      while (n > 0)
      {
         *dst = '\0';
         ++dst;
         --n;
      }
      return origDst;
   }

   while (n > 0 && *src)
   {
      *dst = *src;
      ++src;
      ++dst;
      --n;
   }

   // Pad remaining with null bytes
   while (n > 0)
   {
      *dst = '\0';
      ++dst;
      --n;
   }

   return origDst;
}

int strcmp(const char *a, const char *b)
{
   if (a == NULL && b == NULL) return 0;
   if (a == NULL) return -1;
   if (b == NULL) return 1;

   while (*a && *b)
   {
      if (*a < *b) return -1;
      if (*a > *b) return 1;
      ++a;
      ++b;
   }

   if (*a == *b) return 0;
   return (*a < *b) ? -1 : 1;
}
