/* wuss/test/tasks/display.c -- desktop resolution picker task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

#include "tasks.h" /* app_resize */

#include "display.h"

enum { DISPLAY_MENU_INFO };

/* fixed set of resolutions the picker offers; app_resize rejects (via
 * wuss_frontend_resize) anything a backend can't honour, e.g. RISC OS's
 * fixed screen mode */
static const size2d_t g_display_resolutions[DISPLAY_MAX_RESOLUTIONS] =
{
  { 640,  480  },
  { 800,  600  },
  { 1024, 768  },
  { 1152, 864  },
  { 1280, 720  },
  { 1280, 800  },
  { 1366, 768  },
  { 1920, 1080 }
};

/* WUSS_MENU_ITEM stores text_ as a bare pointer, not a copy, so each label
 * needs storage that outlives the loop that builds the menu -- static
 * strings paired 1:1 with g_display_resolutions, rather than a stack buffer
 * reused (and overwritten) on every iteration */
static const char *const g_display_labels[DISPLAY_MAX_RESOLUTIONS] =
{
  "640x480",
  "800x600",
  "1024x768",
  "1152x864",
  "1280x720",
  "1280x800",
  "1366x768",
  "1920x1080"
};

static result_t display_create_window(display_task_t *task,
                                      wuss_task_t    *delegate)
{
  return wuss_window_create_placed(delegate,
                                   SIZE2D(160, 40),
                                   "Display",
                                   wuss_WINDOW_DEFAULT,
                                   wuss_NO_BACKDROP,
                                   SIZE2D(160, 40),
                                   SIZE2D(0, 0),
                                   &task->window);
}

result_t display_create(wuss_t *wuss, display_task_t **out)
{
  result_t         rc;
  display_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;

  delegate_desc.handle    = display_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "display";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = display_create_window(task, delegate);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, DISPLAY_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * display_mouse */
  for (i = 0; i < DISPLAY_MAX_RESOLUTIONS; i++)
    WUSS_MENU_ITEM(task->menu_items, 1 + i, g_display_labels[i],
                  wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->menu, "Display", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void display_destroy(display_task_t *task)
{
  wuss_menu_close(task->menu_handle);
  free(task);
}

static void display_draw_centred(bmfont_t   *font,
                                 screen_t   *scr,
                                 const char *text,
                                 int         x,
                                 int         y,
                                 colour_t    fg)
{
  bmfont_width_t width;
  point_t        pos;
  int            len, fh, ascent;

  len = (int) strlen(text);
  bmfont_measure(font, text, len, NULL, INT_MAX, NULL, &width);
  bmfont_get_info(font, NULL, &fh, &ascent, NULL);

  pos.x = x - width / 2;
  pos.y = y - fh / 2 + ascent;
  bmfont_draw(font, scr, text, len, fg, colour_rgba(0, 0, 0, 0), NULL, &pos,
             NULL);
}

static result_t display_redraw(display_task_t     *dc,
                               const wuss_event_t *event)
{
  screen_t    *scr;
  const box_t *content, *bounds;
  size2d_t     size;
  char         label[32];
  colour_t     bg, fg;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  bg      = colour_rgb(0x1D, 0x2B, 0x53);
  fg      = colour_rgb(0xFF, 0xF1, 0xE8);

  screen_fill_rect(scr, content->x0, content->y0, box_size(content), bg);

  size = wuss_get_screen_size(dc->wuss);
  snprintf(label, sizeof(label), "%dx%d", size.w, size.h);
  display_draw_centred(wuss_get_font_n(dc->wuss, 0), scr, label,
                       (bounds->x0 + bounds->x1) / 2 - event->data.redraw.scroll.x,
                       (bounds->y0 + bounds->y1) / 2 - event->data.redraw.scroll.y,
                       fg);

  return result_OK;
}

static result_t display_mouse(display_task_t *dc, wuss_button_t button)
{
  if (!(button & wuss_BUTTON_MENU))
    return result_OK;

  {
    static const wuss_proginfo_desc_t desc =
    {
      "Display",
      "Change the desktop resolution",
      "(c) DPTLib contributors",
      "1.0 (" __DATE__ ")"
    };
    wuss_proginfo_set_desc(&desc);
    dc->menu_items[DISPLAY_MENU_INFO].window = wuss_proginfo_window(dc->delegate);
  }

  return wuss_menu_open(dc->delegate, &dc->menu, wuss_get_pointer(dc->wuss),
                        &dc->menu_handle);
}

static result_t display_menu_select(display_task_t     *dc,
                                    const wuss_event_t *event)
{
  int index;

  index = event->data.menu_select.index;

  if (!wuss_menu_should_keep_open(event))
    dc->menu_handle = NULL;

  if (event->data.menu_select.menu != &dc->menu)
    return result_OK;
  if (index < 1 || index > DISPLAY_MAX_RESOLUTIONS)
    return result_OK;

  return app_resize(g_display_resolutions[index - 1]);
}

result_t display_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  display_task_t *dc;

  dc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return display_redraw(dc, event);

  case wuss_EVENT_MOUSE:
    if (window != dc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return display_mouse(dc, event->data.mouse.button);

  case wuss_EVENT_MENU_SELECT:
    return display_menu_select(dc, event);

  case wuss_EVENT_MENU_CLOSED:
    dc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == dc->menu_items[DISPLAY_MENU_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK)
      return rc;
    if (event->data.pre_show.handle == NULL)
      return result_OK;
    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_CLOSE:
    if (window == dc->window)
      dc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    display_destroy(dc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
