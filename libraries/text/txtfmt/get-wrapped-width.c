/* get-wrapped-width.c -- txtfmt - text formatting */

#include "text/txtfmt.h"

#include "impl.h"

int txtfmt_get_wrapped_width(const txtfmt_t *tx)
{
  return tx->wrapped_width;
}
