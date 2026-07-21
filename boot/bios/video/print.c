// SPDX-License-Identifier: GPL-3.0-only

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "video.h"
#include <logging.h>

void putc(char c);
static void put_reverse(const char *buf, int len);
static void put_unsigned(unsigned long long val, unsigned radix,
                         const char *digits, int min_width, bool zero_pad);
static void put_signed(long long val, unsigned radix, const char *digits,
                       int min_width, bool zero_pad);

#define PRINTF_STATE_NORMAL 0
#define PRINTF_STATE_LENGTH 1
#define PRINTF_STATE_LENGTH_LONG 2
#define PRINTF_STATE_SPEC 3

#define PRINTF_LENGTH_DEFAULT 0
#define PRINTF_LENGTH_LONG 1
#define PRINTF_LENGTH_LONG_LONG 2

static const char s_HexLower[] = "0123456789abcdef";
static const char s_HexUpper[] = "0123456789ABCDEF";

static void put_reverse(const char *buf, int len)
{
   while (--len >= 0)
      putc(buf[len]);
}

static void put_unsigned(unsigned long long val, unsigned radix,
                         const char *digits, int min_width, bool zero_pad)
{
   char buf[68]; /* 64 bits in binary = up to 64 chars + sign */
   int pos = 0;

   if (radix < 2 || radix > 16) return;

   do
   {
      buf[pos++] = digits[val % radix];
      val /= radix;
   } while (val > 0 && pos < (int)sizeof(buf));

   while (pos < min_width && pos < (int)sizeof(buf))
      buf[pos++] = zero_pad ? '0' : ' ';

   put_reverse(buf, pos);
}

static void put_signed(long long val, unsigned radix, const char *digits,
                       int min_width, bool zero_pad)
{
   if (val < 0)
   {
      putc('-');
      put_unsigned(-(unsigned long long)val, radix, digits,
                   min_width > 0 ? min_width - 1 : 0, zero_pad);
   }
   else
   {
      put_unsigned((unsigned long long)val, radix, digits, min_width, zero_pad);
   }
}

void putc(char c) { Video_PutChar(c, -1, -1, TEXT_DEFAULT_COLOR); }

void puts(const char *str)
{
   if (!str) return;
   for (; *str; ++str)
      putc(*str);
}

void puti(int val) { put_signed(val, 10, s_HexLower, 0, false); }
void putd(int val) { put_signed(val, 10, s_HexLower, 0, false); }
void putl(long val) { put_signed(val, 10, s_HexLower, 0, false); }
void putll(long long val) { put_signed(val, 10, s_HexLower, 0, false); }

void putu(unsigned val) { put_unsigned(val, 10, s_HexLower, 0, false); }
void putul(unsigned long val) { put_unsigned(val, 10, s_HexLower, 0, false); }
void putull(unsigned long long val)
{
   put_unsigned(val, 10, s_HexLower, 0, false);
}

void putx(unsigned long long val)
{
   put_unsigned(val, 16, s_HexLower, 0, false);
}
void putX(unsigned long long val)
{
   put_unsigned(val, 16, s_HexUpper, 0, false);
}

void puto(unsigned long long val)
{
   put_unsigned(val, 8, s_HexLower, 0, false);
}

void putb(unsigned long long val)
{
   put_unsigned(val, 2, s_HexLower, 0, false);
}

void putp(const void *ptr)
{
   puts("0x");
   if (sizeof(ptr) == 8)
      put_unsigned((unsigned long long)(uintptr_t)ptr, 16, s_HexLower, 16,
                   true);
   else
      put_unsigned((unsigned long long)(uintptr_t)ptr, 16, s_HexLower, 8, true);
}

void printf(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   va_end(args);
}

