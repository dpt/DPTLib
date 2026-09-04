/* wuss/test/tasks/text.c -- static paragraph task with a font picker */

#ifdef WUSS_APP

#include <stdlib.h>

#include <math.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "geom/point.h"
#include "io/path.h"
#include "text/bmtext.h"
#include "wuss/menu.h"

#include "text.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char paragraph[] =
"Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Donec mattis luctus libero. Donec imperdiet, velit quis venenatis iaculis, metus libero cursus ligula, egestas sagittis dui diam in mi.";

/* ----------------------------------------------------------------------- */

/* load fonts[idx] if not already in hand; returns it or NULL on failure.
 * name is the menu label for that row -- the font's leafname sans ".png". */
static bmfont_t *text_load_font(text_task_t *task,
                                const char  *resources,
                                int          idx,
                                const char  *name)
{
  const char *leaf;
  const char *filename;
  bmfont_t   *font;
  result_t    rc;

  if (task->fonts[idx] != NULL)
    return task->fonts[idx];

  leaf     = path_join_leafname(name, "png");
  filename = path_join_filename(resources, 3, "resources", "bmfonts", leaf);

  rc = bmfont_create(filename, &font);
  if (rc != result_OK)
    return NULL;

  task->fonts[idx] = font;
  return font;
}

/* switch the paragraph to the font at menu row idx (label name) */
static result_t text_set_font(text_task_t *task, int idx, const char *name)
{
  bmfont_t *font;

  if (idx < 0 || idx >= task->nfonts || idx == task->current)
    return result_OK;

  font = text_load_font(task, task->resources, idx, name);
  if (font == NULL)
    return result_OK; /* leave the current font in place */

  task->font    = font;
  task->current = idx;

  wuss_window_invalidate_all(task->window);
  return result_OK;
}

static result_t text_open_menu(text_task_t *task)
{
  return wuss_menu_open(task->delegate,
                        wuss_fontmenu_menu(task->fontmenu),
                        wuss_get_pointer(task->wuss), NULL);
}

/* ----------------------------------------------------------------------- */

result_t text_create(wuss_t      *wuss,
                     const char  *resources,
                     text_task_t *task)
{
  wuss_task_t       *delegate;
  wuss_task_desc_t   delegate_desc;
  const char        *bmfonts_dir;
  const wuss_menu_t *menu;
  size2d_t           sz;
  result_t           rc;

  task->wuss        = wuss;
  task->font        = wuss_get_font(wuss);
  task->current     = -1; /* the wuss system font is none of the picker's */
  task->bg          = colour_rgb(0xFF, 0xFF, 0xFF);
  task->fg          = colour_rgb(0x00, 0x00, 0x00);
  task->frame_count = 0;
  task->resizing    = true;

  strncpy(task->resources, resources, sizeof(task->resources) - 1);
  task->resources[sizeof(task->resources) - 1] = '\0';

  /* the picker: every ".png" font under resources/bmfonts, sorted, less any
   * SYSTEM-class font (e.g. the one wuss draws menu ticks/arrows from) */
  bmfonts_dir = path_join_filename(task->resources, 2, "resources", "bmfonts");
  rc = wuss_fontmenu_create(&task->fontmenu, bmfonts_dir, "Font", wuss, NULL);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  menu         = wuss_fontmenu_menu(task->fontmenu);
  task->nfonts = menu->nitems;
  task->fonts  = calloc((size_t) task->nfonts, sizeof(*task->fonts));
  if (task->nfonts > 0 && task->fonts == NULL)
  {
    wuss_fontmenu_destroy(task->fontmenu);
    free(task);
    return result_OOM;
  }

  delegate_desc.handle    = text_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "text";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    wuss_fontmenu_destroy(task->fontmenu);
    free(task->fonts);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  sz               = SIZE2D(220, 180);
  task->base_width  = sz.w;
  task->base_height = sz.h;

  rc = wuss_window_create_placed(delegate,
                                 sz,
                                 "Lorem Ipsum",
                                 wuss_WINDOW_NO_RESIZE_BLIT, /* paragraph reflows across the whole window, so a resize must redraw all of it, not just the newly (un)covered edge */
                                 wuss_BACKDROP_COLOUR(wuss_nearest_colour(wuss, 0xFF, 0xFF, 0xFF)),
                                 sz,
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */

  return rc;
}

#define INSET      4
#define LEADING    2
#define MAX_LINES  64 /* paragraph is short and fixed; overflow is dropped */

static result_t text_redraw(const wuss_event_t *event, void *task_data)
{
  text_task_t  *tcx;
  screen_t     *scr;
  const box_t  *bounds;
  int           sx, sy;
  bmtext_line_t lines[MAX_LINES];
  int           nlines;
  point_t       origin;

  tcx = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  nlines = bmtext_layout(tcx->font,
                         paragraph,
                         (int) strlen(paragraph),
                         (bounds->x1 - INSET) - (bounds->x0 + INSET),
                         lines,
                         MAX_LINES);

  origin.x = bounds->x0 - sx + INSET;
  origin.y = bounds->y0 - sy + INSET;

  bmtext_draw(tcx->font, scr, lines, nlines, tcx->fg, tcx->bg, LEADING, origin);

  return result_OK;
}

static result_t text_mouse(void *task_data)
{
  text_task_t *tcx;

  tcx = task_data;

  tcx->resizing = !tcx->resizing;

  return result_OK;
}

#define TEXT_RESIZE_PERIOD_FRAMES 300 /* one full swing every 5s at 60fps */
#define TEXT_RESIZE_AMPLITUDE     50  /* +/-50px either side of base_width */

static result_t text_idle(void *task_data)
{
  text_task_t *tcx;
  int          height, width;
  double       angle;
  result_t     rc;

  tcx = task_data;

  if (!tcx->resizing)
    return result_OK;

  height = tcx->base_height;

  tcx->frame_count++;
  angle = tcx->frame_count * (2.0 * M_PI / TEXT_RESIZE_PERIOD_FRAMES);
  width = tcx->base_width + (int) (TEXT_RESIZE_AMPLITUDE * sin(angle));

  rc = wuss_window_resize(tcx->window, SIZE2D(width, height));
  if (rc != result_OK)
    logf_warning("text_idle: wuss_window_resize(%d, %d) failed", width, height);

  return rc;
}

result_t text_handle(wuss_window_t      *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  text_task_t *tcx;

  tcx = task_data;

  NOT_USED(window);

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return text_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action == wuss_MOUSE_DOWN &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return text_open_menu(tcx);
    if (event->data.mouse.action != wuss_MOUSE_DOWN ||
        !(event->data.mouse.button & wuss_BUTTON_SELECT))
      return result_OK;
    return text_mouse(task_data);

  case wuss_EVENT_MENU_SELECT:
    {
      const char *name;

      name = wuss_fontmenu_selected(tcx->fontmenu, event);
      if (name != NULL)
        return text_set_font(tcx, event->data.menu_select.index, name);
    }
    return result_OK;

  case wuss_EVENT_QUIT:
    {
      int i;

      for (i = 0; i < tcx->nfonts; i++)
        if (tcx->fonts[i] != NULL)
          bmfont_destroy(tcx->fonts[i]);
      free(tcx->fonts);
      wuss_fontmenu_destroy(tcx->fontmenu);
      free(tcx); /* task_data was calloc'd per instance by the spawner */
    }
    return result_OK;

  case wuss_EVENT_IDLE:
    return text_idle(task_data);

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
