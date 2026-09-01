/* wuss/test/tasks/launcher.c -- clickable list of names that spawn other tasks */

#ifdef USE_SDL

#include <assert.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "wuss/icon.h"

#include "launcher.h"

#define LAUNCHER_ROW_HEIGHT 22
#define LAUNCHER_PAD        4
#define LAUNCHER_WIDTH      160

result_t launcher_create(wuss_t                 *wuss,
                         const launcher_entry_t *entries,
                         int                     nentries,
                         launcher_task_t        *task)
{
  wuss_task_t      delegate;
  wuss_icon_spec_t specs[LAUNCHER_MAX_ENTRIES];
  size2d_t         sz;
  int              i;
  result_t         rc;

  assert(nentries <= LAUNCHER_MAX_ENTRIES);

  task->entries  = entries;
  task->nentries = nentries;
  task->window   = NULL;
  memset(task->icons, 0, sizeof(task->icons));

  delegate = wuss_task_start(launcher_handle, task);
  sz       = SIZE2D(LAUNCHER_WIDTH,
                    LAUNCHER_PAD * 2 + nentries * LAUNCHER_ROW_HEIGHT);

  rc = wuss_window_create_placed(wuss,
                                 sz,
                                 "Launcher",
                                 wuss_WINDOW_NO_CLOSE,
                                 wuss_BACKDROP_COLOUR(palette_PICO8_LIGHT_GREY),
                                 &delegate,
                                 sz,
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    return rc;

  memset(specs, 0, sizeof(specs));

  for (i = 0; i < nentries; i++)
  {
    specs[i].bbox = (box_t) BOX_POS_SIZE(LAUNCHER_PAD,
                                         LAUNCHER_PAD + i * LAUNCHER_ROW_HEIGHT,
                                         LAUNCHER_WIDTH - LAUNCHER_PAD * 2,
                                         LAUNCHER_ROW_HEIGHT - 2);
    specs[i].type = wuss_ICON_TYPE_BUTTON;
    specs[i].text = entries[i].name;
    specs[i].fg   = palette_PICO8_BLACK;
    specs[i].bg   = palette_PICO8_LIGHT_GREY;
  }

  rc = wuss_icon_create_array(task->window, specs, nentries, task->icons);
  if (rc != result_OK)
  {
    wuss_window_close(task->window);
    task->window = NULL;
    return rc;
  }

  return result_OK;
}

void launcher_destroy(launcher_task_t *task)
{
  wuss_window_close(task->window);
}

static result_t launcher_icon(launcher_task_t *lc, const wuss_icon_t *icon)
{
  int i;

  for (i = 0; i < lc->nentries; i++)
    if (lc->icons[i] == icon)
      return lc->entries[i].spawn();

  return result_OK;
}

result_t launcher_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  NOT_USED(window);

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    if (event->data.icon.action != wuss_MOUSE_DOWN)
      return result_OK;
    return launcher_icon(task_data, event->data.icon.icon);

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
