/* get-length.c -- txtfmt - text formatting */

#include "datastruct/txtfmt.h"

#include "impl.h"

int txtfmt_get_length(const txtfmt_t *tx)
{
  return tx->length;
}
