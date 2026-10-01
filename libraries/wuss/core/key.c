/* wuss/key.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

static result_t key(wuss_t              *wuss,
                    int                  code,
                    wuss_key_modifiers_t modifiers,
                    int                 *claimed)
{
  result_t     rc;
#ifdef WUSS_ICONS
  int          used;
#endif
  wuss_event_t event;

  if (claimed != NULL)
    *claimed = 0;

  if (wuss->drag_window != NULL)
  {
    if (code == wuss_KEY_ESCAPE)
    {
      wuss__drag_end(wuss, wuss->pointer, NULL, 1);
      if (claimed != NULL)
        *claimed = 1;
    }
    return result_OK;
  }

#ifdef WUSS_MENUS
  /* Escape dismisses an open menu chain, as a click outside it does: menus
   * never take the input focus, so no menu window would ever see the key */
  if (code == wuss_KEY_ESCAPE && wuss->menu_chain != NULL)
  {
    wuss__menu_abandon(wuss);
    if (claimed != NULL)
      *claimed = 1;
    return result_OK;
  }
#endif

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

result_t wuss_key(wuss_t              *wuss,
                  int                  code,
                  wuss_key_modifiers_t modifiers,
                  int                 *claimed)
{
  result_t rc;

  assert(wuss != NULL);

  wuss__message_enter(wuss);
  rc = key(wuss, code, modifiers, claimed);
  wuss__message_leave(wuss);

  return rc;
}
