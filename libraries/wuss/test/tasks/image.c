/* wuss/test/tasks/image.c -- static bitmap image task */

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

result_t image_create(wuss_t       *wuss,
                      const char   *path,
                      const char   *background_path,
                      image_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t    rc;
  size2d_t    sz;

  rc = bitmap_load_png(&task->bitmap, path);
  if (rc != result_OK)
    return rc;

  rc = bitmap_load_png(&task->ninepatch, background_path);
  if (rc != result_OK)
  {
    free(task->bitmap.base);
    return rc;
  }

  /* shows through the image's transparent pixels */
  delegate_desc.handle    = image_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "image";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
    return rc;

  sz.w = task->bitmap.size.w + BORDER * 2;
  sz.h = task->bitmap.size.h + BORDER * 2;

  return wuss_window_create_placed(delegate,
                                   /* shorter than the bitmap so there's something to scroll through */
                                   SIZE2D(sz.w, sz.h * 2 / 3),
                                   "Image",
                                   wuss_WINDOW_NONE,
                                   wuss_BACKDROP_COLOUR(palette_PICO8_PINK),
                                   sz,
                                   SIZE2D(32, 32),
                                   &task->window);
}

static result_t image_redraw(const wuss_event_t *event, void *task_data)
{
  image_task_t *ic;
  screen_t     *scr;
  const box_t  *bounds;
  int           sx, sy;
  int           bx, by;
  box_t         behind;

  ic = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;
  bx     = bounds->x0 - sx + BORDER;
  by     = bounds->y0 - sy + BORDER;

#define NINEPATCHSZ 9

  behind.x0 = bx - NINEPATCHSZ;
  behind.y0 = by - NINEPATCHSZ;
  behind.x1 = behind.x0 + ic->bitmap.size.w + NINEPATCHSZ * 2;
  behind.y1 = behind.y0 + ic->bitmap.size.h + NINEPATCHSZ * 2;
  screen_draw_ninepatch(scr, &behind, &ic->ninepatch, 0);

  screen_draw_bitmap(scr, bx, by, &ic->bitmap);

  return result_OK;
}

result_t image_handle(wuss_window_t      *window,
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
    free(ic->bitmap.base);
    free(ic->ninepatch.base);
    free(ic); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
