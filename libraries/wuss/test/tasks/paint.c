/* wuss/test/tasks/paint.c -- basic painting task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "io/filetype.h"

#include "paint.h"
#include "paint-new.h"
#include "taskapp.h"
#include "common.h"

#define PAINT_CANVAS_W 640
#define PAINT_CANVAS_H 480
#define PAINT_VIEW_W   400 /* initial visible size of the canvas window */
#define PAINT_VIEW_H   300

/* Toolbox layout, in its window's content space: a grid of tool buttons, the
 * rect Fill option, the size slider, then a grid of colour swatches and the
 * current colours. The swatches and the colour chips are drawn by
 * paint_toolbox_redraw and the swatches hit-tested by paint_toolbox_mouse, not
 * icons */
#define TB_W           96
#define TB_H           276
#define TB_MARGIN      5
#define TB_TOOL_COLS   2
#define TB_TOOL_X      17
#define TB_TOOL_CELL   32  /* pitch of the tool buttons */
#define TB_TOOL_SIZE   30  /* button size, including its bevel */
#define TB_FILL_Y      104
#define TB_SLIDER_Y    128
#define TB_SWATCH_COLS 4
#define TB_SWATCH_ROWS 4
#define TB_SWATCH      20  /* swatch size */
#define TB_SWATCH_GAP  2
#define TB_SWATCH_Y    156
#define TB_CHIPS_Y     250

#define PAINT_NSWATCHES (TB_SWATCH_COLS * TB_SWATCH_ROWS)

/* the tools' images, in the task's icon pool under resources/wuss/tasks/paint */
static const char *paint_tool_names[paint_TOOL__COUNT] =
{
  "draw",
  "erase",
  "line",
  "rectangle",
  "fill"
};

/* the tool's button box in toolbox content space */
static box_t paint_tool_box(int i)
{
  return (box_t) BOX_POS_SIZE(TB_TOOL_X + (i % TB_TOOL_COLS) * TB_TOOL_CELL,
                              TB_MARGIN + (i / TB_TOOL_COLS) * TB_TOOL_CELL,
                              TB_TOOL_SIZE, TB_TOOL_SIZE);
}

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

