/* text/utf8/decode.c -- decode one UTF-8 codepoint */

#include <assert.h>

#include "text/utf8.h"

int utf8_decode(const char *s, int len, unsigned long *codepoint)
{
  /* smallest codepoint each sequence length may encode */
  static const unsigned long min[4] = { 0x0, 0x80, 0x800, 0x10000 };

  const unsigned char *p;
  unsigned long        cp;
  int                  n;
  int                  i;

  assert(s);
  assert(len > 0);
  assert(codepoint);

  p = (const unsigned char *) s;

  if (p[0] < 0x80)
  {
    *codepoint = p[0];
    return 1;
  }

  if ((p[0] & 0xE0) == 0xC0)
  {
    cp = p[0] & 0x1F;
    n  = 2;
  }
  else if ((p[0] & 0xF0) == 0xE0)
  {
    cp = p[0] & 0x0F;
    n  = 3;
  }
  else if ((p[0] & 0xF8) == 0xF0)
  {
    cp = p[0] & 0x07;
    n  = 4;
  }
  else
  {
    goto invalid; /* continuation byte, or 0xF8..0xFF */
  }

  if (n > len)
    goto invalid;

  for (i = 1; i < n; i++)
  {
    if ((p[i] & 0xC0) != 0x80)
      goto invalid;

    cp = (cp << 6) | (p[i] & 0x3F);
  }

  if (cp < min[n - 1] ||
      cp > 0x10FFFF ||
      (cp >= 0xD800 && cp <= 0xDFFF))
    goto invalid;

  *codepoint = cp;
  return n;


invalid:
  *codepoint = utf8_REPLACEMENT;
  return 1;
}
