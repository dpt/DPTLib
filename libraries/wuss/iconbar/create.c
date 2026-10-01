/* wuss/iconbar/create.c -- add an icon to the icon bar */

#include <assert.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

#ifdef WUSS_ICONBAR

/* The bar's window never receives events of its own -- icons are delivered
 * to their individual owner tasks, not this one -- so its handle is never
 * actually called; it exists only to satisfy wuss_task_create's contract. */
static result_t iconbar_window_handle(wuss_window_t      *window,
                                      const wuss_event_t *event,
                                      void               *task_data)
{
  (void) window;
  (void) event;
  (void) task_data;

  return result_OK;
}

/* The internal task that owns the bar's window, created on the first
 * wuss_iconbar_icon_create of a session. Mirrors wuss__menu_task in
 * libraries/wuss/menu/menu.c. */
static wuss_task_t *iconbar_task(wuss_t *wuss)
{
  wuss_task_desc_t desc;

  if (wuss->iconbar_task != NULL)
    return wuss->iconbar_task;

  desc.handle    = iconbar_window_handle;
  desc.task_data = wuss;
  desc.name      = "wuss:iconbar";

  if (wuss_task_create(wuss, &desc, &wuss->iconbar_task) != result_OK)
    return NULL;

  return wuss->iconbar_task;
}

/* Lazily create the bar's own pinned, chromeless window on the first icon.
 * No-op if it already exists. */
static result_t iconbar_window_ensure(wuss_t *wuss)
{
  wuss_task_t   *task;
  box_t          content;
  wuss_window_t *win;
  result_t       rc;

  if (wuss->iconbar_window != NULL)
    return result_OK;

  task = iconbar_task(wuss);
  if (task == NULL)
    return result_OOM;

  /* inset by WUSS_ICONBAR_OUTLINE so the outline furniture wuss_window_create
   * grows the box by lands exactly on the strip's edges, keeping the bar's
   * visible box flush to the screen's bottom/sides as before. */
  content.x0 = WUSS_ICONBAR_OUTLINE;
  content.y0 = wuss->scr->size.h - WUSS_ICONBAR_HEIGHT + WUSS_ICONBAR_OUTLINE;
  content.x1 = wuss->scr->size.w - WUSS_ICONBAR_OUTLINE;
  content.y1 = wuss->scr->size.h - WUSS_ICONBAR_OUTLINE;

  rc = wuss_window_create(task, &content, NULL,
                          wuss_WINDOW_NO_TITLEBAR |
                          wuss_WINDOW_NO_REDRAW   | wuss_WINDOW_PINNED |
                          wuss_WINDOW_STACK_BACK,
                          wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                          SIZE2D(content.x1 - content.x0,
                                 content.y1 - content.y0),
                          SIZE2D(content.x1 - content.x0,
                                 content.y1 - content.y0),
                          &win);
  if (rc != result_OK)
    return rc;

  wuss->iconbar_window = win;

  return result_OK;
}

result_t wuss_iconbar_icon_create(wuss_t                         *wuss,
                                  wuss_task_t                    *owner,
                                  const wuss_iconbar_icon_spec_t *spec,
                                  wuss_iconbar_icon_t           **icon)
{
  result_t             rc;
  wuss_iconbar_icon_t *it;
  box_t                slot;

  assert(wuss  != NULL);
  assert(owner != NULL);
  assert(spec  != NULL);

  rc = iconbar_window_ensure(wuss);
  if (rc != result_OK)
    return rc;

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
  wuss__invalidate_clipped(wuss->iconbar_window, &slot);

  if (icon != NULL)
    *icon = it;

  return result_OK;
}

#endif /* WUSS_ICONBAR */
