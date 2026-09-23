/* wuss/test/tasks/keys.c -- key input demo task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/box.h"
#include "wuss/task.h"

#include "keys.h"

result_t keys_create(wuss_t *wuss, keys_task_t **out)
{
  result_t         rc;
  keys_task_t     *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->font = wuss_get_font_n(wuss, 0);
  task->bg   = colour_rgb(0xFF, 0xFF, 0xFF);
  task->fg   = colour_rgb(0x00, 0x00, 0x00);
  task->key  = -1;

  delegate_desc.handle    = keys_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "keys";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(160, 60),
                                 "Keys",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(160, 60),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* its QUIT frees the task block */
    return rc;
  }

  if (out)
    *out = task;

  return result_OK;
}

void keys_destroy(keys_task_t *task)
{
  free(task);
}

/* describe a key code: the character itself if printable ASCII, a name for
 * the wuss_KEY_* specials and control keys, otherwise U+hex */
static void keys_describe(int key, char *buf, size_t bufsz)
{
  static const char *const specials[] =
  {
    "Up", "Down", "Left", "Right", "Home", "End", "PageUp", "PageDown",
    "Insert", "Delete"
  };

  if (key < 0)
    snprintf(buf, bufsz, "(none)");
  else if (key >= wuss_KEY_UP && key < wuss_KEY_UP + (int) NELEMS(specials))
    snprintf(buf, bufsz, "%s", specials[key - wuss_KEY_UP]);
  else if (key >= wuss_KEY_F1 && key <= wuss_KEY_F12)
    snprintf(buf, bufsz, "F%d", key - wuss_KEY_F1 + 1);
  else if (key == 13)
    snprintf(buf, bufsz, "Return");
  else if (key == 8)
    snprintf(buf, bufsz, "Backspace");
  else if (key == 9)
    snprintf(buf, bufsz, "Tab");
  else if (key == 27)
    snprintf(buf, bufsz, "Escape");
  else if (key >= 0x20 && key < 0x7F)
    snprintf(buf, bufsz, "'%c'", key);
  else
    snprintf(buf, bufsz, "U+%04X", key);
}

static result_t keys_redraw(wuss_window_t      *window,
                            const wuss_event_t *event,
                            keys_task_t        *kt)
{
  screen_t    *scr;
  const box_t *content, *bounds;
  char         name[32];
  char         lines[3][48];
  int          fh, ascent;
  int          i;
  point_t      pos;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  screen_fill_rect(scr, content->x0, content->y0,
                   SIZE2D(content->x1 - content->x0,
                          content->y1 - content->y0),
                   kt->bg);

  keys_describe(kt->key, name, sizeof(name));
  snprintf(lines[0], sizeof(lines[0]), "Key: %s", name);
  snprintf(lines[1], sizeof(lines[1]), "Mods:%s%s%s",
           (kt->mods & wuss_KEY_MOD_SHIFT) ? " Shift" : "",
           (kt->mods & wuss_KEY_MOD_CTRL)  ? " Ctrl"  : "",
           (kt->mods & wuss_KEY_MOD_ALT)   ? " Alt"   : "");
  snprintf(lines[2], sizeof(lines[2]), "Focus: %s",
           (wuss_get_focus(kt->wuss) == window)
             ? "yes" : "no (click)");

  bmfont_get_info(kt->font, NULL, &fh, &ascent, NULL);
  for (i = 0; i < (int) NELEMS(lines); i++)
  {
    pos.x = bounds->x0 + 4;
    pos.y = bounds->y0 + 4 + i * fh + ascent;
    bmfont_draw(kt->font, scr, lines[i], (int) strlen(lines[i]), kt->fg,
                colour_rgba(0, 0, 0, 0), NULL, &pos, NULL);
  }

  return result_OK;
}

result_t keys_handle(wuss_window_t      *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  keys_task_t *kt;

  kt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return keys_redraw(window, event, kt);

  case wuss_EVENT_KEY:
    /* pass the driver's F-key hotkeys through so they keep working */
    if (event->data.key.code >= wuss_KEY_F1 &&
        event->data.key.code <= wuss_KEY_F12)
      return result_WUSS_KEY_UNCLAIMED;

    kt->key  = event->data.key.code;
    kt->mods = event->data.key.modifiers;
    wuss_window_invalidate_visible(window);
    return result_OK;

  case wuss_EVENT_GAIN_FOCUS:
  case wuss_EVENT_LOSE_FOCUS:
    wuss_window_invalidate_visible(window);
    return result_OK;

  case wuss_EVENT_QUIT:
    keys_destroy(kt);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
