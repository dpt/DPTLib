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

  delegate = wuss_task_start(image_handle, task); /* shows through the image's transparent pixels */
  /* shorter than the bitmap so there's something to scroll through */
  box             = (box_t) BOX_POS_SIZE(370, 10, task->bitmap.size.w, task->bitmap.size.h * 2 / 3);

  return wuss_window_create(wuss,
                            &box,
                            "Image",
                            wuss_WINDOW_NONE,
                            palette_PICO8_BLACK,
                            &delegate,
                            (size2d_t) { task->bitmap.size.w, task->bitmap.size.h },
                            &task->window);
}

void image_destroy(image_task_t *task)
{
  wuss_window_close(task->window);
  free(task->bitmap.base);
}

static result_t image_redraw(const wuss_event_t *event, void *task_data)
{
  image_task_t *ic;
  screen_t     *scr;
  const box_t  *bounds;
  int           sx, sy;

  ic = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  screen_draw_bitmap(scr, bounds->x0 - sx, bounds->y0 - sy, &ic->bitmap);

  return result_OK;
}

result_t image_handle(wuss_window_t     *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  image_task_t *ic;

  ic = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return image_redraw(event, task_data);

  case wuss_EVENT_CLOSE:
    wuss_window_close(window);
    ic->window = NULL;
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
