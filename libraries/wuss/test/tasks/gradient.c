/* wuss/test/tasks/gradient.c -- gradient fill task */

#ifdef WUSS_APP

#include <math.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include <stdio.h>
#include <string.h>

#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"

#include "gradient.h"
#include "snapshot.h"

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in gradient_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum
{
  GRADIENT_MENU_INFO,
  GRADIENT_MENU_RESET,
  GRADIENT_MENU_SHAPE,
  GRADIENT_MENU_SAVE
};

#define GRADIENT_SAVE_NAME "gradient.png" /* written to the current dir */

/* "Shape" submenu rows, indexed by gradient_shape_t */
static const char *const gradient_shape_names[gradient_NSHAPES] =
{
  "Linear", "Radial", "Conical"
};

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GRADIENT_DOC_WIDTH  400
#define GRADIENT_DOC_HEIGHT 400
#define GRADIENT_OPEN_WIDTH  100
#define GRADIENT_OPEN_HEIGHT 100

#define GRADIENT_DEFAULT_DITHER   1 /* 4x4, matching the original */
#define GRADIENT_UNITY          256 /* brightness/saturation of 1.0 */
#define GRADIENT_ADJUST_MAX     512
#define GRADIENT_DRAG_SCALE       2 /* adjust units per pixel dragged */

/* ordered (Bayer) dither matrices, each holding values 0 .. dim*dim-1.
 * SELECT/ADJUST clicks cycle task->dither_index forward/backward through
 * this table. */
static const struct
{
  int dim;
  int cell[64]; /* row-major, first dim*dim entries used */
}
gradient_dithers[] =
{
  {
    2,
    {
      0, 2,
      3, 1,
    }
  },
  {
    4,
    {
       0,  8,  2, 10,
      12,  4, 14,  6,
       3, 11,  1,  9,
      15,  7, 13,  5,
    }
  },
  {
    8,
    {
       0, 32,  8, 40,  2, 34, 10, 42,
      48, 16, 56, 24, 50, 18, 58, 26,
      12, 44,  4, 36, 14, 46,  6, 38,
      60, 28, 52, 20, 62, 30, 54, 22,
       3, 35, 11, 43,  1, 33,  9, 41,
      51, 19, 59, 27, 49, 17, 57, 25,
      15, 47,  7, 39, 13, 45,  5, 37,
      63, 31, 55, 23, 61, 29, 53, 21,
    }
  },
};

/* offset "v" by the dither cell for (x, y), spread across one "step" of the
 * target channel (-step/2 .. +step/2) whatever the matrix size, so a larger
 * matrix just gives a finer pattern rather than a louder one. A step of 1
 * (an 8-bit channel) adds nothing and only clamps. */
static int dither(int index, int step, int v, int x, int y)
{
  int dim, n, m;

  dim = gradient_dithers[index].dim;
  n   = dim * dim;
  m   = gradient_dithers[index].cell[(y % dim) * dim + (x % dim)];

  return CLAMP(v + m * step / n - step / 2, 0, 255);
}

/* the gap between adjacent representable 8-bit values of each R, G, B
 * channel in "fmt" */
static void channel_steps(pixelfmt_t fmt, int step[3])
{
  switch (fmt)
  {
  case pixelfmt_bgrx4444:
  case pixelfmt_rgbx4444:
  case pixelfmt_xbgr4444:
  case pixelfmt_xrgb4444:
    step[0] = step[1] = step[2] = 16;
    break;

  case pixelfmt_bgrx5551:
  case pixelfmt_rgbx5551:
  case pixelfmt_xbgr1555:
  case pixelfmt_xrgb1555:
    step[0] = step[1] = step[2] = 8;
    break;

  case pixelfmt_bgr565:
  case pixelfmt_rgb565:
    step[0] = step[2] = 8;
    step[1] = 4;
    break;

  case pixelfmt_y8:
  case pixelfmt_bgrx8888:
  case pixelfmt_rgbx8888:
  case pixelfmt_xbgr8888:
  case pixelfmt_xrgb8888:
  case pixelfmt_bgra8888:
  case pixelfmt_rgba8888:
  case pixelfmt_abgr8888:
  case pixelfmt_argb8888:
    step[0] = step[1] = step[2] = 1;
    break;

  default:
    /* ponytail: paletted gaps depend on the palette; keep the original
     * fixed 16 (+/-8) swing rather than analysing it */
    step[0] = step[1] = step[2] = 16;
    break;
  }
}

