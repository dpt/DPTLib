/* image.c -- wuss test - static bitmap image client */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "image.h"

result_t image_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  image_client_t *ic;
  int             x, y;
  const unsigned char *row;

  NOT_USED(window);

  ic  = client_data;
  row = ic->bitmap.base;

  for (y = 0; y < ic->bitmap.height; y++)
  {
    const pixelfmt_rgba8888_t *px;

    px = (const pixelfmt_rgba8888_t *) row;

    for (x = 0; x < ic->bitmap.width; x++)
    {
      colour_t c;

      c.primary = px[x];
      if (colour_get_alpha(&c) > 0) /* skip fully transparent pixels: no fill to blend against here */
        screen_draw_pixel(scr, content->x0 + x, content->y0 + y, c);
    }

    row += ic->bitmap.rowbytes;
  }

  return result_OK;
}

#endif /* USE_SDL */
