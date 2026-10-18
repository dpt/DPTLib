/* wuss/tasks/configuring.c -- startup settings task (mouse button swap,
 * reverse scroll, system font, backdrop swatches) */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bmfontcache.h"
#include "framebuf/bmfontfamily.h"
#include "framebuf/pattern.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"
#include "geom/stack.h"
#include "io/path.h"
#include "wuss/component/fontmenu.h"
#include "wuss/icon-spec.h"

#include "tasks.h" /* g_tasks.swap_mouse_buttons */

#include "configuring.h"
#include "taskapp.h"
#include "common.h"

/* ----------------------------------------------------------------------- */

#define CONFIGURING_ROW        22  /* px; System frame's option-icon row pitch */

/* Shown in the Timing frame's writable fields until the user edits and
 * applies; match wuss_config_t's documented "0 means default" values. */
#define CONFIGURING_DOUBLE_CLICK_MS_DEFAULT     400
#define CONFIGURING_DOUBLE_CLICK_PX_DEFAULT     4
#define CONFIGURING_DRAG_THRESHOLD_PX_DEFAULT   4

/* Backdrop swatches: colour/pattern cells are 22px square, butted together
 * (no pitch) as in swatches.c; the result swatch is double that. */
#define CONFIGURING_CELL       22
#define CONFIGURING_RESULT     (CONFIGURING_CELL * 2)
#define CONFIGURING_NCOLOURS   wuss_SYSTEM_PALETTE_LENGTH
#define CONFIGURING_GRID_COLS  CONFIGURING_NCOLOURS /* patterns grid is no wider than
                                           * the colour rows, so all three
                                           * line up */

/* The named hatch patterns (stripes, diagonal, dots, grid, crosshatch, and
 * their inverses) come first, padded out with blank cells to the next row
 * boundary. The full Bayer run (screen_PATTERN_BAYER0..BAYER_LIMIT-1, which
 * includes the EMPTY and SOLID flat ends) follows in order, filling the
 * rows below that. */
#define CONFIGURING_NHATCH     (screen_PATTERN__LIMIT - screen_PATTERN_BAYER_LIMIT)
#define CONFIGURING_HATCH_ROWS ((CONFIGURING_NHATCH + CONFIGURING_GRID_COLS - 1) / CONFIGURING_GRID_COLS)
#define CONFIGURING_NBAYER     (screen_PATTERN_BAYER_LIMIT - screen_PATTERN_BAYER0)
#define CONFIGURING_NPATTERNS  (CONFIGURING_NHATCH + CONFIGURING_NBAYER)
#define CONFIGURING_BAYER_ROWS ((CONFIGURING_NBAYER + CONFIGURING_GRID_COLS - 1) / CONFIGURING_GRID_COLS)
#define CONFIGURING_GRID_ROWS  (CONFIGURING_HATCH_ROWS + CONFIGURING_BAYER_ROWS)

#define CONFIGURING_G_LABEL    1 /* size groups, see stack_item_t::group */
#define CONFIGURING_G_UNIT     2

/* Timing frame's writable fields are narrower than a swatch row. */
#define CONFIGURING_FIELD_W    48

/* Font frame's name display, sized so it and its "Choose" button line up
 * with the swatch rows below. */
#define CONFIGURING_FONT_NAME_W (CONFIGURING_GRID_COLS * CONFIGURING_CELL - \
                                 CONFIGURING_FIELD_W - 4)

/* Root layout: a "System" frame (the two option icons), a "Font" frame (the
 * system font's name and a "Choose" button opening the font menu), a
 * "Timing" frame
 * (three writable fields, each followed by its unit, plus an Apply button)
 * and a "Backdrop" frame (colour/pattern swatches). All three frames are
 * stack containers, their children descended from the stack tree and solved
 * by stack_solve, rather than being hand-placed inside a single leaf box. */
enum
{
  CONFIGURING_ST_ROOT,
  CONFIGURING_ST_SYSTEM,
  CONFIGURING_ST_SWAP,
  CONFIGURING_ST_REVERSE_SCROLL,
  CONFIGURING_ST_FONT,
  CONFIGURING_ST_FONT_HBOX,
  CONFIGURING_ST_FONT_LABEL,
  CONFIGURING_ST_FONT_NAME,
  CONFIGURING_ST_FONT_BUTTON,
  CONFIGURING_ST_TIMING,
  CONFIGURING_ST_DBL_MS_HBOX,
  CONFIGURING_ST_DBL_MS_LABEL,
  CONFIGURING_ST_DBL_MS_FIELD,
  CONFIGURING_ST_DBL_MS_UNIT,
  CONFIGURING_ST_DBL_PX_HBOX,
  CONFIGURING_ST_DBL_PX_LABEL,
  CONFIGURING_ST_DBL_PX_FIELD,
  CONFIGURING_ST_DBL_PX_UNIT,
  CONFIGURING_ST_DRAG_PX_HBOX,
  CONFIGURING_ST_DRAG_PX_LABEL,
  CONFIGURING_ST_DRAG_PX_FIELD,
  CONFIGURING_ST_DRAG_PX_UNIT,
  CONFIGURING_ST_TIMING_BUTTON_HBOX,
  CONFIGURING_ST_TIMING_BUTTON_SPACER,
  CONFIGURING_ST_TIMING_BUTTON,
  CONFIGURING_ST_BACKDROP,
  CONFIGURING_ST_FG_HBOX,
  CONFIGURING_ST_FG_LABEL,
  CONFIGURING_ST_FG_ROW,
  CONFIGURING_ST_BG_HBOX,
  CONFIGURING_ST_BG_LABEL,
  CONFIGURING_ST_BG_ROW,
  CONFIGURING_ST_GRID_HBOX,
  CONFIGURING_ST_GRID_LABEL,
  CONFIGURING_ST_GRID,
  CONFIGURING_ST_RESULT_HBOX,
  CONFIGURING_ST_RESULT_SPACER,
  CONFIGURING_ST_RESULT,
  CONFIGURING_ST_BUTTON_HBOX,
  CONFIGURING_ST_BUTTON_SPACER,
  CONFIGURING_ST_BUTTON,
  CONFIGURING_ST__LIMIT
};

