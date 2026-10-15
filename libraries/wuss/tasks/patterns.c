/* wuss/tasks/patterns.c -- dithered colour gradient task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <time.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/pattern.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "utils/rng.h"

#include "patterns.h"
#include "taskapp.h"
#include "common.h"
#include "snapshot.h"

#define PATTERNS_STEP 4 /* pixels per gradient step */

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in patterns_task_t, not as a file-scope static */
enum
{
  PATTERNS_MENU_COLOURS,
  PATTERNS_MENU_RANDOMISE,
  PATTERNS_MENU_RESET,
  PATTERNS_MENU_SAVE
};

#define PATTERNS_SAVE_NAME "patterns.png" /* Save As's initial leafname */

/* Colours submenu rows: the a and b fields, in that order */
static const char *const patterns_colour_names[] = { "Colour A", "Colour B" };

/* channel c0 -> c1, f of n frames into the blend */
static int patterns_lerp(unsigned int c0, unsigned int c1, int f, int n)
{
  return (int) (c0 * (n - f) + c1 * f) / n;
}

/* a dither of the colour f of n steps of the way from a to b */
static pattern_t patterns_step_pattern(patterns_task_t *bc, int f, int n)
{
  unsigned int    ar, ag, ab;
  unsigned int    br, bg, bb;
  colour_t        mix;
  const colour_t *palette;
  int             npalette;

  colour_get_rgb(&bc->a, &ar, &ag, &ab);
  colour_get_rgb(&bc->b, &br, &bg, &bb);

  mix = colour_rgb(patterns_lerp(ar, br, f, n),
                   patterns_lerp(ag, bg, f, n),
                   patterns_lerp(ab, bb, f, n));

  palette = wuss_get_palette(bc->wuss, &npalette);

  return pattern_from_colour(palette, npalette, mix);
}

/* wuss_saveas_save_fn_t: opaque is the patterns_task_t */
static result_t patterns_saveas_save(const char *path, void *opaque)
{
  patterns_task_t *bc;

  bc = opaque;

  return snapshot_save_png(bc->window, patterns_handle, bc, path);
}

/* pick both ends of the gradient afresh, the second standing out from the
 * first */
static void patterns_randomise(patterns_task_t *task)
{
  task->a = colour_random(&task->rng);
  task->b = colour_random_contrast(&task->rng, task->a);
}

result_t patterns_create(wuss_t *wuss, patterns_task_t **out)
{
  result_t         rc;
  patterns_task_t *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  colour_t        *fields[2];

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  rng_seed(&task->rng, (uint32_t) time(NULL));
  patterns_randomise(task);

  /* patterns_redraw paints every pixel itself */
  delegate_desc.handle    = patterns_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "patterns";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate,
                          SIZE2D(200, 160),
                          "Patterns",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  rc = snapshot_saveas_create(&task->saveas, wuss, PATTERNS_SAVE_NAME,
                              patterns_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  fields[0] = &task->a;
  fields[1] = &task->b;
  rc = wuss_colourlist_init(&task->colours, wuss, NELEMS(fields), fields,
                            patterns_colour_names);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  WUSS_MENU_ITEM_MENU(task->menu_items, PATTERNS_MENU_COLOURS, "Colours",
                      wuss_MENU_ITEM_NONE, &task->colours.menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PATTERNS_MENU_RANDOMISE,
                          "Randomise", wuss_MENU_ITEM_NONE, "^R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PATTERNS_MENU_RESET, "Reset",
                          wuss_MENU_ITEM_NONE, "R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PATTERNS_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "F3");
  /* hover opens the Save As dialogue as a submenu; F3 shows it standalone */
  task->menu_items[PATTERNS_MENU_SAVE].window =
    wuss_saveas_window(task->saveas);

  WUSS_MENU_TITLE(task->menu, "Patterns", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

result_t patterns_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Patterns",
    "Dithered two-colour gradient",
    (taskapp_create_fn_t) patterns_create
  };

  return taskapp_install(wuss, &desc);
}

void patterns_destroy(patterns_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  free(task);
}

/* Every submenu leaf fires wuss_EVENT_PRE_SUBMENU_OPEN; only the colour
 * rows retarget the shared colourmenu. */
static result_t patterns_pre_submenu_open(patterns_task_t    *bc,
                                          const wuss_event_t *event)
{
  result_t rc;
  int      index;

  if (wuss_colourlist_pre_open(&bc->colours, bc->wuss, event, &rc))
    return rc;

  index = event->data.pre_submenu_open.index;

  return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle, index,
                                    bc->menu_items[index].submenu);
}

static result_t patterns_redraw(const wuss_event_t *event, void *task_data)
{
  patterns_task_t *bc;
  const box_t     *content;
  const box_t     *bounds;
  int              ox;
  int              nsteps;
  int              s;
  int              s0;
  int              s1;
  pattern_t        pat;
  box_t            col;
  box_t            clip;

  bc = task_data;

  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  /* the gradient spans the work area, anchored at its (0,0) in screen space
   * so it scrolls with the content and every dirty rectangle shares one
   * dither phase */
  ox     = bounds->x0 - event->data.redraw.scroll.x;
  nsteps = (bounds->x1 - bounds->x0 + PATTERNS_STEP - 1) / PATTERNS_STEP;

  s0 = MAX(0, (content->x0 - ox) / PATTERNS_STEP);
  s1 = MIN(nsteps - 1, (content->x1 - 1 - ox) / PATTERNS_STEP);

  for (s = s0; s <= s1; s++)
  {
    pat          = patterns_step_pattern(bc, s, MAX(nsteps - 1, 1));
    pat.origin.x = ox;
    pat.origin.y = bounds->y0 - event->data.redraw.scroll.y;

    col.x0 = ox + s * PATTERNS_STEP;
    col.x1 = col.x0 + PATTERNS_STEP;
    col.y0 = content->y0;
    col.y1 = content->y1;
    if (!box_intersection(&col, content, &clip))
      screen_fill_pattern(event->data.redraw.scr, &clip, &pat);
  }

  return result_OK;
}

result_t patterns_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  patterns_task_t *bc;

  bc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return patterns_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != bc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;

    if (!(event->data.mouse.button & wuss_BUTTON_MENU))
      return result_OK;
    {
    }
    return wuss_menu_open_at_pointer(bc->delegate, &bc->menu,
                                     &bc->menu_handle);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != bc->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    if (!task_key_is_plain(bc->delegate, &bc->menu, event, &rc))
      return rc;

    return result_WUSS_KEY_UNCLAIMED;
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return patterns_pre_submenu_open(bc, event);

  case wuss_EVENT_MENU_SELECT:
    {
      result_t rc;

      rc = result_OK;
      if (wuss_menu_select_is(event, &bc->menu, PATTERNS_MENU_SAVE))
      {
        rc = wuss_saveas_open(bc->saveas);
      }
      else if (wuss_menu_select_is(event, &bc->menu, PATTERNS_MENU_RANDOMISE))
      {
        patterns_randomise(bc);
        wuss_window_invalidate_visible(bc->window);
      }
      else if (wuss_menu_select_is(event, &bc->menu, PATTERNS_MENU_RESET))
      {
        bc->a = colour_rgb(0x00, 0x00, 0x00);
        bc->b = colour_rgb(0xFF, 0xFF, 0xFF);
        wuss_window_invalidate_visible(bc->window);
      }
      else if (wuss_colourlist_selected(&bc->colours, event))
      {
        wuss_window_invalidate_visible(bc->window);
      }
      return rc;
    }

  case wuss_EVENT_MENU_CLOSED:
    bc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == bc->window)
      bc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    patterns_destroy(bc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
