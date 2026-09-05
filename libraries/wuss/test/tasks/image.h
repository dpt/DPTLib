/* wuss/test/tasks/image.h -- static bitmap image task */

#ifndef TASKS_IMAGE_H
#define TASKS_IMAGE_H

#ifdef WUSS_APP

#include "framebuf/bitmap.h"
#include "wuss/window.h"

#define IMAGE_MAX_NAMES 64
#define IMAGE_MAX_NAME_LEN 64

/* window's task: a loaded PNG, alpha-tested against the window
 * background (this test screen is paletted, so fully-transparent source
 * pixels are skipped and everything else is drawn at full strength) */
typedef struct image_task
{
  wuss_window_t *window;
  bitmap_t       bitmap;    /* owned: base freed by the caller when done */
  bitmap_t       ninepatch; /* owned: 9-patch tiled behind the main image */
  const char    *resources; /* root passed to path_join_filename on click */
  int            index;     /* current position in names[] */
  char           names[IMAGE_MAX_NAMES][IMAGE_MAX_NAME_LEN]; /* owned:
                              * leafnames (extension stripped) found under
                              * resources/images at spawn time */
  int            nnames;
}
image_task_t;

wuss_window_fn_t image_handle;

/* load the PNG at path (and the 9-patch PNG at background_path, drawn tiled
 * behind it) and create its window against the given wuss instance.
 * resources is the root resource directory, kept so a click can cycle
 * through resources/images (PNG files) by reloading a different leafname. */
result_t image_create(wuss_t       *wuss,
                      const char   *resources,
                      const char   *path,
                      const char   *background_path,
                      image_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_IMAGE_H */
