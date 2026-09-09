/* framebuf/span-registry/get.c */

#include <stddef.h>

#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-registry.h"

#include "regdata.h"

const span_t *spanregistry_get(pixelfmt_t format)
{
  /* seed the cache with a value no real pixelfmt_t takes, so the first call
   * for pixelfmt_p1 (== 0) still does the lookup rather than returning the
   * zero-initialised lastspan (NULL) */
  static pixelfmt_t    lastformat = pixelfmt_unknown;
  static const span_t *lastspan;
  int                  i;

  /* avoid full lookup where possible */
  if (lastformat == format && lastspan != NULL)
    return lastspan;

  for (i = 0; i < nspans; i++)
    if (spans[i]->format == format)
    {
      lastformat = format;
      lastspan   = spans[i];

      return lastspan;
    }

  return NULL;
}
