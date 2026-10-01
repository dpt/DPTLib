/* text/utf8/prev.c -- find the start of the previous UTF-8 codepoint */

#include <assert.h>

#include "text/utf8.h"

int utf8_prev(const char *s, int index)
{
  unsigned long codepoint;
  int           n;

  assert(s);
  assert(index > 0);

  /* the longest well-formed sequence ending at index; anything else is a
   * malformed byte, which utf8_decode also steps over one at a time */
  for (n = 4; n > 1; n--)
    if (index - n >= 0 && utf8_decode(s + index - n, n, &codepoint) == n)
      return index - n;

  return index - 1;
}
