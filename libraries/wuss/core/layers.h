/* wuss/core/layers.h -- wuss - no-op stubs for absent layers' click hooks */

#ifndef WUSS_CORE_LAYERS_H
#define WUSS_CORE_LAYERS_H

#include "types.h"


#ifndef WUSS_FURNITURE
/* No furniture: nothing is ever held or hit. */
static inline wuss_window_t *wuss__furniture_release(wuss_t             *wuss,
                                                     wuss_mouse_action_t action)
{
  NOT_USED(wuss);
  NOT_USED(action);
  return NULL;
}

static inline int wuss__furniture_click(wuss_window_t      *window,
                                        point_t             p,
                                        wuss_button_t       button,
                                        wuss_mouse_action_t action,
                                        result_t           *rc)
{
  NOT_USED(window);
  NOT_USED(p);
  NOT_USED(button);
  NOT_USED(action);
  NOT_USED(rc);
  return 0;
}
#endif

#ifndef WUSS_ICONS
/* No icon layer: nothing is ever held or hit. */
static inline void wuss__icon_release_held(wuss_t             *wuss,
                                           point_t             p,
                                           wuss_mouse_action_t action)
{
  NOT_USED(wuss);
  NOT_USED(p);
  NOT_USED(action);
}

static inline int wuss__icon_click(wuss_window_t      *window,
                                   point_t             p,
                                   point_t             doc_point,
                                   wuss_button_t       button,
                                   wuss_mouse_action_t action,
                                   result_t           *rc)
{
  NOT_USED(window);
  NOT_USED(p);
  NOT_USED(doc_point);
  NOT_USED(button);
  NOT_USED(action);
  NOT_USED(rc);
  return 0;
}
#endif

#endif /* WUSS_CORE_LAYERS_H */
