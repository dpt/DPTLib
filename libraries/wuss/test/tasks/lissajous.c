/* wuss/test/tasks/lissajous.c -- Lissajous figure task */

#ifdef WUSS_APP

#include <math.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "io/filetype.h"

#include "lissajous.h"
#include "common.h"
#include "snapshot.h"

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in lissajous_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum
{
  LISSAJOUS_MENU_INFO = 0,
  LISSAJOUS_MENU_BACKGROUND,
  LISSAJOUS_MENU_FOREGROUND,
  LISSAJOUS_MENU_PAUSE,
  LISSAJOUS_MENU_RATIO,
  LISSAJOUS_MENU_SAVE
};

#define LISSAJOUS_SAVE_NAME "lissajous.png" /* Save As's initial leafname */

/* frequency pairs cycled by a Select click and listed in Menu > Ratio */
static const struct
{
  int         a, b;
  const char *name;
}
lissajous_freqs[LISSAJOUS_NFREQS] =
{
  { 3, 2, "3:2" }, { 5, 4, "5:4" }, { 3, 4, "3:4" },
  { 5, 6, "5:6" }, { 1, 2, "1:2" }, { 7, 4, "7:4" },
  { 1, 1, "1:1" }, { 1, 3, "1:3" }, { 2, 3, "2:3" },
  { 3, 5, "3:5" }, { 4, 5, "4:5" }, { 7, 6, "7:6" }
};

/* wuss_saveas_save_fn_t: opaque is the lissajous_task_t */
static result_t lissajous_saveas_save(const char *path, void *opaque)
{
  lissajous_task_t *lc;

  lc = opaque;

  return snapshot_save_png(lc->window, lissajous_handle, lc, path);
}

