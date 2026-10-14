/* wuss/tasks/oscillating.c -- Lissajous figure task */

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
#include "utils/easing.h"

#include "oscillating.h"
#include "taskapp.h"
#include "common.h"
#include "snapshot.h"

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in oscillating_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum
{
  OSCILLATING_MENU_RATIO = 0,
  OSCILLATING_MENU_REVERSALS,
  OSCILLATING_MENU_EASING,
  OSCILLATING_MENU_DECAY,
  OSCILLATING_MENU_COLOURS,
  OSCILLATING_MENU_PAUSE,
  OSCILLATING_MENU_RANDOMISE,
  OSCILLATING_MENU_RESET,
  OSCILLATING_MENU_SAVE
};

#define OSCILLATING_SAVE_NAME "oscillating.png" /* Save As's initial leafname */

/* frequency pairs cycled by a Select click and listed in Menu > Ratio */
static const struct
{
  int         a, b;
  const char *name;
}
oscillating_freqs[OSCILLATING_NFREQS] =
{
  { 3, 2, "3:2" }, { 5, 4, "5:4" }, { 3, 4, "3:4" },
  { 5, 6, "5:6" }, { 1, 2, "1:2" }, { 7, 4, "7:4" },
  { 1, 1, "1:1" }, { 1, 3, "1:3" }, { 2, 3, "2:3" },
  { 3, 5, "3:5" }, { 4, 5, "4:5" }, { 7, 6, "7:6" }
};

/* easing curves listed in Menu > Easing, indexed by easing_kind_t */
static const char *const oscillating_easings[OSCILLATING_NEASINGS] =
{
  "None", "Quad", "Cubic", "Quart", "Quint", "Sine",
  "Expo", "Circ", "Back", "Elastic", "Bounce"
};

/* trail lengths listed in Menu > Decay */
static const struct
{
  int         frames;
  const char *name;
}
oscillating_decays[OSCILLATING_NDECAYS] =
{
  { 0, "Off" }, { 4, "Short" }, { 8, "Medium" }, { 16, "Long" }
};

/* wuss_saveas_save_fn_t: opaque is the oscillating_task_t */
static result_t oscillating_saveas_save(const char *path, void *opaque)
{
  oscillating_task_t *lc;

  lc = opaque;

  return snapshot_save_png(lc->window, oscillating_handle, lc, path);
}

/* the starting figure, as oscillating_create and Reset restore it */
static void oscillating_defaults(oscillating_task_t *task)
{
  task->bg           = colour_rgb(0x00, 0x00, 0x00);
  task->fg           = colour_rgb(0x00, 0xFF, 0x00);
  task->freq_index   = 0;
  task->a            = oscillating_freqs[0].a;
  task->b            = oscillating_freqs[0].b;
  task->phase        = 0.0;
  task->drift        = 0.01;
  task->reversals    = 1;
  task->reversing    = 0.0;
  task->easing_index = easing_LINEAR; /* row 0, "None" */
  task->decay_index  = 0;             /* row 0, "Off" */
  task->trail_count  = 0;
}

/* pick the ratio, easing, decay, drift direction and colours afresh */
static void oscillating_randomise(oscillating_task_t *task)
{
  task->freq_index   = rng_range(&task->rng, OSCILLATING_NFREQS);
  task->a            = oscillating_freqs[task->freq_index].a;
  task->b            = oscillating_freqs[task->freq_index].b;
  task->easing_index = rng_range(&task->rng, OSCILLATING_NEASINGS);
  task->decay_index  = rng_range(&task->rng, OSCILLATING_NDECAYS);
  task->drift        = rng_range(&task->rng, 2) ? task->drift : -task->drift;
  task->reversing    = 0.0;
  task->trail_count  = 0;
  task->bg           = colour_random(&task->rng);
  task->fg           = colour_random_contrast(&task->rng, task->bg);
}

