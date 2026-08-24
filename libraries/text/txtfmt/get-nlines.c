/* get-nlines.c -- txtfmt - text formatting */

#include "text/txtfmt.h"

#include "impl.h"

int txtfmt_get_nlines(const txtfmt_t *tx)
{
  return tx->nspans;
}
