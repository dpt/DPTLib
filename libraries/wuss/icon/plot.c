/* wuss/icon/plot.c -- draw an icon from a spec without retaining it */

#include <assert.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

result_t wuss_icon_plot(wuss_window_t          *window,
                        const wuss_icon_spec_t *spec,
                        const box_t            *content,
                        point_t                 scroll)
{
  result_t    rc;
  wuss_icon_t scratch;

  assert(window  != NULL);
  assert(spec    != NULL);
  assert(content != NULL);

  rc = wuss__icon_from_spec(window->wuss, spec, &scratch);
  if (rc != result_OK)
    return rc;

  wuss__icon_draw(window->wuss, window, &scratch, content, scroll);

  return result_OK;
}
