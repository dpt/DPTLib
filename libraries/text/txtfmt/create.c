/* text/txtfmt/create.c -- text formatting */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "text/txtfmt.h"

#include "impl.h"

result_t txtfmt_create(const char *s, txtfmt_t **tx)
{
  result_t  err;
  txtfmt_t *newtx;
  size_t    length;

  newtx = calloc(1, sizeof(*newtx));
  if (newtx == NULL)
    goto OOM;

  length = strlen(s);

  newtx->s = malloc(length + 1); /* include terminator */
  if (newtx->s == NULL)
    goto OOM;

  memcpy(newtx->s, s, length + 1);

  newtx->length = length;

  newtx->spans = malloc(StartAt * sizeof(*newtx->spans));
  if (newtx->spans == NULL)
    goto OOM;

  newtx->nspans          = 1;
  newtx->allocated       = StartAt;

  /* initially the string is unwrapped */
  newtx->spans[0].start  = 0;
  newtx->spans[0].length = length;

  newtx->width           = 0;

  newtx->wrapped_width   = length;

  *tx = newtx;

  return result_OK;


OOM:

  err = result_OOM;

  txtfmt_destroy(newtx);

  return err;
}