#define CONFIGURING_GRID_H           (CONFIGURING_GRID_ROWS * CONFIGURING_CELL)

/* CONFIGURING_ST_SYSTEM is itself the container the two option icons stack
 * inside (a VBOX, not a leaf, so stack_solve descends into it): padded off
 * the frame's caption row on top and off the frame edges on the other three
 * sides, its children laid out at CONFIGURING_ROW pitch. CONFIGURING_ST_BACKDROP is
 * likewise a VBOX -- its fg/bg swatch rows, pattern grid, result square and
 * "Set backdrop" button are each a leaf, hugging the frame's caption row on
 * top and its bottom edge below; none of them carry a retained icon (the
 * swatches/grid/result are redrawn from task state via wuss_icon_plot), but
 * their solved boxes still drive both the redraw and the click hit-test, so
 * the two can never drift apart. */
static const stack_item_t g_configuring_stack[CONFIGURING_ST__LIMIT] =
{
  [CONFIGURING_ST_ROOT] =
  {
    .kind = stack_KIND_VBOX,
    .parent = -1,
    .gap = wuss_STD_GAP,
    .pad = wuss_STD_INSETS
  },

  [CONFIGURING_ST_SYSTEM] =
  {
    .kind      = stack_KIND_VBOX,
    .parent    = CONFIGURING_ST_ROOT,
    .axis_size = STACK_HUG,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
    .pad       = wuss_STD_FRAME_INSETS,
  },

  [CONFIGURING_ST_SWAP] = STACK_LEAF(CONFIGURING_ST_SYSTEM,
                                CONFIGURING_ROW,
                                0,
                                stack_ALIGN_FILL),

  [CONFIGURING_ST_REVERSE_SCROLL] = STACK_LEAF(CONFIGURING_ST_SYSTEM,
                                          CONFIGURING_ROW,
                                          0,
                                          stack_ALIGN_FILL),

  [CONFIGURING_ST_FONT] =
  {
    .kind      = stack_KIND_VBOX,
    .parent    = CONFIGURING_ST_ROOT,
    .axis_size = STACK_HUG,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
    .pad       = wuss_STD_FRAME_INSETS,
  },

  [CONFIGURING_ST_FONT_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_FONT,
    .axis_size = CONFIGURING_ROW,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_FONT_LABEL] = STACK_LEAF_GROUP(CONFIGURING_ST_FONT_HBOX,
                                                 0,
                                                 CONFIGURING_ROW,
                                                 stack_ALIGN_FILL,
                                                 CONFIGURING_G_LABEL),

  [CONFIGURING_ST_FONT_NAME] = STACK_LEAF(CONFIGURING_ST_FONT_HBOX,
                                          CONFIGURING_FONT_NAME_W,
                                          CONFIGURING_ROW,
                                          stack_ALIGN_FILL),

  [CONFIGURING_ST_FONT_BUTTON] = STACK_LEAF(CONFIGURING_ST_FONT_HBOX,
                                            CONFIGURING_FIELD_W,
                                            CONFIGURING_ROW,
                                            stack_ALIGN_FILL),

  [CONFIGURING_ST_TIMING] =
  {
    .kind      = stack_KIND_VBOX,
    .parent    = CONFIGURING_ST_ROOT,
    .axis_size = STACK_HUG,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
    .pad       = wuss_STD_FRAME_INSETS,
  },

  [CONFIGURING_ST_DBL_MS_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_TIMING,
    .axis_size = CONFIGURING_ROW,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_DBL_MS_LABEL] = STACK_LEAF_GROUP(CONFIGURING_ST_DBL_MS_HBOX,
                                              0,
                                              CONFIGURING_ROW,
                                              stack_ALIGN_FILL,
                                              CONFIGURING_G_LABEL),

  [CONFIGURING_ST_DBL_MS_FIELD] = STACK_LEAF(CONFIGURING_ST_DBL_MS_HBOX,
                                        CONFIGURING_FIELD_W,
                                        CONFIGURING_ROW,
                                        stack_ALIGN_FILL),

  [CONFIGURING_ST_DBL_MS_UNIT] = STACK_LEAF_GROUP(CONFIGURING_ST_DBL_MS_HBOX,
                                             0,
                                             CONFIGURING_ROW,
                                             stack_ALIGN_FILL,
                                             CONFIGURING_G_UNIT),

  [CONFIGURING_ST_DBL_PX_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_TIMING,
    .axis_size = CONFIGURING_ROW,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_DBL_PX_LABEL] = STACK_LEAF_GROUP(CONFIGURING_ST_DBL_PX_HBOX,
                                              0,
                                              CONFIGURING_ROW,
                                              stack_ALIGN_FILL,
                                              CONFIGURING_G_LABEL),

  [CONFIGURING_ST_DBL_PX_FIELD] = STACK_LEAF(CONFIGURING_ST_DBL_PX_HBOX,
                                        CONFIGURING_FIELD_W,
                                        CONFIGURING_ROW,
                                        stack_ALIGN_FILL),

  [CONFIGURING_ST_DBL_PX_UNIT] = STACK_LEAF_GROUP(CONFIGURING_ST_DBL_PX_HBOX,
                                             0,
                                             CONFIGURING_ROW,
                                             stack_ALIGN_FILL,
                                             CONFIGURING_G_UNIT),

  [CONFIGURING_ST_DRAG_PX_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_TIMING,
    .axis_size = CONFIGURING_ROW,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_DRAG_PX_LABEL] = STACK_LEAF_GROUP(CONFIGURING_ST_DRAG_PX_HBOX,
                                               0,
                                               CONFIGURING_ROW,
                                               stack_ALIGN_FILL,
                                               CONFIGURING_G_LABEL),

  [CONFIGURING_ST_DRAG_PX_FIELD] = STACK_LEAF(CONFIGURING_ST_DRAG_PX_HBOX,
                                         CONFIGURING_FIELD_W,
                                         CONFIGURING_ROW,
                                         stack_ALIGN_FILL),

  [CONFIGURING_ST_DRAG_PX_UNIT] = STACK_LEAF_GROUP(CONFIGURING_ST_DRAG_PX_HBOX,
                                              0,
                                              CONFIGURING_ROW,
                                              stack_ALIGN_FILL,
                                              CONFIGURING_G_UNIT),

  [CONFIGURING_ST_TIMING_BUTTON_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_TIMING,
    .axis_size = wuss_STD_SECONDARY_BUTTON_HEIGHT,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_TIMING_BUTTON_SPACER] = STACK_LEAF_GROUP(CONFIGURING_ST_TIMING_BUTTON_HBOX,
                                                       0,
                                                       wuss_STD_SECONDARY_BUTTON_HEIGHT,
                                                       stack_ALIGN_FILL,
                                                       CONFIGURING_G_LABEL),

  [CONFIGURING_ST_TIMING_BUTTON] = STACK_LEAF(CONFIGURING_ST_TIMING_BUTTON_HBOX,
                                         CONFIGURING_FIELD_W,
                                         wuss_STD_SECONDARY_BUTTON_HEIGHT,
                                         stack_ALIGN_FILL),

  [CONFIGURING_ST_BACKDROP] =
  {
    .kind      = stack_KIND_VBOX,
    .parent    = CONFIGURING_ST_ROOT,
    .axis_size = STACK_HUG,
    .gap       = wuss_STD_GAP,
    .align     = stack_ALIGN_FILL,
    .pad       = wuss_STD_FRAME_INSETS,
  },

  [CONFIGURING_ST_FG_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_BACKDROP,
    .axis_size = CONFIGURING_CELL,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_FG_LABEL] = STACK_LEAF_GROUP(CONFIGURING_ST_FG_HBOX,
                                          0,
                                          CONFIGURING_CELL,
                                          stack_ALIGN_FILL,
                                          CONFIGURING_G_LABEL),

  [CONFIGURING_ST_FG_ROW] = STACK_LEAF(CONFIGURING_ST_FG_HBOX,
                                  CONFIGURING_GRID_COLS * CONFIGURING_CELL,
                                  CONFIGURING_CELL,
                                  stack_ALIGN_FILL),

  [CONFIGURING_ST_BG_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_BACKDROP,
    .axis_size = CONFIGURING_CELL,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_BG_LABEL] = STACK_LEAF_GROUP(CONFIGURING_ST_BG_HBOX,
                                          0,
                                          CONFIGURING_CELL,
                                          stack_ALIGN_FILL,
                                          CONFIGURING_G_LABEL),

  [CONFIGURING_ST_BG_ROW] = STACK_LEAF(CONFIGURING_ST_BG_HBOX,
                                  CONFIGURING_GRID_COLS * CONFIGURING_CELL,
                                  CONFIGURING_CELL,
                                  stack_ALIGN_FILL),

  [CONFIGURING_ST_GRID_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_BACKDROP,
    .axis_size = CONFIGURING_GRID_H,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_GRID_LABEL] = STACK_LEAF_GROUP(CONFIGURING_ST_GRID_HBOX,
                                            0,
                                            CONFIGURING_CELL,
                                            stack_ALIGN_START,
                                            CONFIGURING_G_LABEL),

  [CONFIGURING_ST_GRID] = STACK_LEAF(CONFIGURING_ST_GRID_HBOX,
                                CONFIGURING_GRID_COLS * CONFIGURING_CELL,
                                CONFIGURING_GRID_H,
                                stack_ALIGN_FILL),

  [CONFIGURING_ST_RESULT_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_BACKDROP,
    .axis_size = CONFIGURING_RESULT,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_RESULT_SPACER] = STACK_LEAF_GROUP(CONFIGURING_ST_RESULT_HBOX,
                                               0,
                                               CONFIGURING_RESULT,
                                               stack_ALIGN_FILL,
                                               CONFIGURING_G_LABEL),

  [CONFIGURING_ST_RESULT] = STACK_LEAF(CONFIGURING_ST_RESULT_HBOX,
                                  CONFIGURING_GRID_COLS * CONFIGURING_CELL,
                                  CONFIGURING_RESULT,
                                  stack_ALIGN_FILL),

  [CONFIGURING_ST_BUTTON_HBOX] =
  {
    .kind      = stack_KIND_HBOX,
    .parent    = CONFIGURING_ST_BACKDROP,
    .axis_size = wuss_STD_SECONDARY_BUTTON_HEIGHT,
    .gap       = 4,
    .align     = stack_ALIGN_FILL,
  },

  [CONFIGURING_ST_BUTTON_SPACER] = STACK_LEAF_GROUP(CONFIGURING_ST_BUTTON_HBOX,
                                               0,
                                               wuss_STD_SECONDARY_BUTTON_HEIGHT,
                                               stack_ALIGN_FILL,
                                               CONFIGURING_G_LABEL),

  [CONFIGURING_ST_BUTTON] = STACK_LEAF(CONFIGURING_ST_BUTTON_HBOX,
                                  CONFIGURING_GRID_COLS * CONFIGURING_CELL,
                                  wuss_STD_SECONDARY_BUTTON_HEIGHT,
                                  stack_ALIGN_FILL),
};

