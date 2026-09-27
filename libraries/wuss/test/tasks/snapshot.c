/* wuss/test/tasks/snapshot.c -- save a task window's content as a PNG */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "snapshot.h"

result_t snapshot_save_png(wuss_window_t    *window,
                           wuss_window_fn_t *handle,
                           void             *task_data,
                           const char       *filename)
{
  result_t     rc;
  box_t        bounds;
  size2d_t     size;
  void        *pixels;
  bitmap_t     bm;
  screen_t     scr;
  wuss_event_t event;

  if (window == NULL)
    return result_OK;

  wuss_window_get_content_bounds(window, &bounds);
  size = box_size(&bounds);

  /* zeroed, as not every task paints every pixel */
  pixels = calloc((size_t) size.w * (size_t) size.h, 4);
  if (pixels == NULL)
    return result_OOM;

  rc = bitmap_init(&bm, size, pixelfmt_bgrx8888, size.w * 4, NULL, pixels);
  if (rc != result_OK)
    goto cleanup;

  screen_for_bitmap(&scr, &bm);

  /* the redraw sees the whole bitmap as both the dirty and full content box */
  bounds.x0 = 0;
  bounds.y0 = 0;
  bounds.x1 = size.w;
  bounds.y1 = size.h;

  memset(&event, 0, sizeof(event));
  event.kind                = wuss_EVENT_REDRAW;
  event.data.redraw.scr     = &scr;
  event.data.redraw.content = &bounds;
  event.data.redraw.bounds  = &bounds;
  wuss_window_get_scroll(window, &event.data.redraw.scroll);

  rc = handle(window, &event, task_data);
  if (rc != result_OK)
    goto cleanup;

  rc = bitmap_save_png(&bm, filename);
  if (rc != result_OK)
    logf_warning("snapshot: saving \"%s\" failed (rc=0x%X)", filename, rc);
  else
    logf_info("snapshot: saved \"%s\"", filename);

cleanup:
  free(pixels);
  return rc;
}

/* ----------------------------------------------------------------------- */

int snapshot_is_save_key(const wuss_event_t *event)
{
  return event->kind == wuss_EVENT_KEY &&
         (event->data.key.modifiers & wuss_KEY_MOD_CTRL) &&
         (event->data.key.code == 's' || event->data.key.code == 'S');
}

#endif /* WUSS_APP */
