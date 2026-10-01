/* text/utf8/encode.c -- encode one UTF-8 codepoint */

#include <assert.h>

#include "text/utf8.h"

int utf8_encode(unsigned long codepoint, char *buf)
{
  unsigned char *p;

  assert(buf);

  p = (unsigned char *) buf;

  if (codepoint < 0x80)
  {
    p[0] = (unsigned char) codepoint;
    return 1;
  }

  if (codepoint < 0x800)
  {
    p[0] = (unsigned char) (0xC0 | (codepoint >> 6));
    p[1] = (unsigned char) (0x80 | (codepoint & 0x3F));
    return 2;
  }

  if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
    return 0; /* surrogate */

  if (codepoint < 0x10000)
  {
    p[0] = (unsigned char) (0xE0 | (codepoint >> 12));
    p[1] = (unsigned char) (0x80 | ((codepoint >> 6) & 0x3F));
    p[2] = (unsigned char) (0x80 | (codepoint & 0x3F));
    return 3;
  }

  if (codepoint > 0x10FFFF)
    return 0;

  p[0] = (unsigned char) (0xF0 | (codepoint >> 18));
  p[1] = (unsigned char) (0x80 | ((codepoint >> 12) & 0x3F));
  p[2] = (unsigned char) (0x80 | ((codepoint >> 6) & 0x3F));
  p[3] = (unsigned char) (0x80 | (codepoint & 0x3F));
  return 4;
}
