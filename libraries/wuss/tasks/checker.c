/* wuss/tasks/checker.c -- checkerboard task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "io/filetype.h"

#include "checker.h"
#include "taskapp.h"
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
  CHECKER_MENU_COLOURS = 0,
  CHECKER_MENU_PATTERN,
  CHECKER_MENU_SWAP,
  CHECKER_MENU_RANDOMISE,
  CHECKER_MENU_RESET,
  CHECKER_MENU_SAVE
};

#define CHECKER_SAVE_NAME "checker.png" /* Save As's initial leafname */

/* Colours submenu rows: the black and white fields, in that order */
static const char *const checker_colour_names[] = { "Ink", "Paper" };

/* Pattern submenu rows, in checker_pattern_t order */
static const char *checker_pattern_names[checker_PATTERN__COUNT] =
{
  "Checkerboard",
  "Horizontal",
  "Vertical",
  "Diagonal"
};

/* wuss_saveas_save_fn_t: opaque is the checker_task_t */
static result_t checker_saveas_save(const char *path, void *opaque)
{
  checker_task_t *cc;

  cc = opaque;

  return snapshot_save_png(cc->window, checker_handle, cc, path);
}

/* the starting pattern, as checker_create and Reset restore it */
static void checker_defaults(checker_task_t *task)
{
  task->black   = colour_rgb(0x00, 0x00, 0x00);
  task->white   = colour_rgb(0xFF, 0xFF, 0xFF);
  task->pattern = checker_PATTERN_CHECKERBOARD;
  task->band    = CHECKER_BAND_DEFAULT;
}

/* pick the pattern, band width and both colours afresh */
static void checker_randomise(checker_task_t *task)
{
  task->pattern = (checker_pattern_t) rng_range(&task->rng,
                                                checker_PATTERN__COUNT);
  task->band    = CHECKER_BAND_MIN +
                  rng_range(&task->rng,
                            CHECKER_BAND_MAX - CHECKER_BAND_MIN + 1);
  task->black   = colour_random(&task->rng);
  task->white   = colour_random_contrast(&task->rng, task->black);
}

result_t checker_create(wuss_t *wuss, checker_task_t **out)
{
  result_t         rc;
  checker_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  filetype_t       png_type;
  colour_t        *fields[2];
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss     = wuss;
  rng_seed(&task->rng, (uint32_t) rand());
  checker_defaults(task);

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

  rc = task_window_create(delegate,
                          SIZE2D(160, 160),
                          "Checker",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  /* window up: from here, closing it reaps the task and its wuss_EVENT_QUIT
   * frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, wuss, &png_type,
                          CHECKER_SAVE_NAME, checker_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  fields[0] = &task->black;
  fields[1] = &task->white;
  rc = wuss_colourlist_init(&task->colours, wuss, NELEMS(fields), fields,
                            checker_colour_names);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  WUSS_MENU_ITEM_MENU(task->menu_items, CHECKER_MENU_COLOURS, "Colours",
                      wuss_MENU_ITEM_NONE, &task->colours.menu);

  for (i = 0; i < checker_PATTERN__COUNT; i++)
    WUSS_MENU_ITEM(task->pattern_items, i, checker_pattern_names[i],
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->pattern_menu, "Pattern", task->pattern_items,
                 NELEMS(task->pattern_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, CHECKER_MENU_PATTERN, "Pattern",
                      wuss_MENU_ITEM_NONE, &task->pattern_menu);

  WUSS_MENU_ITEM(task->menu_items, CHECKER_MENU_SWAP, "Swap colours",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CHECKER_MENU_RANDOMISE,
                          "Randomise", wuss_MENU_ITEM_NONE, "^R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CHECKER_MENU_RESET, "Reset",
                          wuss_MENU_ITEM_NONE, "R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, CHECKER_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");
  /* hover opens the Save As dialogue as a submenu; ^S shows it standalone */
  task->menu_items[CHECKER_MENU_SAVE].window = wuss_saveas_window(task->saveas);

  WUSS_MENU_TITLE(task->menu, "Checker", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

result_t checker_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Checker",
    "Cycling checkerboard pattern",
    (taskapp_create_fn_t) checker_create
  };

  return taskapp_install(wuss, &desc);
}

void checker_destroy(checker_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  free(task);
}

static result_t checker_redraw(const wuss_event_t *event, void *task_data)
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

  pattern = cc->pattern;
  band_px = cc->band;

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

/* step the pattern by dir (+1 or -1), wrapping round */
static result_t checker_mouse(int dir, void *task_data)
{
  checker_task_t *cc;

  cc = task_data;

  cc->pattern = (cc->pattern + checker_PATTERN__COUNT + dir) %
                checker_PATTERN__COUNT;

  wuss_window_invalidate_visible(cc->window);

  return result_OK;
}

static result_t checker_scroll(int delta, void *task_data)
{
  checker_task_t *cc;

  cc = task_data;

  cc->band += delta;
  cc->band  = CLAMP(cc->band, CHECKER_BAND_MIN, CHECKER_BAND_MAX);

  wuss_window_invalidate_visible(cc->window);

  return result_OK;
}

/* Left/Right step the pattern back/forward, as Adjust and Select clicks do;
 * Up/Down widen/narrow its bands, as the wheel does. Anything else is passed
 * back unclaimed. */
static result_t checker_key(checker_task_t *cc, int code)
{
  switch (code)
  {
  case wuss_KEY_LEFT:  return checker_mouse(-1, cc);
  case wuss_KEY_RIGHT: return checker_mouse(+1, cc);
  case wuss_KEY_UP:    return checker_scroll(+1, cc);
  case wuss_KEY_DOWN:  return checker_scroll(-1, cc);
  default:             return result_WUSS_KEY_UNCLAIMED;
  }
}

/* Every submenu leaf fires wuss_EVENT_PRE_SUBMENU_OPEN; only the Ink and
 * Paper rows retarget the shared colourmenu. */
static result_t checker_pre_submenu_open(checker_task_t     *cc,
                                         const wuss_event_t *event)
{
  result_t rc;
  int      index;

  if (wuss_colourlist_pre_open(&cc->colours, cc->wuss, event, &rc))
    return rc;

  index = event->data.pre_submenu_open.index;

  return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle, index,
                                    cc->menu_items[index].submenu);
}

