/* wuss/test/tasks/config.c -- startup settings task (mouse button swap,
 * reverse scroll, backdrop swatches) */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/pattern.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"
#include "geom/stack.h"
#include "wuss/icon-spec.h"

#include "tasks.h" /* g_tasks.swap_mouse_buttons */

#include "config.h"

/* ----------------------------------------------------------------------- */

#define CONFIG_ROW    20  /* px; System frame's option-icon row pitch */

/* Backdrop swatches: colour/pattern cells are 22px square, butted together
 * (no pitch) as in swatches.c; the result swatch is double that. */
#define CONFIG_CELL       22
#define CONFIG_RESULT     (CONFIG_CELL * 2)
#define CONFIG_NCOLOURS   16 /* see swatches.c's SWATCHES_NCOLOURS ponytail
                              * note: same hardcoded system-palette size */
#define CONFIG_GRID_COLS  CONFIG_NCOLOURS /* patterns grid is no wider than
                                           * the colour rows, so all three
                                           * line up */

/* "Solid" patterns (the Bayer run's flat ends, EMPTY and SOLID) come first,
 * on their own row; every other pattern -- the remaining Bayer dither
 * levels, then the named hatches (stripes, diagonal, dots, grid,
 * crosshatch, and their inverses) -- follows on the rows below, after a
 * blank spacer row. */
#define CONFIG_NSOLID     2
#define CONFIG_NDITHER    (screen_PATTERN_BAYER_LIMIT - screen_PATTERN_BAYER0 - \
                           CONFIG_NSOLID) /* Bayer levels 1..63 */
#define CONFIG_NHATCH     (screen_PATTERN__LIMIT - screen_PATTERN_BAYER_LIMIT)
#define CONFIG_NREST      (CONFIG_NDITHER + CONFIG_NHATCH)
#define CONFIG_REST_ROWS  ((CONFIG_NREST + CONFIG_GRID_COLS - 1) / CONFIG_GRID_COLS)
#define CONFIG_GRID_ROWS  (1 /* solids */ + 1 /* space */ + CONFIG_REST_ROWS)

#define CONFIG_LABEL_W    70 /* px; enough for "Background" at 6px/char */

/* MENU click pops this single-item menu; the item table and wuss_menu_t live
 * per-instance in config_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum { CONFIG_MENU_INFO };

/* Root layout: a "System" frame (the two option icons) above a "Backdrop"
 * frame (colour/pattern swatches). Each frame is a single stack leaf --
 * its children are hand-placed inside the solved box below, the same way
 * icons.c positions a grouping frame's contents, rather than being
 * descended from the stack tree themselves. */
enum { CONFIG_ST_ROOT, CONFIG_ST_SYSTEM, CONFIG_ST_BACKDROP, CONFIG_ST__LIMIT };

#define CONFIG_SYSTEM_H   (20 + CONFIG_ROW * 2 + 8) /* caption + 2 rows + pad */
#define CONFIG_GRID_H     (CONFIG_GRID_ROWS * CONFIG_CELL)
#define CONFIG_BACKDROP_H (20 + CONFIG_CELL + wuss_STD_GAP + CONFIG_CELL + \
                          wuss_STD_GAP + CONFIG_GRID_H + wuss_STD_GAP + \
                          CONFIG_RESULT + wuss_STD_GAP + \
                          wuss_STD_SECONDARY_BUTTON_HEIGHT + 8)

static const stack_item_t g_config_stack[CONFIG_ST__LIMIT] =
{
  [CONFIG_ST_ROOT]     = STACK_VBOX_EX(-1, 0, wuss_STD_GAP,
                                       wuss_STD_INSET, wuss_STD_INSET,
                                       wuss_STD_INSET, wuss_STD_INSET),
  [CONFIG_ST_SYSTEM]   = STACK_LEAF(CONFIG_ST_ROOT, CONFIG_SYSTEM_H, 0,
                                    stack_ALIGN_FILL),
  [CONFIG_ST_BACKDROP] = STACK_LEAF(CONFIG_ST_ROOT, CONFIG_BACKDROP_H, 0,
                                    stack_ALIGN_FILL),
};

#define CONFIG_DOC_W (wuss_STD_INSET * 2 + CONFIG_LABEL_W + \
                     CONFIG_GRID_COLS * CONFIG_CELL)

/* ----------------------------------------------------------------------- */