/* ----------------------------------------------------------------------- */

/* Icons made inside the "Backdrop" frame's box that aren't swatches: two
 * option-row labels' worth of static furniture (frame captions are drawn by
 * the frame icon itself, not a separate label) plus the row labels and the
 * "Set backdrop" button. Swatches (colour rows, pattern grid, result) have
 * no retained icons -- they're redrawn from task state via wuss_icon_plot,
 * the same approach as swatches.c's grid, and hit-tested by arithmetic in
 * configuring_click. */
enum
{
  CONFIGURING_ICON_SYSTEM_FRAME,
  CONFIGURING_ICON_SWAP,
  CONFIGURING_ICON_REVERSE_SCROLL,
  CONFIGURING_ICON_FONT_FRAME,
  CONFIGURING_ICON_FONT_LABEL,
  CONFIGURING_ICON_FONT_NAME,
  CONFIGURING_ICON_FONT_CHOOSE,
  CONFIGURING_ICON_TIMING_FRAME,
  CONFIGURING_ICON_DBL_MS_LABEL,
  CONFIGURING_ICON_DBL_MS_FIELD,
  CONFIGURING_ICON_DBL_MS_UNIT,
  CONFIGURING_ICON_DBL_PX_LABEL,
  CONFIGURING_ICON_DBL_PX_FIELD,
  CONFIGURING_ICON_DBL_PX_UNIT,
  CONFIGURING_ICON_DRAG_PX_LABEL,
  CONFIGURING_ICON_DRAG_PX_FIELD,
  CONFIGURING_ICON_DRAG_PX_UNIT,
  CONFIGURING_ICON_APPLY_TIMING,
  CONFIGURING_ICON_BACKDROP_FRAME,
  CONFIGURING_ICON_FG_LABEL,
  CONFIGURING_ICON_BG_LABEL,
  CONFIGURING_ICON_PATTERNS_LABEL,
  CONFIGURING_ICON_SET_BACKDROP,
  CONFIGURING_NICONS
};

