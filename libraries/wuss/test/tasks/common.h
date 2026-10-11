/* wuss/test/tasks/common.h -- helpers shared by the wuss demo tasks */

#ifndef TASKS_COMMON_H
#define TASKS_COMMON_H

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "geom/size.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

/* braced initialiser for a wuss_proginfo_desc_t, filling in the author and
 * version lines every demo task shares */
#define TASK_PROGINFO_DESC(name, purpose) \
  { name, purpose, "© DPTLib contributors", "1.0 (" __DATE__ ")" }

/* bitmap_load_png keeps a palette-type or greyscale PNG paletted, so convert one
 * back to a deep format here for screen_copy_bitmap and
 * screen_copy_ninepatch. Callers wanting an untouched load (e.g. to inspect
 * the palette) pass pixelfmt_unknown to skip conversion. */
static inline result_t load_png_deep(bitmap_t   *bm,
                                     const char *filename,
                                     pixelfmt_t  fmt)
{
  result_t  rc;
  bitmap_t *deep;

  rc = bitmap_load_png(bm, filename);
  if (rc != result_OK)
    return rc;

  if (fmt == pixelfmt_unknown || bm->format < pixelfmt_p1 ||
      bm->format > pixelfmt_p8)
    return result_OK;

  rc = bitmap_convert(bm, fmt, &deep);
  if (rc != result_OK)
  {
    free(bm->base);
    return rc;
  }

  free(bm->base);
  free(bm->palette);
  *bm = *deep;
  free(deep);

  return result_OK;
}

/* create a focusable window with no backdrop whose document is exactly size,
 * as most demo tasks want */
static inline result_t task_window_create(wuss_task_t    *task,
                                          size2d_t        size,
                                          const char     *title,
                                          wuss_window_t **window)
{
  return wuss_window_create_placed(task,
                                   size,
                                   title,
                                   wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                   wuss_NO_BACKDROP,
                                   size,
                                   SIZE2D(0, 0),
                                   window);
}

/* offer a key event to menu's shortcuts. returns non-zero if the task should
 * handle the key itself: no shortcut claimed it and neither Ctrl nor Alt is
 * held. otherwise returns zero with *rc set to what the handler returns */
static inline int task_key_is_plain(wuss_task_t        *task,
                                    const wuss_menu_t  *menu,
                                    const wuss_event_t *key,
                                    result_t           *rc)
{
  *rc = wuss_menu_dispatch_shortcut(task, menu, key);
  if (*rc != result_WUSS_KEY_UNCLAIMED)
    return 0;

  return (key->data.key.modifiers &
          (wuss_KEY_MOD_CTRL | wuss_KEY_MOD_ALT)) == 0;
}

/* pixel width of text in the desktop font, for sizing stack leaves */
static inline int task_text_width(const wuss_t *wuss, const char *text)
{
  bmfont_width_t width;

  bmfont_measure(wuss_get_font(wuss), text, (int) strlen(text), NULL,
                 INT_MAX, NULL, &width);

  return width;
}

/* smallest width of a button labelled text inside a border of the given
 * width: the text plus a space either side */
static inline int task_button_width(const wuss_t *wuss,
                                    const char   *text,
                                    int           border)
{
  return task_text_width(wuss, text) + 2 * task_text_width(wuss, " ") +
         2 * border;
}

#endif /* TASKS_COMMON_H */
