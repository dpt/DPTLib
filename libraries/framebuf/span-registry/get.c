/* get.c */

#include <stddef.h>

#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-registry.h"

#include "regdata.h"

const span_t *spanregistry_get(pixelfmt_t format)
{
  static pixelfmt_t    lastformat;
  static const span_t *lastspan;
  int                  i;

  /* avoid full lookup where possible */
  if (lastformat == format)
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
