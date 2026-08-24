/* print.c -- txtfmt - text formatting */

#include <stdio.h>

#include "datastruct/txtfmt.h"

#include "impl.h"

result_t txtfmt_print(const txtfmt_t *tx)
{
  int i;

  for (i = 0; i < tx->nspans; i++)
  {
    int         start;
    int         length;
    const char *s;

    start  = tx->spans[i].start;
    length = tx->spans[i].length;
    s      = tx->s + start;

    if (length == 0)
      printf("%3d: (%d,%d)\n", i, start, length);
    else
      printf("%3d: %.*s (%d,%d)\n", i, length, s, start, length);
  }

  return result_OK;
}
