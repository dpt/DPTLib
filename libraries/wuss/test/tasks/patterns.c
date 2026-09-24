/* wuss/test/tasks/patterns.c -- dithered colour-blending task */

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

#define PATTERNS_FPS 60 /* idle ticks per second */

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in patterns_task_t, not as a file-scope static, so that
 * each window's Info row can hold its own .window pointer to the shared
 * proginfo singleton, retargeted just before wuss_menu_open */
enum { PATTERNS_MENU_INFO, PATTERNS_MENU_SPEED };

/* one entry per row of the "Speed" submenu: seconds per a-to-b blend */
static const struct
{
  const char *name;
  int         seconds;
}
patterns_speeds[PATTERNS_NSPEEDS] =
{
  { "1s",   1 },
  { "2s",   2 },
  { "5s",   5 },
  { "10s", 10 },
  { "15s", 15 }
};

#define PATTERNS_DEFAULT_SPEED 2 /* index into patterns_speeds: 5s */

static colour_t patterns_random_colour(rng_t *rng)
{
  int r, g, b;

  r = rng_range(rng, 256);
  g = rng_range(rng, 256);
  b = rng_range(rng, 256);

  return colour_rgb(r, g, b);
}

/* channel c0 -> c1, f of n frames into the blend */
static int patterns_lerp(unsigned int c0, unsigned int c1, int f, int n)
{
  return (int) (c0 * (n - f) + c1 * f) / n;
}

/* blend a->b by frame_count and pick the nearest dither of the result */
static void patterns_update_pattern(patterns_task_t *bc)
{
  unsigned int    ar, ag, ab;
  unsigned int    br, bg, bb;
  int             f;
  int             n;
  colour_t        mix;
  const colour_t *palette;
  int             npalette;

  colour_get_rgb(&bc->a, &ar, &ag, &ab);
  colour_get_rgb(&bc->b, &br, &bg, &bb);

  f   = bc->frame_count;
  n   = bc->blend_frames;
  mix = colour_rgb(patterns_lerp(ar, br, f, n),
                   patterns_lerp(ag, bg, f, n),
                   patterns_lerp(ab, bb, f, n));

  palette     = wuss_get_palette(bc->wuss, &npalette);
  bc->pattern = pattern_from_colour(palette, npalette, mix);
}

/* switch the blend period to patterns_speeds[idx], rescaling frame_count so
 * the current blend carries on from the same colour */
static result_t patterns_set_speed(patterns_task_t *bc, int idx)
{
  int n;

  if (idx < 0 || idx >= PATTERNS_NSPEEDS)
    return result_OK;

  n                = patterns_speeds[idx].seconds * PATTERNS_FPS;
  bc->frame_count  = bc->frame_count * n / bc->blend_frames;
  bc->blend_frames = n;

  wuss_menu_tick_exclusive(&bc->speed_menu, idx);
  wuss_menu_tick_exclusive_live(bc->menu_handle, &bc->speed_menu, idx);

  return result_OK;
}

result_t patterns_create(wuss_t *wuss, patterns_task_t **out)
{
  result_t         rc;
  patterns_task_t *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  rng_seed(&task->rng, (uint32_t) time(NULL));
  task->a           = patterns_random_colour(&task->rng);
  task->b           = patterns_random_colour(&task->rng);
  task->blend_frames = patterns_speeds[PATTERNS_DEFAULT_SPEED].seconds *
                       PATTERNS_FPS;
  task->frame_count  = 0;
  patterns_update_pattern(task);

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

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(200, 160),
                                 "Patterns",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(200, 160),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, PATTERNS_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * patterns_handle */

  for (i = 0; i < PATTERNS_NSPEEDS; i++)
    WUSS_MENU_ITEM(task->speed_items, i, patterns_speeds[i].name,
                   i == PATTERNS_DEFAULT_SPEED ? wuss_MENU_ITEM_TICKED
                                               : wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->speed_menu, "Speed", task->speed_items,
                  NELEMS(task->speed_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, PATTERNS_MENU_SPEED, "Speed",
                      wuss_MENU_ITEM_NONE, &task->speed_menu);

  WUSS_MENU_TITLE(task->menu, "Patterns", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void patterns_destroy(patterns_task_t *task)
{
  wuss_menu_close(task->menu_handle);
  free(task);
}

static result_t patterns_idle(void *task_data)
{
  patterns_task_t *bc;

  bc = task_data;

  /* the proginfo dialogue is a second window on this same (autoclose)
   * delegate, so closing the main window alone never empties task->windows
   * and the task lingers until the dialogue closes too -- guard against the
   * dangling window in the meantime */
  if (bc->window == NULL)
    return result_OK;

  if (++bc->frame_count >= bc->blend_frames)
  {
    bc->frame_count = 0;
    bc->a           = bc->b;
    bc->b           = patterns_random_colour(&bc->rng);
  }

  patterns_update_pattern(bc);
  wuss_window_invalidate_visible(bc->window);

  return result_OK;
}

static result_t patterns_redraw(const wuss_event_t *event, void *task_data)
{
  patterns_task_t *bc;
  const box_t     *content;
  const box_t     *bounds;
  pattern_t        pat;

  bc = task_data;

  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  /* anchor the tile at the work area's (0,0) in screen space, so every
   * dirty rectangle shares one phase and it scrolls with the content */
  pat          = bc->pattern;
  pat.origin.x = bounds->x0 - event->data.redraw.scroll.x;
  pat.origin.y = bounds->y0 - event->data.redraw.scroll.y;

  screen_fill_pattern(event->data.redraw.scr, content, &pat);

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
  case wuss_EVENT_IDLE:
    return patterns_idle(task_data);

  case wuss_EVENT_REDRAW:
    return patterns_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != bc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN ||
        !(event->data.mouse.button & wuss_BUTTON_MENU))
      return result_OK;
    {
      static const wuss_proginfo_desc_t desc =
      {
        "Patterns",
        "Ordered-dither blend between random colours",
        "(c) DPTLib contributors",
        "1.0 (" __DATE__ ")"
      };
      wuss_proginfo_set_desc(&desc);
      bc->menu_items[PATTERNS_MENU_INFO].window =
        wuss_proginfo_window(bc->delegate);
    }
    return wuss_menu_open(bc->delegate, &bc->menu,
                          wuss_get_pointer(bc->wuss), &bc->menu_handle);

  case wuss_EVENT_MENU_SELECT:
    {
      result_t rc;

      rc = result_OK;
      if (event->data.menu_select.menu == &bc->speed_menu)
        rc = patterns_set_speed(bc, event->data.menu_select.index);
      if (!wuss_menu_should_keep_open(event))
        bc->menu_handle = NULL;
      return rc;
    }

  case wuss_EVENT_MENU_CLOSED:
    bc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == bc->window)
      bc->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == bc->menu_items[PATTERNS_MENU_INFO].window)
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
    patterns_destroy(bc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
