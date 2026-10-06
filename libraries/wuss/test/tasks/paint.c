/* wuss/test/tasks/paint.c -- basic painting task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "paint.h"
#include "taskapp.h"
#include "common.h"

#define PAINT_CANVAS_W 640
#define PAINT_CANVAS_H 480
#define PAINT_VIEW_W   400 /* initial visible size of the canvas window */
#define PAINT_VIEW_H   300

/* Toolbox layout, in its window's content space: tool radios, the rect Fill
 * option, the size slider, then a grid of colour swatches and the current
 * colours. The swatches and the colour chips are drawn by paint_toolbox_redraw
 * and hit-tested by paint_toolbox_mouse, not icons */
#define TB_W           96
#define TB_H           260
#define TB_MARGIN      5
#define TB_ROW         18  /* pitch of the tool radios */
#define TB_SLIDER_Y    116
#define TB_SWATCH_COLS 4
#define TB_SWATCH_ROWS 4
#define TB_SWATCH      20  /* swatch size */
#define TB_SWATCH_GAP  2
#define TB_SWATCH_Y    144
#define TB_CHIPS_Y     238

#define PAINT_NSWATCHES (TB_SWATCH_COLS * TB_SWATCH_ROWS)

static const char *paint_tool_names[paint_TOOL__COUNT] =
{
  "Brush",
  "Eraser",
  "Line",
  "Rect",
  "Flood fill"
};

/* the fixed palette, as RGB, in swatch order */
static const unsigned long paint_swatches[PAINT_NSWATCHES] =
{
  0x000000, 0xFFFFFF, 0x808080, 0xC0C0C0,
  0xFF0000, 0x800000, 0xFF8000, 0xFFFF00,
  0x00C000, 0x006000, 0x00FFFF, 0x0000FF,
  0x000080, 0xFF00FF, 0x800080, 0x804000
};