/* The system font faces this task has handed to wuss_set_font, and their
 * names: wuss borrows both, so they live here rather than in any one
 * window's task block. NULL until the first pick (the start-up faces are
 * main's to free). */
static struct
{
  bmfont_t *fonts[2];      /* [0] regular, [1] bold */
  char      names[2][128]; /* "Family Style" */
}
g_configuring_font;

/* The pattern shown in grid cell i, 0 <= i < CONFIGURING_NPATTERNS: the named
 * hatches first, then the full Bayer run (EMPTY, dither levels 1..63,
 * SOLID) in order. */
static screen_pattern_t configuring_grid_pattern(int i)
{
  if (i < CONFIGURING_NHATCH)
    return (screen_pattern_t) (screen_PATTERN_BAYER_LIMIT + i);
  return (screen_pattern_t) (screen_PATTERN_BAYER0 + (i - CONFIGURING_NHATCH));
}

/* Cell (col,row) for grid pattern index i: the named hatches fill row 0
 * onward, padded to a row boundary; the Bayer run starts its own fresh row
 * after that padding, wrapping at CONFIGURING_GRID_COLS. */
static void configuring_grid_cell(int i, int *col, int *row)
{
  if (i < CONFIGURING_NHATCH)
  {
    *col = i % CONFIGURING_GRID_COLS;
    *row = i / CONFIGURING_GRID_COLS;
    return;
  }

  i    -= CONFIGURING_NHATCH;
  *col  = i % CONFIGURING_GRID_COLS;
  *row  = CONFIGURING_HATCH_ROWS + i / CONFIGURING_GRID_COLS;
}

/* ----------------------------------------------------------------------- */

