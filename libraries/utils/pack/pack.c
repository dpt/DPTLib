/* pack.c -- structure packing */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "base/types.h"

#include "utils/pack.h"

size_t pack(uint8_t *buf, const char *fmt, ...)
{
  va_list     args;
  uint8_t    *bp;
  const char *p;

  va_start(args, fmt);

  bp = buf;

  for (p = fmt; *p != '\0'; )
  {
    int c;
    int n;

    c = *p++;

    if (c == '*') /* array mode */
    {
      /* next arg is length of array */
      n = va_arg(args, int);
      c = *p++;

      switch (c)
      {
      case 'c': /* (array of) char */
      {
        const uint8_t *a; /* array */

        a = (const uint8_t *) va_arg(args, uint8_t *);

        memcpy(bp, a, n);
        bp += n;
      }
      break;

      case 's': /* (array of) short */
      {
        const uint16_t *a;

        a = (const uint16_t *) va_arg(args, uint16_t *);

        while (n--)
        {
          uint16_t v;

          v = *a++;
          *bp++ = (uint8_t) (v      ); /* little endian */
          *bp++ = (uint8_t) (v >>  8);
        }
      }
      break;

      case 'i': /* (array of) int */
      {
        const uint32_t *a;

        a = (const uint32_t *) va_arg(args, uint32_t *);

        while (n--)
        {
          uint32_t v;

          v = *a++;
          *bp++ = (uint8_t) (v      ); /* little endian */
          *bp++ = (uint8_t) (v >>  8);
          *bp++ = (uint8_t) (v >> 16);
          *bp++ = (uint8_t) (v >> 24);
        }
      }
      break;

      case 'q': /* (array of) long long / 'quad' */
      {
        const uint64_t *a;

        a = (const uint64_t *) va_arg(args, uint64_t *);

        while (n--)
        {
          uint64_t v;

          v = *a++;
          *bp++ = (uint8_t) (v      ); /* little endian */
          *bp++ = (uint8_t) (v >>  8);
          *bp++ = (uint8_t) (v >> 16);
          *bp++ = (uint8_t) (v >> 24);
          *bp++ = (uint8_t) (v >> 32);
          *bp++ = (uint8_t) (v >> 40);
          *bp++ = (uint8_t) (v >> 48);
          *bp++ = (uint8_t) (v >> 56);
        }
      }
      break;

      default:
        assert("pack: Illegal array format specifier" == NULL);
        va_end(args);
        return 0;
      }
    }
    else /* N single characters */
    {
      /* get repeat count, if any */
      if (isdigit(c))
      {
        n = c - '0';
        for (;;)
        {
          c = *p++;
          if (!isdigit(c)) break;
          n = n * 10 + (c - '0');
        }
      }
      else
      {
        /* n wasn't specified: assume the default of one */
        n = 1;
      }

      switch (c)
      {
      case 'c': /* char */
        while (n--)
          *bp++ = (uint8_t) va_arg(args, uint32_t);
        break;

      case 's': /* short */
      {
        uint32_t v;

        while (n--)
        {
          v = (uint16_t) va_arg(args, uint32_t);
          *bp++ = (uint8_t) (v); /* little endian */
          *bp++ = (uint8_t) (v >> 8);
        }
      }
      break;

      case 'i': /* int */
      {
        uint32_t v;

        while (n--)
        {
          v = va_arg(args, uint32_t);
          *bp++ = (uint8_t) (v); /* little endian */
          *bp++ = (uint8_t) (v >> 8);
          *bp++ = (uint8_t) (v >> 16);
          *bp++ = (uint8_t) (v >> 24);
        }
      }
      break;

      case 'q': /* long long / 'quad' */
      {
        uint64_t v;

        while (n--)
        {
          v = va_arg(args, uint64_t);
          *bp++ = (uint8_t) (v); /* little endian */
          *bp++ = (uint8_t) (v >> 8);
          *bp++ = (uint8_t) (v >> 16);
          *bp++ = (uint8_t) (v >> 24);
          *bp++ = (uint8_t) (v >> 32);
          *bp++ = (uint8_t) (v >> 40);
          *bp++ = (uint8_t) (v >> 48);
          *bp++ = (uint8_t) (v >> 56);
        }
      }
      break;

      default: /* illegal type character */
        assert("pack: Illegal format specifier" == NULL);
        va_end(args);
        return 0;
      }
    }
  }

  va_end(args);

  return (size_t) (bp - buf);
}
