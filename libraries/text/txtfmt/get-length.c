/* text/txtfmt/get-length.c -- text formatting */

#include "text/txtfmt.h"

#include "impl.h"

int txtfmt_get_length(const txtfmt_t *tx)
{
  return tx->length;
}
