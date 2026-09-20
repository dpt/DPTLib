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

#define CONFIG_ROW        22  /* px; System frame's option-icon row pitch */

/* Backdrop swatches: colour/pattern cells are 22px square, butted together
 * (no pitch) as in swatches.c; the result swatch is double that. */
#define CONFIG_CELL       22
#define CONFIG_RESULT     (CONFIG_CELL * 2)
#define CONFIG_NCOLOURS   wuss_SYSTEM_PALETTE_LENGTH
#define CONFIG_GRID_COLS  CONFIG_NCOLOURS /* patterns grid is no wider than
                                           * the colour rows, so all three
                                           * line up */

/* The named hatch patterns (stripes, diagonal, dots, grid, crosshatch, and
 * their inverses) come first, padded out with blank cells to the next row
 * boundary. The full Bayer run (screen_PATTERN_BAYER0..BAYER_LIMIT-1, which
 * includes the EMPTY and SOLID flat ends) follows in order, filling the
 * rows below that. */
#define CONFIG_NHATCH     (screen_PATTERN__LIMIT - screen_PATTERN_BAYER_LIMIT)
#define CONFIG_HATCH_ROWS ((CONFIG_NHATCH + CONFIG_GRID_COLS - 1) / CONFIG_GRID_COLS)
#define CONFIG_NBAYER     (screen_PATTERN_BAYER_LIMIT - screen_PATTERN_BAYER0)
#define CONFIG_NPATTERNS  (CONFIG_NHATCH + CONFIG_NBAYER)
#define CONFIG_BAYER_ROWS ((CONFIG_NBAYER + CONFIG_GRID_COLS - 1) / CONFIG_GRID_COLS)
#define CONFIG_GRID_ROWS  (CONFIG_HATCH_ROWS + CONFIG_BAYER_ROWS)

#define CONFIG_LABEL_W    (10 * 6) /* px; enough for "Background" at 6px/char */

/* MENU click pops this single-item menu; the item table and wuss_menu_t live
 * per-instance in config_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum
{
  CONFIG_MENU_INFO
};

/* Root layout: a "System" frame (the two option icons) above a "Backdrop"
 * frame (colour/pattern swatches). Each frame is a single stack leaf --
 * its children are hand-placed inside the solved box below, the same way
 * icons.c positions a grouping frame's contents, rather than being
 * descended from the stack tree themselves. */
enum
{
  CONFIG_ST_ROOT,
  CONFIG_ST_SYSTEM,
  CONFIG_ST_SWAP,
  CONFIG_ST_REVERSE_SCROLL,
  CONFIG_ST_BACKDROP,
  CONFIG_ST__LIMIT
};

#define CONFIG_DOC_W            (wuss_STD_INSET * 4 + CONFIG_LABEL_W + CONFIG_GRID_COLS * CONFIG_CELL)

#define CONFIG_GRID_H           (CONFIG_GRID_ROWS * CONFIG_CELL)

#define CONFIG_BACKDROP_H       (20 + CONFIG_CELL + wuss_STD_GAP + CONFIG_CELL + wuss_STD_GAP + CONFIG_GRID_H + wuss_STD_GAP + CONFIG_RESULT + wuss_STD_GAP + wuss_STD_SECONDARY_BUTTON_HEIGHT + 8)

/* CONFIG_ST_SYSTEM is itself the container the two option icons stack
 * inside (a VBOX, not a leaf, so stack_solve descends into it): padded off
 * the frame's caption row on top and off the frame edges on the other three
 * sides, its children laid out at CONFIG_ROW pitch. The Backdrop frame's
 * contents remain hand-placed inside its solved leaf box, the same way
 * icons.c positions a grouping frame's contents. */
static const stack_item_t g_config_stack[CONFIG_ST__LIMIT] =
{
  [CONFIG_ST_ROOT] = { .kind = stack_KIND_VBOX, .parent = -1,
                      .gap = wuss_STD_GAP, .pad = wuss_STD_INSETS },

  [CONFIG_ST_SYSTEM] =
  {
    .kind      = stack_KIND_VBOX,
    .parent    = CONFIG_ST_ROOT,
    .axis_size = STACK_HUG,
    .gap       = CONFIG_ROW - 16, /* CONFIG_ROW is the option icon's outer
                                   * pitch; the leaf itself is 16px tall, so
                                   * this gap closes the pitch back up */
    .align     = stack_ALIGN_FILL,
    .pad       = INSET(wuss_STD_FRAME_INSET + wuss_STD_INSET,
                       wuss_STD_FRAME_INSET + wuss_STD_INSET,
                       wuss_STD_FRAME_INSET + wuss_STD_INSET,
                       wuss_STD_FRAME_INSET + wuss_STD_INSET),
  },

  [CONFIG_ST_SWAP] = STACK_LEAF(CONFIG_ST_SYSTEM,
                                16,
                                0,
                                stack_ALIGN_FILL),

  [CONFIG_ST_REVERSE_SCROLL] = STACK_LEAF(CONFIG_ST_SYSTEM,
                                          16,
                                          0,
                                          stack_ALIGN_FILL),

  [CONFIG_ST_BACKDROP] = STACK_LEAF(CONFIG_ST_ROOT,
                                    CONFIG_BACKDROP_H,
                                    0,
                                    stack_ALIGN_FILL),
};

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
  out->swatch_x = frame->x0     + CONFIG_LABEL_W;
  out->fg_y     = frame->y0     + 20;
  out->bg_y     = out->fg_y     + CONFIG_CELL   + wuss_STD_GAP;
  out->grid_y   = out->bg_y     + CONFIG_CELL   + wuss_STD_GAP;
  out->result_y = out->grid_y   + CONFIG_GRID_H + wuss_STD_GAP;
  out->button_y = out->result_y + CONFIG_RESULT + wuss_STD_GAP;
}

