/* wuss/tasks/fern/fern.c -- parameterised Barnsley fern */

#ifdef WUSS_APP

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/colour.h"
#include "geom/box.h"
#include "geom/size.h"
#include "geom/stack.h"
#include "utils/rng.h"

#include "fern.h"
#include "../taskapp.h"
#include "../common.h"
#include "../snapshot.h"

/* Barnsley's fern: start at (0,0) and repeatedly apply one of four affine
 * maps chosen at random (1%, 85%, 7%, 7%), plotting each result. The fern
 * lies in x -2.182..2.656, y 0..9.998. config.curl scales the 0.04 shear
 * terms of the 85% map. */
#define FERN_SAVE_NAME "fern.png"

#define FERN_X_MIN (-2.2)
#define FERN_Y_MAX 10.0
#define FERN_SEED_STEP 0x9E3779B9UL /* golden-ratio step between ferns */

enum
{
  FERN_MENU_COLOURS = 0,
  FERN_MENU_SIZE,
  FERN_MENU_ANIMATE,
  FERN_MENU_RANDOMISE,
  FERN_MENU_RESET,
  FERN_MENU_SAVE
};

enum
{
  FERN_ICON_LABEL1 = 0, FERN_ICON_SLIDER1, FERN_ICON_VALUE1,
  FERN_ICON_LABEL2,     FERN_ICON_SLIDER2, FERN_ICON_VALUE2,
  FERN_ICON_LABEL3,     FERN_ICON_SLIDER3, FERN_ICON_VALUE3,
  FERN_ICON_DEFAULT,
  FERN_ICON_CANCEL,
  FERN_ICON_APPLY,

  FERN_NICONS
};

typedef struct fern_rowdesc
{
  const char *label;
  int         min, max, step;
  size_t      config_offset;
}
fern_rowdesc_t;

static const fern_rowdesc_t g_fern_rows[FERN_NROWS] =
{
  [FERN_ROW_SIZE]   = { "Size",   FERN_SIZE_MIN, FERN_SIZE_MAX, FERN_SIZE_STEP, offsetof(fern_config_t, size) },
  [FERN_ROW_POINTS] = { "Points", 100, 100000, 0, offsetof(fern_config_t, points) },
  [FERN_ROW_CURL]   = { "Curl",   -300, 300, 0, offsetof(fern_config_t, curl) },
};

static int *fern_field(fern_task_t *task, int row)
{
  return (int *) ((char *) &task->config + g_fern_rows[row].config_offset);
}

static result_t fern_conf_dialogue_create(fern_task_t *task);
static result_t fern_conf_fillout(void *opaque);

/* wuss_saveas_save_fn_t: opaque is the fern_task_t */
static result_t fern_saveas_save(const char *path, void *opaque)
{
  fern_task_t *task;

  task = opaque;

  return snapshot_save_png(task->window, fern_handle, task, path);
}

/* the starting fern, as fern_create and Reset restore it */
static void fern_defaults(fern_task_t *task)
{
  static const fern_config_t default_config = FERN_CONFIG_DEFAULT;

  task->config  = default_config;
  task->bg      = colour_rgb(0x00, 0x00, 0x00);
  task->fg      = colour_rgb(0x40, 0xE0, 0x40);
  task->seed    = 1;
  task->animate = 0;
  task->zoom    = 1.0;
}

