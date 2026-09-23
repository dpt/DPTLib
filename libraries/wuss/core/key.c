/* wuss/key.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

result_t wuss_key(wuss_t              *wuss,
                  int                  code,
                  wuss_key_modifiers_t modifiers,
                  int                 *claimed)
{
  result_t     rc;
  wuss_event_t event;

  assert(wuss != NULL);

  if (claimed != NULL)
    *claimed = 0;

  if (wuss->focus == NULL || wuss->focus->task->handle == NULL)
    return result_OK;

  event.kind               = wuss_EVENT_KEY;
  event.data.key.code      = code;
  event.data.key.modifiers = modifiers;
  rc = wuss__deliver(wuss->focus->task, wuss->focus, &event);
  if (rc == result_WUSS_KEY_UNCLAIMED)
    return result_OK;

  if (claimed != NULL)
    *claimed = 1;

  return rc;
}