result_t lissajous_create(wuss_t *wuss, lissajous_task_t **out)
{
  result_t          rc;
  lissajous_task_t *task;
  wuss_task_t      *delegate;
  wuss_task_desc_t  delegate_desc;
  filetype_t        png_type;
  int               i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss       = wuss;
  task->bg         = colour_rgb(0x00, 0x00, 0x00);
  task->fg         = colour_rgb(0x00, 0xFF, 0x00);
  task->freq_index = 0;
  task->a          = lissajous_freqs[0].a;
  task->b          = lissajous_freqs[0].b;
  task->phase      = 0.0;
  task->drift      = 0.01;

  /* redraw paints its own background every frame */
  delegate_desc.handle    = lissajous_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "lissajous";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate,
                          SIZE2D(220, 220),
                          "Lissajous",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, wuss, &png_type,
                          LISSAJOUS_SAVE_NAME, lissajous_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, LISSAJOUS_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * lissajous_mouse */

  WUSS_MENU_ITEM_MENU(task->menu_items, LISSAJOUS_MENU_BACKGROUND, "Background",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  WUSS_MENU_ITEM_MENU(task->menu_items, LISSAJOUS_MENU_FOREGROUND, "Foreground",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, LISSAJOUS_MENU_PAUSE, "Pause",
                          wuss_MENU_ITEM_NONE, "SPACE");

  for (i = 0; i < LISSAJOUS_NFREQS; i++)
    WUSS_MENU_ITEM(task->ratio_items, i, lissajous_freqs[i].name,
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->ratio_menu, "Ratio", task->ratio_items,
                 NELEMS(task->ratio_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, LISSAJOUS_MENU_RATIO, "Ratio",
                      wuss_MENU_ITEM_NONE, &task->ratio_menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, LISSAJOUS_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");
  /* hover opens the Save As dialogue as a submenu; ^S shows it standalone */
  task->menu_items[LISSAJOUS_MENU_SAVE].window =
    wuss_saveas_window(task->saveas);

  WUSS_MENU_TITLE(task->menu, "Lissajous", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void lissajous_destroy(lissajous_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  free(task);
}

static result_t lissajous_redraw(const wuss_event_t *event, void *task_data)
{
  lissajous_task_t *lc;
  screen_t         *scr;
  const box_t      *content, *bounds;
  int               sx, sy;
  int               width, height, cx, cy, rx, ry;
  int               i;
  pixelfmt_any_t    fgpix;

  lc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content), lc->bg);

  width  = bounds->x1 - bounds->x0;
  height = bounds->y1 - bounds->y0;
  cx     = bounds->x0 - sx + width  / 2;
  cy     = bounds->y0 - sy + height / 2;
  rx     = width  / 2 - 10;
  ry     = height / 2 - 10;

  /* resolve the colour once, not once per plotted point */
  fgpix = screen_colour_to_pixel(scr, lc->fg);

  for (i = 0; i < LISSAJOUS_POINTS; i++)
  {
    double t;
    int    px, py;

    t  = (double) i / LISSAJOUS_POINTS * 2.0 * M_PI;
    px = cx + (int) (rx * sin(lc->a * t + lc->phase));
    py = cy + (int) (ry * sin(lc->b * t));

    screen_set_pixel_value(scr, px, py, fgpix);
  }

  return result_OK;
}

/* switch to the given entry of lissajous_freqs */
static void lissajous_set_freq(lissajous_task_t *lc, int index)
{
  lc->freq_index = index;
  lc->a          = lissajous_freqs[index].a;
  lc->b          = lissajous_freqs[index].b;
  wuss_window_invalidate_visible(lc->window); /* whole figure changes */
}

static result_t lissajous_mouse(wuss_window_t      *window,
                                wuss_mouse_action_t action,
                                wuss_button_t       button,
                                void               *task_data)
{
  lissajous_task_t *lc;

  lc = task_data;

  if (window != lc->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & wuss_BUTTON_MENU)
  {
    static const wuss_proginfo_desc_t desc =
      TASK_PROGINFO_DESC("Lissajous", "Lissajous figure, drifting frequencies");
    wuss_proginfo_set_desc(&desc);
    lc->menu_items[LISSAJOUS_MENU_INFO].window =
      wuss_proginfo_window(lc->delegate);

    wuss_menu_tick_item(&lc->menu, LISSAJOUS_MENU_PAUSE, lc->paused);
    wuss_menu_tick_exclusive(&lc->ratio_menu, lc->freq_index);

    return wuss_menu_open_at_pointer(lc->delegate, &lc->menu,
                                     &lc->menu_handle);
  }

  if (button & wuss_BUTTON_SELECT)
  {
    lissajous_set_freq(lc, (lc->freq_index + 1) % LISSAJOUS_NFREQS);
  }
  else if (button & wuss_BUTTON_ADJUST)
  {
    lc->drift = -lc->drift;
  }

  return result_OK;
}

/* Left/Right step to the previous/next frequency pair. Anything else is
 * passed back unclaimed. */
static result_t lissajous_key(lissajous_task_t *lc, int code)
{
  switch (code)
  {
  case wuss_KEY_LEFT:
    lissajous_set_freq(lc, (lc->freq_index + LISSAJOUS_NFREQS - 1) %
                           LISSAJOUS_NFREQS);
    return result_OK;

  case wuss_KEY_RIGHT:
    lissajous_set_freq(lc, (lc->freq_index + 1) % LISSAJOUS_NFREQS);
    return result_OK;

  default:
    return result_WUSS_KEY_UNCLAIMED;
  }
}

static result_t lissajous_idle(void *task_data)
{
  lissajous_task_t *lc;

  lc = task_data;

  /* the proginfo dialogue is a second window on this same (autoclose)
   * delegate, so closing the main window alone never empties task->windows
   * and the task lingers until the dialogue closes too -- guard against the
   * dangling window in the meantime */
  if (lc->window == NULL)
    return result_OK;

  if (lc->paused)
    return result_OK;

  lc->phase += lc->drift;
  if (lc->phase > 2.0 * M_PI)
    lc->phase -= 2.0 * M_PI;
  else if (lc->phase < 0.0)
    lc->phase += 2.0 * M_PI;

  wuss_window_invalidate_visible(lc->window);

  return result_OK;
}

/* The "Background" and "Foreground" rows' submenu: the shared colourmenu
 * singleton, reconfigured here rather than at create time since other tasks
 * retitle it and toggle its None row too; also notes which colour a pick
 * lands in. */
static result_t lissajous_pre_submenu_open(lissajous_task_t   *lc,
                                           const wuss_event_t *event)
{
  const char *title;

  if (event->data.pre_submenu_open.index == LISSAJOUS_MENU_FOREGROUND)
  {
    lc->colourmenu_target = &lc->fg;
    title                 = "Foreground";
  }
  else
  {
    lc->colourmenu_target = &lc->bg;
    title                 = "Background";
  }

  return wuss_colourmenu_open_rgb(lc->wuss, event, title,
                                  *lc->colourmenu_target);
}

static result_t lissajous_menu_select(lissajous_task_t   *lc,
                                      const wuss_event_t *event)
{
  if (event->data.menu_select.menu == &lc->ratio_menu)
  {
    lissajous_set_freq(lc, event->data.menu_select.index);
    wuss_menu_tick_exclusive_live(lc->menu_handle, &lc->ratio_menu,
                                  lc->freq_index);
    return result_OK;
  }

  if (lc->colourmenu_target != NULL &&
      wuss_colourmenu_selected_rgb(event, lc->colourmenu_target))
    wuss_window_invalidate_visible(lc->window);

  return result_OK;
}

/* The "Pause" row: stop or restart the idle animation. An ADJUST pick keeps
 * the menu open, so retick the live row; a SELECT pick has already closed
 * it. */
static result_t lissajous_toggle_pause(lissajous_task_t   *lc,
                                       const wuss_event_t *event)
{
  lc->paused = !lc->paused;

  wuss_menu_tick_item_live(lc->menu_handle, &lc->menu, LISSAJOUS_MENU_PAUSE,
                           lc->paused);

  return result_OK;
}

result_t lissajous_handle(wuss_window_t      *window,
                          const wuss_event_t *event,
                          void               *task_data)
{
  lissajous_task_t *lc;

  lc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return lissajous_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    return lissajous_mouse(window, event->data.mouse.action,
                           event->data.mouse.button, task_data);

  case wuss_EVENT_IDLE:
    return lissajous_idle(task_data);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != lc->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    if (!task_key_is_plain(lc->delegate, &lc->menu, event, &rc))
      return rc;

    return lissajous_key(lc, event->data.key.code);
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return lissajous_pre_submenu_open(lc, event);

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &lc->menu &&
        event->data.menu_select.index == LISSAJOUS_MENU_PAUSE)
      return lissajous_toggle_pause(lc, event);
    if (event->data.menu_select.menu == &lc->menu &&
        event->data.menu_select.index == LISSAJOUS_MENU_SAVE)
    {
      return wuss_saveas_open(lc->saveas);
    }
    return lissajous_menu_select(lc, event);

  case wuss_EVENT_MENU_CLOSED:
    lc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == lc->window)
      lc->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == lc->menu_items[LISSAJOUS_MENU_INFO].window)
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

  case wuss_EVENT_QUIT:
    lissajous_destroy(lc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
