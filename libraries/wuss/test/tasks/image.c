/* image.c -- wuss test - static bitmap image task */

#ifdef USE_SDL

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "image.h"

#define BORDER 16

result_t image_create(wuss_t         *wuss,
                      const colour_t *palette,
                      const char     *path,
                      image_task_t   *task)
{
  wuss_task_t delegate;
  box_t       box;
  result_t    rc;
  size2d_t    sz;

  rc = bitmap_load_png(&task->bitmap, path);
  if (rc != result_OK)
    return rc;

  delegate = wuss_task_start(image_handle, task); /* shows through the image's transparent pixels */
  
  sz.w = task->bitmap.size.w + BORDER * 2;
  sz.h = task->bitmap.size.h + BORDER * 2;
  
  /* shorter than the bitmap so there's something to scroll through */
  box = (box_t) BOX_POS_SIZE(370, 10, sz.w, sz.h * 2 / 3);

  return wuss_window_create(wuss,
                            &box,
                            "Image",
                            wuss_WINDOW_NONE,
                            palette_PICO8_PINK,
                            &delegate,
                            sz,
                            SIZE2D(32, 32),
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

  screen_draw_bitmap(scr, bounds->x0 - sx + BORDER, bounds->y0 - sy + BORDER, &ic->bitmap);

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
