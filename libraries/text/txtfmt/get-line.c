/* get-line.c -- txtfmt - text formatting */

#include "text/txtfmt.h"

#include "impl.h"

result_t txtfmt_get_line(const txtfmt_t *tx,
                         int             index,
                         const char    **line,
                         int            *length)
{
  if (index < 0 || index >= tx->nspans)
    return result_BAD_ARG;

  *line   = tx->s + tx->spans[index].start;
  *length = tx->spans[index].length;

  return result_OK;
}
