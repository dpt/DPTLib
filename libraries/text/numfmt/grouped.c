/* text/numfmt/grouped.c -- format a number with comma separated groups */

#include <string.h>

#include "text/numfmt.h"

char *numfmt_grouped(unsigned long long value, char *buf)
{
  char  tmp[numfmt_GROUPED_MAX];
  char *p;
  int   ndigits;

  /* fill backwards, as the digits emerge least significant first */
  p       = tmp + sizeof(tmp);
  ndigits = 0;

  *--p = '\0';
  do
  {
    if (ndigits > 0 && ndigits % 3 == 0)
      *--p = ',';
    *--p = (char) ('0' + value % 10);
    value /= 10;
    ndigits++;
  }
  while (value != 0);

  memcpy(buf, p, (size_t) (tmp + sizeof(tmp) - p));

  return buf;
}