/* fully saturated hue for t in 0..1535 (six 256-step segments round the
 * colour wheel: red, yellow, green, cyan, blue, magenta, back to red) */
static void hue(int t, int rgb[3])
{
  int f;

  f = t & 255;
  switch ((t >> 8) % 6)
  {
  case 0:  rgb[0] = 255;     rgb[1] = f;       rgb[2] = 0;       break;
  case 1:  rgb[0] = 255 - f; rgb[1] = 255;     rgb[2] = 0;       break;
  case 2:  rgb[0] = 0;       rgb[1] = 255;     rgb[2] = f;       break;
  case 3:  rgb[0] = 0;       rgb[1] = 255 - f; rgb[2] = 255;     break;
  case 4:  rgb[0] = f;       rgb[1] = 0;       rgb[2] = 255;     break;
  default: rgb[0] = 255;     rgb[1] = 0;       rgb[2] = 255 - f; break;
  }
}

/* the unadjusted colour of document point (lx, ly) under "shape" */
static void shade(gradient_shape_t shape, int lx, int ly, int rgb[3])
{
  static const double max_r = GRADIENT_DOC_WIDTH / 2 * 1.4142136;

  int    dx, dy;
  double t;

  dx = lx - GRADIENT_DOC_WIDTH  / 2;
  dy = ly - GRADIENT_DOC_HEIGHT / 2;

  switch (shape)
  {
  default:
  case gradient_SHAPE_LINEAR:
    rgb[0] = lx * 255 / GRADIENT_DOC_WIDTH;
    rgb[1] = ly * 255 / GRADIENT_DOC_HEIGHT;
    rgb[2] = 255 - (lx + ly) * 255 / (GRADIENT_DOC_WIDTH + GRADIENT_DOC_HEIGHT);
    return;

  case gradient_SHAPE_RADIAL:
    /* 0 at the centre, 1 at a corner */
    t = sqrt((double) (dx * dx + dy * dy)) / max_r;
    break;

  case gradient_SHAPE_CONICAL:
    t = (atan2(dy, dx) + M_PI) / (2.0 * M_PI); /* 0..1 round the centre */
    break;
  }

  hue(CLAMP((int) (t * 1535.0), 0, 1535), rgb);
}

/* apply the task's saturation (lerp away from luma) then brightness (scale)
 * to an 8-bit RGB triple, in place; results may exceed 0..255 until
 * dither() clamps them */
static void adjust(const gradient_task_t *gc, int rgb[3])
{
  int luma, i;

  luma = (rgb[0] * 77 + rgb[1] * 150 + rgb[2] * 29) >> 8;

  for (i = 0; i < 3; i++)
  {
    rgb[i] = luma + (rgb[i] - luma) * gc->saturation / GRADIENT_UNITY;
    rgb[i] = rgb[i] * gc->brightness / GRADIENT_UNITY;
  }
}