result_t configuring_create(wuss_t *wuss, configuring_task_t **out)
{
  result_t            rc;
  configuring_task_t *task;
  wuss_task_t        *delegate;
  wuss_task_desc_t    delegate_desc;
  box_t               boxes[CONFIGURING_ST__LIMIT];
  box_t               root;
  box_t               backdrop_frame;
  wuss_icon_spec_t    specs[CONFIGURING_NICONS];
  wuss_icon_t        *icons[CONFIGURING_NICONS];
  stack_item_t        items[CONFIGURING_ST__LIMIT];
  size2d_t            doc;
  size2d_t            min_sz;
  const char         *font_name;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss    = wuss;
  task->fg      = wuss_COLOUR_BLACK;
  task->bg      = wuss_COLOUR_WHITE;
  task->pattern = screen_PATTERN_SOLID;

  delegate_desc.handle    = configuring_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "configuring";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  task->delegate = delegate;

  /* each label's natural width; the size group widens the rest to match */
  memcpy(items, g_configuring_stack, sizeof(items));
  items[CONFIGURING_ST_FG_LABEL].axis_size =
    task_text_width(wuss, "Foreground");
  items[CONFIGURING_ST_BG_LABEL].axis_size =
    task_text_width(wuss, "Background");
  items[CONFIGURING_ST_GRID_LABEL].axis_size =
    task_text_width(wuss, "Patterns");
  items[CONFIGURING_ST_FONT_LABEL].axis_size =
    task_text_width(wuss, "System font");
  items[CONFIGURING_ST_DBL_MS_LABEL].axis_size =
    task_text_width(wuss, "Double-click");
  items[CONFIGURING_ST_DBL_PX_LABEL].axis_size =
    task_text_width(wuss, "Double-click");
  items[CONFIGURING_ST_DRAG_PX_LABEL].axis_size =
    task_text_width(wuss, "Drag threshold");
  items[CONFIGURING_ST_DBL_MS_UNIT].axis_size  = task_text_width(wuss, "ms");
  items[CONFIGURING_ST_DBL_PX_UNIT].axis_size  = task_text_width(wuss, "px");
  items[CONFIGURING_ST_DRAG_PX_UNIT].axis_size = task_text_width(wuss, "px");

  rc = stack_smallest(items, NELEMS(items), &min_sz);
  if (rc != result_OK)
    goto fail_delegate;

  doc = min_sz;

  rc = wuss_window_create_at(delegate,
                             &task_place,
                             doc,
                             "Configuring",
                             wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
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
  rc = stack_solve(items, NELEMS(items), &root, boxes);
  if (rc != result_OK)
    goto fail_delegate;

  backdrop_frame  = boxes[CONFIGURING_ST_BACKDROP];
  task->swatch_x  = boxes[CONFIGURING_ST_FG_ROW].x0;
  task->fg_y      = boxes[CONFIGURING_ST_FG_ROW].y0;
  task->bg_y      = boxes[CONFIGURING_ST_BG_ROW].y0;
  task->grid_y    = boxes[CONFIGURING_ST_GRID].y0;
  task->result_y  = boxes[CONFIGURING_ST_RESULT].y0;

  memset(specs, 0, sizeof(specs));

  wuss_icon_spec_frame(&specs[CONFIGURING_ICON_SYSTEM_FRAME],
                       boxes[CONFIGURING_ST_SYSTEM], "System");

  wuss_icon_spec_option(&specs[CONFIGURING_ICON_SWAP], boxes[CONFIGURING_ST_SWAP],
                        "Swap right/middle mouse buttons");

  wuss_icon_spec_option(&specs[CONFIGURING_ICON_REVERSE_SCROLL],
                        boxes[CONFIGURING_ST_REVERSE_SCROLL],
                        "Reverse mouse scroll direction");

  wuss_icon_spec_frame(&specs[CONFIGURING_ICON_FONT_FRAME],
                       boxes[CONFIGURING_ST_FONT], "Font");

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_FONT_LABEL],
                       boxes[CONFIGURING_ST_FONT_LABEL],
                       "System font", wuss_ICON_FLAGS_JUSTIFY_RIGHT);

  font_name = wuss_get_font_name_n(wuss, 0);
  wuss_icon_spec_display(&specs[CONFIGURING_ICON_FONT_NAME],
                         boxes[CONFIGURING_ST_FONT_NAME],
                         (font_name != NULL) ? font_name : "", 0);

  wuss_icon_spec_action(&specs[CONFIGURING_ICON_FONT_CHOOSE],
                        boxes[CONFIGURING_ST_FONT_BUTTON],
                        "Choose", 0);

  wuss_icon_spec_frame(&specs[CONFIGURING_ICON_TIMING_FRAME],
                       boxes[CONFIGURING_ST_TIMING], "Timing");

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_DBL_MS_LABEL],
                       boxes[CONFIGURING_ST_DBL_MS_LABEL],
                       "Double-click", wuss_ICON_FLAGS_JUSTIFY_RIGHT);

  snprintf(task->double_click_ms_text, sizeof(task->double_click_ms_text),
          "%d", CONFIGURING_DOUBLE_CLICK_MS_DEFAULT);
  wuss_icon_spec_writable(&specs[CONFIGURING_ICON_DBL_MS_FIELD],
                          boxes[CONFIGURING_ST_DBL_MS_FIELD],
                          task->double_click_ms_text,
                          sizeof(task->double_click_ms_text), 0);

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_DBL_MS_UNIT],
                       boxes[CONFIGURING_ST_DBL_MS_UNIT], "ms", 0);

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_DBL_PX_LABEL],
                       boxes[CONFIGURING_ST_DBL_PX_LABEL],
                       "Double-click", wuss_ICON_FLAGS_JUSTIFY_RIGHT);

  snprintf(task->double_click_px_text, sizeof(task->double_click_px_text),
          "%d", CONFIGURING_DOUBLE_CLICK_PX_DEFAULT);
  wuss_icon_spec_writable(&specs[CONFIGURING_ICON_DBL_PX_FIELD],
                          boxes[CONFIGURING_ST_DBL_PX_FIELD],
                          task->double_click_px_text,
                          sizeof(task->double_click_px_text), 0);

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_DBL_PX_UNIT],
                       boxes[CONFIGURING_ST_DBL_PX_UNIT], "px", 0);

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_DRAG_PX_LABEL],
                       boxes[CONFIGURING_ST_DRAG_PX_LABEL],
                       "Drag threshold", wuss_ICON_FLAGS_JUSTIFY_RIGHT);

  snprintf(task->drag_threshold_px_text,
          sizeof(task->drag_threshold_px_text),
          "%d", CONFIGURING_DRAG_THRESHOLD_PX_DEFAULT);
  wuss_icon_spec_writable(&specs[CONFIGURING_ICON_DRAG_PX_FIELD],
                          boxes[CONFIGURING_ST_DRAG_PX_FIELD],
                          task->drag_threshold_px_text,
                          sizeof(task->drag_threshold_px_text), 0);

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_DRAG_PX_UNIT],
                       boxes[CONFIGURING_ST_DRAG_PX_UNIT], "px", 0);

  wuss_icon_spec_action(&specs[CONFIGURING_ICON_APPLY_TIMING],
                        boxes[CONFIGURING_ST_TIMING_BUTTON],
                        "Apply", 0);

  wuss_icon_spec_frame(&specs[CONFIGURING_ICON_BACKDROP_FRAME], backdrop_frame,
                       "Backdrop");

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_FG_LABEL],
                       boxes[CONFIGURING_ST_FG_LABEL],
                       "Foreground", wuss_ICON_FLAGS_JUSTIFY_RIGHT);

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_BG_LABEL],
                       boxes[CONFIGURING_ST_BG_LABEL],
                       "Background", wuss_ICON_FLAGS_JUSTIFY_RIGHT);

  wuss_icon_spec_label(&specs[CONFIGURING_ICON_PATTERNS_LABEL],
                       boxes[CONFIGURING_ST_GRID_LABEL],
                       "Patterns", wuss_ICON_FLAGS_JUSTIFY_RIGHT);

  wuss_icon_spec_action(&specs[CONFIGURING_ICON_SET_BACKDROP],
                        boxes[CONFIGURING_ST_BUTTON],
                        "Set backdrop", 0);

  rc = wuss_icon_create_array(task->window, specs, CONFIGURING_NICONS, icons);
  if (rc != result_OK)
    goto fail_delegate;

  task->set_backdrop_icon       = icons[CONFIGURING_ICON_SET_BACKDROP];
  task->swap_icon               = icons[CONFIGURING_ICON_SWAP];
  task->reverse_scroll_icon     = icons[CONFIGURING_ICON_REVERSE_SCROLL];
  task->font_name_icon          = icons[CONFIGURING_ICON_FONT_NAME];
  task->choose_font_icon        = icons[CONFIGURING_ICON_FONT_CHOOSE];
  task->double_click_ms_icon    = icons[CONFIGURING_ICON_DBL_MS_FIELD];
  task->double_click_px_icon    = icons[CONFIGURING_ICON_DBL_PX_FIELD];
  task->drag_threshold_px_icon  = icons[CONFIGURING_ICON_DRAG_PX_FIELD];
  task->apply_timing_icon       = icons[CONFIGURING_ICON_APPLY_TIMING];

  wuss_icon_set_selected(task->window, task->swap_icon,
                        g_tasks.swap_mouse_buttons);
  wuss_icon_set_selected(task->window, task->reverse_scroll_icon,
                        g_tasks.reverse_scroll);

  /* fully built: from here a last-window close reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  if (out)
    *out = task;

  return result_OK;

fail_delegate:
  wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
  return rc;
}

static const taskapp_desc_t configuring_desc =
{
  "Configuring",
  "System settings",
  (taskapp_create_fn_t) configuring_create,
  NULL,
  false,
  NULL,
  true
};

result_t configuring_app_create(wuss_t *wuss)
{
  return taskapp_install(wuss, &configuring_desc);
}

static result_t configuring__spawn(wuss_t *wuss, void *param, void **out)
{
  NOT_USED(param);

  return configuring_create(wuss, (configuring_task_t **) out);
}

result_t configuring_open(wuss_t *wuss)
{
  void *task;

  return taskapp_spawn(wuss, &configuring_desc, configuring__spawn, NULL, &task);
}

void configuring_destroy(configuring_task_t *task)
{
  free(task);
}

/* Plot one CONFIGURING_CELL-square colour swatch, solid-filled in colour; a
 * selected swatch first gets an oversized black square underneath, giving
 * it a 2px black surround. */
