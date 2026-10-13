/* wuss/test/tasks/painting.c -- basic painting task */

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
#include "io/path.h"
#include "utils/array.h"
#include "utils/rng.h"

#include "painting.h"
#include "painting-new.h"
#include "taskapp.h"
#include "common.h"

#define PAINTING_CANVAS_W 640
#define PAINTING_CANVAS_H 480
#define PAINTING_VIEW_W   400 /* initial visible size of the canvas window */
#define PAINTING_VIEW_H   300

/* Toolbox layout, in its window's content space: a grid of tool buttons, the
 * rect Fill option, the size slider, then a grid of colour swatches and the
 * current colours. The swatches and the colour chips are drawn by
 * painting_toolbox_redraw and the swatches hit-tested by painting_toolbox_mouse, not
 * icons */
#define TB_W           96
#define TB_H           286
#define TB_MARGIN      5
#define TB_TOOL_COLS   2
#define TB_TOOL_X      23
#define TB_TOOL_CELL   26  /* pitch of the tool buttons */
#define TB_TOOL_SIZE   24  /* button size, including its bevel */
#define TB_SLIDER_Y    138
#define TB_SWATCH_COLS 4
#define TB_SWATCH_ROWS 4
#define TB_SWATCH      20  /* swatch size */
#define TB_SWATCH_GAP  2
#define TB_SWATCH_Y    166
#define TB_CHIPS_Y     260

#define PAINTING_ANTS_IDLES 6 /* idles between steps of the marching ants */

/* the spray's radius in pixels per unit of the size slider */
#define PAINTING_SPRAY_SCALE 3

#define PAINTING_NSWATCHES (TB_SWATCH_COLS * TB_SWATCH_ROWS)

/* the tools' images, in the task's icon pool under resources/wuss/tasks/painting */
static const char *painting_tool_names[painting_TOOL__COUNT] =
{
  "draw",
  "erase",
  "line",
  "rectangle",
  "rectangle-fill",
  "fill",
  "spray",
  "select",
  "lasso"
};

/* the tool's button box in toolbox content space */
static box_t painting_tool_box(int i)
{
  return (box_t) BOX_POS_SIZE(TB_TOOL_X + (i % TB_TOOL_COLS) * TB_TOOL_CELL,
                              TB_MARGIN + (i / TB_TOOL_COLS) * TB_TOOL_CELL,
                              TB_TOOL_SIZE, TB_TOOL_SIZE);
}

/* the fixed palette, as RGB, in swatch order */
static const unsigned long painting_swatches[PAINTING_NSWATCHES] =
{
  0x000000, 0xFFFFFF, 0x808080, 0xC0C0C0,
  0xFF0000, 0x800000, 0xFF8000, 0xFFFF00,
  0x00C000, 0x006000, 0x00FFFF, 0x0000FF,
  0x000080, 0xFF00FF, 0x800080, 0x804000
};