result_t oscillating_create(wuss_t *wuss, oscillating_task_t **out)
{
  result_t            rc;
  oscillating_task_t *task;
  wuss_task_t        *delegate;
  wuss_task_desc_t    delegate_desc;
  filetype_t          png_type;
  int                 i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss       = wuss;
  rng_seed(&task->rng, (uint32_t) rand());
  oscillating_defaults(task);

  /* redraw paints its own background every frame */
  delegate_desc.handle    = oscillating_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "oscillating";
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
                          "Oscillating",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, wuss, &png_type,
                          OSCILLATING_SAVE_NAME, oscillating_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  for (i = 0; i < OSCILLATING_NFREQS; i++)
    WUSS_MENU_ITEM(task->ratio_items, i, oscillating_freqs[i].name,
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->ratio_menu, "Ratio", task->ratio_items,
                 NELEMS(task->ratio_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, OSCILLATING_MENU_RATIO, "Ratio",
                      wuss_MENU_ITEM_NONE, &task->ratio_menu);

  WUSS_MENU_ITEM(task->menu_items, OSCILLATING_MENU_REVERSALS, "Reversals",
                 wuss_MENU_ITEM_NONE);

  for (i = 0; i < OSCILLATING_NDECAYS; i++)
    WUSS_MENU_ITEM(task->decay_items, i, oscillating_decays[i].name,
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->decay_menu, "Decay", task->decay_items,
                 NELEMS(task->decay_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, OSCILLATING_MENU_DECAY, "Decay",
                      wuss_MENU_ITEM_NONE, &task->decay_menu);

  for (i = 0; i < OSCILLATING_NEASINGS; i++)
    WUSS_MENU_ITEM(task->easing_items, i, oscillating_easings[i],
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->easing_menu, "Easing", task->easing_items,
                 NELEMS(task->easing_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, OSCILLATING_MENU_EASING, "Easing",
                      wuss_MENU_ITEM_NONE, &task->easing_menu);

  wuss_colourlist_init_pair(&task->colours, wuss, &task->fg, &task->bg);

  WUSS_MENU_ITEM_MENU(task->menu_items, OSCILLATING_MENU_COLOURS, "Colours",
                      wuss_MENU_ITEM_NONE, &task->colours.menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, OSCILLATING_MENU_PAUSE, "Pause",
                          wuss_MENU_ITEM_NONE, "SPACE");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, OSCILLATING_MENU_RANDOMISE,
                          "Randomise", wuss_MENU_ITEM_NONE, "^R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, OSCILLATING_MENU_RESET, "Reset",
                          wuss_MENU_ITEM_NONE, "R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, OSCILLATING_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");
  /* hover opens the Save As dialogue as a submenu; ^S shows it standalone */
  task->menu_items[OSCILLATING_MENU_SAVE].window =
    wuss_saveas_window(task->saveas);

  WUSS_MENU_TITLE(task->menu, "Oscillating", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

result_t oscillating_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Oscillating",
    "Drifting Lissajous figure",
    (taskapp_create_fn_t) oscillating_create
  };

  return taskapp_install(wuss, &desc);
}

void oscillating_destroy(oscillating_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  free(task);
}

static result_t oscillating_redraw(const wuss_event_t *event,
                                   void               *task_data)
{
  oscillating_task_t *lc;
  screen_t           *scr;
  const box_t        *content, *bounds;
  int                 sx, sy;
  int                 width, height, cx, cy, rx, ry;
  int                 i, age, ntrail;
  double              phase;
  pixelfmt_any_t      fgpix;

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

  ntrail = MIN(oscillating_decays[lc->decay_index].frames, lc->trail_count);

  /* oldest first so the newest frame is drawn on top; each age fades fg
   * towards bg. Resolve the colour once per frame, not once per point */
  for (age = ntrail; age >= 0; age--)
  {
    if (age == 0)
      phase = lc->phase;
    else
      phase = lc->trail[(lc->trail_head - (age - 1) + OSCILLATING_MAX_TRAIL) %
                        OSCILLATING_MAX_TRAIL];

    fgpix = screen_colour_to_pixel(scr,
                                   colour_mix(lc->fg, lc->bg, age, ntrail + 1));

    for (i = 0; i < OSCILLATING_POINTS; i++)
    {
      double t;
      int    px, py;

      t  = (double) i / OSCILLATING_POINTS * 2.0 * M_PI;
      px = cx + (int) (rx * sin(lc->a * t + phase));
      py = cy + (int) (ry * sin(lc->b * t));

      screen_set_pixel_value(scr, px, py, fgpix);
    }
  }

  return result_OK;
}

/* switch to the given entry of oscillating_freqs */
static void oscillating_set_freq(oscillating_task_t *lc, int index)
{
  lc->freq_index = index;
  lc->a          = oscillating_freqs[index].a;
  lc->b          = oscillating_freqs[index].b;
  wuss_window_invalidate_visible(lc->window); /* whole figure changes */
}

static result_t oscillating_mouse(wuss_window_t      *window,
                                  wuss_mouse_action_t action,
                                  wuss_button_t       button,
                                  void               *task_data)
{
  oscillating_task_t *lc;

  lc = task_data;

  if (window != lc->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & wuss_BUTTON_MENU)
  {
    wuss_menu_tick_item(&lc->menu, OSCILLATING_MENU_PAUSE, lc->paused);
    wuss_menu_tick_item(&lc->menu, OSCILLATING_MENU_REVERSALS, lc->reversals);
    wuss_menu_tick_exclusive(&lc->ratio_menu, lc->freq_index);
    wuss_menu_tick_exclusive(&lc->easing_menu, lc->easing_index);
    wuss_menu_tick_exclusive(&lc->decay_menu, lc->decay_index);

    return wuss_menu_open_at_pointer(lc->delegate, &lc->menu,
                                     &lc->menu_handle);
  }

  if (button & wuss_BUTTON_SELECT)
  {
    oscillating_set_freq(lc, (lc->freq_index + 1) % OSCILLATING_NFREQS);
  }
  else if (button & wuss_BUTTON_ADJUST)
  {
    lc->drift = -lc->drift;
  }

  return result_OK;
}

/* Left/Right step to the previous/next frequency pair. Anything else is
 * passed back unclaimed. */
static result_t oscillating_key(oscillating_task_t *lc, int code)
{
  switch (code)
  {
  case wuss_KEY_LEFT:
    oscillating_set_freq(lc, (lc->freq_index + OSCILLATING_NFREQS - 1) %
                           OSCILLATING_NFREQS);
    return result_OK;

  case wuss_KEY_RIGHT:
    oscillating_set_freq(lc, (lc->freq_index + 1) % OSCILLATING_NFREQS);
    return result_OK;

  default:
    return result_WUSS_KEY_UNCLAIMED;
  }
}

static result_t oscillating_idle(void *task_data)
{
  oscillating_task_t *lc;

  lc = task_data;

  /* the proginfo dialogue is a second window on this same (autoclose)
   * delegate, so closing the main window alone never empties task->windows
   * and the task lingers until the dialogue closes too -- guard against the
   * dangling window in the meantime */
  if (lc->window == NULL)
    return result_OK;

  if (lc->paused)
    return result_OK;

  /* remember the frame about to be replaced for the decay trail */
  if (oscillating_decays[lc->decay_index].frames > 0)
  {
    lc->trail_head = (lc->trail_head + 1) % OSCILLATING_MAX_TRAIL;
    lc->trail[lc->trail_head] = lc->phase;
    lc->trail_count = MIN(lc->trail_count + 1, OSCILLATING_MAX_TRAIL);
  }

  /* 2% of ticks start a 90 degree backwards run, then the drift resumes */
  if (lc->reversals && lc->reversing <= 0.0 && rand() % 100 < 2)
  {
    lc->reversing    = 1.0;
    lc->reverse_from = lc->phase;
  }

  if (lc->reversing > 0.0)
  {
    /* same duration as a linear run at the drift rate; the chosen curve
     * shapes it (None is linear) */
    lc->reversing = MAX(lc->reversing - fabs(lc->drift) / (M_PI / 2.0), 0.0);
    lc->phase     = lc->reverse_from -
                    M_PI / 2.0 * easing((easing_kind_t) lc->easing_index,
                                        easing_IN_OUT, 1.0 - lc->reversing);
  }
  else
  {
    lc->phase += lc->drift;
  }

  if (lc->phase > 2.0 * M_PI)
    lc->phase -= 2.0 * M_PI;
  else if (lc->phase < 0.0)
    lc->phase += 2.0 * M_PI;

  wuss_window_invalidate_visible(lc->window);

  return result_OK;
}

/* Only the Colours rows are PRE_OPEN, so the event is the pair's. */
static result_t oscillating_pre_submenu_open(oscillating_task_t *lc,
                                             const wuss_event_t *event)
{
  result_t rc;

  if (wuss_colourlist_pre_open(&lc->colours, lc->wuss, event, &rc))
    return rc;

  return result_OK;
}

static result_t oscillating_menu_select(oscillating_task_t *lc,
                                        const wuss_event_t *event)
{
  if (event->data.menu_select.menu == &lc->menu &&
      (event->data.menu_select.index == OSCILLATING_MENU_RANDOMISE ||
       event->data.menu_select.index == OSCILLATING_MENU_RESET))
  {
    if (event->data.menu_select.index == OSCILLATING_MENU_RANDOMISE)
      oscillating_randomise(lc);
    else
      oscillating_defaults(lc);

    wuss_menu_tick_exclusive_live(lc->menu_handle, &lc->ratio_menu,
                                  lc->freq_index);
    wuss_menu_tick_exclusive_live(lc->menu_handle, &lc->easing_menu,
                                  lc->easing_index);
    wuss_menu_tick_exclusive_live(lc->menu_handle, &lc->decay_menu,
                                  lc->decay_index);
    wuss_menu_tick_item_live(lc->menu_handle, &lc->menu,
                             OSCILLATING_MENU_REVERSALS, lc->reversals);
    wuss_window_invalidate_visible(lc->window);
    return result_OK;
  }

  if (event->data.menu_select.menu == &lc->ratio_menu)
  {
    oscillating_set_freq(lc, event->data.menu_select.index);
    wuss_menu_tick_exclusive_live(lc->menu_handle, &lc->ratio_menu,
                                  lc->freq_index);
    return result_OK;
  }

  if (event->data.menu_select.menu == &lc->decay_menu)
  {
    lc->decay_index = event->data.menu_select.index;
    wuss_menu_tick_exclusive_live(lc->menu_handle, &lc->decay_menu,
                                  lc->decay_index);
    return result_OK;
  }

  if (event->data.menu_select.menu == &lc->easing_menu)
  {
    lc->easing_index = event->data.menu_select.index;
    wuss_menu_tick_exclusive_live(lc->menu_handle, &lc->easing_menu,
                                  lc->easing_index);
    return result_OK;
  }

  if (wuss_colourlist_selected(&lc->colours, event))
    wuss_window_invalidate_visible(lc->window);

  return result_OK;
}

/* The "Pause" row: stop or restart the idle animation. An ADJUST pick keeps
 * the menu open, so retick the live row; a SELECT pick has already closed
 * it. */
static result_t oscillating_toggle_pause(oscillating_task_t *lc,
                                         const wuss_event_t *event)
{
  lc->paused = !lc->paused;

  wuss_menu_tick_item_live(lc->menu_handle, &lc->menu, OSCILLATING_MENU_PAUSE,
                           lc->paused);

  return result_OK;
}

result_t oscillating_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  oscillating_task_t *lc;

  lc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return oscillating_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    return oscillating_mouse(window, event->data.mouse.action,
                           event->data.mouse.button, task_data);

  case wuss_EVENT_IDLE:
    return oscillating_idle(task_data);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != lc->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    if (!task_key_is_plain(lc->delegate, &lc->menu, event, &rc))
      return rc;

    return oscillating_key(lc, event->data.key.code);
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return oscillating_pre_submenu_open(lc, event);

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &lc->menu &&
        event->data.menu_select.index == OSCILLATING_MENU_PAUSE)
      return oscillating_toggle_pause(lc, event);
    if (event->data.menu_select.menu == &lc->menu &&
        event->data.menu_select.index == OSCILLATING_MENU_REVERSALS)
    {
      lc->reversals = !lc->reversals;
      wuss_menu_tick_item_live(lc->menu_handle, &lc->menu,
                               OSCILLATING_MENU_REVERSALS, lc->reversals);
      return result_OK;
    }
    if (event->data.menu_select.menu == &lc->menu &&
        event->data.menu_select.index == OSCILLATING_MENU_SAVE)
    {
      return wuss_saveas_open(lc->saveas);
    }
    return oscillating_menu_select(lc, event);

  case wuss_EVENT_MENU_CLOSED:
    lc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == lc->window)
      lc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    oscillating_destroy(lc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