/* Icons made inside the "Backdrop" frame's box that aren't swatches: two
 * option-row labels' worth of static furniture (frame captions are drawn by
 * the frame icon itself, not a separate label) plus the row labels and the
 * "Set backdrop" button. Swatches (colour rows, pattern grid, result) have
 * no retained icons -- they're redrawn from task state via wuss_icon_plot,
 * the same approach as swatches.c's grid, and hit-tested by arithmetic in
 * config_click. */
enum
{
  CONFIG_ICON_SYSTEM_FRAME,
  CONFIG_ICON_BACKDROP_FRAME,
  CONFIG_ICON_FG_LABEL,
  CONFIG_ICON_BG_LABEL,
  CONFIG_ICON_PATTERNS_LABEL,
  CONFIG_ICON_SET_BACKDROP,
  CONFIG_NICONS
};

/* Backdrop-frame-relative Y of each row/block, filled once by
 * config_layout_backdrop and reused by both the redraw and the click
 * hit-test so the two can never drift apart. */
typedef struct config_backdrop_layout
{
  box_t frame;   /* the Backdrop frame's own box, document space */
  int   fg_y;
  int   bg_y;
  int   grid_y;
  int   result_y;
  int   button_y;
  int   swatch_x; /* left edge of every swatch column and the grid */
}
config_backdrop_layout_t;

static void config_layout_backdrop(const box_t              *frame,
                                   config_backdrop_layout_t *out)
{
  out->frame    = *frame;
  out->swatch_x = frame->x0 + CONFIG_LABEL_W;
  out->fg_y     = frame->y0 + 20;
  out->bg_y     = out->fg_y + CONFIG_CELL + wuss_STD_GAP;
  out->grid_y   = out->bg_y + CONFIG_CELL + wuss_STD_GAP;
  out->result_y = out->grid_y + CONFIG_GRID_H + wuss_STD_GAP;
  out->button_y = out->result_y + CONFIG_RESULT + wuss_STD_GAP;
}

/* The pattern shown in grid cell i, 0 <= i < CONFIG_NSOLID + CONFIG_NREST:
 * the two solids (EMPTY, then SOLID) first, each alone on row 0; every other
 * pattern next (Bayer levels 1..63, then the named hatches), starting on
 * row 2 (row 1 left as the "space to the next row" the brief asks for). */
static screen_pattern_t config_grid_pattern(int i)
{
  if (i == 0)
    return screen_PATTERN_EMPTY;
  if (i == 1)
    return screen_PATTERN_SOLID;

  i -= CONFIG_NSOLID;
  if (i < CONFIG_NDITHER)
    return (screen_pattern_t) (screen_PATTERN_BAYER0 + 1 + i);
  return (screen_pattern_t) (screen_PATTERN_BAYER_LIMIT + (i - CONFIG_NDITHER));
}

/* Cell (col,row) for grid pattern index i: the solids sit at columns 0/1 of
 * row 0; every Bayer level starts a fresh row 2 (row 1 is the blank
 * spacer), wrapping at CONFIG_GRID_COLS. */
static void config_grid_cell(int i, int *col, int *row)
{
  if (i < CONFIG_NSOLID)
  {
    *col = i;
    *row = 0;
    return;
  }

  i    -= CONFIG_NSOLID;
  *col  = i % CONFIG_GRID_COLS;
  *row  = 2 + i / CONFIG_GRID_COLS;
}

/* ----------------------------------------------------------------------- */