result_t fern_create(wuss_t *wuss, fern_task_t **out)
{
  result_t         rc;
  fern_task_t     *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss   = wuss;
  rng_seed(&task->rng, (uint32_t) rand());
  fern_defaults(task);

  delegate_desc.handle    = fern_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "fern";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task);
    return rc;
  }
  task->delegate = delegate;
  wuss_task_set_autoclose(delegate, 1);

  rc = snapshot_saveas_create(&task->saveas, wuss, FERN_SAVE_NAME,
                              fern_saveas_save, task);
  if (rc != result_OK)
    goto fail_delegate;

  wuss_colourlist_init_pair(&task->colours, wuss, &task->fg, &task->bg);

  WUSS_MENU_ITEM_MENU(task->menu_items, FERN_MENU_COLOURS, "Colours",
                      wuss_MENU_ITEM_NONE, &task->colours.menu);

  WUSS_MENU_ITEM(task->menu_items, FERN_MENU_SIZE, "Configuration",
                 wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, FERN_MENU_ANIMATE, "Animate",
                          wuss_MENU_ITEM_NONE, "SPACE");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, FERN_MENU_RANDOMISE, "Randomise",
                          wuss_MENU_ITEM_NONE, "^R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, FERN_MENU_RESET, "Reset",
                          wuss_MENU_ITEM_NONE, "R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, FERN_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_NONE, "F3");
  task->menu_items[FERN_MENU_SAVE].window = wuss_saveas_window(task->saveas);

  WUSS_MENU_TITLE(task->menu, "Fern", task->menu_items,
                  NELEMS(task->menu_items));

  rc = task_window_create(delegate,
                          SIZE2D(task->config.size, task->config.size),
                          "Fern", &task->window);
  if (rc != result_OK)
    goto fail_delegate;

  rc = fern_conf_dialogue_create(task);
  if (rc != result_OK)
    goto fail_delegate;

  task->menu_items[FERN_MENU_SIZE].window =
    wuss_dialogue_window(task->conf.dialogue);

  if (out)
    *out = task;

  return result_OK;

fail_delegate:
  wuss_saveas_destroy(task->saveas);
  wuss_task_destroy(delegate); /* its QUIT frees the task block */
  return rc;
}

result_t fern_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Fern",
    "Barnsley fern",
    (taskapp_create_fn_t) fern_create
  };

  return taskapp_install(wuss, &desc);
}

void fern_destroy(fern_task_t *task)
{
  wuss_dialogue_destroy(task->conf.dialogue);
  wuss_saveas_destroy(task->saveas);
  free(task);
}

static result_t fern_redraw(const wuss_event_t *event, fern_task_t *task)
{
  rng_t          rng;
  screen_t      *scr;
  const box_t   *content, *bounds;
  int            size, ox, oy, i, px, py, r;
  double         x, y, nx, ny, shear, scale;
  pixelfmt_any_t pix;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content),
                   task->bg);

  size  = task->config.size;
  ox    = bounds->x0 - event->data.redraw.scroll.x;
  oy    = bounds->y0 - event->data.redraw.scroll.y;
  shear = 0.04 * task->config.curl / 100.0;
  scale = size / FERN_Y_MAX; /* fern is taller than wide: fit its height */
  pix   = screen_colour_to_pixel(scr, task->fg);

  rng_seed(&rng, (uint32_t) task->seed);
  x = y = 0.0;
  for (i = 0; i < task->config.points; i++)
  {
    r = (int) ((rng_lcg32(&rng) >> 16) % 100);
    if (r < 1)
    {
      nx = 0.0;
      ny = 0.16 * y;
    }
    else if (r < 86)
    {
      nx = 0.85 * x + shear * y;
      ny = -shear * x + 0.85 * y + 1.6;
    }
    else if (r < 93)
    {
      nx = 0.2 * x - 0.26 * y;
      ny = 0.23 * x + 0.22 * y + 1.6;
    }
    else
    {
      nx = -0.15 * x + 0.28 * y;
      ny = 0.26 * x + 0.24 * y + 0.44;
    }
    x = nx;
    y = ny;

    px = (int) (((x - FERN_X_MIN) * scale - size / 2.0) * task->zoom +
                size / 2.0);
    py = (int) ((size - 1 - y * scale - size / 2.0) * task->zoom +
                size / 2.0);
    if (px >= 0 && px < size && py >= 0 && py < size)
      screen_set_pixel_value(scr, ox + px, oy + py, pix);
  }

  return result_OK;
}