static colour_t painting_swatch_colour(int i)
{
  unsigned long rgb;

  rgb = painting_swatches[i];

  return colour_rgb((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

/* the swatch's box in toolbox content space */
static box_t painting_swatch_box(int i)
{
  int   x, y;
  box_t box;

  x = TB_MARGIN + (i % TB_SWATCH_COLS) * (TB_SWATCH + TB_SWATCH_GAP);
  y = TB_SWATCH_Y + (i / TB_SWATCH_COLS) * (TB_SWATCH + TB_SWATCH_GAP);

  box = (box_t) BOX_POS_SIZE(x, y, TB_SWATCH, TB_SWATCH);

  return box;
}

/* ----------------------------------------------------------------------- */

/* the tool buttons and size slider */
static result_t painting_toolbox_icons(painting_task_t *pt)
{
  result_t         rc;
  char             dir[512];
  wuss_icon_spec_t specs[painting_TOOL__COUNT + 2];
  wuss_icon_t     *icons[painting_TOOL__COUNT + 2];
  int              i, n;

  /* pathf maps to native separators (RISC OS); it returns a shared static
   * buffer, so copy it */
  snprintf(dir, sizeof(dir), "%s",
           pathf("%s/resources/wuss/tasks/painting", wuss_get_resources(pt->wuss)));
  rc = wuss_task_iconpool_load(pt->delegate, dir);
  if (rc != result_OK)
    return rc;

  memset(specs, 0, sizeof(specs));
  n = 0;

  for (i = 0; i < painting_TOOL__COUNT; i++)
  {
    box_t cell;

    cell = painting_tool_box(i);

    specs[n].bbox = cell;
    specs[n].type             = wuss_ICON_TYPE_BITMAP;
    specs[n].flags            = wuss_ICON_FLAGS_INTERACTIVE;
    specs[n].u.bitmap.border  = wuss_ICON_BORDER_RIDGE;
    specs[n].u.bitmap.group   = 1;
    specs[n].bg               = wuss_COLOUR_BUTTON_PRESSED; /* the latched fill */
    specs[n].u.bitmap.name    = painting_tool_names[i];
    n++;
  }

  wuss_icon_spec_slider_row(&specs[n], &specs[n + 1],
                            (box_t) BOX_POS_SIZE(TB_MARGIN, TB_SLIDER_Y,
                                                 58, 18),
                            (box_t) BOX_POS_SIZE(TB_MARGIN + 60, TB_SLIDER_Y,
                                                 26, 18),
                            wuss_SLIDER_HORIZONTAL,
                            PAINTING_SIZE_MIN, PAINTING_SIZE_MAX, pt->size,
                            NULL, 0,
                            pt->size_text, sizeof(pt->size_text));
  n += 2;

  rc = wuss_icon_create_array(pt->toolbox, specs, n, icons);
  if (rc != result_OK)
    return rc;

  for (i = 0; i < painting_TOOL__COUNT; i++)
    pt->tool_icons[i] = icons[i];

  wuss_slider_row_bind(&pt->size_row,
                       icons[painting_TOOL__COUNT],
                       icons[painting_TOOL__COUNT + 1],
                       NULL, PAINTING_SIZE_MIN, PAINTING_SIZE_MAX, 0);

  wuss_icon_set_selected(pt->toolbox, pt->tool_icons[pt->tool], 1);

  return result_OK;
}

/* wuss_saveas_save_fn_t: opaque is the painting_task_t */
static result_t painting_saveas_save(const char *path, void *opaque)
{
  painting_task_t *pt;

  pt = opaque;

  return bitmap_save_png(paintingdoc_bitmap(pt->doc), path);
}

/* load the PNG at path as the canvas, shrinking the window to the image if
 * that is smaller than the usual view. A failure keeps the current canvas */
static result_t painting_load(painting_task_t *pt, const char *path)
{
  result_t rc;
  size2d_t size;

  pt->held = 0; /* abandon any stroke in progress */

  rc = paintingdoc_load_png(pt->doc, path);
  if (rc != result_OK)
    return rc;

  size = paintingdoc_bitmap(pt->doc)->size;

  rc = wuss_window_resize(pt->window,
                          SIZE2D(MIN(size.w, PAINTING_VIEW_W),
                                 MIN(size.h, PAINTING_VIEW_H)));
  if (rc != result_OK)
    return rc;

  return wuss_window_set_doc(pt->window, size);
}

result_t painting_create(wuss_t *wuss, painting_task_t **out)
{
  paintingnew_params_t params;

  params.size = SIZE2D(PAINTING_CANVAS_W, PAINTING_CANVAS_H);
  params.bg   = colour_rgb(0xFF, 0xFF, 0xFF);
  params.fg   = colour_rgb(0x00, 0x00, 0x00);

  return painting_create_with(wuss, &params, out);
}

result_t painting_create_with(wuss_t                     *wuss,
                              const paintingnew_params_t *params,
                              painting_task_t           **out)
{
  result_t         rc;
  painting_task_t *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  filetype_t       png_type;
  wuss_place_t     place;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->fg   = params->fg;
  task->bg   = params->bg;
  task->tool = painting_TOOL_BRUSH;
  rng_seed(&task->rng, 1);
  task->size = 3;

  rc = paintingdoc_create_filled(params->size, params->bg, &task->doc);
  if (rc != result_OK)
  {
    free(task);
    return rc;
  }

  /* painting_redraw copies the canvas over the whole content area */
  delegate_desc.handle    = painting_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "painting";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    paintingdoc_destroy(task->doc);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate;

  rc = wuss_window_create_at(delegate,
                             &task_place,
                             SIZE2D(MIN(params->size.w, PAINTING_VIEW_W),
                                    MIN(params->size.h, PAINTING_VIEW_H)),
                             "Painting",
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
  place = task_place_after(task->window);
  rc = wuss_window_create_at(delegate,
                             &place,
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

  rc = painting_toolbox_icons(task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PAINTING_MENU_UNDO, "Undo",
                          wuss_MENU_ITEM_NONE, "^Z");

  WUSS_MENU_ITEM(task->sel_items, PAINTING_SEL_ALL, "Select All",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->sel_items, PAINTING_SEL_NONE, "Deselect",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->sel_items, PAINTING_SEL_INVERT, "Invert",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->sel_menu, "Selection", task->sel_items,
                  NELEMS(task->sel_items));
  WUSS_MENU_ITEM_MENU(task->menu_items, PAINTING_MENU_SELECTION, "Selection",
                      wuss_MENU_ITEM_NONE, &task->sel_menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PAINTING_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_DASHED, "^S");
  WUSS_MENU_TITLE(task->menu, "Painting", task->menu_items,
                  NELEMS(task->menu_items));

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, wuss, &png_type, "painting.png",
                          painting_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* closes both windows, QUIT frees the block */
    return rc;
  }
  /* hover opens the Save As dialogue as a submenu */
  task->menu_items[PAINTING_MENU_SAVE].window = wuss_saveas_window(task->saveas);

  /* both windows up: from here, closing the last one reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  if (out)
    *out = task;

  return result_OK;
}

/* the New dialogue: made when first wanted, as a child of the app's task so
 * Quit takes it too, which also clears this */
static paintingnew_t *g_painting_new;

static result_t painting_new_dialogue(wuss_t *wuss);

static const taskapp_desc_t painting_desc =
{
  "Painting",
  "Basic painting",
  (taskapp_create_fn_t) painting_create,
  (taskapp_data_load_fn_t) painting_load,
  false,
  painting_new_dialogue
};

/* taskapp_spawn_fn_t: param is the dialogue's paintingnew_params_t */
static result_t painting_spawn_new(wuss_t *wuss, void *param, void **out)
{
  (void) out;

  return painting_create_with(wuss, param, NULL);
}

/* paintingnew_accept_fn_t: opaque is the wuss_t */
static result_t painting_accept_new(const paintingnew_params_t *params,
                                    void                       *opaque)
{
  return taskapp_spawn(opaque, &painting_desc, painting_spawn_new,
                       (void *) params, NULL);
}

static result_t painting_new_dialogue(wuss_t *wuss)
{
  result_t rc;

  if (g_painting_new == NULL)
  {
    rc = paintingnew_create(&g_painting_new, wuss, painting_accept_new, wuss);
    if (rc != result_OK)
      return rc;
  }

  return paintingnew_open(g_painting_new);
}

result_t painting_app_create(wuss_t *wuss)
{
  return taskapp_install(wuss, &painting_desc);
}

void painting_destroy(painting_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  free(task->lasso);
  paintingdoc_destroy(task->doc);
  free(task);
}

/* ----------------------------------------------------------------------- */

/* line, rect and select are rubber-banded: shown on the window, not drawn
 * into the canvas, until the button is released */
static int painting_is_rubber_band(const painting_task_t *pt)
{
  return pt->tool == painting_TOOL_LINE || pt->tool == painting_TOOL_RECT ||
         pt->tool == painting_TOOL_RECT_FILL || pt->tool == painting_TOOL_SELECT;
}

/* select and lasso make a selection rather than paint */
static int painting_is_selector(const painting_task_t *pt)
{
  return pt->tool == painting_TOOL_SELECT || pt->tool == painting_TOOL_LASSO;
}

/* the lasso's path grows by one point; false if out of memory, when the
 * point is dropped */
static int painting_lasso_add(painting_task_t *pt, point_t p)
{
  if (pt->nlasso > 0 && pt->lasso[pt->nlasso - 1].x == p.x &&
      pt->lasso[pt->nlasso - 1].y == p.y)
    return 1;

  if (array_grow((void **) &pt->lasso, sizeof(*pt->lasso), pt->nlasso,
                 &pt->caplasso, 1, 256))
    return 0;

  pt->lasso[pt->nlasso++] = p;

  return 1;
}

/* the box covering a and b, with room for a line a few pixels wide */
static box_t painting_span_box(point_t a, point_t b)
{
  box_t box;

  box.x0 = MIN(a.x, b.x) - 2;
  box.y0 = MIN(a.y, b.y) - 2;
  box.x1 = MAX(a.x, b.x) + 3;
  box.y1 = MAX(a.y, b.y) + 3;

  return box;
}

static void painting_invalidate_box(painting_task_t *pt, const box_t *box)
{
  if (!box_is_empty(box))
    wuss_window_invalidate(pt->window, box);
}

/* the colour a stroke in progress paints: the eraser always uses the
 * background colour, the others the colour of the button held */
static colour_t painting_stroke_colour(const painting_task_t *pt)
{
  if (pt->tool == painting_TOOL_ERASER || pt->held == wuss_BUTTON_ADJUST)
    return pt->bg;

  return pt->fg;
}

/* the canvas box the rubber band at its current size may cover */
static box_t painting_band_box(const painting_task_t *pt)
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

/* finish a selection drag: the new selection replaces the old, and all that
 * the ants and the drag's own outline covered is repainted */
static void painting_select_commit(painting_task_t *pt, point_t p)
{
  box_t old, now, drag;
  box_t box;
  int   i;

  old = paintingdoc_selection_bounds(pt->doc);
  pt->last = p;

  if (pt->tool == painting_TOOL_SELECT)
  {
    drag = painting_band_box(pt);

    if (pt->anchor.x == p.x && pt->anchor.y == p.y)
    {
      paintingdoc_select_none(pt->doc); /* a click drops the selection */
    }
    else
    {
      box.x0 = MIN(pt->anchor.x, p.x);
      box.y0 = MIN(pt->anchor.y, p.y);
      box.x1 = MAX(pt->anchor.x, p.x) + 1;
      box.y1 = MAX(pt->anchor.y, p.y) + 1;
      paintingdoc_select_rect(pt->doc, box);
    }
  }
  else
  {
    drag = (box_t) BOX_INIT;
    for (i = 0; i < pt->nlasso; i++)
    {
      box    = painting_span_box(pt->lasso[i], pt->lasso[i]);
      drag.x0 = MIN(drag.x0, box.x0);
      drag.y0 = MIN(drag.y0, box.y0);
      drag.x1 = MAX(drag.x1, box.x1);
      drag.y1 = MAX(drag.y1, box.y1);
    }

    paintingdoc_select_poly(pt->doc, pt->lasso, pt->nlasso);
    pt->nlasso = 0;
  }

  now = paintingdoc_selection_bounds(pt->doc);

  painting_invalidate_box(pt, &old);
  painting_invalidate_box(pt, &now);
  painting_invalidate_box(pt, &drag);
}

/* the marching ants: a one pixel outline of black and white dashes round the
 * selection, drawn over the canvas and shifted along by ants_phase */
static void painting_draw_ants(const painting_task_t *pt,
                               screen_t              *scr,
                               int                    ox,
                               int                    oy)
{
  const box_t *spans;
  colour_t     black, white;
  int          n, i, x, y;

  spans = paintingdoc_selection_outline(pt->doc, &n);
  black = colour_rgb(0x00, 0x00, 0x00);
  white = colour_rgb(0xFF, 0xFF, 0xFF);

  for (i = 0; i < n; i++)
  {
    y = oy + spans[i].y0;
    if (y < scr->clip.y0 || y >= scr->clip.y1)
      continue;

    for (x = MAX(ox + spans[i].x0, scr->clip.x0);
         x < MIN(ox + spans[i].x1, scr->clip.x1);
         x++)
      screen_set_pixel(scr, x, y,
                       ((x - ox + y - oy - pt->ants_phase) & 7) < 4 ? black : white);
  }
}

static result_t painting_redraw(painting_task_t    *pt,
                                const wuss_event_t *event)
{
  result_t     rc;
  const box_t *bounds;
  screen_t    *scr;
  point_t      a, b;
  int          i, ox, oy;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  ox     = bounds->x0 - event->data.redraw.scroll.x;
  oy     = bounds->y0 - event->data.redraw.scroll.y;

  rc = screen_copy_bitmap(scr, ox, oy, paintingdoc_bitmap(pt->doc));

  if (pt->held && painting_is_rubber_band(pt))
  {
    a = POINT(pt->anchor.x + ox, pt->anchor.y + oy);
    b = POINT(pt->last.x + ox, pt->last.y + oy);

    if (pt->tool == painting_TOOL_LINE)
      paintingdoc_draw_stroke(scr, a, b, pt->size, painting_stroke_colour(pt));
    else if (pt->tool == painting_TOOL_SELECT)
      paintingdoc_draw_rect(scr, a, b, 1, 0, colour_rgb(0x00, 0x00, 0x00));
    else
      paintingdoc_draw_rect(scr, a, b, pt->size, pt->tool == painting_TOOL_RECT_FILL,
                         painting_stroke_colour(pt));
  }

  if (pt->held && pt->tool == painting_TOOL_LASSO)
  {
    for (i = 1; i < pt->nlasso; i++)
      screen_draw_line(scr, pt->lasso[i - 1].x + ox, pt->lasso[i - 1].y + oy,
                       pt->lasso[i].x + ox, pt->lasso[i].y + oy,
                       colour_rgb(0x00, 0x00, 0x00));
  }

  painting_draw_ants(pt, scr, ox, oy);

  return rc;
}

/* the swatch grid and, below it, the background colour behind the foreground
 * colour. The icons draw over the window's own backdrop */
static result_t painting_toolbox_redraw(painting_task_t    *pt,
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

  for (i = 0; i < PAINTING_NSWATCHES; i++)
  {
    box = painting_swatch_box(i);

    screen_fill_rect(scr, ox + box.x0, oy + box.y0,
                     SIZE2D(TB_SWATCH, TB_SWATCH), painting_swatch_colour(i));
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
static result_t painting_toolbox_mouse(painting_task_t    *pt,
                                       const wuss_event_t *event)
{
  box_t box;
  int   i;

  if (event->data.mouse.action != wuss_MOUSE_DOWN)
    return result_OK;

  for (i = 0; i < PAINTING_NSWATCHES; i++)
  {
    box = painting_swatch_box(i);

    if (event->data.mouse.point.x < box.x0 ||
        event->data.mouse.point.x >= box.x1 ||
        event->data.mouse.point.y < box.y0 ||
        event->data.mouse.point.y >= box.y1)
      continue;

    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      pt->fg = painting_swatch_colour(i);
    else if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      pt->bg = painting_swatch_colour(i);
    else
      return result_OK;

    wuss_window_invalidate_visible(pt->toolbox);
    return result_OK;
  }

  return result_OK;
}

static result_t painting_toolbox_icon(painting_task_t    *pt,
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

  for (i = 0; i < painting_TOOL__COUNT; i++)
  {
    if (icon == pt->tool_icons[i])
      pt->tool = (painting_tool_t) i;
  }

  return result_OK;
}

/* draw the finished rubber band into the canvas */
static box_t painting_band_commit(painting_task_t *pt)
{
  colour_t colour;

  colour = painting_stroke_colour(pt);

  if (pt->tool == painting_TOOL_RECT || pt->tool == painting_TOOL_RECT_FILL)
    return paintingdoc_rect(pt->doc, pt->anchor, pt->last, pt->size,
                         pt->tool == painting_TOOL_RECT_FILL, colour);

  if (paintingdoc_checkpoint(pt->doc) != result_OK)
    return (box_t) BOX_INIT; /* no undo step, so no line */

  return paintingdoc_stroke(pt->doc, pt->anchor, pt->last, pt->size, colour);
}

/* a MOVE always reports the Select button, hovering or not, so a stroke in
 * progress is tracked in pt->held instead */
static result_t painting_mouse(painting_task_t    *pt,
                               const wuss_event_t *event)
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
    case painting_TOOL_FILL:
      colour   = painting_stroke_colour(pt);
      pt->held = 0; /* a click, not a stroke */
      dirty    = paintingdoc_fill(pt->doc, p, colour);
      break;

    case painting_TOOL_LASSO:
      pt->nlasso = 0;
      painting_lasso_add(pt, p);
      dirty = painting_span_box(p, p);
      break;

    case painting_TOOL_SPRAY:
      if (paintingdoc_checkpoint(pt->doc) != result_OK)
      {
        pt->held = 0;
        return result_OK;
      }
      dirty = paintingdoc_spray(pt->doc, p, pt->size * PAINTING_SPRAY_SCALE,
                             painting_stroke_colour(pt), &pt->rng);
      break;

    case painting_TOOL_BRUSH:
    case painting_TOOL_ERASER:
      if (paintingdoc_checkpoint(pt->doc) != result_OK)
      {
        pt->held = 0; /* no undo step, so no stroke */
        return result_OK;
      }
      dirty = paintingdoc_stroke(pt->doc, p, p, pt->size,
                              painting_stroke_colour(pt));
      break;

    default:
      dirty = painting_band_box(pt);
      break;
    }
    break;

  case wuss_MOUSE_MOVE:
    if (!pt->held)
      return result_OK;

    if (pt->tool == painting_TOOL_LASSO)
    {
      painting_lasso_add(pt, p);
      dirty    = painting_span_box(pt->last, p);
      pt->last = p;
    }
    else if (painting_is_rubber_band(pt))
    {
      old = painting_band_box(pt);
      wuss_window_invalidate(pt->window, &old);
      pt->last = p;
      dirty    = painting_band_box(pt);
    }
    else if (pt->tool == painting_TOOL_SPRAY)
    {
      dirty    = paintingdoc_spray(pt->doc, p, pt->size * PAINTING_SPRAY_SCALE,
                                painting_stroke_colour(pt), &pt->rng);
      pt->last = p;
    }
    else
    {
      dirty    = paintingdoc_stroke(pt->doc, pt->last, p, pt->size,
                                 painting_stroke_colour(pt));
      pt->last = p;
    }
    break;

  case wuss_MOUSE_UP:
    if (!pt->held)
      return result_OK;

    if (painting_is_selector(pt))
    {
      painting_select_commit(pt, p);
    }
    else if (painting_is_rubber_band(pt))
    {
      pt->last = p;
      dirty    = painting_band_commit(pt);

      /* the band is no longer drawn over the canvas */
      old = painting_band_box(pt);
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

/* shade Undo when there is nothing to undo and Deselect when nothing is
 * selected, also on the menu if it is open (an ADJUST pick keeps it open) */
static void painting_menu_shade(painting_task_t *pt)
{
  int no_undo, no_sel;

  no_undo = !paintingdoc_can_undo(pt->doc);
  no_sel  = !paintingdoc_has_selection(pt->doc);

  pt->menu_items[PAINTING_MENU_UNDO].flags = no_undo ?
                                          wuss_MENU_ITEM_DISABLED :
                                          wuss_MENU_ITEM_NONE;
  pt->sel_items[PAINTING_SEL_NONE].flags   = no_sel ?
                                          wuss_MENU_ITEM_DISABLED :
                                          wuss_MENU_ITEM_NONE;

  wuss_menu_disable_item_live(pt->menu_handle, &pt->menu, PAINTING_MENU_UNDO,
                              no_undo);
  wuss_menu_disable_item_live(pt->menu_handle, &pt->sel_menu, PAINTING_SEL_NONE,
                              no_sel);
}

static result_t painting_undo(painting_task_t *pt)
{
  box_t dirty;

  pt->held = 0; /* abandon any stroke in progress */

  dirty = paintingdoc_undo(pt->doc);
  if (!box_is_empty(&dirty))
    wuss_window_invalidate(pt->window, &dirty);

  painting_menu_shade(pt);

  return result_OK;
}

static void painting_set_tool(painting_task_t *pt, painting_tool_t tool)
{
  pt->tool = tool;
  wuss_icon_set_selected(pt->toolbox, pt->tool_icons[tool], 1);
}

/* step the brush size by delta, keeping the toolbox slider in step */
static void painting_step_size(painting_task_t *pt, int delta)
{
  pt->size = CLAMP(pt->size + delta, PAINTING_SIZE_MIN, PAINTING_SIZE_MAX);
  (void) wuss_slider_row_set(pt->toolbox, &pt->size_row, pt->size);
}

/* B, E, L, R, X, F, P, S and O pick the matching tool, [ and ] step the
 * size down and up, Escape drops the selection. Anything else is passed back
 * unclaimed */
static result_t painting_key(painting_task_t *pt, int code)
{
  box_t box;

  switch (code)
  {
  case 'b': case 'B': painting_set_tool(pt, painting_TOOL_BRUSH);  break;
  case 'e': case 'E': painting_set_tool(pt, painting_TOOL_ERASER); break;
  case 'l': case 'L': painting_set_tool(pt, painting_TOOL_LINE);   break;
  case 'r': case 'R': painting_set_tool(pt, painting_TOOL_RECT);   break;
  case 'x': case 'X': painting_set_tool(pt, painting_TOOL_RECT_FILL); break;
  case 'f': case 'F': painting_set_tool(pt, painting_TOOL_FILL);   break;
  case 'p': case 'P': painting_set_tool(pt, painting_TOOL_SPRAY);  break;
  case 's': case 'S': painting_set_tool(pt, painting_TOOL_SELECT); break;
  case 'o': case 'O': painting_set_tool(pt, painting_TOOL_LASSO);  break;
  case '[':           painting_step_size(pt, -1);               break;
  case ']':           painting_step_size(pt, +1);               break;
  case wuss_KEY_ESCAPE:
    box = paintingdoc_selection_bounds(pt->doc);
    paintingdoc_select_none(pt->doc);
    painting_invalidate_box(pt, &box);
    break;
  default:            return result_WUSS_KEY_UNCLAIMED;
  }

  return result_OK;
}

/* apply a Selection menu row, redrawing where the selection was and is */
static result_t painting_selection_op(painting_task_t *pt, int op)
{
  result_t rc;
  box_t    old, now;

  old = paintingdoc_selection_bounds(pt->doc);
  rc  = result_OK;

  switch (op)
  {
  case PAINTING_SEL_ALL:    rc = paintingdoc_select_all(pt->doc);    break;
  case PAINTING_SEL_NONE:   paintingdoc_select_none(pt->doc);        break;
  case PAINTING_SEL_INVERT: rc = paintingdoc_select_invert(pt->doc); break;
  default:               break;
  }

  now = paintingdoc_selection_bounds(pt->doc);
  painting_invalidate_box(pt, &old);
  painting_invalidate_box(pt, &now);
  painting_menu_shade(pt);

  return rc;
}

/* a MENU click on either window opens the menu, with Undo greyed out when
 * there is nothing to undo and Deselect when there is nothing selected */
static result_t painting_menu_open(painting_task_t *pt)
{
  painting_menu_shade(pt);

  return wuss_menu_open_at_pointer(pt->delegate, &pt->menu, &pt->menu_handle);
}

result_t painting_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  painting_task_t *pt;

  pt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    if (window == pt->toolbox)
      return painting_toolbox_redraw(pt, event);
    return painting_redraw(pt, event);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action == wuss_MOUSE_DOWN &&
        (event->data.mouse.button & wuss_BUTTON_MENU))
      return painting_menu_open(pt);

    if (window == pt->toolbox)
      return painting_toolbox_mouse(pt, event);
    return painting_mouse(pt, event);

  case wuss_EVENT_ICON:
    return painting_toolbox_icon(pt, event);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (!task_key_is_plain(pt->delegate, &pt->menu, event, &rc))
      return rc;

    return painting_key(pt, event->data.key.code);
  }

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &pt->sel_menu)
      return painting_selection_op(pt, event->data.menu_select.index);
    if (event->data.menu_select.index == PAINTING_MENU_UNDO)
      return painting_undo(pt);
    /* clicking the row, rather than hovering, shows Save As standalone */
    if (event->data.menu_select.index == PAINTING_MENU_SAVE)
      return wuss_saveas_open(pt->saveas);
    return result_OK;

  case wuss_EVENT_MESSAGE:
    if (pt->window == NULL || window != pt->window)
      return result_OK;
    return taskapp_window_data_load(pt->wuss, pt->delegate, pt,
                                    (taskapp_data_load_fn_t) painting_load,
                                    event->data.message);

  case wuss_EVENT_IDLE:
    if (pt->window == NULL)
      return result_OK;

    /* the spray keeps building while the button is held still */
    if (pt->held && pt->tool == painting_TOOL_SPRAY)
    {
      box_t dirty;

      dirty = paintingdoc_spray(pt->doc, pt->last, pt->size * PAINTING_SPRAY_SCALE,
                             painting_stroke_colour(pt), &pt->rng);
      painting_invalidate_box(pt, &dirty);
    }

    if (!paintingdoc_has_selection(pt->doc))
      return result_OK;

    if (++pt->ants_tick >= PAINTING_ANTS_IDLES)
    {
      box_t bounds;

      pt->ants_tick  = 0;
      pt->ants_phase = (pt->ants_phase + 1) & 7;

      bounds = paintingdoc_selection_bounds(pt->doc);
      wuss_window_invalidate(pt->window, &bounds);
    }
    return result_OK;

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
    painting_destroy(pt);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