result_t config_create(wuss_t *wuss, config_task_t **out)
{
  result_t                 rc;
  config_task_t           *task;
  wuss_task_t             *delegate;
  wuss_task_desc_t         delegate_desc;
  box_t                    boxes[CONFIG_ST__LIMIT];
  box_t                    root;
  config_backdrop_layout_t lay;
  wuss_icon_spec_t         specs[CONFIG_NICONS];
  wuss_icon_t             *made[CONFIG_NICONS];
  size2d_t                 doc;
  int                      y;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss    = wuss;
  task->fg      = wuss_COLOUR_BLACK;
  task->bg      = wuss_COLOUR_WHITE;
  task->pattern = screen_PATTERN_SOLID;

  delegate_desc.handle    = config_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "config";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate;

  doc = SIZE2D(CONFIG_DOC_W,
              wuss_STD_INSET * 3 + CONFIG_SYSTEM_H + CONFIG_BACKDROP_H);

  rc = wuss_window_create_placed(delegate,
                                 doc,
                                 "Configure",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT,
                                 wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                                 doc,
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  root = (box_t) BOX_POS_SIZE(0, 0, doc.w, doc.h);
  rc = stack_solve(g_config_stack, NELEMS(g_config_stack), &root, boxes);
  if (rc != result_OK)
    goto fail_delegate;

  task->backdrop_frame = boxes[CONFIG_ST_BACKDROP];
  config_layout_backdrop(&task->backdrop_frame, &lay);

  memset(specs, 0, sizeof(specs));

  specs[CONFIG_ICON_SYSTEM_FRAME].bbox = boxes[CONFIG_ST_SYSTEM];
  specs[CONFIG_ICON_SYSTEM_FRAME].type = wuss_ICON_TYPE_FRAME;
  specs[CONFIG_ICON_SYSTEM_FRAME].text = "System";
  specs[CONFIG_ICON_SYSTEM_FRAME].fg   = wuss_COLOUR_BLACK;
  specs[CONFIG_ICON_SYSTEM_FRAME].bg   = wuss_NO_BACKGROUND;

  specs[CONFIG_ICON_BACKDROP_FRAME].bbox = lay.frame;
  specs[CONFIG_ICON_BACKDROP_FRAME].type = wuss_ICON_TYPE_FRAME;
  specs[CONFIG_ICON_BACKDROP_FRAME].text = "Backdrop";
  specs[CONFIG_ICON_BACKDROP_FRAME].fg   = wuss_COLOUR_BLACK;
  specs[CONFIG_ICON_BACKDROP_FRAME].bg   = wuss_NO_BACKGROUND;

  wuss_icon_spec_label(&specs[CONFIG_ICON_FG_LABEL],
                       (box_t) BOX_POS_SIZE(lay.frame.x0 + wuss_STD_INSET,
                                            lay.fg_y, CONFIG_LABEL_W - wuss_STD_INSET,
                                            CONFIG_CELL),
                       "Foreground", 0);
  wuss_icon_spec_label(&specs[CONFIG_ICON_BG_LABEL],
                       (box_t) BOX_POS_SIZE(lay.frame.x0 + wuss_STD_INSET,
                                            lay.bg_y, CONFIG_LABEL_W - wuss_STD_INSET,
                                            CONFIG_CELL),
                       "Background", 0);
  wuss_icon_spec_label(&specs[CONFIG_ICON_PATTERNS_LABEL],
                       (box_t) BOX_POS_SIZE(lay.frame.x0 + wuss_STD_INSET,
                                            lay.grid_y, CONFIG_LABEL_W - wuss_STD_INSET,
                                            CONFIG_CELL),
                       "Patterns", 0);

  wuss_icon_spec_action(&specs[CONFIG_ICON_SET_BACKDROP],
                        (box_t) BOX_POS_SIZE(lay.swatch_x, lay.button_y,
                                             CONFIG_GRID_COLS * CONFIG_CELL,
                                             wuss_STD_SECONDARY_BUTTON_HEIGHT),
                        "Set backdrop", 0);

  y = boxes[CONFIG_ST_SYSTEM].y0 + 20;

  rc = wuss_icon_create_array(task->window, specs, CONFIG_NICONS, made);
  if (rc != result_OK)
    goto fail_delegate;

  task->set_backdrop_icon = made[CONFIG_ICON_SET_BACKDROP];

  {
    wuss_icon_spec_t spec;

    memset(&spec, 0, sizeof(spec));
    spec.bbox = (box_t) BOX_POS_SIZE(boxes[CONFIG_ST_SYSTEM].x0 + wuss_STD_INSET,
                                     y,
                                     box_size(&boxes[CONFIG_ST_SYSTEM]).w - 2 * wuss_STD_INSET,
                                     16);
    spec.type = wuss_ICON_TYPE_OPTION;
    spec.text = "Swap right/middle mouse buttons";
    spec.fg   = wuss_COLOUR_BLACK;
    spec.bg   = wuss_NO_BACKGROUND;

    rc = wuss_icon_create(task->window, &spec, &task->swap_icon);
    if (rc != result_OK)
      goto fail_delegate;
    wuss_icon_set_selected(task->window, task->swap_icon,
                           g_tasks.swap_mouse_buttons);

    spec.bbox = (box_t) BOX_POS_SIZE(boxes[CONFIG_ST_SYSTEM].x0 + wuss_STD_INSET,
                                     y + CONFIG_ROW,
                                     box_size(&boxes[CONFIG_ST_SYSTEM]).w - 2 * wuss_STD_INSET,
                                     16);
    spec.text = "Reverse mouse scroll direction";

    rc = wuss_icon_create(task->window, &spec, &task->reverse_scroll_icon);
    if (rc != result_OK)
      goto fail_delegate;
    wuss_icon_set_selected(task->window, task->reverse_scroll_icon,
                           g_tasks.reverse_scroll);
  }

  /* fully built: from here a last-window close reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  WUSS_MENU_ITEM_WINDOW(task->menu_items, CONFIG_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * config_handle */

  WUSS_MENU_TITLE(task->menu, "Configure", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;

fail_delegate:
  wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
  return rc;
}

void config_destroy(config_task_t *task)
{
  if (task->menu_handle != NULL)
    wuss_menu_close(task->menu_handle);
  free(task);
}

/* Plot one CONFIG_CELL-square colour swatch, solid-filled in colour; a
 * selected swatch first gets an oversized black square underneath, giving
 * it a 2px black surround. */
static result_t config_plot_swatch(wuss_window_t *window,
                                   int            x,
                                   int            y,
                                   wuss_colour_t  colour,
                                   int            selected,
                                   const box_t   *bounds,
                                   point_t        scroll)
{
  wuss_icon_spec_t spec;
  result_t         rc;

  memset(&spec, 0, sizeof(spec));
  spec.type           = wuss_ICON_TYPE_PATTERN;
  spec.u.pattern.tile = screen_PATTERN_SOLID;

  if (selected)
  {
    spec.bbox = (box_t) BOX_POS_SIZE(x, y, CONFIG_CELL, CONFIG_CELL);
    spec.fg = spec.bg = wuss_COLOUR_BLACK;
    rc = wuss_icon_plot(window, &spec, bounds, scroll);
    if (rc != result_OK)
      return rc;

    x += 2;
    y += 2;
  }

  spec.bbox = (box_t) BOX_POS_SIZE(x, y, CONFIG_CELL - (selected ? 4 : 0),
                                   CONFIG_CELL - (selected ? 4 : 0));
  spec.fg = spec.bg = colour;
  return wuss_icon_plot(window, &spec, bounds, scroll);
}

/* Redraw: the two swatch rows (fg/bg -- CONFIG_NCOLOURS palette entries each,
 * every cell a solid block of its own colour; the selected cell gets a 2px
 * black surround), the pattern grid, and the result square mixing
 * task->fg/task->pattern/task->bg. No icons are retained for any of this;
 * wuss_icon_plot resolves the palette live, so a palette change just
 * redraws. */
static result_t config_redraw(config_task_t *task, const wuss_event_t *event)
{
  result_t                 rc;
  config_backdrop_layout_t lay;
  wuss_icon_spec_t         spec;
  const box_t             *bounds;
  point_t                  scroll;
  int                      col, i;

  config_layout_backdrop(&task->backdrop_frame, &lay);

  bounds = event->data.redraw.bounds;
  scroll = event->data.redraw.scroll;

  memset(&spec, 0, sizeof(spec));
  spec.type = wuss_ICON_TYPE_PATTERN;

  for (col = 0; col < CONFIG_NCOLOURS; col++)
  {
    rc = config_plot_swatch(task->window, lay.swatch_x + col * CONFIG_CELL,
                            lay.fg_y, (wuss_colour_t) col, col == task->fg,
                            bounds, scroll);
    if (rc != result_OK)
      return rc;

    rc = config_plot_swatch(task->window, lay.swatch_x + col * CONFIG_CELL,
                            lay.bg_y, (wuss_colour_t) col, col == task->bg,
                            bounds, scroll);
    if (rc != result_OK)
      return rc;
  }

  spec.fg = task->fg;
  spec.bg = task->bg;
  for (i = 0; i < CONFIG_NSOLID + CONFIG_NREST; i++)
  {
    int row, gcol;

    config_grid_cell(i, &gcol, &row);
    spec.bbox = (box_t) BOX_POS_SIZE(lay.swatch_x + gcol * CONFIG_CELL,
                                     lay.grid_y + row * CONFIG_CELL,
                                     CONFIG_CELL, CONFIG_CELL);
    spec.u.pattern.tile = config_grid_pattern(i);

    rc = wuss_icon_plot(task->window, &spec, bounds, scroll);
    if (rc != result_OK)
      return rc;
  }

  spec.bbox = (box_t) BOX_POS_SIZE(lay.swatch_x, lay.result_y,
                                   CONFIG_RESULT, CONFIG_RESULT);
  spec.u.pattern.tile = task->pattern;
  rc = wuss_icon_plot(task->window, &spec, bounds, scroll);
  if (rc != result_OK)
    return rc;

  return result_OK;
}

static result_t config_icon(const wuss_event_t *event, void *task_data)
{
  config_task_t *cc;
  wuss_icon_t   *icon;

  cc   = task_data;
  icon = event->data.icon.icon;

  if (event->data.icon.action != wuss_MOUSE_UP)
    return result_OK;

  if (icon == cc->swap_icon)
    g_tasks.swap_mouse_buttons = wuss_icon_get_selected(icon) ? true : false;
  else if (icon == cc->reverse_scroll_icon)
    g_tasks.reverse_scroll = wuss_icon_get_selected(icon) ? true : false;
  else if (icon == cc->set_backdrop_icon)
  {
    wuss_backdrop_t backdrop;

    backdrop = wuss_BACKDROP_PATTERN(cc->fg, cc->pattern, cc->bg);
    return wuss_set_backdrop(cc->wuss, &backdrop);
  }

  return result_OK;
}

/* A click on a swatch (fg row, bg row or pattern grid) updates the matching
 * task field and redraws every swatch (the selection highlight on the fg/bg
 * rows and the result both depend on all three fields together). Clicks
 * outside every swatch fall through untouched. */
static result_t config_click(config_task_t *task, const wuss_event_t *event)
{
  config_backdrop_layout_t lay;
  point_t                  pt;
  int                      col, row;

  config_layout_backdrop(&task->backdrop_frame, &lay);

  pt  = event->data.mouse.point;
  col = (pt.x - lay.swatch_x) / CONFIG_CELL;

  if (pt.x < lay.swatch_x || col >= CONFIG_NCOLOURS)
    return result_OK;

  if (pt.y >= lay.fg_y && pt.y < lay.fg_y + CONFIG_CELL)
    task->fg = (wuss_colour_t) col;
  else if (pt.y >= lay.bg_y && pt.y < lay.bg_y + CONFIG_CELL)
    task->bg = (wuss_colour_t) col;
  else if (pt.y >= lay.grid_y && pt.y < lay.grid_y + CONFIG_GRID_H)
  {
    int i;

    row = (pt.y - lay.grid_y) / CONFIG_CELL;
    for (i = 0; i < CONFIG_NSOLID + CONFIG_NREST; i++)
    {
      int gcol, grow;

      config_grid_cell(i, &gcol, &grow);
      if (gcol == col && grow == row)
      {
        task->pattern = config_grid_pattern(i);
        break;
      }
    }
  }
  else
    return result_OK;

  wuss_window_invalidate_extent(task->window);
  return result_OK;
}

result_t config_handle(wuss_window_t      *window,
                       const wuss_event_t *event,
                       void               *task_data)
{
  config_task_t *cc;

  cc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return config_redraw(cc, event);

  case wuss_EVENT_ICON:
    return config_icon(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != cc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action == wuss_MOUSE_DOWN &&
        (event->data.mouse.button & wuss_BUTTON_SELECT))
      return config_click(cc, event);
    if (event->data.mouse.action != wuss_MOUSE_DOWN ||
        !(event->data.mouse.button & wuss_BUTTON_MENU))
      return result_OK;
    {
      static const wuss_proginfo_desc_t desc =
      {
        "Configure",
        "Startup settings: buttons, scroll, backdrop",
        "(c) DPTLib contributors",
        "1.0 (" __DATE__ ")"
      };
      wuss_proginfo_set_desc(&desc);
      cc->menu_items[CONFIG_MENU_INFO].window =
        wuss_proginfo_window(cc->delegate);
    }
    return wuss_menu_open(cc->delegate, &cc->menu,
                          wuss_get_pointer(cc->wuss), &cc->menu_handle);

  case wuss_EVENT_MENU_CLOSED:
    cc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == cc->window)
      cc->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == cc->menu_items[CONFIG_MENU_INFO].window)
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
    config_destroy(cc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
