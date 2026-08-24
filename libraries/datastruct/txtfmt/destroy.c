/* destroy.c -- txtfmt - text formatting */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/txtfmt.h"

#include "impl.h"

void txtfmt_destroy(txtfmt_t *tx)
{
  if (tx)
  {
    free(tx->spans);
    free(tx->s);
    free(tx);
  }
}
