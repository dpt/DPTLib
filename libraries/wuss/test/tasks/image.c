/* wuss/test/tasks/image.c -- static bitmap image task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "io/dirscan.h"
#include "io/path.h"

#include "image.h"

#define NINEPATCHSZ 9
#define IMAGE_EXT     ".png"
#define IMAGE_EXT_LEN 4

static result_t image__scan_entry(const char *leaf, void *opaque)
{
  image_task_t *ic;
  size_t        leaflen;

  ic      = opaque;
  leaflen = strlen(leaf);

  if (leaflen <= IMAGE_EXT_LEN ||
      leaflen - IMAGE_EXT_LEN >= IMAGE_MAX_NAME_LEN)
    return result_OK;
  if (strcmp(leaf + leaflen - IMAGE_EXT_LEN, IMAGE_EXT) != 0)
    return result_OK;
  if (ic->nnames >= IMAGE_MAX_NAMES)
    return result_STOP_WALK;

  memcpy(ic->names[ic->nnames], leaf, leaflen - IMAGE_EXT_LEN);
  ic->names[ic->nnames][leaflen - IMAGE_EXT_LEN] = '\0';
  ic->nnames++;

  return result_OK;
}

result_t image_create(wuss_t       *wuss,
                      const char   *resources,
                      const char   *path,
                      const char   *background_path,
                      image_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  const char      *images_dir;
  result_t    rc;
  size2d_t    sz;

  task->resources = resources;
  task->index     = 0;
  task->nnames    = 0;

  images_dir = path_join_filename(resources, 2, "resources", "images");
  rc = dirscan_walk(images_dir, image__scan_entry, task);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  rc = bitmap_load_png(&task->bitmap, path);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  rc = bitmap_load_png(&task->ninepatch, background_path);
  if (rc != result_OK)
  {
    free(task->bitmap.base);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  /* shows through the image's transparent pixels */
  delegate_desc.handle    = image_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "image";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task->bitmap.base);
    free(task->ninepatch.base);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);

  sz.w = task->bitmap.size.w + NINEPATCHSZ * 2;
  sz.h = task->bitmap.size.h + NINEPATCHSZ * 2;

  rc = wuss_window_create_placed(delegate,
                                 /* shorter than the bitmap so there's something to scroll through */
                                 SIZE2D(sz.w, sz.h * 2 / 3),
                                 "Image",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_COLOUR(palette_PICO8_PINK),
                                 sz,
                                 SIZE2D(32, 32),
                                 &task->window);
  if (rc != result_OK)
    wuss_task_destroy(delegate); /* QUIT frees the two bitmaps and the block */

  return rc;
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
  bx     = bounds->x0 - sx + NINEPATCHSZ;
  by     = bounds->y0 - sy + NINEPATCHSZ;

  behind.x0 = bx - NINEPATCHSZ;
  behind.y0 = by - NINEPATCHSZ;
  behind.x1 = behind.x0 + ic->bitmap.size.w + NINEPATCHSZ * 2;
  behind.y1 = behind.y0 + ic->bitmap.size.h + NINEPATCHSZ * 2;
  screen_copy_ninepatch(scr, &behind, &ic->ninepatch, 0);

  screen_copy_bitmap(scr, bx, by, &ic->bitmap);

  return result_OK;
}

static result_t image_click(wuss_window_t *window,
                            image_task_t  *ic,
                            int            step)
{
  const char *leafname;
  const char *filename;
  char        buf[DPTLIB_MAXPATH];
  bitmap_t    next;
  size2d_t    sz;
  result_t    rc;

  if (ic->nnames == 0)
    return result_OK; /* nothing to cycle to */

  ic->index = (ic->index + step + ic->nnames) % ic->nnames;

  leafname = path_join_leafname(ic->names[ic->index], "png");
  filename = path_join_filename(ic->resources, 3, "resources", "images",
                                leafname);
  strcpy(buf, filename);

  rc = bitmap_load_png(&next, buf);
  if (rc != result_OK)
  {
    logf_warning("image: skipping \"%s\" (rc=0x%X)", buf, rc);
    return result_OK; /* keep showing the current image */
  }

  free(ic->bitmap.base);
  ic->bitmap = next;

  sz.w = ic->bitmap.size.w + NINEPATCHSZ * 2;
  sz.h = ic->bitmap.size.h + NINEPATCHSZ * 2;
  return wuss_window_set_doc(window, sz);
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

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      return image_click(window, ic, 1);
    if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      return image_click(window, ic, -1);
    return result_OK;

  case wuss_EVENT_QUIT:
    free(ic->bitmap.base);
    free(ic->ninepatch.base);
    free(ic); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