static result_t fern_scroll(fern_task_t   *task,
                            wuss_window_t *window,
                            int            delta)
{
  if (window != task->window)
    return result_OK;

  task->zoom += (delta > 0 ? 1 : -1) * FERN_ZOOM_STEP;
  task->zoom  = CLAMP(task->zoom, FERN_ZOOM_MIN, FERN_ZOOM_MAX);

  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

static result_t fern_mouse(fern_task_t        *task,
                           wuss_mouse_action_t action,
                           wuss_button_t       button,
                           wuss_window_t      *window)
{
  if (action != wuss_MOUSE_DOWN || window != task->window)
    return result_OK;

  if (button & wuss_BUTTON_MENU)
  {
    wuss_menu_tick_item(&task->menu, FERN_MENU_ANIMATE, task->animate);

    return wuss_menu_open_at_pointer(task->delegate, &task->menu,
                                     &task->menu_handle);
  }

  /* the seed step is invertible, so Adjust walks back through the ferns */
  if (button & wuss_BUTTON_SELECT)
    task->seed += FERN_SEED_STEP;
  else if (button & wuss_BUTTON_ADJUST)
    task->seed -= FERN_SEED_STEP;
  else
    return result_OK;

  wuss_window_invalidate_visible(window);

  return result_OK;
}

/* Left/Right step back/forward through the ferns; anything else is passed
 * back unclaimed. */
static result_t fern_key(fern_task_t *task, wuss_window_t *window, int code)
{
  switch (code)
  {
  case wuss_KEY_LEFT:  task->seed -= FERN_SEED_STEP; break;
  case wuss_KEY_RIGHT: task->seed += FERN_SEED_STEP; break;

  default:
    return result_WUSS_KEY_UNCLAIMED;
  }

  wuss_window_invalidate_visible(window);

  return result_OK;
}

/* Menu > Animate: a fresh fern every null event. The Configuration window
 * shares this delegate, so the task can outlive the fern window. */
static result_t fern_idle(fern_task_t *task)
{
  if (!task->animate || task->window == NULL)
    return result_OK;

  task->seed += FERN_SEED_STEP;
  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

/* Every submenu leaf fires wuss_EVENT_PRE_SUBMENU_OPEN; only the colour
 * pair's retargets the shared colourmenu. */
static result_t fern_pre_submenu_open(fern_task_t        *task,
                                      const wuss_event_t *event)
{
  result_t rc;
  int      index;

  if (wuss_colourlist_pre_open(&task->colours, task->wuss, event, &rc))
    return rc;

  index = event->data.pre_submenu_open.index;

  return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle, index,
                                    task->menu_items[index].submenu);
}

/* ----- configuration dialogue ----- */

enum
{
  ST_ROOT,

  ST_ROW1, ST_LABL1, ST_SLDR1, ST_VAL1,
  ST_ROW2, ST_LABL2, ST_SLDR2, ST_VAL2,
  ST_ROW3, ST_LABL3, ST_SLDR3, ST_VAL3,

  ST_BTNS,
  ST_BSPC,
  ST_BBOX,
  ST_DFLT,
  ST_CNCL,
  ST_APLY,

  FERN_STACK__LIMIT
};

#define ST_G_LABEL      1 /* size groups, see stack_item_t::group */
#define ST_G_VALUE      2
#define ST_SLIDER_MIN_W (64)

static const stack_item_t g_fern_conf_stack[FERN_STACK__LIMIT] =
{
  [ST_ROOT]  = { .kind = stack_KIND_VBOX, .parent = -1,
                 .gap = wuss_STD_GAP, .pad = wuss_STD_INSETS },

  [ST_ROW1]  = STACK_HBOX(ST_ROOT, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [ST_LABL1] = STACK_LEAF_GROUP(ST_ROW1, 0, 16, stack_ALIGN_CENTRE, ST_G_LABEL),
  [ST_SLDR1] = STACK_LEAF_EX(ST_ROW1, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, ST_SLIDER_MIN_W, 0),
  [ST_VAL1]  = STACK_LEAF_GROUP(ST_ROW1, 0, 16, stack_ALIGN_CENTRE, ST_G_VALUE),

  [ST_ROW2]  = STACK_HBOX(ST_ROOT, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [ST_LABL2] = STACK_LEAF_GROUP(ST_ROW2, 0, 16, stack_ALIGN_CENTRE, ST_G_LABEL),
  [ST_SLDR2] = STACK_LEAF_EX(ST_ROW2, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, ST_SLIDER_MIN_W, 0),
  [ST_VAL2]  = STACK_LEAF_GROUP(ST_ROW2, 0, 16, stack_ALIGN_CENTRE, ST_G_VALUE),

  [ST_ROW3]  = STACK_HBOX(ST_ROOT, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [ST_LABL3] = STACK_LEAF_GROUP(ST_ROW3, 0, 16, stack_ALIGN_CENTRE, ST_G_LABEL),
  [ST_SLDR3] = STACK_LEAF_EX(ST_ROW3, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, ST_SLIDER_MIN_W, 0),
  [ST_VAL3]  = STACK_LEAF_GROUP(ST_ROW3, 0, 16, stack_ALIGN_CENTRE, ST_G_VALUE),

  [ST_BTNS]  = STACK_HBOX(ST_ROOT, wuss_STD_SECONDARY_BUTTON_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [ST_BSPC]  = STACK_SPACER_GROUP(ST_BTNS, 0, ST_G_LABEL),
  [ST_BBOX]  = { .kind = stack_KIND_HBOX, .parent = ST_BTNS,
                 .gap = wuss_STD_GAP, .flex = 1 },
  [ST_DFLT]  = STACK_LEAF_EX(ST_BBOX, 0, wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE, 1, 0, 0),
  [ST_CNCL]  = STACK_LEAF_EX(ST_BBOX, 0, wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE, 1, 0, 0),
  [ST_APLY]  = STACK_LEAF_EX(ST_BBOX, 0, wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE, 1, 0, 0),
};

static result_t fern_conf_cancel(void *opaque, wuss_button_t button);
static result_t fern_conf_apply_action(void *opaque, wuss_button_t button);
static result_t fern_conf_default_action(void *opaque, wuss_button_t button);

static result_t fern_conf_dialogue_create(fern_task_t *task)
{
  static const int label_box[FERN_NROWS]   = { ST_LABL1, ST_LABL2, ST_LABL3 };
  static const int slider_box[FERN_NROWS]  = { ST_SLDR1, ST_SLDR2, ST_SLDR3 };
  static const int value_box[FERN_NROWS]   = { ST_VAL1, ST_VAL2, ST_VAL3 };
  static const int label_icon[FERN_NROWS]  = { FERN_ICON_LABEL1, FERN_ICON_LABEL2, FERN_ICON_LABEL3 };
  static const int slider_icon[FERN_NROWS] = { FERN_ICON_SLIDER1, FERN_ICON_SLIDER2, FERN_ICON_SLIDER3 };
  static const int value_icon[FERN_NROWS]  = { FERN_ICON_VALUE1, FERN_ICON_VALUE2, FERN_ICON_VALUE3 };

  result_t               rc;
  wuss_icon_spec_t       specs[FERN_NICONS];
  wuss_icon_t           *made[FERN_NICONS];
  stack_item_t           items[FERN_STACK__LIMIT];
  box_t                  boxes[FERN_STACK__LIMIT];
  box_t                  root;
  char                   value_bufs[FERN_NROWS][WUSS_SLIDER_ROW_BUF];
  int                    value, row;
  size2d_t               min_sz;
  wuss_dialogue_action_t actions[3];

  memcpy(items, g_fern_conf_stack, sizeof(items));
  for (row = 0; row < FERN_NROWS; row++)
  {
    const fern_rowdesc_t *desc = &g_fern_rows[row];
    char                  widest[WUSS_SLIDER_ROW_BUF];

    items[label_box[row]].axis_size = task_text_width(task->wuss, desc->label);
    snprintf(widest, sizeof(widest), "%d", desc->max);
    items[value_box[row]].axis_size = task_text_width(task->wuss, widest);
    snprintf(widest, sizeof(widest), "%d", desc->min);
    items[value_box[row]].axis_size = MAX(items[value_box[row]].axis_size,
                                          task_text_width(task->wuss, widest));
  }

  items[ST_DFLT].min = task_button_width(task->wuss, "Default",
                                         wuss_STD_SECONDARY_BUTTON_BORDER);
  items[ST_CNCL].min = items[ST_DFLT].min;
  items[ST_APLY].min = items[ST_DFLT].min;

  rc = stack_smallest(items, NELEMS(items), &min_sz);
  if (rc != result_OK)
    return rc;

  rc = wuss_dialogue_create(&task->conf.dialogue, task->delegate, min_sz,
                            "Configuration", fern_conf_fillout, task);
  if (rc != result_OK)
    return rc;

  wuss_dialogue_set_focusable(task->conf.dialogue, 0); /* no writables */

  root = (box_t) BOX_POS_SIZE(0, 0, min_sz.w, min_sz.h);
  rc = stack_solve(items, NELEMS(items), &root, boxes);
  if (rc != result_OK)
    goto exit;

  for (row = 0; row < FERN_NROWS; row++)
  {
    const fern_rowdesc_t *desc = &g_fern_rows[row];

    value = *fern_field(task, row);

    wuss_icon_spec_label(&specs[label_icon[row]], boxes[label_box[row]],
                         desc->label, wuss_ICON_FLAGS_JUSTIFY_RIGHT);
    wuss_icon_spec_slider_row(&specs[slider_icon[row]], &specs[value_icon[row]],
                              boxes[slider_box[row]], boxes[value_box[row]],
                              wuss_SLIDER_HORIZONTAL,
                              desc->min, desc->max, value, NULL, desc->step,
                              value_bufs[row], sizeof(value_bufs[row]));
  }

  wuss_icon_spec_action(&specs[FERN_ICON_DEFAULT], boxes[ST_DFLT], "Default", 0);
  wuss_icon_spec_action(&specs[FERN_ICON_CANCEL], boxes[ST_CNCL], "Cancel", 0);
  wuss_icon_spec_action(&specs[FERN_ICON_APPLY], boxes[ST_APLY], "Apply", 0);

  rc = wuss_icon_create_array(wuss_dialogue_window(task->conf.dialogue),
                              specs, FERN_NICONS, made);
  if (rc != result_OK)
    goto exit;

  for (row = 0; row < FERN_NROWS; row++)
    wuss_slider_row_bind(&task->conf.rows[row], made[slider_icon[row]],
                         made[value_icon[row]], NULL,
                         g_fern_rows[row].min, g_fern_rows[row].max,
                         g_fern_rows[row].step);
  task->conf.deflt  = made[FERN_ICON_DEFAULT];
  task->conf.cancel = made[FERN_ICON_CANCEL];
  task->conf.apply  = made[FERN_ICON_APPLY];

  actions[0].icon = task->conf.deflt;
  actions[0].fn   = fern_conf_default_action;
  actions[1].icon = task->conf.cancel;
  actions[1].fn   = fern_conf_cancel;
  actions[2].icon = task->conf.apply;
  actions[2].fn   = fern_conf_apply_action;

  rc = wuss_dialogue_set_actions(task->conf.dialogue, actions,
                                 NELEMS(actions));
  if (rc != result_OK)
    goto exit;

  return result_OK;

exit:
  wuss_dialogue_destroy(task->conf.dialogue);
  task->conf.dialogue = NULL;
  return rc;
}

/* resync the sliders to task->config; called on every reveal and by Cancel */
static result_t fern_conf_fillout(void *opaque)
{
  result_t     rc;
  fern_task_t *task;
  int          row;

  task = opaque;

  rc = result_OK;
  for (row = 0; row < FERN_NROWS; row++)
    rc = wuss_slider_row_set(wuss_dialogue_window(task->conf.dialogue),
                             &task->conf.rows[row], *fern_field(task, row));

  return rc;
}

/* resize the window if config.size has moved off old_size, then redraw */
static result_t fern_resize(fern_task_t *task, int old_size)
{
  result_t rc;
  int      size;

  if (task->window == NULL)
    return result_OK; /* fern window closed while the dialogue lingers */

  size = task->config.size;
  if (size != old_size)
  {
    rc = wuss_window_resize(task->window, SIZE2D(size, size));
    if (rc != result_OK)
      return rc;
    rc = wuss_window_set_doc(task->window, SIZE2D(size, size));
    if (rc != result_OK)
      return rc;
  }

  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

/* sliders to task->config, then resize and redraw the window */
static result_t fern_conf_apply(fern_task_t *task)
{
  int row, old_size;

  old_size = task->config.size;
  for (row = 0; row < FERN_NROWS; row++)
    *fern_field(task, row) = wuss_icon_get_value(task->conf.rows[row].slider);

  return fern_resize(task, old_size);
}

/* Select dismisses the menu chain; Adjust resets the sliders instead */
static result_t fern_conf_cancel(void *opaque, wuss_button_t button)
{
  fern_task_t *task;

  task = opaque;

  if (button & wuss_BUTTON_SELECT)
  {
    wuss_menu_close(task->menu_handle);
    task->menu_handle = NULL;
  }
  else if (button & wuss_BUTTON_ADJUST)
  {
    return fern_conf_fillout(task);
  }

  return result_OK;
}

/* Select applies and dismisses; Adjust applies and leaves the dialogue open */
static result_t fern_conf_apply_action(void *opaque, wuss_button_t button)
{
  fern_task_t *task;

  task = opaque;

  if (button & wuss_BUTTON_SELECT)
  {
    wuss_menu_close(task->menu_handle);
    task->menu_handle = NULL;
  }
  else if (!(button & wuss_BUTTON_ADJUST))
  {
    return result_OK;
  }

  return fern_conf_apply(task);
}

static result_t fern_conf_default_action(void *opaque, wuss_button_t button)
{
  static const fern_config_t default_config = FERN_CONFIG_DEFAULT;

  result_t     rc;
  fern_task_t *task;

  task = opaque;

  task->config = default_config;
  rc = fern_conf_fillout(task);
  if (rc != result_OK)
    return rc;

  return fern_conf_apply_action(task, button);
}

static result_t fern_conf_dialogue_icon(fern_task_t        *task,
                                        const wuss_event_t *event)
{
  result_t rc;
  int      row;

  for (row = 0; row < FERN_NROWS; row++)
    if (wuss_slider_row_event(wuss_dialogue_window(task->conf.dialogue),
                              &task->conf.rows[row], event, NULL))
      return result_OK;

  if (wuss_dialogue_handle_icon(task->conf.dialogue, event, &rc))
    return rc;

  return result_OK;
}

static result_t fern_menu_select(fern_task_t        *task,
                                 const wuss_event_t *event)
{
  if (wuss_menu_select_is(event, &task->menu, FERN_MENU_ANIMATE))
  {
    task->animate = !task->animate;
    wuss_menu_tick_item_live(task->menu_handle, &task->menu,
                             FERN_MENU_ANIMATE, task->animate);
    return result_OK;
  }

  if (wuss_menu_select_is(event, &task->menu, FERN_MENU_SAVE))
    return wuss_saveas_open(task->saveas);

  if (wuss_menu_select_is(event, &task->menu, FERN_MENU_RANDOMISE))
  {
    task->seed        = rng_xorshift32(&task->rng);
    task->config.curl = rng_range(&task->rng, 601) - 300;
    task->bg          = colour_random(&task->rng);
    task->fg          = colour_random_contrast(&task->rng, task->bg);
    wuss_window_invalidate_visible(task->window);
    return result_OK;
  }

  if (wuss_menu_select_is(event, &task->menu, FERN_MENU_RESET))
  {
    int old_size;

    old_size = task->config.size;
    fern_defaults(task);
    wuss_menu_tick_item_live(task->menu_handle, &task->menu,
                             FERN_MENU_ANIMATE, task->animate);
    return fern_resize(task, old_size);
  }

  if (wuss_colourlist_selected(&task->colours, event))
    wuss_window_invalidate_visible(task->window);

  return result_OK;
}

result_t fern_handle(wuss_window_t      *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  fern_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return fern_redraw(event, task);

  case wuss_EVENT_MOUSE:
    return fern_mouse(task, event->data.mouse.action,
                      event->data.mouse.button, window);

  case wuss_EVENT_SCROLL:
    return fern_scroll(task, window, event->data.scroll.delta);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != task->window)
      return result_WUSS_KEY_UNCLAIMED;

    if (!task_key_is_plain(task->delegate, &task->menu, event, &rc))
      return rc;

    return fern_key(task, window, event->data.key.code);
  }

  case wuss_EVENT_ICON:
    if (window == wuss_dialogue_window(task->conf.dialogue))
      return fern_conf_dialogue_icon(task, event);
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == wuss_dialogue_window(task->conf.dialogue))
    {
      rc = wuss_dialogue_handle_pre_show(task->conf.dialogue);
      if (rc != result_OK)
        return rc;
    }

    if (event->data.pre_show.handle == NULL)
      return result_OK;

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return fern_pre_submenu_open(task, event);

  case wuss_EVENT_MENU_SELECT:
    return fern_menu_select(task, event);

  case wuss_EVENT_IDLE:
    return fern_idle(task);

  case wuss_EVENT_CLOSE:
    wuss_dialogue_handle_close(task->conf.dialogue, window);
    if (window == task->window)
      task->window = NULL;
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    fern_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
