/* wuss/key.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

result_t wuss_key(wuss_t              *wuss,
                  int                  code,
                  wuss_key_modifiers_t modifiers,
                  int                 *claimed)
{
  result_t     rc;
#ifdef WUSS_ICONS
  int          used;
#endif
  wuss_event_t event;

  assert(wuss != NULL);

  if (claimed != NULL)
    *claimed = 0;

#ifdef WUSS_ICONS
  /* the caret's writable gets first refusal */
  rc = wuss__writable_key(wuss, code, modifiers, &used);
  if (used)
  {
    if (claimed != NULL)
      *claimed = 1;
    return rc;
  }
#endif

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
