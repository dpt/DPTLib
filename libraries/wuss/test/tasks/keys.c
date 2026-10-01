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
#include "common.h"

/* MENU click pops this menu; the item table lives per-instance in
 * keys_task_t so each window's Info row can hold its own .window pointer to
 * the shared proginfo singleton, retargeted just before wuss_menu_open */
enum
{
  KEYS_MENU_INFO = 0,
  KEYS_MENU_CLEAR
};

result_t keys_create(wuss_t *wuss, keys_task_t **out)
{
  result_t         rc;
  keys_task_t     *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  int              fh;
  size2d_t         size;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->font = wuss_get_font_n(wuss, 0);
  task->bg   = colour_rgb(0xFF, 0xFF, 0xFF);
  task->fg   = colour_rgb(0x00, 0x00, 0x00);

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
  task->delegate = delegate;

  /* a Focus line, then one line per history entry */
  bmfont_get_info(task->font, NULL, &fh, NULL, NULL);
  size = SIZE2D(160, 8 + (1 + KEYS_HISTORY) * fh);

  rc = task_window_create(delegate, size, "Keys", &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, KEYS_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in keys_mouse */

  WUSS_MENU_ITEM(task->menu_items, KEYS_MENU_CLEAR, "Clear",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->menu, "Keys", task->menu_items,
                 NELEMS(task->menu_items));

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

  if (key >= wuss_KEY_UP && key < wuss_KEY_UP + (int) NELEMS(specials))
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
  char         lines[1 + KEYS_HISTORY][48];
  int          nlines;
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

  snprintf(lines[0], sizeof(lines[0]), "Focus: %s",
           (wuss_get_focus(kt->wuss) == window)
             ? "yes" : "no (click)");
  nlines = 1;

  if (kt->nkeys == 0)
    snprintf(lines[nlines++], sizeof(lines[0]), "Keys: (none)");

  for (i = 0; i < kt->nkeys; i++)
  {
    keys_describe(kt->keys[i], name, sizeof(name));
    snprintf(lines[nlines++], sizeof(lines[0]), "%s%s%s%s",
             (kt->mods[i] & wuss_KEY_MOD_SHIFT) ? "Shift+" : "",
             (kt->mods[i] & wuss_KEY_MOD_CTRL)  ? "Ctrl+"  : "",
             (kt->mods[i] & wuss_KEY_MOD_ALT)   ? "Alt+"   : "",
             name);
  }

  bmfont_get_info(kt->font, NULL, &fh, &ascent, NULL);
  for (i = 0; i < nlines; i++)
  {
    pos.x = bounds->x0 + 4;
    pos.y = bounds->y0 + 4 + i * fh + ascent;
    bmfont_draw(kt->font, scr, lines[i], (int) strlen(lines[i]), kt->fg,
                colour_rgba(0, 0, 0, 0), NULL, &pos, NULL);
  }

  return result_OK;
}

static result_t keys_mouse(keys_task_t *kt, const wuss_event_t *event)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("Keys", "Key input and focus demo");

  if (event->data.mouse.action != wuss_MOUSE_DOWN ||
      !(event->data.mouse.button & wuss_BUTTON_MENU))
    return result_OK;

  wuss_proginfo_set_desc(&desc);
  kt->menu_items[KEYS_MENU_INFO].window = wuss_proginfo_window(kt->delegate);

  /* Clear has nothing to do with an empty history */
  if (kt->nkeys > 0)
    kt->menu_items[KEYS_MENU_CLEAR].flags &= ~wuss_MENU_ITEM_DISABLED;
  else
    kt->menu_items[KEYS_MENU_CLEAR].flags |= wuss_MENU_ITEM_DISABLED;

  return wuss_menu_open_at_pointer(kt->delegate, &kt->menu, &kt->menu_handle);
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

    /* push onto the front of the history, dropping the oldest when full */
    kt->nkeys = MIN(kt->nkeys + 1, KEYS_HISTORY);
    memmove(&kt->keys[1], &kt->keys[0],
            (size_t) (kt->nkeys - 1) * sizeof(kt->keys[0]));
    memmove(&kt->mods[1], &kt->mods[0],
            (size_t) (kt->nkeys - 1) * sizeof(kt->mods[0]));
    kt->keys[0] = event->data.key.code;
    kt->mods[0] = event->data.key.modifiers;
    wuss_window_invalidate_visible(window);
    return result_OK;

  case wuss_EVENT_MOUSE:
    if (window != kt->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    return keys_mouse(kt, event);

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &kt->menu &&
        event->data.menu_select.index == KEYS_MENU_CLEAR &&
        kt->window != NULL)
    {
      kt->nkeys = 0;
      wuss_window_invalidate_visible(kt->window);
    }
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    kt->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == kt->window)
      kt->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == kt->menu_items[KEYS_MENU_INFO].window)
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
