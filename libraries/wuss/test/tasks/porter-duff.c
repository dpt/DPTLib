/* wuss/test/tasks/porter-duff.c -- animated Porter-Duff compositing task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"
#include "geom/point.h"
#include "io/path.h"

#include "porter-duff.h"
#include "common.h"
#include "snapshot.h"

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in porter_duff_task_t, not as a file-scope static, so that
 * each window's Info row can hold its own .window pointer to the shared
 * proginfo singleton, retargeted just before wuss_menu_open */
enum
{
  PORTER_DUFF_MENU_INFO,
  PORTER_DUFF_MENU_PAUSE,
  PORTER_DUFF_MENU_RULE,
  PORTER_DUFF_MENU_SWAP,
  PORTER_DUFF_MENU_SAVE
};

#define PORTER_DUFF_SAVE_NAME "porter-duff.png" /* written to the current dir */

#define PD_SIZE            (256) /* the demo images are 256x256 */
#define PD_LABEL_HEIGHT     (20) /* strip below the pane, for the rule name */
#define PD_CHECKER_BAND      (8) /* checkerboard square size, in pixels */
#define PD_FORMAT          (pixelfmt_bgra8888)

#define PD_FRAMES_DEFAULT  (120) /* ~2s per rule at 60 main-loop passes/sec */
#define PD_FRAMES_MIN       (10)
#define PD_FRAMES_MAX      (600)

/* ----------------------------------------------------------------------- */

/* These three are lifted from libraries/framebuf/composite/test/composite-test.c,
 * where they're static helpers rather than library functions: their memory
 * management is raw malloc and the original carries a FIXME saying as much, so
 * they're copied rather than promoted to framebuf. */

static result_t bitmap_clone_by_size(bitmap_t *cloned, const bitmap_t *src)
{
  size_t pixelbytes;
  void  *pixels;

  pixelbytes = src->size.h * src->rowbytes;
  pixels     = malloc(pixelbytes);
  if (pixels == NULL)
    return result_OOM;

  *cloned      = *src;
  cloned->base = pixels;

  return result_OK;
}

static result_t bitmap_clone_pixels(bitmap_t *dst, const bitmap_t *src)
{
  if (dst->size.w    != src->size.w  ||
      dst->size.h    != src->size.h  ||
      dst->format    != src->format  ||
      dst->rowbytes  != src->rowbytes)
    return result_INCOMPATIBLE;

  memcpy(dst->base, src->base, src->size.h * src->rowbytes);

  return result_OK;
}

/* Only the arms reachable from bitmap_load_png's output are implemented. */
static result_t bitmap_convert_inplace(bitmap_t *bm, pixelfmt_t new_fmt)
{
  pixelfmt_any_t *p;
  int             x, y;

  if (new_fmt != pixelfmt_bgra8888)
    return result_NOT_IMPLEMENTED;

  switch (bm->format)
  {
  case pixelfmt_bgra8888:
    return result_OK;

  case pixelfmt_rgbx8888:
    p = bm->base;
    for (y = 0; y < bm->size.h; y++)
      for (x = 0; x < bm->size.w; x++)
      {
        pixelfmt_rgbx8888_t px = *p;
        /* the x byte isn't a real alpha channel, so force it opaque */
        *p++ = PIXELFMT_MAKE_BGRA8888(PIXELFMT_Bxxx8888(px),
                                      PIXELFMT_xGxx8888(px),
                                      PIXELFMT_xxRx8888(px),
                                      0xFF);
      }
    bm->format = pixelfmt_bgra8888;
    return result_OK;

  case pixelfmt_rgba8888:
    p = bm->base;
    for (y = 0; y < bm->size.h; y++)
      for (x = 0; x < bm->size.w; x++)
      {
        pixelfmt_rgba8888_t px = *p;
        *p++ = PIXELFMT_MAKE_BGRA8888(PIXELFMT_Bxxx8888(px),
                                      PIXELFMT_xGxx8888(px),
                                      PIXELFMT_xxRx8888(px),
                                      PIXELFMT_xxxA8888(px));
      }
    bm->format = pixelfmt_bgra8888;
    return result_OK;

  default:
    return result_NOT_IMPLEMENTED;
  }
}

/* ----------------------------------------------------------------------- */

static const char *const rule_names[composite_RULE__LIMIT] =
{
  "CLEAR",
  "SRC",
  "DST",
  "SRC OVER",
  "DST OVER",
  "SRC IN",
  "DST IN",
  "SRC OUT",
  "DST OUT",
  "SRC ATOP",
  "DST ATOP",
  "XOR"
};