static colour_t paint_swatch_colour(int i)
{
  unsigned long rgb;

  rgb = paint_swatches[i];

  return colour_rgb((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

/* the swatch's box in toolbox content space */
static box_t paint_swatch_box(int i)
{
  int   x, y;
  box_t box;

  x = TB_MARGIN + (i % TB_SWATCH_COLS) * (TB_SWATCH + TB_SWATCH_GAP);
  y = TB_SWATCH_Y + (i / TB_SWATCH_COLS) * (TB_SWATCH + TB_SWATCH_GAP);

  box = (box_t) BOX_POS_SIZE(x, y, TB_SWATCH, TB_SWATCH);

  return box;
}

/* ----------------------------------------------------------------------- */

/* the tool radios, Fill option and size slider */
static result_t paint_toolbox_icons(paint_task_t *pt)
{
  result_t         rc;
  wuss_icon_spec_t specs[paint_TOOL__COUNT + 3];
  wuss_icon_t     *icons[paint_TOOL__COUNT + 3];
  int              i, n;

  n = 0;

  for (i = 0; i < paint_TOOL__COUNT; i++)
    wuss_icon_spec_radio(&specs[n++],
                         (box_t) BOX_POS_SIZE(TB_MARGIN, TB_MARGIN + i * TB_ROW,
                                              TB_W - 2 * TB_MARGIN, 16),
                         paint_tool_names[i],
                         1);

  wuss_icon_spec_option(&specs[n++],
                        (box_t) BOX_POS_SIZE(TB_MARGIN,
                                             TB_MARGIN + i * TB_ROW,
                                             TB_W - 2 * TB_MARGIN, 16),
                        "Fill");

  wuss_icon_spec_slider_row(&specs[n], &specs[n + 1],
                            (box_t) BOX_POS_SIZE(TB_MARGIN, TB_SLIDER_Y,
                                                 58, 18),
                            (box_t) BOX_POS_SIZE(TB_MARGIN + 60, TB_SLIDER_Y,
                                                 26, 18),
                            wuss_SLIDER_HORIZONTAL,
                            PAINT_SIZE_MIN, PAINT_SIZE_MAX, pt->size,
                            NULL, 0,
                            pt->size_text, sizeof(pt->size_text));
  n += 2;

  rc = wuss_icon_create_array(pt->toolbox, specs, n, icons);
  if (rc != result_OK)
    return rc;

  for (i = 0; i < paint_TOOL__COUNT; i++)
    pt->tool_icons[i] = icons[i];
  pt->fill_icon = icons[paint_TOOL__COUNT];

  wuss_slider_row_bind(&pt->size_row,
                       icons[paint_TOOL__COUNT + 1],
                       icons[paint_TOOL__COUNT + 2],
                       NULL, PAINT_SIZE_MIN, PAINT_SIZE_MAX, 0);

  wuss_icon_set_selected(pt->toolbox, pt->tool_icons[pt->tool], 1);

  return result_OK;
}

result_t paint_create(wuss_t *wuss, paint_task_t **out)
{
  result_t         rc;
  paint_task_t    *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->fg   = colour_rgb(0x00, 0x00, 0x00);
  task->bg   = colour_rgb(0xFF, 0xFF, 0xFF);
  task->tool = paint_TOOL_BRUSH;
  task->size = 3;

  rc = paintdoc_create(SIZE2D(PAINT_CANVAS_W, PAINT_CANVAS_H), &task->doc);
  if (rc != result_OK)
  {
    free(task);
    return rc;
  }

  /* paint_redraw copies the canvas over the whole content area */
  delegate_desc.handle    = paint_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "paint";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    paintdoc_destroy(task->doc);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(PAINT_VIEW_W, PAINT_VIEW_H),
                                 "Paint",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(PAINT_CANVAS_W, PAINT_CANVAS_H),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  /* no close box: closing the canvas window is what ends the task */
  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(TB_W, TB_H),
                                 "Tools",
                                 wuss_WINDOW_BACK | wuss_WINDOW_FOCUSABLE,
                                 wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                                 SIZE2D(TB_W, TB_H),
                                 SIZE2D(0, 0),
                                 &task->toolbox);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* closes the canvas, QUIT frees the block */
    return rc;
  }

  rc = paint_toolbox_icons(task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  /* both windows up: from here, closing the last one reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  if (out)
    *out = task;

  return result_OK;
}

result_t paint_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Paint",
    "Basic painting",
    (taskapp_create_fn_t) paint_create
  };

  return taskapp_install(wuss, &desc);
}

void paint_destroy(paint_task_t *task)
{
  paintdoc_destroy(task->doc);
  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t paint_redraw(paint_task_t *pt, const wuss_event_t *event)
{
  const box_t *bounds;

  bounds = event->data.redraw.bounds;

  return screen_copy_bitmap(event->data.redraw.scr,
                            bounds->x0 - event->data.redraw.scroll.x,
                            bounds->y0 - event->data.redraw.scroll.y,
                            paintdoc_bitmap(pt->doc));
}

/* the swatch grid and, below it, the background colour behind the foreground
 * colour. The icons draw over the window's own backdrop */
static result_t paint_toolbox_redraw(paint_task_t       *pt,
                                     const wuss_event_t *event)
{
  screen_t    *scr;
  const box_t *bounds;
  box_t        box;
  colour_t     black;
  int          i, ox, oy;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  ox     = bounds->x0 - event->data.redraw.scroll.x;
  oy     = bounds->y0 - event->data.redraw.scroll.y;
  black  = colour_rgb(0x00, 0x00, 0x00);

  for (i = 0; i < PAINT_NSWATCHES; i++)
  {
    box = paint_swatch_box(i);

    screen_fill_rect(scr, ox + box.x0, oy + box.y0,
                     SIZE2D(TB_SWATCH, TB_SWATCH), paint_swatch_colour(i));
    screen_draw_rect(scr, ox + box.x0, oy + box.y0,
                     SIZE2D(TB_SWATCH, TB_SWATCH), black);
  }

  screen_fill_rect(scr, ox + TB_MARGIN + 18, oy + TB_CHIPS_Y + 6,
                   SIZE2D(30, 16), pt->bg);
  screen_draw_rect(scr, ox + TB_MARGIN + 18, oy + TB_CHIPS_Y + 6,
                   SIZE2D(30, 16), black);
  screen_fill_rect(scr, ox + TB_MARGIN, oy + TB_CHIPS_Y,
                   SIZE2D(30, 16), pt->fg);
  screen_draw_rect(scr, ox + TB_MARGIN, oy + TB_CHIPS_Y,
                   SIZE2D(30, 16), black);

  return result_OK;
}

/* Select sets the foreground colour from the swatch under the pointer, Adjust
 * the background colour */
static result_t paint_toolbox_mouse(paint_task_t       *pt,
                                    const wuss_event_t *event)
{
  box_t box;
  int   i;

  if (event->data.mouse.action != wuss_MOUSE_DOWN)
    return result_OK;

  for (i = 0; i < PAINT_NSWATCHES; i++)
  {
    box = paint_swatch_box(i);

    if (event->data.mouse.point.x < box.x0 ||
        event->data.mouse.point.x >= box.x1 ||
        event->data.mouse.point.y < box.y0 ||
        event->data.mouse.point.y >= box.y1)
      continue;

    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      pt->fg = paint_swatch_colour(i);
    else if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      pt->bg = paint_swatch_colour(i);
    else
      return result_OK;

    wuss_window_invalidate_visible(pt->toolbox);
    return result_OK;
  }

  return result_OK;
}

static result_t paint_toolbox_icon(paint_task_t       *pt,
                                   const wuss_event_t *event)
{
  wuss_icon_t *icon;
  int          i, size;

  if (wuss_slider_row_event(pt->toolbox, &pt->size_row, event, &size))
  {
    pt->size = size;
    return result_OK;
  }

  /* a radio or option latches on MOUSE_UP */
  if (event->data.icon.action != wuss_MOUSE_UP)
    return result_OK;

  icon = event->data.icon.icon;

  for (i = 0; i < paint_TOOL__COUNT; i++)
  {
    if (icon == pt->tool_icons[i])
      pt->tool = (paint_tool_t) i;
  }

  if (icon == pt->fill_icon)
    pt->fill = wuss_icon_get_selected(icon);

  return result_OK;
}

/* the colour a brush stroke in progress paints: the eraser always uses the
 * background colour, the brush the colour of the button held */
static colour_t paint_stroke_colour(const paint_task_t *pt)
{
  if (pt->tool == paint_TOOL_ERASER || pt->held == wuss_BUTTON_ADJUST)
    return pt->bg;

  return pt->fg;
}

/* a MOVE always reports the Select button, hovering or not, so a stroke in
 * progress is tracked in pt->held instead */
static result_t paint_mouse(paint_task_t *pt, const wuss_event_t *event)
{
  box_t    dirty;
  point_t  p;
  colour_t colour;

  p = event->data.mouse.point;

  switch (event->data.mouse.action)
  {
  case wuss_MOUSE_DOWN:
    if (!(event->data.mouse.button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      return result_OK;

    pt->held = (event->data.mouse.button & wuss_BUTTON_SELECT) ?
               wuss_BUTTON_SELECT : wuss_BUTTON_ADJUST;

    switch (pt->tool)
    {
    case paint_TOOL_FILL:
      colour   = (pt->held == wuss_BUTTON_SELECT) ? pt->fg : pt->bg;
      pt->held = 0; /* a click, not a stroke */
      dirty    = paintdoc_fill(pt->doc, p, colour);
      break;

    case paint_TOOL_BRUSH:
    case paint_TOOL_ERASER:
      if (paintdoc_checkpoint(pt->doc) != result_OK)
      {
        pt->held = 0; /* no undo step, so no stroke */
        return result_OK;
      }
      pt->last = p;
      dirty    = paintdoc_stroke(pt->doc, p, p, pt->size,
                                 paint_stroke_colour(pt));
      break;

    default:
      pt->held = 0; /* line and rect are not drawn yet */
      return result_OK;
    }
    break;

  case wuss_MOUSE_MOVE:
    if (!pt->held)
      return result_OK;

    dirty    = paintdoc_stroke(pt->doc, pt->last, p, pt->size,
                               paint_stroke_colour(pt));
    pt->last = p;
    break;

  case wuss_MOUSE_UP:
    pt->held = 0;
    return result_OK;

  default:
    return result_OK;
  }

  if (!box_is_empty(&dirty))
    wuss_window_invalidate(pt->window, &dirty);

  return result_OK;
}

result_t paint_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  paint_task_t *pt;

  pt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    if (window == pt->toolbox)
      return paint_toolbox_redraw(pt, event);
    return paint_redraw(pt, event);

  case wuss_EVENT_MOUSE:
    if (window == pt->toolbox)
      return paint_toolbox_mouse(pt, event);
    return paint_mouse(pt, event);

  case wuss_EVENT_ICON:
    return paint_toolbox_icon(pt, event);

  case wuss_EVENT_CLOSE:
    if (window == pt->toolbox)
    {
      pt->toolbox = NULL;
      return result_OK;
    }

    pt->window = NULL;

    /* the toolbox has no close box of its own: it goes with the canvas. When
     * it was the last window, that reaps the task and frees pt, so touch
     * nothing afterwards */
    if (pt->toolbox != NULL)
      wuss_window_close(pt->toolbox);
    return result_OK;

  case wuss_EVENT_QUIT:
    /* the task autocloses once its windows go, so free the block here */
    paint_destroy(pt);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
