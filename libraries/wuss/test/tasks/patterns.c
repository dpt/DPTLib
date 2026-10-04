/* wuss/test/tasks/patterns.c -- dithered colour gradient task */

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
#include "io/filetype.h"
#include "utils/rng.h"

#include "patterns.h"
#include "common.h"
#include "snapshot.h"

#define PATTERNS_STEP 4 /* pixels per gradient step */

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in patterns_task_t, not as a file-scope static, so that
 * each window's Info row can hold its own .window pointer to the shared
 * proginfo singleton, retargeted just before wuss_menu_open */
enum
{
  PATTERNS_MENU_INFO,
  PATTERNS_MENU_COLOUR_A,
  PATTERNS_MENU_COLOUR_B,
  PATTERNS_MENU_SAVE
};

#define PATTERNS_SAVE_NAME "patterns.png" /* Save As's initial leafname */

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

result_t patterns_create(wuss_t *wuss, patterns_task_t **out)
{
  result_t         rc;
  patterns_task_t *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  filetype_t       png_type;
  rng_t            rng;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  rng_seed(&rng, (uint32_t) time(NULL));
  task->a    = patterns_random_colour(&rng);
  task->b    = patterns_random_colour(&rng);

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

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, wuss, &png_type,
                          PATTERNS_SAVE_NAME, patterns_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, PATTERNS_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * patterns_handle */

  /* both rows open the shared colourmenu; patterns_pre_submenu_open retitles
   * it and picks which colour a pick lands in */
  WUSS_MENU_ITEM_MENU(task->menu_items, PATTERNS_MENU_COLOUR_A, "Colour A",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));
  WUSS_MENU_ITEM_MENU(task->menu_items, PATTERNS_MENU_COLOUR_B, "Colour B",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PATTERNS_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");
  /* hover opens the Save As dialogue as a submenu; ^S shows it standalone */
  task->menu_items[PATTERNS_MENU_SAVE].window =
    wuss_saveas_window(task->saveas);

  WUSS_MENU_TITLE(task->menu, "Patterns", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void patterns_destroy(patterns_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  free(task);
}

/* The Colour A and Colour B rows' submenu: the shared colourmenu singleton,
 * retitled and retargeted at the matching colour each time it opens. */
static result_t patterns_pre_submenu_open(patterns_task_t    *bc,
                                          const wuss_event_t *event)
{
  const char *title;

  if (event->data.pre_submenu_open.index == PATTERNS_MENU_COLOUR_A)
  {
    bc->colourmenu_target = &bc->a;
    title                 = "Colour A";
  }
  else
  {
    bc->colourmenu_target = &bc->b;
    title                 = "Colour B";
  }

  return wuss_colourmenu_open_rgb(bc->wuss, event, title,
                                  *bc->colourmenu_target);
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
      static const wuss_proginfo_desc_t desc =
        TASK_PROGINFO_DESC("Patterns",
                           "Ordered-dither gradient between two chosen colours");
      wuss_proginfo_set_desc(&desc);
      bc->menu_items[PATTERNS_MENU_INFO].window =
        wuss_proginfo_window(bc->delegate);
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
      if (event->data.menu_select.menu == &bc->menu &&
          event->data.menu_select.index == PATTERNS_MENU_SAVE)
      {
        rc = wuss_saveas_open(bc->saveas);
      }
      else if (bc->colourmenu_target != NULL &&
               wuss_colourmenu_selected_rgb(event, bc->colourmenu_target))
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