static result_t load_demo_png(bitmap_t   *bm,
                              const char *resources,
                              const char *leafname)
{
  result_t    rc;
  const char *filename;

  filename = pathf("%s/resources/composite/%s.png", resources, leafname);

  rc = bitmap_load_png(bm, filename);
  if (rc != result_OK)
    return rc;

  rc = bitmap_convert_inplace(bm, PD_FORMAT);
  if (rc != result_OK)
  {
    free(bm->base);
    return rc;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t porter_duff_create(wuss_t *wuss, porter_duff_task_t **out)
{
  result_t            rc;
  porter_duff_task_t *task;
  wuss_task_t        *delegate;
  wuss_task_desc_t    delegate_desc;
  const char         *resources;
  const colour_t     *palette;
  int                 i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss            = wuss;
  palette               = wuss_get_palette(wuss, NULL);
  task->font            = wuss_get_font_n(wuss, 0);
  task->rule            = composite_RULE_CLEAR;
  task->frame           = 0;
  task->frames_per_rule = PD_FRAMES_DEFAULT;
  task->light           = palette[palette_PICO8_LIGHT_GREY];
  task->dark            = palette[palette_PICO8_DARK_GREY];
  task->fg              = palette[palette_PICO8_WHITE];
  task->bg              = palette[palette_PICO8_BLACK];

  resources = wuss_get_resources(wuss);

  rc = load_demo_png(&task->a, resources, "A");
  if (rc != result_OK)
    return rc;

  rc = load_demo_png(&task->b, resources, "B");
  if (rc != result_OK)
    goto free_a;

  rc = bitmap_clone_by_size(&task->src, &task->a);
  if (rc != result_OK)
    goto free_b;

  rc = bitmap_clone_by_size(&task->dst, &task->b);
  if (rc != result_OK)
    goto free_src;

  /* porter_duff_redraw paints every pixel itself */
  delegate_desc.handle    = porter_duff_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "porter-duff";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
    goto free_dst; /* nothing registered yet; nobody else owns task */
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate,
                          SIZE2D(PD_SIZE, PD_SIZE + PD_LABEL_HEIGHT),
                          "Porter-Duff",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* QUIT frees the four bitmaps and task */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, PORTER_DUFF_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * porter_duff_handle */

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PORTER_DUFF_MENU_PAUSE, "Pause",
                          wuss_MENU_ITEM_NONE, "SPACE");

  for (i = 0; i < composite_RULE__LIMIT; i++)
    WUSS_MENU_ITEM(task->rule_items, i, rule_names[i], wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->rule_menu, "Rule", task->rule_items,
                 NELEMS(task->rule_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, PORTER_DUFF_MENU_RULE, "Rule",
                      wuss_MENU_ITEM_NONE, &task->rule_menu);

  WUSS_MENU_ITEM(task->menu_items, PORTER_DUFF_MENU_SWAP, "Swap images",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PORTER_DUFF_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "^S");

  WUSS_MENU_TITLE(task->menu, "Porter-Duff", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;

free_dst:
  free(task->dst.base);
free_src:
  free(task->src.base);
free_b:
  free(task->b.base);
free_a:
  free(task->a.base);
  free(task); /* no task was registered on any goto here; nobody else owns it */

  return rc;
}

void porter_duff_destroy(porter_duff_task_t *task)
{
  free(task->dst.base);
  free(task->src.base);
  free(task->b.base);
  free(task->a.base);
  free(task);
}

/* ----------------------------------------------------------------------- */

/* Triangle ramp: 0 at the start of the rule's turn, 255 at its midpoint, back
 * to 0 at its end. */
static int porter_duff_ramp(const porter_duff_task_t *pd)
{
  int half;

  half = pd->frames_per_rule / 2;
  if (half <= 0)
    return 255;

  if (pd->frame < half)
    return pd->frame * 255 / half;
  else
    return MAX(0, (pd->frames_per_rule - pd->frame) * 255 / half);
}

/* Copy "a" into "src", scaling its alpha channel by "ramp" (0..255). The
 * library's compositing works on non-premultiplied values, so the colour
 * components are left alone. */
static void porter_duff_ramp_src(porter_duff_task_t *pd, int ramp)
{
  pixelfmt_any_t *sp;
  pixelfmt_any_t *dp;
  int             x, y;

  sp = pd->a.base;
  dp = pd->src.base;

  for (y = 0; y < pd->a.size.h; y++)
    for (x = 0; x < pd->a.size.w; x++)
    {
      pixelfmt_any_t px = *sp++;
      unsigned int   alpha;

      alpha = PIXELFMT_xxxA8888(px) * ramp / 255;
      *dp++ = (px & ~PIXELFMT_xxxA8888_MASK) |
              ((pixelfmt_any_t) alpha << PIXELFMT_xxxA8888_SHIFT);
    }
}

static void porter_duff_draw_checkerboard(const porter_duff_task_t *pd,
                                          screen_t                 *scr,
                                          const box_t              *content,
                                          const box_t              *bounds,
                                          int                       sx,
                                          int                       sy)
{
  pixelfmt_any_t darkpix, lightpix;
  int            ox, oy;
  int            col0, row0, col1, row1;
  int            col, row;

  /* resolve both colours once, not once per pixel */
  darkpix  = screen_colour_to_pixel(scr, pd->dark);
  lightpix = screen_colour_to_pixel(scr, pd->light);

  /* screen position of document (0, 0); content lies within bounds, so the
   * square indices below are never negative */
  ox = bounds->x0 - sx;
  oy = bounds->y0 - sy;

  col0 = (content->x0 - ox) / PD_CHECKER_BAND;
  row0 = (content->y0 - oy) / PD_CHECKER_BAND;
  col1 = (content->x1 - ox + PD_CHECKER_BAND - 1) / PD_CHECKER_BAND;
  row1 = (content->y1 - oy + PD_CHECKER_BAND - 1) / PD_CHECKER_BAND;

  /* one rect fill per square, trimmed to the content box */
  for (row = row0; row < row1; row++)
    for (col = col0; col < col1; col++)
    {
      box_t square, visible;

      square.x0 = ox + col * PD_CHECKER_BAND;
      square.y0 = oy + row * PD_CHECKER_BAND;
      square.x1 = square.x0 + PD_CHECKER_BAND;
      square.y1 = square.y0 + PD_CHECKER_BAND;
      if (box_intersection(&square, content, &visible))
        continue;

      screen_fill_rect_value(scr, visible.x0, visible.y0, box_size(&visible),
                             ((row + col) & 1) ? darkpix : lightpix);
    }
}

static result_t porter_duff_redraw(const wuss_event_t *event,
                                   void               *task_data)
{
  result_t            rc;
  porter_duff_task_t *pd;
  screen_t           *scr;
  const box_t        *content, *bounds;
  const char         *name;
  point_t             pos;
  int                 sx, sy;

  pd = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  porter_duff_draw_checkerboard(pd, scr, content, bounds, sx, sy);

  /* ponytail: the whole 256x256 pane is recomposited on every redraw -- two
   * full-image memcpys plus two full-image passes. Fine for one window in a
   * test harness; if the main loop ever gets tight, cache the composited
   * bitmap and rebuild it only when the ramp value or rule actually changes. */
  rc = bitmap_clone_pixels(&pd->dst, &pd->b);
  if (rc != result_OK)
    return rc;

  porter_duff_ramp_src(pd, porter_duff_ramp(pd));

  rc = composite(pd->rule, &pd->src, &pd->dst);
  if (rc != result_OK)
    return rc;

  screen_copy_bitmap(scr, bounds->x0 - sx, bounds->y0 - sy, &pd->dst);

  {
    int ascent;

    bmfont_get_info(pd->font, NULL, NULL, &ascent, NULL);
    name  = rule_names[pd->rule];
    pos.x = bounds->x0 - sx + 2;
    pos.y = bounds->y0 - sy + PD_SIZE + 2 + ascent;
  }

  return bmfont_draw(pd->font, scr, name, (int) strlen(name),
                     pd->fg, pd->bg, NULL, &pos, NULL);
}

static result_t porter_duff_idle(void *task_data)
{
  porter_duff_task_t *pd;

  pd = task_data;

  /* the shared proginfo singleton is a second window on this same
   * (autoclose) delegate while its dialogue is open, so closing the main
   * window alone doesn't necessarily empty task->windows immediately --
   * guard against the dangling window in the meantime */
  if (pd->window == NULL)
    return result_OK;

  if (pd->paused)
    return result_OK;

  if (++pd->frame >= pd->frames_per_rule)
  {
    pd->frame = 0;
    pd->rule  = (pd->rule + 1) % composite_RULE__LIMIT;
  }

  /* the ramp changes every frame, so the whole pane is stale every frame */
  wuss_window_invalidate_visible(pd->window);

  return result_OK;
}

/* step the rule by dir (+1 or -1), wrapping round; paused, restart at the
 * ramp's midpoint, as porter_duff_pick_rule does */
static result_t porter_duff_mouse(wuss_window_t *window,
                                  int            dir,
                                  void          *task_data)
{
  porter_duff_task_t *pd;

  pd = task_data;

  pd->rule  = (pd->rule + composite_RULE__LIMIT + dir) %
              composite_RULE__LIMIT;
  pd->frame = pd->paused ? pd->frames_per_rule / 2 : 0;

  wuss_window_invalidate_visible(window);

  return result_OK;
}

static result_t porter_duff_scroll(wuss_window_t *window,
                                   int            delta,
                                   void          *task_data)
{
  porter_duff_task_t *pd;

  pd = task_data;

  pd->frames_per_rule += delta * 10;
  pd->frames_per_rule  = CLAMP(pd->frames_per_rule,
                               PD_FRAMES_MIN, PD_FRAMES_MAX);
  pd->frame            = MIN(pd->frame, pd->frames_per_rule);

  wuss_window_invalidate_visible(window);

  return result_OK;
}

/* Left/Right step to the previous/next rule, as Adjust and Select clicks
 * do. Anything else is passed back unclaimed. */
static result_t porter_duff_key(porter_duff_task_t *pd,
                                wuss_window_t      *window,
                                int                 code)
{
  switch (code)
  {
  case wuss_KEY_LEFT:  return porter_duff_mouse(window, -1, pd);
  case wuss_KEY_RIGHT: return porter_duff_mouse(window, +1, pd);
  default:             return result_WUSS_KEY_UNCLAIMED;
  }
}

/* The "Pause" row stops or restarts the idle animation; the "Swap" row
 * exchanges the source and destination images (with their scratch bitmaps,
 * which are sized to match). An ADJUST pick keeps the menu open, so retick
 * the live row; a SELECT pick has already closed it. */
static result_t porter_duff_toggle(porter_duff_task_t *pd,
                                   const wuss_event_t *event)
{
  int      index;
  int     *flag;
  bitmap_t t;

  index = event->data.menu_select.index;

  if (index == PORTER_DUFF_MENU_SWAP)
  {
    t       = pd->a;
    pd->a   = pd->b;
    pd->b   = t;
    t       = pd->src;
    pd->src = pd->dst;
    pd->dst = t;
    flag    = &pd->swapped;
    wuss_window_invalidate_visible(pd->window);
  }
  else
  {
    flag = &pd->paused;
  }
  *flag = !*flag;

  wuss_menu_tick_item_live(pd->menu_handle, &pd->menu, index, *flag);

  return result_OK;
}

/* A "Rule" submenu pick: jump straight to that rule. Paused, restart it at
 * the ramp's midpoint rather than its start, where the source is fully
 * transparent and the rule would look like DST. */
static result_t porter_duff_pick_rule(porter_duff_task_t *pd,
                                      const wuss_event_t *event)
{
  pd->rule  = (composite_rule_t) event->data.menu_select.index;
  pd->frame = pd->paused ? pd->frames_per_rule / 2 : 0;

  wuss_menu_tick_exclusive_live(pd->menu_handle, &pd->rule_menu, pd->rule);

  wuss_window_invalidate_visible(pd->window);

  return result_OK;
}

result_t porter_duff_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  porter_duff_task_t *pd;

  pd = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return porter_duff_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != pd->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    if (event->data.mouse.button & wuss_BUTTON_MENU)
    {
      static const wuss_proginfo_desc_t desc =
        TASK_PROGINFO_DESC("Porter-Duff",
                           "Animated Porter-Duff compositing demo");

      wuss_proginfo_set_desc(&desc);
      pd->menu_items[PORTER_DUFF_MENU_INFO].window =
        wuss_proginfo_window(pd->delegate);

      wuss_menu_tick_item(&pd->menu, PORTER_DUFF_MENU_PAUSE, pd->paused);
      wuss_menu_tick_item(&pd->menu, PORTER_DUFF_MENU_SWAP, pd->swapped);
      wuss_menu_tick_exclusive(&pd->rule_menu, pd->rule);

      return wuss_menu_open_at_pointer(pd->delegate, &pd->menu,
                                       &pd->menu_handle);
    }
    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      return porter_duff_mouse(window, +1, task_data);
    if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      return porter_duff_mouse(window, -1, task_data);
    return result_OK;

  case wuss_EVENT_SCROLL:
    return porter_duff_scroll(window, event->data.scroll.delta, task_data);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != pd->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    if (!task_key_is_plain(pd->delegate, &pd->menu, event, &rc))
      return rc;

    return porter_duff_key(pd, window, event->data.key.code);
  }

  case wuss_EVENT_IDLE:
    return porter_duff_idle(task_data);

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &pd->menu &&
        (event->data.menu_select.index == PORTER_DUFF_MENU_PAUSE ||
         event->data.menu_select.index == PORTER_DUFF_MENU_SWAP))
      return porter_duff_toggle(pd, event);
    if (event->data.menu_select.menu == &pd->menu &&
        event->data.menu_select.index == PORTER_DUFF_MENU_SAVE)
      return snapshot_save_png(pd->window, porter_duff_handle, pd,
                               PORTER_DUFF_SAVE_NAME);
    if (event->data.menu_select.menu == &pd->rule_menu)
      return porter_duff_pick_rule(pd, event);
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    pd->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == pd->window)
      pd->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == pd->menu_items[PORTER_DUFF_MENU_INFO].window)
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
    porter_duff_destroy(pd);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