/* A colourmenu pick lands in whichever colour last opened it. */
static result_t checker_menu_select(checker_task_t     *cc,
                                    const wuss_event_t *event)
{
  colour_t        tmp;

  if (event->data.menu_select.menu == &cc->pattern_menu)
  {
    cc->pattern = event->data.menu_select.index;
    wuss_menu_tick_exclusive_live(cc->menu_handle, &cc->pattern_menu,
                                  event->data.menu_select.index);
    wuss_window_invalidate_visible(cc->window);
    return result_OK;
  }

  /* shows the Save As dialogue */
  if (event->data.menu_select.menu == &cc->menu &&
      event->data.menu_select.index == CHECKER_MENU_SAVE)
  {
    return wuss_saveas_open(cc->saveas);
  }

  if (event->data.menu_select.menu == &cc->menu &&
      (event->data.menu_select.index == CHECKER_MENU_RANDOMISE ||
       event->data.menu_select.index == CHECKER_MENU_RESET))
  {
    if (event->data.menu_select.index == CHECKER_MENU_RANDOMISE)
      checker_randomise(cc);
    else
      checker_defaults(cc);

    wuss_menu_tick_exclusive_live(cc->menu_handle, &cc->pattern_menu,
                                  cc->pattern);
    wuss_window_invalidate_visible(cc->window);
    return result_OK;
  }

  if (event->data.menu_select.menu == &cc->menu &&
      event->data.menu_select.index == CHECKER_MENU_SWAP)
  {
    tmp       = cc->black;
    cc->black = cc->white;
    cc->white = tmp;
  }
  else if (!wuss_colourlist_selected(&cc->colours, event))
  {
    return result_OK;
  }

  wuss_window_invalidate_visible(cc->window);

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
    return checker_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != cc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    if (event->data.mouse.button & wuss_BUTTON_MENU)
    {
      wuss_menu_tick_exclusive(&cc->pattern_menu, cc->pattern);

      return wuss_menu_open_at_pointer(cc->delegate, &cc->menu,
                                       &cc->menu_handle);
    }
    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      return checker_mouse(+1, task_data);
    if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      return checker_mouse(-1, task_data);
    return result_OK;

  case wuss_EVENT_SCROLL:
    return checker_scroll(event->data.scroll.delta, task_data);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != cc->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    if (!task_key_is_plain(cc->delegate, &cc->menu, event, &rc))
      return rc;

    return checker_key(cc, event->data.key.code);
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return checker_pre_submenu_open(cc, event);

  case wuss_EVENT_MENU_SELECT:
    return checker_menu_select(cc, event);

  case wuss_EVENT_MENU_CLOSED:
    cc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == cc->window)
      cc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    /* the task autocloses once its window goes, so free the block here */
    checker_destroy(cc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