static result_t configuring_plot_swatch(wuss_window_t *window,
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
    spec.bbox = (box_t) BOX_POS_SIZE(x, y, CONFIGURING_CELL, CONFIGURING_CELL);
    spec.fg = spec.bg = wuss_COLOUR_BLACK;
    rc = wuss_icon_plot(window, &spec, bounds, scroll);
    if (rc != result_OK)
      return rc;

    x += 2;
    y += 2;
  }

  spec.bbox = (box_t) BOX_POS_SIZE(x, y, CONFIGURING_CELL - (selected ? 4 : 0),
                                   CONFIGURING_CELL - (selected ? 4 : 0));
  spec.fg = spec.bg = colour;
  return wuss_icon_plot(window, &spec, bounds, scroll);
}

/* Redraw: the two swatch rows (fg/bg -- CONFIGURING_NCOLOURS palette entries each,
 * every cell a solid block of its own colour; the selected cell gets a 2px
 * black surround), the pattern grid, and the result square mixing
 * task->fg/task->pattern/task->bg. No icons are retained for any of this;
 * wuss_icon_plot resolves the palette live, so a palette change just
 * redraws. */
static result_t configuring_redraw(configuring_task_t *task,
                                   const wuss_event_t *event)
{
  result_t         rc;
  wuss_icon_spec_t spec;
  const box_t     *bounds;
  point_t          scroll;
  int              col, i;

  bounds = event->data.redraw.bounds;
  scroll = event->data.redraw.scroll;

  memset(&spec, 0, sizeof(spec));
  spec.type = wuss_ICON_TYPE_PATTERN;

  for (col = 0; col < CONFIGURING_NCOLOURS; col++)
  {
    rc = configuring_plot_swatch(task->window,
                            task->swatch_x + col * CONFIGURING_CELL,
                            task->fg_y,
                            (wuss_colour_t) col, col == task->fg,
                            bounds,
                            scroll);
    if (rc != result_OK)
      return rc;

    rc = configuring_plot_swatch(task->window,
                            task->swatch_x + col * CONFIGURING_CELL,
                            task->bg_y,
                            (wuss_colour_t) col, col == task->bg,
                            bounds,
                            scroll);
    if (rc != result_OK)
      return rc;
  }

  spec.fg = task->fg;
  spec.bg = task->bg;

  for (i = 0; i < CONFIGURING_NPATTERNS; i++)
  {
    int row, gcol;

    configuring_grid_cell(i, &gcol, &row);
    spec.bbox = (box_t) BOX_POS_SIZE(task->swatch_x + gcol * CONFIGURING_CELL,
                                     task->grid_y   + row  * CONFIGURING_CELL,
                                     CONFIGURING_CELL, CONFIGURING_CELL);
    spec.u.pattern.tile = configuring_grid_pattern(i);

    rc = wuss_icon_plot(task->window, &spec, bounds, scroll);
    if (rc != result_OK)
      return rc;
  }

  spec.bbox = (box_t) BOX_POS_SIZE(task->swatch_x, task->result_y,
                                   CONFIGURING_RESULT, CONFIGURING_RESULT);
  spec.u.pattern.tile = task->pattern;
  rc = wuss_icon_plot(task->window, &spec, bounds, scroll);
  if (rc != result_OK)
    return rc;

  return result_OK;
}