void vprintf(const char *fmt, va_list args)
{
   int state = PRINTF_STATE_NORMAL;
   int length = PRINTF_LENGTH_DEFAULT;
   int width = 0;
   bool zero_pad = false;

   while (*fmt)
   {
      switch (state)
      {
      case PRINTF_STATE_NORMAL:
         if (*fmt == '%')
         {
            state = PRINTF_STATE_LENGTH;
            width = 0;
            zero_pad = false;
         }
         else
         {
            putc(*fmt);
         }
         break;

      case PRINTF_STATE_LENGTH:
         if (*fmt == '0')
         {
            zero_pad = true;
         }
         else if (*fmt >= '1' && *fmt <= '9')
         {
            width = width * 10 + (*fmt - '0');
         }
         else if (*fmt == 'l')
         {
            length = PRINTF_LENGTH_LONG;
            state = PRINTF_STATE_LENGTH_LONG;
         }
         else
         {
            goto handle_spec;
         }
         break;

      case PRINTF_STATE_LENGTH_LONG:
         if (*fmt == 'l')
         {
            length = PRINTF_LENGTH_LONG_LONG;
            state = PRINTF_STATE_SPEC;
         }
         else
         {
            goto handle_spec;
         }
         break;

      case PRINTF_STATE_SPEC:
      handle_spec:
         switch (*fmt)
         {
         case 'c':
            putc((char)va_arg(args, int));
            break;

         case 's':
            puts(va_arg(args, const char *));
            break;

         case '%':
            putc('%');
            break;

         case 'd':
         case 'i':
            switch (length)
            {
            case PRINTF_LENGTH_LONG:
               put_signed(va_arg(args, long), 10, s_HexLower, width, zero_pad);
               break;
            case PRINTF_LENGTH_LONG_LONG:
               put_signed(va_arg(args, long long), 10, s_HexLower, width,
                          zero_pad);
               break;
            default:
               put_signed(va_arg(args, int), 10, s_HexLower, width, zero_pad);
               break;
            }
            break;

         case 'u':
            switch (length)
            {
            case PRINTF_LENGTH_LONG:
               put_unsigned(va_arg(args, unsigned long), 10, s_HexLower, width,
                            zero_pad);
               break;
            case PRINTF_LENGTH_LONG_LONG:
               put_unsigned(va_arg(args, unsigned long long), 10, s_HexLower,
                            width, zero_pad);
               break;
            default:
               put_unsigned(va_arg(args, unsigned int), 10, s_HexLower, width,
                            zero_pad);
               break;
            }
            break;

         case 'x':
            switch (length)
            {
            case PRINTF_LENGTH_LONG:
               put_unsigned(va_arg(args, unsigned long), 16, s_HexLower, width,
                            zero_pad);
               break;
            case PRINTF_LENGTH_LONG_LONG:
               put_unsigned(va_arg(args, unsigned long long), 16, s_HexLower,
                            width, zero_pad);
               break;
            default:
               put_unsigned(va_arg(args, unsigned int), 16, s_HexLower, width,
                            zero_pad);
               break;
            }
            break;

         case 'X':
            switch (length)
            {
            case PRINTF_LENGTH_LONG:
               put_unsigned(va_arg(args, unsigned long), 16, s_HexUpper, width,
                            zero_pad);
               break;
            case PRINTF_LENGTH_LONG_LONG:
               put_unsigned(va_arg(args, unsigned long long), 16, s_HexUpper,
                            width, zero_pad);
               break;
            default:
               put_unsigned(va_arg(args, unsigned int), 16, s_HexUpper, width,
                            zero_pad);
               break;
            }
            break;

         case 'o':
            switch (length)
            {
            case PRINTF_LENGTH_LONG:
               put_unsigned(va_arg(args, unsigned long), 8, s_HexLower, width,
                            zero_pad);
               break;
            case PRINTF_LENGTH_LONG_LONG:
               put_unsigned(va_arg(args, unsigned long long), 8, s_HexLower,
                            width, zero_pad);
               break;
            default:
               put_unsigned(va_arg(args, unsigned int), 8, s_HexLower, width,
                            zero_pad);
               break;
            }
            break;

         case 'p':
            putp(va_arg(args, const void *));
            break;

         case 'b':
            switch (length)
            {
            case PRINTF_LENGTH_LONG:
               put_unsigned(va_arg(args, unsigned long), 2, s_HexLower, width,
                            zero_pad);
               break;
            case PRINTF_LENGTH_LONG_LONG:
               put_unsigned(va_arg(args, unsigned long long), 2, s_HexLower,
                            width, zero_pad);
               break;
            default:
               put_unsigned(va_arg(args, unsigned int), 2, s_HexLower, width,
                            zero_pad);
               break;
            }
            break;

         default:
            break;
         }

         state = PRINTF_STATE_NORMAL;
         length = PRINTF_LENGTH_DEFAULT;
         width = 0;
         zero_pad = false;
         break;
      }

      fmt++;
   }
}

void logfmt(int log_type, const char *fmt, ...)
{
#ifdef RELEASE
   if (log_type == LOG_INFO)
      return;
#endif

   switch (log_type)
   {
   case LOG_INFO:
      puts("[INFO] ");
      break;
   case LOG_WARNING:
      puts("[WARN] ");
      break;
   case LOG_ERROR:
      puts("[ERROR] ");
      break;
   case LOG_FATAL:
      puts("[FATAL] ");
      break;
   default:
      puts("[LOG] ");
      break;
   }

   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   va_end(args);
}