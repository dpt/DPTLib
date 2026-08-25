/* checker.c -- wuss test - checkerboard client */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"

#include "checker.h"

result_t checker_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  checker_client_t *cc;
  int               x, y;

  NOT_USED(window);

  cc = client_data;

  for (y = content->y0; y < content->y1; y++)
    for (x = content->x0; x < content->x1; x++)
      screen_draw_pixel(scr, x, y, ((x + y) & 1) ? cc->black : cc->white);

  return result_OK;
}

#endif /* USE_SDL */
