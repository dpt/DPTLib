/* text/txtfmt/destroy.c -- text formatting */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "text/txtfmt.h"

#include "impl.h"

void txtfmt_destroy(txtfmt_t *tx)
{
  if (tx)
  {
    tx->alloc->free(tx->spans);
    tx->alloc->free(tx->s);
    tx->alloc->free(tx);
  }
}
