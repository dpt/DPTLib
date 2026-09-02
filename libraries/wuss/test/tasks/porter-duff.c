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
  const char *leafname_ext;
  const char *filename;
  result_t    rc;

  leafname_ext = path_join_leafname(leafname, "png");
  filename     = path_join_filename(resources, 3,
                                    "resources", "composite", leafname_ext);

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

result_t porter_duff_create(wuss_t             *wuss,
                            const colour_t     *palette,
                            bmfont_t           *font,
                            const char         *resources,
                            porter_duff_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t    rc;

  task->font            = font;
  task->rule            = composite_RULE_CLEAR;
  task->frame           = 0;
  task->frames_per_rule = PD_FRAMES_DEFAULT;
  task->light           = palette[palette_PICO8_LIGHT_GREY];
  task->dark            = palette[palette_PICO8_DARK_GREY];
  task->fg              = palette[palette_PICO8_WHITE];
  task->bg              = palette[palette_PICO8_BLACK];

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
    return rc;
  wuss_task_set_autoclose(delegate, 1);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(PD_SIZE, PD_SIZE + PD_LABEL_HEIGHT),
                                 "Porter-Duff",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                 SIZE2D(PD_SIZE, PD_SIZE + PD_LABEL_HEIGHT),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    goto free_dst;

  return result_OK;

free_dst:
  free(task->dst.base);
free_src:
  free(task->src.base);
free_b:
  free(task->b.base);
free_a:
  free(task->a.base);

  return rc;
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
                                          const box_t              *bounds)
{
  int x, y, lx, ly, band;

  for (y = content->y0; y < content->y1; y++)
    for (x = content->x0; x < content->x1; x++)
    {
      lx   = x - bounds->x0;
      ly   = y - bounds->y0;
      band = lx / PD_CHECKER_BAND + ly / PD_CHECKER_BAND;

      screen_set_pixel(scr, x, y, (band & 1) ? pd->dark : pd->light);
    }
}

static result_t porter_duff_redraw(const wuss_event_t *event,
                                   void               *task_data)
{
  porter_duff_task_t *pd;
  screen_t           *scr;
  const box_t        *content, *bounds;
  const char         *name;
  point_t             pos;
  result_t            rc;

  pd = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  porter_duff_draw_checkerboard(pd, scr, content, bounds);

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

  screen_draw_bitmap(scr, bounds->x0, bounds->y0, &pd->dst);

  name  = rule_names[pd->rule];
  pos.x = bounds->x0 + 2;
  pos.y = bounds->y0 + PD_SIZE + 2;

  return bmfont_draw(pd->font, scr, name, (int) strlen(name),
                     pd->fg, pd->bg, &pos, NULL);
}

static result_t porter_duff_idle(void *task_data)
{
  porter_duff_task_t *pd;

  pd = task_data;

  if (++pd->frame >= pd->frames_per_rule)
  {
    pd->frame = 0;
    pd->rule  = (pd->rule + 1) % composite_RULE__LIMIT;
  }

  /* the ramp changes every frame, so the whole pane is stale every frame */
  wuss_window_invalidate_all(pd->window);

  return result_OK;
}

static result_t porter_duff_mouse(wuss_window_t *window, void *task_data)
{
  porter_duff_task_t *pd;

  pd = task_data;

  pd->rule  = (pd->rule + 1) % composite_RULE__LIMIT;
  pd->frame = 0;

  wuss_window_invalidate_all(window);

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

  wuss_window_invalidate_all(window);

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
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return porter_duff_mouse(window, task_data);

  case wuss_EVENT_SCROLL:
    return porter_duff_scroll(window, event->data.scroll.delta, task_data);

  case wuss_EVENT_IDLE:
    return porter_duff_idle(task_data);

  case wuss_EVENT_QUIT:
    free(pd->dst.base);
    free(pd->src.base);
    free(pd->b.base);
    free(pd->a.base);
    free(pd); /* calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
