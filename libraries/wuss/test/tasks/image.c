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

result_t image_create(wuss_t *wuss, const colour_t *palette, const char *resources, image_task_t *task)
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

  delegate = wuss_task_make(image_redraw, NULL, task, palette_PICO8_BLACK); /* shows through the image's transparent pixels */
  box      = (box_t) BOX_POS_SIZE(370, 10, task->bitmap.width, task->bitmap.height);

  return wuss_window_create(wuss, &box, "Image", wuss_WINDOW_NONE, &delegate, &task->window);
}

void image_destroy(image_task_t *task)
{
  wuss_window_destroy(task->window);
  free(task->bitmap.base);
}

result_t image_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *task_data)
{
  image_task_t *ic;

  NOT_USED(window);

  ic = task_data;

  screen_draw_bitmap(scr, content->x0, content->y0, &ic->bitmap);

  return result_OK;
}

#endif /* USE_SDL */
