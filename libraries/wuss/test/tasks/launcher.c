/* launcher.c -- wuss test - clickable list of names that spawn other tasks */

#ifdef USE_SDL

#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "geom/point.h"

#include "launcher.h"

#define LAUNCHER_ROW_HEIGHT 20
#define LAUNCHER_PAD        4
#define LAUNCHER_WIDTH      140

result_t launcher_create(wuss_t           *wuss,
                         launcher_entry_t *entries,
                         int               nentries,
                         bmfont_t         *font,
                         const colour_t   *palette,
                         launcher_task_t  *task)
{
  wuss_task_t delegate;
  box_t       box;

  task->entries    = entries;
  task->nentries   = nentries;
  task->font       = font;
  task->fg         = palette[palette_PICO8_BLACK];
  task->bg         = palette[palette_PICO8_WHITE];
  task->running_fg = palette[palette_PICO8_LIGHT_GREY];

  delegate = wuss_task_start(launcher_handle, task); /* launcher_redraw paints its own background */
  box      = (box_t) BOX_POS_SIZE(10, 10, LAUNCHER_WIDTH, LAUNCHER_PAD * 2 + nentries * LAUNCHER_ROW_HEIGHT);

  return wuss_window_create(wuss,
                            &box,
                            "Launcher",
                            wuss_WINDOW_NO_CLOSE,
                            wuss_NO_BACKGROUND,
                            &delegate,
                            box.x1 - box.x0,
                            box.y1 - box.y0,
                            &task->window);
}

void launcher_destroy(launcher_task_t *task)
{
  wuss_window_close(task->window);
}

static result_t launcher_redraw(const wuss_event_t *event, void *task_data)
{
  launcher_task_t        *lc;
  screen_t                *scr;
  const box_t             *content, *bounds;
  int                      i, font_width, font_height, sx, sy;
  point_t                  pos;
  const launcher_entry_t  *entry;

  lc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_draw_rect(scr, content->x0, content->y0,
                   content->x1 - content->x0, content->y1 - content->y0,
                   lc->bg);

  bmfont_get_info(lc->font, &font_width, &font_height);
  NOT_USED(font_width);

  for (i = 0; i < lc->nentries; i++)
  {
    entry = &lc->entries[i];

    pos.x = bounds->x0 - sx + LAUNCHER_PAD;
    pos.y = bounds->y0 - sy + LAUNCHER_PAD + i * LAUNCHER_ROW_HEIGHT + (LAUNCHER_ROW_HEIGHT - font_height) / 2;

    bmfont_draw(lc->font, scr, entry->name, (int) strlen(entry->name),
               entry->running ? lc->running_fg : lc->fg, lc->bg, &pos, NULL);
  }

  return result_OK;
}

static result_t launcher_mouse(wuss_window_t *window, int y, void *task_data)
{
  launcher_task_t  *lc;
  int               i;
  point_t           scroll;
  launcher_entry_t *entry;
  result_t          rc;

  lc = task_data;

  wuss_window_get_scroll(window, &scroll);
  NOT_USED(scroll.x);

  i = (y + scroll.y - LAUNCHER_PAD) / LAUNCHER_ROW_HEIGHT;
  if (i < 0 || i >= lc->nentries)
    return result_OK;

  entry = &lc->entries[i];
  if (entry->running)
    return result_OK;

  rc = entry->spawn();
  if (rc == result_OK)
  {
    entry->running = true;
    wuss_window_invalidate_all(window);
  }

  return rc;
}

result_t launcher_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return launcher_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return launcher_mouse(window, event->data.mouse.point.y, task_data);

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