/* The pattern shown in grid cell i, 0 <= i < CONFIG_NPATTERNS: the named
 * hatches first, then the full Bayer run (EMPTY, dither levels 1..63,
 * SOLID) in order. */
static screen_pattern_t config_grid_pattern(int i)
{
  if (i < CONFIG_NHATCH)
    return (screen_pattern_t) (screen_PATTERN_BAYER_LIMIT + i);
  return (screen_pattern_t) (screen_PATTERN_BAYER0 + (i - CONFIG_NHATCH));
}

/* Cell (col,row) for grid pattern index i: the named hatches fill row 0
 * onward, padded to a row boundary; the Bayer run starts its own fresh row
 * after that padding, wrapping at CONFIG_GRID_COLS. */
static void config_grid_cell(int i, int *col, int *row)
{
  if (i < CONFIG_NHATCH)
  {
    *col = i % CONFIG_GRID_COLS;
    *row = i / CONFIG_GRID_COLS;
    return;
  }

  i    -= CONFIG_NHATCH;
  *col  = i % CONFIG_GRID_COLS;
  *row  = CONFIG_HATCH_ROWS + i / CONFIG_GRID_COLS;
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
  size2d_t                 min_sz;

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

  rc = stack_smallest(g_config_stack, NELEMS(g_config_stack), &min_sz);
  if (rc != result_OK)
    goto fail_delegate;

  doc = SIZE2D(CONFIG_DOC_W, min_sz.h);

  rc = wuss_window_create_placed(delegate,
                                 doc,
                                 "Configure",
                                 wuss_WINDOW_DEFAULT,
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

  wuss_icon_spec_frame(&specs[CONFIG_ICON_SYSTEM_FRAME],
                       boxes[CONFIG_ST_SYSTEM], "System");

  wuss_icon_spec_frame(&specs[CONFIG_ICON_BACKDROP_FRAME], lay.frame,
                       "Backdrop");

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

  rc = wuss_icon_create_array(task->window, specs, CONFIG_NICONS, made);
  if (rc != result_OK)
    goto fail_delegate;

  task->set_backdrop_icon = made[CONFIG_ICON_SET_BACKDROP];

  {
    wuss_icon_spec_t spec;

    wuss_icon_spec_option(&spec, boxes[CONFIG_ST_SWAP],
                          "Swap right/middle mouse buttons");
    rc = wuss_icon_create(task->window, &spec, &task->swap_icon);
    if (rc != result_OK)
      goto fail_delegate;

    wuss_icon_set_selected(task->window, task->swap_icon,
                           g_tasks.swap_mouse_buttons);

    wuss_icon_spec_option(&spec, boxes[CONFIG_ST_REVERSE_SCROLL],
                          "Reverse mouse scroll direction");
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
    rc = config_plot_swatch(task->window,
                            lay.swatch_x + col * CONFIG_CELL,
                            lay.fg_y,
                            (wuss_colour_t) col, col == task->fg,
                            bounds,
                            scroll);
    if (rc != result_OK)
      return rc;

    rc = config_plot_swatch(task->window,
                            lay.swatch_x + col * CONFIG_CELL,
                            lay.bg_y,
                            (wuss_colour_t) col, col == task->bg,
                            bounds,
                            scroll);
    if (rc != result_OK)
      return rc;
  }

  spec.fg = task->fg;
  spec.bg = task->bg;

  for (i = 0; i < CONFIG_NPATTERNS; i++)
  {
    int row, gcol;

    config_grid_cell(i, &gcol, &row);
    spec.bbox = (box_t) BOX_POS_SIZE(lay.swatch_x + gcol * CONFIG_CELL,
                                     lay.grid_y   + row  * CONFIG_CELL,
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
    g_tasks.swap_mouse_buttons = !!wuss_icon_get_selected(icon);
  else if (icon == cc->reverse_scroll_icon)
    g_tasks.reverse_scroll = !!wuss_icon_get_selected(icon);
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
    for (i = 0; i < CONFIG_NPATTERNS; i++)
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
        "System settings",
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
