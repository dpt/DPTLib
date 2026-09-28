/* wuss/iconbar/create.c -- add an icon to the icon bar */

#include <assert.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

#ifdef WUSS_ICONBAR

result_t wuss_iconbar_icon_create(wuss_t                         *wuss,
                                  wuss_task_t                    *owner,
                                  const wuss_iconbar_icon_spec_t *spec,
                                  wuss_iconbar_icon_t           **icon)
{
  wuss_iconbar_icon_t *it;
  box_t                slot;

  assert(wuss  != NULL);
  assert(owner != NULL);
  assert(spec  != NULL);

  if (wuss__array_grow(&wuss->alloc, (void **) &wuss->iconbar_icons,
                       sizeof(*wuss->iconbar_icons), wuss->niconbar_icons,
                       &wuss->cap_iconbar_icons, 1, 4) != 0)
    return result_OOM;

  it = wuss__malloc(wuss, sizeof(*it));
  if (it == NULL)
    return result_OOM;

  it->spec        = *spec;
  it->spec.text   = wuss__alloc_strdup(&wuss->alloc,
                                       spec->text != NULL ? spec->text : "");
  it->owner       = owner;
  it->state       = wuss_ICONBAR_ICON_STATE_NONE;

  if (it->spec.text == NULL)
  {
    wuss__free(wuss, it);
    return result_OOM;
  }

  wuss->iconbar_icons[wuss->niconbar_icons++] = it;

  wuss__iconbar_slot_box(wuss, wuss->niconbar_icons - 1, &slot);
  wuss_invalidate(wuss, &slot);

  if (icon != NULL)
    *icon = it;

  return result_OK;
}

#endif /* WUSS_ICONBAR */
