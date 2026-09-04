/* text/txtfmt/wrap.c -- text formatting */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "text/txtfmt.h"

#include "impl.h"

static result_t emit_line(txtfmt_t *tx, int start, int length)
{
  int i;

  i = tx->nspans;

  if (i + 1 == tx->allocated)
  {
    int   n;
    span *newspans;

    /* doubling strategy */

    n = MAX(tx->allocated * 2, StartAt);

    newspans = realloc(tx->spans, sizeof(*tx->spans) * n);
    if (newspans == NULL)
      return result_OOM;

    tx->spans     = newspans;
    tx->allocated = n;
  }

  tx->spans[i].start  = start;
  tx->spans[i].length = length;

  tx->wrapped_width = MAX(tx->wrapped_width, length);

  tx->nspans = ++i;

  return result_OK;
}

/* wrap to a character width */
result_t txtfmt_wrap(txtfmt_t *tx, int width)
{
  result_t    err;
  const char *startofline;
  int         curlen;
  const char *p;

  if (tx->width == width)
    return result_OK; /* already the required size */

  /* reset the spans */

  tx->nspans        = 0;

  tx->wrapped_width = 0;

  startofline       = tx->s;
  curlen            = 0;

  for (p = tx->s; *p != '\0'; )
  {
    int spacelen;
    int wordlen;

    /* deal with newlines */

    if (*p == '\n' || *p == '\r')
    {
      err = emit_line(tx, startofline - tx->s, curlen);
      if (err)
        return err;

      startofline = NULL;
      p++; /* skip newline character */
      curlen      = 0;

      continue;
    }

    /* count leading spaces */

    spacelen = strspn(p, " ");

    /* quit if no more words are left */

    if (p[spacelen] == '\0')
      break;

    /* get length of word */

    wordlen = strcspn(p + spacelen, " \n\r");

    /* we can't break until we've placed something on the line */

    if (curlen == 0)
    {
      startofline = p + spacelen;
      p          += spacelen + wordlen;
      curlen      = wordlen;

      continue;
    }

    if (curlen + spacelen + wordlen > width)
    {
      /* emit the line */

      err = emit_line(tx, startofline - tx->s, curlen);
      if (err)
        return err;

      startofline = NULL;
      curlen      = 0;

      /* now we jump back and re-measure the spaces and word length */
    }
    else
    {
      /* add the word to the to-be-output counts */

      p      += spacelen + wordlen;
      curlen += spacelen + wordlen;
    }
  }

  /* take care of trailing characters */

  if (curlen)
  {
    err = emit_line(tx, startofline - tx->s, curlen);
    if (err)
      return err;
  }

  tx->width = width;

  return result_OK;
}
