/* image.c -- wuss test - static bitmap image task */

#ifdef USE_SDL

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "io/path.h"

#include "image.h"

result_t image_create(wuss_t         *wuss,
                      const colour_t *palette,
                      const char     *resources,
                      image_task_t   *task)
{
  const char *leafname;
  const char *filename;
  wuss_task_t delegate;
  box_t       box;
  result_t    rc;

  leafname = path_join_leafname("jessica", "png");
  filename = path_join_filename(resources, 3, "resources", "images", leafname);
  rc = bitmap_load_png(&task->bitmap, filename);
  if (rc != result_OK)
    return rc;

  delegate = wuss_task_start(image_handle, task, palette_PICO8_BLACK); /* shows through the image's transparent pixels */
  /* shorter than the bitmap so there's something to scroll through */
  box             = (box_t) BOX_POS_SIZE(370, 10, task->bitmap.width, task->bitmap.height * 2 / 3);

  return wuss_window_create(wuss, &box, "Image", wuss_WINDOW_NONE, &delegate, &task->window);
}

void image_destroy(image_task_t *task)
{
  wuss_window_destroy(task->window);
  free(task->bitmap.base);
}

static result_t image_redraw(wuss_window_t *window,
                             screen_t      *scr,
                             const box_t   *content,
                             void          *task_data)
{
  image_task_t *ic;
  int           sx, sy;

  ic = task_data;

  wuss_window_get_scroll(window, &sx, &sy);

  screen_draw_bitmap(scr, content->x0 - sx, content->y0 - sy, &ic->bitmap);

  return result_OK;
}

static result_t image_scroll(wuss_window_t *window, int delta, void *task_data)
{
  image_task_t *ic;
  box_t         bounds;
  int           sx, sy, max_y;

  ic = task_data;

  wuss_window_get_content_bounds(window, &bounds);
  wuss_window_get_scroll(window, &sx, &sy);

  sy += delta;

  max_y = ic->bitmap.height - (bounds.y1 - bounds.y0);
  if (max_y < 0)
    max_y = 0;

  if (sy < 0)
    sy = 0;
  else if (sy > max_y)
    sy = max_y;

  wuss_window_set_scroll(window, sx, sy);

  return result_OK;
}

result_t image_handle(wuss_window_t     *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return image_redraw(window, event->data.redraw.scr, event->data.redraw.content, task_data);

  case wuss_EVENT_SCROLL:
    return image_scroll(window, event->data.scroll.delta, task_data);

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
