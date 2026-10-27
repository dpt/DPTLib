/* text/utf8/rune-start.c -- find the start of the UTF-8 codepoint holding a
 * byte */

#include <assert.h>

#include "text/utf8.h"

int utf8_rune_start(const char *s, int len, int index)
{
  unsigned long codepoint;
  int           i;
  int           n;

  assert(s);

  /* ponytail: a scan from the start, fine for field-sized strings */
  for (i = 0; i < len; i += n)
  {
    n = utf8_decode(s + i, len - i, &codepoint);
    if (i + n > index)
      break;
  }

  return i;
}