/* the tool buttons, Fill option and size slider */
static result_t paint_toolbox_icons(paint_task_t *pt)
{
  result_t         rc;
  char             dir[512];
  wuss_icon_spec_t specs[paint_TOOL__COUNT + 3];
  wuss_icon_t     *icons[paint_TOOL__COUNT + 3];
  int              i, n;

  snprintf(dir, sizeof(dir), "%s/resources/wuss/tasks/paint",
           wuss_get_resources(pt->wuss));
  rc = wuss_task_iconpool_load(pt->delegate, dir);
  if (rc != result_OK)
    return rc;

  memset(specs, 0, sizeof(specs));
  n = 0;

  for (i = 0; i < paint_TOOL__COUNT; i++)
  {
    box_t cell;

    cell = paint_tool_box(i);

    specs[n].bbox = cell;
    specs[n].type             = wuss_ICON_TYPE_BITMAP;
    specs[n].flags            = wuss_ICON_FLAGS_INTERACTIVE;
    specs[n].u.bitmap.border  = wuss_ICON_BORDER_RIDGE;
    specs[n].u.bitmap.group   = 1;
    specs[n].bg               = wuss_COLOUR_BUTTON_PRESSED; /* the latched fill */
    specs[n].u.bitmap.name    = paint_tool_names[i];
    n++;
  }

  wuss_icon_spec_option(&specs[n++],
                        (box_t) BOX_POS_SIZE(TB_MARGIN, TB_FILL_Y,
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

/* wuss_saveas_save_fn_t: opaque is the paint_task_t */
static result_t paint_saveas_save(const char *path, void *opaque)
{
  paint_task_t *pt;

  pt = opaque;

  return bitmap_save_png(paintdoc_bitmap(pt->doc), path);
}

/* load the PNG at path as the canvas, shrinking the window to the image if
 * that is smaller than the usual view. A failure keeps the current canvas */
static result_t paint_load(paint_task_t *pt, const char *path)
{
  result_t rc;
  size2d_t size;

  pt->held = 0; /* abandon any stroke in progress */

  rc = paintdoc_load_png(pt->doc, path);
  if (rc != result_OK)
    return rc;

  size = paintdoc_bitmap(pt->doc)->size;

  rc = wuss_window_resize(pt->window,
                          SIZE2D(MIN(size.w, PAINT_VIEW_W),
                                 MIN(size.h, PAINT_VIEW_H)));
  if (rc != result_OK)
    return rc;

  return wuss_window_set_doc(pt->window, size);
}

result_t paint_create(wuss_t *wuss, paint_task_t **out)
{
  paintnew_params_t params;

  params.size = SIZE2D(PAINT_CANVAS_W, PAINT_CANVAS_H);
  params.bg   = colour_rgb(0xFF, 0xFF, 0xFF);
  params.fg   = colour_rgb(0x00, 0x00, 0x00);

  return paint_create_with(wuss, &params, out);
}

result_t paint_create_with(wuss_t                  *wuss,
                           const paintnew_params_t *params,
                           paint_task_t           **out)
{
  result_t         rc;
  paint_task_t    *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  filetype_t       png_type;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->fg   = params->fg;
  task->bg   = params->bg;
  task->tool = paint_TOOL_BRUSH;
  task->size = 3;

  rc = paintdoc_create_filled(params->size, params->bg, &task->doc);
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
                                 SIZE2D(MIN(params->size.w, PAINT_VIEW_W),
                                        MIN(params->size.h, PAINT_VIEW_H)),
                                 "Paint",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP,
                                 params->size,
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

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PAINT_MENU_UNDO, "Undo",
                          wuss_MENU_ITEM_NONE, "^Z");
  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PAINT_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_DASHED, "^S");
  WUSS_MENU_TITLE(task->menu, "Paint", task->menu_items,
                  NELEMS(task->menu_items));

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, wuss, &png_type, "paint.png",
                          paint_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* closes both windows, QUIT frees the block */
    return rc;
  }
  /* hover opens the Save As dialogue as a submenu */
  task->menu_items[PAINT_MENU_SAVE].window = wuss_saveas_window(task->saveas);

  /* both windows up: from here, closing the last one reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  if (out)
    *out = task;

  return result_OK;
}

/* the New dialogue: made when first wanted, as a child of the app's task so
 * Quit takes it too, which also clears this */
static paintnew_t *g_paint_new;

static result_t paint_new_dialogue(wuss_t *wuss);

static const taskapp_desc_t paint_desc =
{
  "Paint",
  "Basic painting",
  (taskapp_create_fn_t) paint_create,
  (taskapp_data_load_fn_t) paint_load,
  false,
  paint_new_dialogue
};

/* taskapp_spawn_fn_t: param is the dialogue's paintnew_params_t */
static result_t paint_spawn_new(wuss_t *wuss, void *param, void **out)
{
  (void) out;

  return paint_create_with(wuss, param, NULL);
}

/* paintnew_accept_fn_t: opaque is the wuss_t */
static result_t paint_accept_new(const paintnew_params_t *params,
                                 void                    *opaque)
{
  return taskapp_spawn(opaque, &paint_desc, paint_spawn_new,
                       (void *) params, NULL);
}

static result_t paint_new_dialogue(wuss_t *wuss)
{
  result_t rc;

  if (g_paint_new == NULL)
  {
    rc = paintnew_create(&g_paint_new, wuss, paint_accept_new, wuss);
    if (rc != result_OK)
      return rc;
  }

  return paintnew_open(g_paint_new);
}

result_t paint_app_create(wuss_t *wuss)
{
  return taskapp_install(wuss, &paint_desc);
}

void paint_destroy(paint_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  paintdoc_destroy(task->doc);
  free(task);
}

/* ----------------------------------------------------------------------- */

/* line and rect are rubber-banded: shown on the window, not drawn into the
 * canvas, until the button is released */
static int paint_is_rubber_band(const paint_task_t *pt)
{
  return pt->tool == paint_TOOL_LINE || pt->tool == paint_TOOL_RECT;
}

/* the colour a stroke in progress paints: the eraser always uses the
 * background colour, the others the colour of the button held */
static colour_t paint_stroke_colour(const paint_task_t *pt)
{
  if (pt->tool == paint_TOOL_ERASER || pt->held == wuss_BUTTON_ADJUST)
    return pt->bg;

  return pt->fg;
}

/* the canvas box the rubber band at its current size may cover */
static box_t paint_band_box(const paint_task_t *pt)
{
  int   r;
  box_t box;

  r = pt->size / 2 + 1;

  box.x0 = MIN(pt->anchor.x, pt->last.x) - r;
  box.y0 = MIN(pt->anchor.y, pt->last.y) - r;
  box.x1 = MAX(pt->anchor.x, pt->last.x) + r + 1;
  box.y1 = MAX(pt->anchor.y, pt->last.y) + r + 1;

  return box;
}

static result_t paint_redraw(paint_task_t *pt, const wuss_event_t *event)
{
  result_t     rc;
  const box_t *bounds;
  screen_t    *scr;
  point_t      a, b;
  int          ox, oy;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  ox     = bounds->x0 - event->data.redraw.scroll.x;
  oy     = bounds->y0 - event->data.redraw.scroll.y;

  rc = screen_copy_bitmap(scr, ox, oy, paintdoc_bitmap(pt->doc));

  if (pt->held && paint_is_rubber_band(pt))
  {
    a = POINT(pt->anchor.x + ox, pt->anchor.y + oy);
    b = POINT(pt->last.x + ox, pt->last.y + oy);

    if (pt->tool == paint_TOOL_LINE)
      paintdoc_draw_stroke(scr, a, b, pt->size, paint_stroke_colour(pt));
    else
      paintdoc_draw_rect(scr, a, b, pt->size, pt->fill,
                         paint_stroke_colour(pt));
  }

  return rc;
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

/* draw the finished rubber band into the canvas */
static box_t paint_band_commit(paint_task_t *pt)
{
  colour_t colour;

  colour = paint_stroke_colour(pt);

  if (pt->tool == paint_TOOL_RECT)
    return paintdoc_rect(pt->doc, pt->anchor, pt->last, pt->size, pt->fill,
                         colour);

  if (paintdoc_checkpoint(pt->doc) != result_OK)
    return (box_t) BOX_INIT; /* no undo step, so no line */

  return paintdoc_stroke(pt->doc, pt->anchor, pt->last, pt->size, colour);
}

/* a MOVE always reports the Select button, hovering or not, so a stroke in
 * progress is tracked in pt->held instead */
static result_t paint_mouse(paint_task_t *pt, const wuss_event_t *event)
{
  box_t    dirty = BOX_INIT;
  box_t    old;
  point_t  p;
  colour_t colour;

  p = event->data.mouse.point;

  switch (event->data.mouse.action)
  {
  case wuss_MOUSE_DOWN:
    if (!(event->data.mouse.button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      return result_OK;

    pt->held   = (event->data.mouse.button & wuss_BUTTON_SELECT) ?
                 wuss_BUTTON_SELECT : wuss_BUTTON_ADJUST;
    pt->anchor = p;
    pt->last   = p;

    switch (pt->tool)
    {
    case paint_TOOL_FILL:
      colour   = paint_stroke_colour(pt);
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
      dirty = paintdoc_stroke(pt->doc, p, p, pt->size,
                              paint_stroke_colour(pt));
      break;

    default:
      dirty = paint_band_box(pt);
      break;
    }
    break;

  case wuss_MOUSE_MOVE:
    if (!pt->held)
      return result_OK;

    if (paint_is_rubber_band(pt))
    {
      old = paint_band_box(pt);
      wuss_window_invalidate(pt->window, &old);
      pt->last = p;
      dirty    = paint_band_box(pt);
    }
    else
    {
      dirty    = paintdoc_stroke(pt->doc, pt->last, p, pt->size,
                                 paint_stroke_colour(pt));
      pt->last = p;
    }
    break;

  case wuss_MOUSE_UP:
    if (!pt->held)
      return result_OK;

    if (paint_is_rubber_band(pt))
    {
      pt->last = p;
      dirty    = paint_band_commit(pt);

      /* the band is no longer drawn over the canvas */
      old = paint_band_box(pt);
      wuss_window_invalidate(pt->window, &old);
    }

    pt->held = 0;
    break;

  default:
    return result_OK;
  }

  if (!box_is_empty(&dirty))
    wuss_window_invalidate(pt->window, &dirty);

  return result_OK;
}

static result_t paint_undo(paint_task_t *pt)
{
  box_t dirty;

  pt->held = 0; /* abandon any stroke in progress */

  dirty = paintdoc_undo(pt->doc);
  if (!box_is_empty(&dirty))
    wuss_window_invalidate(pt->window, &dirty);

  return result_OK;
}

static void paint_set_tool(paint_task_t *pt, paint_tool_t tool)
{
  pt->tool = tool;
  wuss_icon_set_selected(pt->toolbox, pt->tool_icons[tool], 1);
}

/* B, E, L, R and F pick the matching tool. Anything else is passed back
 * unclaimed */
static result_t paint_key(paint_task_t *pt, int code)
{
  switch (code)
  {
  case 'b': case 'B': paint_set_tool(pt, paint_TOOL_BRUSH);  break;
  case 'e': case 'E': paint_set_tool(pt, paint_TOOL_ERASER); break;
  case 'l': case 'L': paint_set_tool(pt, paint_TOOL_LINE);   break;
  case 'r': case 'R': paint_set_tool(pt, paint_TOOL_RECT);   break;
  case 'f': case 'F': paint_set_tool(pt, paint_TOOL_FILL);   break;
  default:            return result_WUSS_KEY_UNCLAIMED;
  }

  return result_OK;
}

/* a MENU click on either window opens the menu, with Undo greyed out when
 * there is nothing to undo */
static result_t paint_menu_open(paint_task_t *pt)
{
  pt->menu_items[PAINT_MENU_UNDO].flags = paintdoc_can_undo(pt->doc) ?
                                          wuss_MENU_ITEM_NONE :
                                          wuss_MENU_ITEM_DISABLED;

  return wuss_menu_open_at_pointer(pt->delegate, &pt->menu, &pt->menu_handle);
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
    if (event->data.mouse.action == wuss_MOUSE_DOWN &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return paint_menu_open(pt);

    if (window == pt->toolbox)
      return paint_toolbox_mouse(pt, event);
    return paint_mouse(pt, event);

  case wuss_EVENT_ICON:
    return paint_toolbox_icon(pt, event);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (!task_key_is_plain(pt->delegate, &pt->menu, event, &rc))
      return rc;

    return paint_key(pt, event->data.key.code);
  }

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.index == PAINT_MENU_UNDO)
      return paint_undo(pt);
    /* clicking the row, rather than hovering, shows Save As standalone */
    if (event->data.menu_select.index == PAINT_MENU_SAVE)
      return wuss_saveas_open(pt->saveas);
    return result_OK;

  case wuss_EVENT_MESSAGE:
    if (pt->window == NULL || window != pt->window)
      return result_OK;
    return taskapp_window_data_load(pt->wuss, pt->delegate, pt,
                                    (taskapp_data_load_fn_t) paint_load,
                                    event->data.message);

  case wuss_EVENT_MENU_CLOSED:
    pt->menu_handle = NULL;
    return result_OK;

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
