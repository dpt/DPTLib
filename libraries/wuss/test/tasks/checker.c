/* wuss/test/tasks/checker.c -- checkerboard task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "checker.h"
#include "common.h"
#include "snapshot.h"

#define CHECKER_BAND_DEFAULT 8  /* pixels per band, so each pattern reads clearly */
#define CHECKER_BAND_MIN     1
#define CHECKER_BAND_MAX     32

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in checker_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum
{
  CHECKER_MENU_INFO = 0,
  CHECKER_MENU_INK,
  CHECKER_MENU_PAPER,
  CHECKER_MENU_PATTERN,
  CHECKER_MENU_SWAP,
  CHECKER_MENU_SAVE
};

#define CHECKER_SAVE_NAME "checker.png" /* written to the current dir */

/* Pattern submenu rows, in checker_pattern_t order */
static const char *checker_pattern_names[checker_PATTERN__COUNT] =
{
  "Checkerboard",
  "Horizontal",
  "Vertical",
  "Diagonal"
};

result_t checker_create(wuss_t *wuss, checker_task_t **out)
{
  result_t         rc;
  checker_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss     = wuss;
  task->black    = colour_rgb(0x00, 0x00, 0x00);
  task->white    = colour_rgb(0xFF, 0xFF, 0xFF);
  task->pattern  = checker_PATTERN_CHECKERBOARD;
  task->pattern2 = checker_PATTERN_VERTICAL;
  task->band     = CHECKER_BAND_DEFAULT;
  task->band2    = CHECKER_BAND_DEFAULT;

  /* checker_redraw paints every pixel itself */
  delegate_desc.handle    = checker_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "checker";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(160, 160),
                                 "Checker 1",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(160, 160),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(160, 160),
                                 "Checker 2",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(160, 160),
                                 SIZE2D(0, 0),
                                 &task->window2);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* closes "Checker 1", QUIT frees the block */
    return rc;
  }

  /* both windows up: from here, closing the last one reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  WUSS_MENU_ITEM_WINDOW(task->menu_items, CHECKER_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * checker_handle */

  /* both rows open the shared colourmenu; checker_pre_submenu_open retitles
   * it and picks which colour a pick lands in */
  WUSS_MENU_ITEM_MENU(task->menu_items, CHECKER_MENU_INK, "Ink",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));
  WUSS_MENU_ITEM_MENU(task->menu_items, CHECKER_MENU_PAPER, "Paper",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  for (i = 0; i < checker_PATTERN__COUNT; i++)
    WUSS_MENU_ITEM(task->pattern_items, i, checker_pattern_names[i],
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->pattern_menu, "Pattern", task->pattern_items,
                 NELEMS(task->pattern_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, CHECKER_MENU_PATTERN, "Pattern",
                      wuss_MENU_ITEM_NONE, &task->pattern_menu);

  WUSS_MENU_ITEM(task->menu_items, CHECKER_MENU_SWAP, "Swap colours",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CHECKER_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");

  WUSS_MENU_TITLE(task->menu, "Checker", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void checker_destroy(checker_task_t *task)
{
  free(task);
}

static result_t checker_redraw(wuss_window_t      *window,
                               const wuss_event_t *event,
                               void               *task_data)
{
  checker_task_t   *cc;
  screen_t         *scr;
  const box_t      *content, *bounds;
  checker_pattern_t pattern;
  int               x, y, lx, ly, sx, sy, band_px, band;

  cc = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  pattern = (window == cc->window2) ? cc->pattern2 : cc->pattern;
  band_px = (window == cc->window2) ? cc->band2    : cc->band;

  for (y = content->y0; y < content->y1; y++)
  {
    for (x = content->x0; x < content->x1; x++)
    {
      lx = x - bounds->x0 + sx;
      ly = y - bounds->y0 + sy;

      switch (pattern)
      {
      case checker_PATTERN_HORIZONTAL: band = ly / band_px;                break;
      case checker_PATTERN_VERTICAL:   band = lx / band_px;                break;
      case checker_PATTERN_DIAGONAL:   band = (lx + ly) / band_px;         break;
      default:                         band = lx / band_px + ly / band_px; break; /* CHECKERBOARD */
      }

      screen_set_pixel(scr, x, y, (band & 1) ? cc->black : cc->white);
    }
  }

  return result_OK;
}

/* step the clicked window's pattern by dir (+1 or -1), wrapping round */
static result_t checker_mouse(wuss_window_t *window,
                              int            dir,
                              void          *task_data)
{
  checker_task_t    *cc;
  checker_pattern_t *pattern;

  cc = task_data;

  pattern  = (window == cc->window2) ? &cc->pattern2 : &cc->pattern;
  *pattern = (*pattern + checker_PATTERN__COUNT + dir) %
             checker_PATTERN__COUNT;

  wuss_window_invalidate_visible(window);

  return result_OK;
}

static result_t checker_scroll(wuss_window_t *window,
                               int            delta,
                               void          *task_data)
{
  checker_task_t *cc;
  int            *band;

  cc   = task_data;
  band = (window == cc->window2) ? &cc->band2 : &cc->band;

  *band += delta;
  *band  = CLAMP(*band, CHECKER_BAND_MIN, CHECKER_BAND_MAX);

  wuss_window_invalidate_visible(window);

  return result_OK;
}

/* Left/Right step the focused window's pattern back/forward, as Adjust and
 * Select clicks do; Up/Down widen/narrow its bands, as the wheel does.
 * Anything else is passed back unclaimed. */
static result_t checker_key(checker_task_t *cc,
                            wuss_window_t  *window,
                            int             code)
{
  switch (code)
  {
  case wuss_KEY_LEFT:  return checker_mouse(window, -1, cc);
  case wuss_KEY_RIGHT: return checker_mouse(window, +1, cc);
  case wuss_KEY_UP:    return checker_scroll(window, +1, cc);
  case wuss_KEY_DOWN:  return checker_scroll(window, -1, cc);
  default:             return result_WUSS_KEY_UNCLAIMED;
  }
}

/* The Ink and Paper rows' submenu: the shared colourmenu singleton,
 * retitled and retargeted at the matching colour each time it opens. */
static result_t checker_pre_submenu_open(checker_task_t     *cc,
                                         const wuss_event_t *event)
{
  const char *title;

  if (event->data.pre_submenu_open.index == CHECKER_MENU_INK)
  {
    cc->colourmenu_target = &cc->black;
    title                 = "Ink";
  }
  else
  {
    cc->colourmenu_target = &cc->white;
    title                 = "Paper";
  }

  return wuss_colourmenu_open_rgb(cc->wuss, event, title,
                                  *cc->colourmenu_target);
}

/* A Pattern pick lands in whichever window opened the menu. A colourmenu
 * pick lands in whichever colour last opened it; both windows share the two
 * colours, so both repaint. */
static result_t checker_menu_select(checker_task_t     *cc,
                                    const wuss_event_t *event)
{
  colour_t        tmp;

  if (event->data.menu_select.menu == &cc->pattern_menu)
  {
    if (cc->menu_window == NULL)
      return result_OK;

    if (cc->menu_window == cc->window2)
      cc->pattern2 = event->data.menu_select.index;
    else
      cc->pattern = event->data.menu_select.index;
    wuss_menu_tick_exclusive_live(cc->menu_handle, &cc->pattern_menu,
                                  event->data.menu_select.index);
    wuss_window_invalidate_visible(cc->menu_window);
    return result_OK;
  }

  /* saves whichever window the menu was opened from */
  if (event->data.menu_select.menu == &cc->menu &&
      event->data.menu_select.index == CHECKER_MENU_SAVE)
    return snapshot_save_png(cc->menu_window, checker_handle, cc,
                             CHECKER_SAVE_NAME);

  if (event->data.menu_select.menu == &cc->menu &&
      event->data.menu_select.index == CHECKER_MENU_SWAP)
  {
    tmp       = cc->black;
    cc->black = cc->white;
    cc->white = tmp;
  }
  else
  {
    if (cc->colourmenu_target == NULL ||
        !wuss_colourmenu_selected_rgb(event, cc->colourmenu_target))
      return result_OK;
  }

  wuss_window_invalidate_visible(cc->window);
  wuss_window_invalidate_visible(cc->window2);

  return result_OK;
}

result_t checker_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  checker_task_t *cc;

  cc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return checker_redraw(window, event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != cc->window && window != cc->window2)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    if (event->data.mouse.button & wuss_BUTTON_MENU)
    {
      static const wuss_proginfo_desc_t desc =
        TASK_PROGINFO_DESC("Checker",
                           "Two independent cycling checkerboard patterns");
      wuss_proginfo_set_desc(&desc);
      cc->menu_items[CHECKER_MENU_INFO].window =
        wuss_proginfo_window(cc->delegate);

      cc->menu_window = window;
      wuss_menu_tick_exclusive(&cc->pattern_menu,
                               (window == cc->window2) ? cc->pattern2
                                                       : cc->pattern);

      return wuss_menu_open_at_pointer(cc->delegate, &cc->menu,
                                       &cc->menu_handle);
    }
    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      return checker_mouse(window, +1, task_data);
    if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      return checker_mouse(window, -1, task_data);
    return result_OK;

  case wuss_EVENT_SCROLL:
    return checker_scroll(window, event->data.scroll.delta, task_data);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != cc->window && window != cc->window2)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    cc->menu_window = window; /* a shortcut acts on the focused window */
    rc = wuss_menu_dispatch_shortcut(cc->delegate, &cc->menu, event);
    if (rc != result_WUSS_KEY_UNCLAIMED)
      return rc;

    if (event->data.key.modifiers & (wuss_KEY_MOD_CTRL | wuss_KEY_MOD_ALT))
      return result_WUSS_KEY_UNCLAIMED;
    return checker_key(cc, window, event->data.key.code);
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return checker_pre_submenu_open(cc, event);

  case wuss_EVENT_MENU_SELECT:
    return checker_menu_select(cc, event);

  case wuss_EVENT_MENU_CLOSED:
    cc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == cc->menu_items[CHECKER_MENU_INFO].window)
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
    if (window == cc->menu_window)
      cc->menu_window = NULL;
    if (window == cc->window2)
      cc->window2 = NULL;
    else
      cc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    /* one calloc'd block backs both windows; the task autocloses once the
     * second window goes, so free it here */
    checker_destroy(cc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