/* the shared fontmenu singleton, retargeted at task->wuss's bmfonts dir */
static const wuss_menu_t *configuring_fontmenu(configuring_task_t *task)
{
  const char *resources;

  resources = wuss_get_resources(task->wuss);
  return wuss_fontmenu_menu(pathf("%s/resources/bmfonts", resources), "Font",
                            task->wuss);
}

/* Open the font menu at the pointer with the system font's row ticked. */
static result_t configuring_open_fontmenu(configuring_task_t *task)
{
  const wuss_menu_t *menu;
  const char        *current;
  int                ticked;
  int                i;

  menu = configuring_fontmenu(task);
  if (menu == NULL)
    return result_OOM;

  current = wuss_get_font_name_n(task->wuss, 0);
  ticked  = -1;
  if (current != NULL)
    for (i = 0; i < menu->nitems; i++)
      if (strcmp(menu->items[i].text, current) == 0)
        ticked = i;

  wuss_fontmenu_set_ticked(ticked);
  return wuss_menu_open_at_pointer(task->delegate, menu, NULL);
}

/* Make the face labelled name ("Family Style") the system font, and the next
 * heavier face of its family the bold one, as main does at start-up. */
static result_t configuring_set_font(configuring_task_t *task,
                                     const char         *name)
{
  result_t            rc;
  const char         *style;
  char                family_name[128];
  const char         *resources;
  bmfontfamily_t     *family;
  const bmfontface_t *faces[2];
  int                 i;
  bmfontcache_t      *cache;
  bmfont_t           *fonts[2];
  wuss_font_desc_t    desc;

  style = strrchr(name, ' ');
  if (style == NULL || (size_t) (style - name) >= sizeof(family_name))
    return result_BAD_ARG;

  memcpy(family_name, name, (size_t) (style - name));
  family_name[style - name] = '\0';
  style++;

  resources = wuss_get_resources(task->wuss);
  rc = bmfontfamily_scan(pathf("%s/resources/bmfonts/%s", resources,
                               family_name),
                         &family);
  if (rc != result_OK)
    return rc;

  faces[0] = NULL;
  for (i = 0; i < bmfontfamily_count(family); i++)
    if (strcmp(bmfontfamily_face(family, i)->style, style) == 0)
      faces[0] = bmfontfamily_face(family, i);

  if (faces[0] == NULL)
  {
    bmfontfamily_destroy(family);
    return result_NOT_FOUND;
  }

  faces[1] = bmfontfamily_heavier(family, faces[0]);

  cache = wuss_get_font_cache(task->wuss);
  rc = bmfontcache_acquire(cache, faces[0]->path, &fonts[0]);
  if (rc != result_OK)
  {
    bmfontfamily_destroy(family);
    return rc;
  }

  rc = bmfontcache_acquire(cache, faces[1]->path, &fonts[1]);
  if (rc != result_OK)
  {
    bmfontcache_release(cache, fonts[0]);
    bmfontfamily_destroy(family);
    return rc;
  }

  for (i = 0; i < 2; i++)
  {
    result_t frc;

    snprintf(g_configuring_font.names[i], sizeof(g_configuring_font.names[i]),
             "%s %s", family_name, faces[i]->style);

    desc.font       = fonts[i];
    desc.font_class = wuss_FONT_CLASS_NONE;
    desc.name       = g_configuring_font.names[i];

    /* the slot is replaced even when a task's relayout handler fails */
    frc = wuss_set_font(task->wuss, i, &desc);
    if (frc != result_OK && rc == result_OK)
      rc = frc;

    if (g_configuring_font.fonts[i] != NULL)
      bmfontcache_release(cache, g_configuring_font.fonts[i]);
    g_configuring_font.fonts[i] = fonts[i];
  }

  bmfontfamily_destroy(family);

  if (task->window != NULL)
  {
    result_t trc;

    trc = wuss_icon_set_text(task->window, task->font_name_icon,
                             g_configuring_font.names[0]);
    if (trc != result_OK && rc == result_OK)
      rc = trc;
  }

  return rc;
}

