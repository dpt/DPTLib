/* wuss/test/tasks/iconbar-demo.c -- icon bar demo task */

#if defined(WUSS_APP) && defined(WUSS_ICONBAR)

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

#include "iconbar-demo.h"
#include "common.h"

static const char *const iconbar_demo_labels[3] = { "A", "B", "C" };

result_t iconbar_demo_create(wuss_t *wuss, iconbar_demo_task_t **out)
{
  result_t             rc;
  iconbar_demo_task_t *task;
  wuss_task_t         *delegate;
  wuss_task_desc_t     delegate_desc;
  int                  i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss       = wuss;
  task->open_index = -1;

  delegate_desc.handle    = iconbar_demo_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "iconbar-demo";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate;

  for (i = 0; i < (int) NELEMS(iconbar_demo_labels); i++)
  {
    wuss_iconbar_icon_spec_t spec;

    spec.text  = iconbar_demo_labels[i];
    spec.image = NULL;
    rc = wuss_iconbar_icon_create(wuss, delegate, &spec, &task->icons[i]);
    if (rc != result_OK)
    {
      wuss_task_destroy(delegate); /* unregister; its QUIT frees the task
                                    * block and any icons already made */
      return rc;
    }
  }

  if (out)
    *out = task;

  return result_OK;
}

void iconbar_demo_destroy(iconbar_demo_task_t *task)
{
  free(task);
}

static result_t iconbar_demo_redraw(iconbar_demo_task_t *task,
                                    const wuss_event_t  *event)
{
  screen_t    *scr;
  const box_t *content, *bounds;
  bmfont_t    *font;
  char         label[16];
  colour_t     bg, fg;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  bg = colour_rgb(0xFF, 0xF1, 0xE8);
  fg = colour_rgb(0x1D, 0x2B, 0x53);
  screen_fill_rect(scr, content->x0, content->y0, box_size(content), bg);

  font = wuss_get_font(task->wuss);
  if (font == NULL || task->open_index < 0)
    return result_OK;

  snprintf(label, sizeof(label), "Icon %s",
           iconbar_demo_labels[task->open_index]);

  {
    int     ascent;
    point_t pos;

    bmfont_get_info(font, NULL, NULL, &ascent, NULL);
    pos = POINT(bounds->x0 + 4, bounds->y0 + 4 + ascent);
    wuss_text_draw(task->wuss, 0, scr, label, (int) strlen(label), fg, bg,
                   &pos, NULL);
  }

  return result_OK;
}

/* Click an icon: if its window is already open, close it (matching RISC OS
 * -- clicking an already-running app's icon reactivates or closes its
 * window rather than opening a second one); otherwise open it, closing
 * whichever other icon's window was open first, since this demo only ever
 * shows one window at a time. */
static result_t iconbar_demo_icon(iconbar_demo_task_t *task,
                                  const wuss_event_t  *event)
{
  int      index, i;
  result_t rc;

  if (event->data.iconbar_icon.action != wuss_MOUSE_UP)
    return result_OK;

  index = -1;
  for (i = 0; i < (int) NELEMS(task->icons); i++)
    if (task->icons[i] == event->data.iconbar_icon.icon)
      index = i;
  if (index < 0)
    return result_OK;

  if (task->window != NULL)
  {
    int was_index;

    was_index    = task->open_index;
    wuss_window_close(task->window);
    task->window     = NULL;
    task->open_index = -1;

    if (was_index == index)
      return result_OK; /* clicking the already-open icon just closes it */
  }

  rc = task_window_create(task->delegate, SIZE2D(120, 40),
                          iconbar_demo_labels[index], &task->window);
  if (rc == result_OK)
    task->open_index = index;

  return rc;
}

result_t iconbar_demo_handle(wuss_window_t      *window,
                             const wuss_event_t *event,
                             void               *task_data)
{
  iconbar_demo_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return iconbar_demo_redraw(task, event);

  case wuss_EVENT_ICON:
    if (window != NULL)
      return result_OK; /* not this task's icon bar icons */
    return iconbar_demo_icon(task, event);

  case wuss_EVENT_CLOSE:
    if (window == task->window)
    {
      task->window     = NULL;
      task->open_index = -1;
    }
    return result_OK;

  case wuss_EVENT_QUIT:
    iconbar_demo_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP && WUSS_ICONBAR */