result_t gradient_create(wuss_t *wuss, gradient_task_t **out)
{
  result_t         rc;
  gradient_task_t *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss         = wuss;
  task->dither_index = GRADIENT_DEFAULT_DITHER;
  task->brightness   = GRADIENT_UNITY;
  task->saturation   = GRADIENT_UNITY;

  /* gradient_redraw paints every pixel itself */
  delegate_desc.handle    = gradient_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "gradient";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(GRADIENT_OPEN_WIDTH, GRADIENT_OPEN_HEIGHT),
                                 "Gradient",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(GRADIENT_DOC_WIDTH, GRADIENT_DOC_HEIGHT),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, GRADIENT_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * gradient_mouse */

  WUSS_MENU_ITEM(task->menu_items, GRADIENT_MENU_RESET, "Reset",
                 wuss_MENU_ITEM_NONE);
  task->menu_items[GRADIENT_MENU_RESET].shortcut = "R";

  for (i = 0; i < gradient_NSHAPES; i++)
    WUSS_MENU_ITEM(task->shape_items, i, gradient_shape_names[i],
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->shape_menu, "Shape", task->shape_items,
                 NELEMS(task->shape_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, GRADIENT_MENU_SHAPE, "Shape",
                      wuss_MENU_ITEM_NONE, &task->shape_menu);

  WUSS_MENU_ITEM(task->menu_items, GRADIENT_MENU_SAVE, "Save PNG",
                 wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->menu, "Gradient", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void gradient_destroy(gradient_task_t *task)
{
  wuss_menu_close(task->menu_handle);
  free(task);
}

static result_t gradient_redraw(const wuss_event_t *event, void *task_data)
{
  gradient_task_t *gc;
  screen_t        *scr;
  const box_t     *content, *bounds;
  int              di, step[3], sx, sy, x, y, lx, ly;
  int              rgb[3];

  gc  = task_data;
  scr = event->data.redraw.scr;

  di  = gc->dither_index;

  channel_steps(pixelfmt_base(scr->format), step);

  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  sx      = event->data.redraw.scroll.x;
  sy      = event->data.redraw.scroll.y;

  for (y = content->y0; y < content->y1; y++)
  {
    for (x = content->x0; x < content->x1; x++)
    {
      lx = x - bounds->x0 + sx;
      ly = y - bounds->y0 + sy;

      shade(gc->shape, lx, ly, rgb);
      adjust(gc, rgb);

      screen_set_pixel(scr, x, y,
                       colour_rgb(dither(di, step[0], rgb[0], lx, ly),
                                  dither(di, step[1], rgb[1], lx, ly),
                                  dither(di, step[2], rgb[2], lx, ly)));
    }
  }

  /* matrix-size label, anchored at document (2, 2) so it scrolls with the
   * content: screen pos = content top-left - scroll + doc offset. Uses bounds,
   * not the per-redraw dirty piece, so it is drawn whole on a partial redraw */
  {
    bmfont_t   *font = wuss_get_font(gc->wuss);
    int       dim  = gradient_dithers[gc->dither_index].dim;
    char      label[32];
    colour_t    ink  = colour_rgb(0xFF, 0xFF, 0xFF);
    colour_t    bg   = colour_rgba(0, 0, 0, 0); /* transparent */

    snprintf(label, sizeof(label), "%dx%d%s b%d%% s%d%%",
             dim, dim, step[1] == 1 ? " (off)" : "",
             gc->brightness * 100 / GRADIENT_UNITY,
             gc->saturation * 100 / GRADIENT_UNITY);

    if (font != NULL)
    {
      int     ascent;
      point_t pos;

      bmfont_get_info(font, NULL, NULL, &ascent, NULL);
      pos = POINT(bounds->x0 - sx + 2, bounds->y0 - sy + 2 + ascent);
      wuss_text_draw(gc->wuss, 0, scr, label, (int) strlen(label), ink, bg,
                     &pos, NULL);
    }
  }

  return result_OK;
}

static result_t gradient_mouse(const wuss_event_t *event, void *task_data)
{
  gradient_task_t *gc;
  point_t          p;
  wuss_button_t    button;
  int              n;

  gc = task_data;
  p  = event->data.mouse.point;
  n  = (int) NELEMS(gradient_dithers);

  /* an ADJUST drag moves brightness/saturation; an ADJUST click with no
   * movement still steps the dither matrix backward, on release */
  if (event->data.mouse.action == wuss_MOUSE_MOVE)
  {
    if (!gc->dragging)
      return result_OK;

    gc->saturation = CLAMP(gc->saturation + (p.x - gc->drag_x) * GRADIENT_DRAG_SCALE,
                           0, GRADIENT_ADJUST_MAX);
    gc->brightness = CLAMP(gc->brightness - (p.y - gc->drag_y) * GRADIENT_DRAG_SCALE,
                           0, GRADIENT_ADJUST_MAX);
    gc->drag_x  = p.x;
    gc->drag_y  = p.y;
    gc->dragged = 1;
    wuss_window_invalidate_visible(gc->window);

    return result_OK;
  }

  if (event->data.mouse.action == wuss_MOUSE_UP)
  {
    if (!gc->dragging)
      return result_OK;

    gc->dragging = 0;
    if (gc->dragged)
      return result_OK;

    gc->dither_index = (gc->dither_index + n - 1) % n;
    wuss_window_invalidate_visible(gc->window);

    return result_OK;
  }

  button = event->data.mouse.button;

  if (button & wuss_BUTTON_MENU)
  {
    static const wuss_proginfo_desc_t desc =
    {
      "Gradient",
      "Colour gradients with ordered dithering",
      "© DPTLib contributors",
      "1.0 (" __DATE__ ")"
    };
    wuss_proginfo_set_desc(&desc);
    gc->menu_items[GRADIENT_MENU_INFO].window =
      wuss_proginfo_window(gc->delegate);
    wuss_menu_tick_exclusive(&gc->shape_menu, gc->shape);

    return wuss_menu_open(gc->delegate, &gc->menu,
                          wuss_get_pointer(gc->wuss), &gc->menu_handle);
  }

  if (button & wuss_BUTTON_ADJUST)
  {
    gc->dragging = 1;
    gc->dragged  = 0;
    gc->drag_x   = p.x;
    gc->drag_y   = p.y;

    return result_OK;
  }

  if (!(button & wuss_BUTTON_SELECT))
    return result_OK;

  gc->dither_index = (gc->dither_index + 1) % n;

  wuss_window_invalidate_visible(gc->window); /* whole fill changes */

  return result_OK;
}

/* restore the default shape, dither matrix, brightness and saturation */
static void gradient_reset(gradient_task_t *gc)
{
  gc->shape        = gradient_SHAPE_LINEAR;
  gc->dither_index = GRADIENT_DEFAULT_DITHER;
  gc->brightness   = GRADIENT_UNITY;
  gc->saturation   = GRADIENT_UNITY;
  wuss_window_invalidate_visible(gc->window);
}

/* Menu pick: a Shape row switches the fill; Reset restores the default
 * shape, dither matrix, brightness and saturation; Save PNG writes the
 * window's content, at its current scroll, out. A SELECT pick has already
 * closed and freed the chain, so drop the handle then.
 * ponytail: fixed filename in the current dir; wuss has no save dialogue. */
static result_t gradient_menu_select(gradient_task_t    *gc,
                                     const wuss_event_t *event)
{
  if (event->data.menu_select.menu == &gc->shape_menu)
  {
    gc->shape = (gradient_shape_t) event->data.menu_select.index;
    wuss_menu_tick_exclusive_live(gc->menu_handle, &gc->shape_menu,
                                  gc->shape);
    if (!wuss_menu_should_keep_open(event))
      gc->menu_handle = NULL;
    wuss_window_invalidate_visible(gc->window);

    return result_OK;
  }

  if (event->data.menu_select.menu != &gc->menu)
    return result_OK;

  if (!wuss_menu_should_keep_open(event))
    gc->menu_handle = NULL;

  if (event->data.menu_select.index == GRADIENT_MENU_SAVE)
    return snapshot_save_png(gc->window, gradient_handle, gc,
                             GRADIENT_SAVE_NAME);

  if (event->data.menu_select.index != GRADIENT_MENU_RESET)
    return result_OK;

  gradient_reset(gc);

  return result_OK;
}

/* S steps the shape and D the dither matrix forward; R resets, as Menu >
 * Reset does. The arrow keys are left unclaimed for scrolling, as is
 * anything else or a key aimed at the proginfo dialogue. */
static result_t gradient_key(gradient_task_t *gc,
                             wuss_window_t   *window,
                             int              code)
{
  if (window != gc->window)
    return result_WUSS_KEY_UNCLAIMED;

  switch (code)
  {
  case 'S':
  case 's':
    gc->shape = (gradient_shape_t) ((gc->shape + 1) % gradient_NSHAPES);
    break;

  case 'D':
  case 'd':
    gc->dither_index = (gc->dither_index + 1) %
                       (int) NELEMS(gradient_dithers);
    break;

  case 'R':
  case 'r':
    gradient_reset(gc);
    return result_OK;

  default:
    return result_WUSS_KEY_UNCLAIMED;
  }

  wuss_window_invalidate_visible(window);

  return result_OK;
}

result_t gradient_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  gradient_task_t *gc;

  gc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return gradient_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != gc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    return gradient_mouse(event, task_data);

  case wuss_EVENT_KEY:
    return gradient_key(gc, window, event->data.key.code);

  case wuss_EVENT_MENU_SELECT:
    return gradient_menu_select(gc, event);

  case wuss_EVENT_MENU_CLOSED:
    gc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == gc->menu_items[GRADIENT_MENU_INFO].window)
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
    gradient_destroy(gc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