static result_t configuring_icon(const wuss_event_t *event, void *task_data)
{
  configuring_task_t *cc;
  wuss_icon_t        *icon;

  cc   = task_data;
  icon = event->data.icon.icon;

  if (event->data.icon.action != wuss_MOUSE_UP)
    return result_OK;

  if (icon == cc->swap_icon)
  {
    g_tasks.swap_mouse_buttons = !!wuss_icon_get_selected(icon);
  }
  else if (icon == cc->reverse_scroll_icon)
  {
    g_tasks.reverse_scroll = !!wuss_icon_get_selected(icon);
  }
  else if (icon == cc->choose_font_icon)
  {
    return configuring_open_fontmenu(cc);
  }
  else if (icon == cc->set_backdrop_icon)
  {
    wuss_backdrop_t backdrop;

    backdrop = wuss_BACKDROP_PATTERN(cc->fg, cc->pattern, cc->bg);
    return wuss_set_backdrop(cc->wuss, &backdrop);
  }
  else if (icon == cc->apply_timing_icon)
  {
    wuss_config_t config;

    /* Round-trip the rest of the config unchanged: wuss_set_config replaces
     * every field, there is no partial update. */
    wuss_get_config(cc->wuss, &config);
    config.mouse.double_click_ms   = atoi(wuss_icon_get_text(cc->double_click_ms_icon));
    config.mouse.double_click_px   = atoi(wuss_icon_get_text(cc->double_click_px_icon));
    config.mouse.drag_threshold_px = atoi(wuss_icon_get_text(cc->drag_threshold_px_icon));

    if (config.mouse.double_click_ms < 0 || config.mouse.double_click_px < 0 ||
        config.mouse.drag_threshold_px < 0)
      return result_BAD_ARG;

    return wuss_set_config(cc->wuss, &config);
  }

  return result_OK;
}

/* A click on a swatch (fg row, bg row or pattern grid) updates the matching
 * task field and redraws every swatch (the selection highlight on the fg/bg
 * rows and the result both depend on all three fields together). Clicks
 * outside every swatch fall through untouched. */
static result_t configuring_click(configuring_task_t *task,
                                  const wuss_event_t *event)
{
  point_t pt;
  int     col, row;

  pt  = event->data.mouse.point;
  col = (pt.x - task->swatch_x) / CONFIGURING_CELL;

  if (pt.x < task->swatch_x || col >= CONFIGURING_NCOLOURS)
    return result_OK;

  if (pt.y >= task->fg_y && pt.y < task->fg_y + CONFIGURING_CELL)
  {
    task->fg = (wuss_colour_t) col;
  }
  else if (pt.y >= task->bg_y && pt.y < task->bg_y + CONFIGURING_CELL)
  {
    task->bg = (wuss_colour_t) col;
  }
  else if (pt.y >= task->grid_y && pt.y < task->grid_y + CONFIGURING_GRID_H)
  {
    int i;

    row = (pt.y - task->grid_y) / CONFIGURING_CELL;
    for (i = 0; i < CONFIGURING_NPATTERNS; i++)
    {
      int gcol, grow;

      configuring_grid_cell(i, &gcol, &grow);
      if (gcol == col && grow == row)
      {
        task->pattern = configuring_grid_pattern(i);
        break;
      }
    }
  }
  else
  {
    return result_OK;
  }

  wuss_window_invalidate_extent(task->window);
  return result_OK;
}

result_t configuring_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  configuring_task_t *cc;

  cc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return configuring_redraw(cc, event);

  case wuss_EVENT_ICON:
    return configuring_icon(event, task_data);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action == wuss_MOUSE_DOWN &&
        (event->data.mouse.button & wuss_BUTTON_SELECT))
      return configuring_click(cc, event);

    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    {
      const char *name;

      name = wuss_fontmenu_selected(event);
      if (name == NULL)
        return result_OK;

      return configuring_set_font(cc, name);
    }

  case wuss_EVENT_CLOSE:
    if (window == cc->window)
      cc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    configuring_destroy(cc);
    return result_OK;

  case wuss_EVENT_KEY:
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
