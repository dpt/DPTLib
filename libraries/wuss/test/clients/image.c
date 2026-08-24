/* image.c -- wuss test - static bitmap image client */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"

#include "image.h"

result_t image_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  image_client_t *ic;

  NOT_USED(window);

  ic = client_data;

  screen_draw_bitmap(scr, content->x0, content->y0, &ic->bitmap);

  return result_OK;
}

#endif /* USE_SDL */
