/* wuss/test/tasks/image.h -- static bitmap image task */

#ifndef TASKS_IMAGE_H
#define TASKS_IMAGE_H

#ifdef USE_SDL

#include "framebuf/bitmap.h"
#include "wuss/window.h"

/* window's task: a loaded PNG, alpha-tested against the window
 * background (this test screen is paletted, so fully-transparent source
 * pixels are skipped and everything else is drawn at full strength) */
typedef struct image_task
{
  wuss_window_t *window;
  bitmap_t       bitmap;    /* owned: base freed by the caller when done */
  bitmap_t       ninepatch; /* owned: 9-patch tiled behind the main image */
}
image_task_t;

wuss_event_fn_t image_handle;

/* load the PNG at path (and the 9-patch PNG at background_path, drawn tiled
 * behind it) and create its window against the given wuss instance */
result_t image_create(wuss_t         *wuss,
                      const colour_t *palette,
                      const char     *path,
                      const char     *background_path,
                      image_task_t   *task);


#endif /* USE_SDL */

#endif /* TASKS_IMAGE_H */
