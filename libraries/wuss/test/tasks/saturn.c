/* wuss/test/tasks/saturn.c -- Elite loading-screen planet, recreated */

#ifdef WUSS_APP

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "geom/size.h"
#include "geom/stack.h"
#include "utils/rng.h"
#include "wuss/component/dialogue.h"
#include "wuss/menu.h"

#include "saturn.h"

/* A recreation of the ringed planet from the loading screen of Elite (Ian
 * Bell and David Braben, Acornsoft, 1984), the stippled Saturn-like world
 * the BBC Micro drew while the game loaded from tape.
 *
 * The source is one line of BBC BASIC (MODE 4, VDU5) that seeds the RNG from
 * TIME then runs three FOR loops, each rejection-sampling random points and
 * PLOT 69 (plot single point) at the survivors. In MODE 4 the plot area is
 * 1280x1024 OS units, so the original scales its -128..127 sample space by 4
 * and plots each point at y = (255 - (v + 128)). RISC OS screen coords run
 * bottom-up and wuss's run top-down, so that same expression - kept verbatim
 * here, only without the *4 - lands the sketch the same way up as it drew on
 * a BBC/RISC OS screen. The shear in loop 2 is asymmetric in y, so this is
 * the term that actually decides which way the ring's shadow tilts.
 *
 *   loop 1: X%,Y% in -127..127; keep if (X*X+Y*Y)/256 > 17  -> the stars
 *   loop 2: sheared X% = R6+R5/4, Y% = R6; keep if 32<=E<80 and
 *           (R5<0 or (X*X+Y*Y)/256 > 16)                    -> the ring
 *   loop 3: R1,R2 random; keep if R1*R1+R2*R2 < 16384;
 *           x = sqrt(16384-P)/2                             -> planet body
 */

/* the sample space matches task->config.size (square), scaling the original
 * -128..127 signed-byte range up or down with it; SATURN_SIZE_DEFAULT is
 * only the value saturn_create falls back to when the caller passes NULL
 * (see SATURN_CONFIG_DEFAULT in saturn.h). */
#define SATURN_SIZE_DEFAULT 256

/* The original works in a -128..127 sample space (BBC BASIC signed byte),
 * here scaled to whatever size is live; saturn_redraw derives these per-call
 * as half/range/flip/energy_shift from task->config.size. */

#define SATURN_BODY_R2(half) ((half) * (half)) /* planet body: keep if r1*r1+r2*r2 < this */

/* BBC BASIC RND(n>0) returns an integer 1..n. utils/rng's LCG stands in for
 * it (its high bits, which is where an LCG's usable randomness sits) so a
 * Select click can re-seed for a fresh sketch. */
static rng_t saturn_rnd_state;

static void saturn_rnd_seed(unsigned long seed)
{
  rng_seed(&saturn_rnd_state, (uint32_t) seed);
}

static int saturn_rnd(int n)
{
  return (int) ((rng_lcg32(&saturn_rnd_state) >> 16) % (uint32_t) n) + 1;
}

/* one signed sample in the original -128..127 space, scaled to half/range */
#define SATURN_SAMPLE(half, range) (saturn_rnd(range) - (half))

/* MENU click over the content pops this. "Colours" leads to a submenu with
 * one row per task->fg/task->bg, each of which pops a wuss_colourmenu (see
 * saturn_create -- the two leaf items' submenu pointers are patched in there,
 * once the colourmenus exist). "Configuration" hover-opens task->size_dialogue, a
 * borrowed window built once in saturn_create and patched into
 * g_saturn_menu_items[SATURN_MENU_SIZE].window there, the same way. */
enum { SATURN_MENU_COLOURS = 0, SATURN_MENU_SIZE };
enum { SATURN_COLOURS_MENU_FOREGROUND = 0, SATURN_COLOURS_MENU_BACKGROUND };

/* the size dialogue's icons, in creation order (see saturn_conf_dialogue_create):
 * one label/slider/value triple per SATURN_SIZEDLG_ROW_*, then the buttons */
enum {
  SATURN_SIZE_ICON_LABEL1 = 0,
  SATURN_SIZE_ICON_SLIDER1,
  SATURN_SIZE_ICON_VALUE1,

  SATURN_SIZE_ICON_LABEL2,
  SATURN_SIZE_ICON_SLIDER2,
  SATURN_SIZE_ICON_VALUE2,

  SATURN_SIZE_ICON_LABEL3,
  SATURN_SIZE_ICON_SLIDER3,
  SATURN_SIZE_ICON_VALUE3,

  SATURN_SIZE_ICON_LABEL4,
  SATURN_SIZE_ICON_SLIDER4,
  SATURN_SIZE_ICON_VALUE4,

  SATURN_SIZE_ICON_CANCEL,
  SATURN_SIZE_ICON_APPLY,

  SATURN_SIZE_NICONS
};

/* per-row static description, indexed by SATURN_SIZEDLG_ROW_*: label text,
 * slider [min,max] and the byte offset of the saturn_config_t field the row
 * reads/writes (via *(int *) ((char *) &task->config + offset)), shared by
 * saturn_conf_dialogue_create (to fill specs) and
 * saturn_sizedlg_pre_show/saturn_sizedlg_apply (to sync task->config) */
typedef struct saturn_sizedlg_rowdesc
{
  const char *label;
  int         min, max, step;
  size_t      config_offset;
}
saturn_sizedlg_rowdesc_t;

static const saturn_sizedlg_rowdesc_t g_saturn_sizedlg_rows[SATURN_SIZEDLG_NROWS] =
{
  [SATURN_SIZEDLG_ROW_SIZE]  = { "Size",  SATURN_SIZE_MIN, SATURN_SIZE_MAX, SATURN_SIZE_STEP, offsetof(saturn_config_t, size) },
  [SATURN_SIZEDLG_ROW_STARS] = { "Stars", 1, 9999, 0, offsetof(saturn_config_t, stars_iters) },
  [SATURN_SIZEDLG_ROW_RING]  = { "Ring",  1, 9999, 0, offsetof(saturn_config_t, ring_iters) },
  [SATURN_SIZEDLG_ROW_BODY]  = { "Body",  1, 9999, 0, offsetof(saturn_config_t, body_iters) },
};

/* task->config field a row reads/writes, per g_saturn_sizedlg_rows */
static int *saturn_sizedlg_field(saturn_task_t *task, int row)
{
  return (int *) ((char *) &task->config + g_saturn_sizedlg_rows[row].config_offset);
}

static wuss_menu_item_t g_saturn_colours_items[] =
{
  { "Foreground", wuss_MENU_ITEM_BORROWED_SUBMENU, NULL, NULL, 0 },
  { "Background", wuss_MENU_ITEM_BORROWED_SUBMENU, NULL, NULL, 0 }
};

static wuss_menu_t g_saturn_colours_menu =
{
  "Colours", g_saturn_colours_items, NELEMS(g_saturn_colours_items)
};

static wuss_menu_item_t g_saturn_menu_items[] =
{
  { "Colours", wuss_MENU_ITEM_NONE, &g_saturn_colours_menu, NULL, 0 },
  { "Configuration", wuss_MENU_ITEM_BORROWED_SUBMENU, NULL, NULL, 0 }
};

static wuss_menu_t g_saturn_menu =
{
  "Saturn", g_saturn_menu_items, NELEMS(g_saturn_menu_items)
};

static result_t saturn_conf_dialogue_create(saturn_task_t *task);
static result_t saturn_conf_fillout(void *opaque);
static result_t saturn_conf_cancel(void *opaque, wuss_button_t button);
static result_t saturn_conf_apply_action(void *opaque, wuss_button_t button);

result_t saturn_create(wuss_t                *wuss,
                       saturn_task_t         *task,
                       const saturn_config_t *config)
{
  static const saturn_config_t default_config = SATURN_CONFIG_DEFAULT;

  result_t         rc;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task->wuss             = wuss;
  task->bg               = colour_rgb(0x00, 0x00, 0x00);
  task->fg               = colour_rgb(0xFF, 0xFF, 0xFF);
  task->seed             = 1;
  task->config           = (config != NULL) ? *config : default_config;
  task->fg_colourmenu    = NULL;
  task->bg_colourmenu    = NULL;
  task->menu_handle      = NULL;
  task->conf.dialogue    = NULL;
  memset(task->conf.rows, 0, sizeof(task->conf.rows));
  task->conf.cancel      = NULL;
  task->conf.apply       = NULL;

  /* saturn_redraw paints its own background */
  delegate_desc.handle    = saturn_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "saturn";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate; /* the task the menu opens against */
  wuss_task_set_autoclose(delegate, 1);

  /* one instance per target, not shared: a submenu leaf (wuss_MENU_ITEM_BORROWED_SUBMENU)
   * opens on hover with no pre-open hook to retitle/retarget a shared
   * colourmenu first -- only item->window leaves get that (wuss_EVENT_PRE_SHOW),
   * and colourmenu owns its own wuss_menu_t, not a window. */
  rc = wuss_colourmenu_create(&task->fg_colourmenu, wuss, "Foreground");
  if (rc != result_OK)
    goto fail_delegate;

  rc = wuss_colourmenu_create(&task->bg_colourmenu, wuss, "Background");
  if (rc != result_OK)
    goto fail_fg_colourmenu;

  g_saturn_colours_items[SATURN_COLOURS_MENU_FOREGROUND].submenu =
    wuss_colourmenu_menu(task->fg_colourmenu);
  g_saturn_colours_items[SATURN_COLOURS_MENU_BACKGROUND].submenu =
    wuss_colourmenu_menu(task->bg_colourmenu);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(task->config.size, task->config.size),
                                 "Saturn",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(task->config.size, task->config.size),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    goto fail_bg_colourmenu;

  rc = saturn_conf_dialogue_create(task);
  if (rc != result_OK)
    goto fail_bg_colourmenu; /* wuss_task_destroy closes task->window too */
  
  g_saturn_menu_items[SATURN_MENU_SIZE].window =
    wuss_dialogue_window(task->conf.dialogue);

  return result_OK;

fail_bg_colourmenu:
  wuss_colourmenu_destroy(task->bg_colourmenu);
fail_fg_colourmenu:
  wuss_colourmenu_destroy(task->fg_colourmenu);
fail_delegate:
  wuss_task_destroy(delegate); /* unregisters; its QUIT frees the task block */
  return rc;
}

/* plot one point in window content space, clipped to the window. x,y are
 * already in the window's top-down pixel space (callers apply the RISC OS
 * (255 - ...) flip). */
static void saturn_plot(screen_t *scr,
                        int       ox,
                        int       oy,
                        int       size,
                        int       x,
                        int       y,
                        colour_t  c)
{
  if (x < 0 || x >= size || y < 0 || y >= size)
    return;

  screen_set_pixel(scr, ox + x, oy + y, c);
}

static result_t saturn_redraw(const wuss_event_t *event, saturn_task_t *task)
{
  screen_t    *scr;
  const box_t *content, *bounds;
  int          size, half, range, flip, energy_shift;
  int          ox, oy;
  int          i;
  int          x, y, p;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content), task->bg);

  /* sample space scales with the window: -size/2..size/2-1, matching the
   * original's -128..127 at task->config.size == SATURN_SIZE_DEFAULT */
  size         = task->config.size;
  half         = size / 2;
  range        = size - 1;
  flip         = size - 1;
  energy_shift = size;

  /* plot in doc space (saturn_plot clips to 0..size); origin carries the
   * scroll so a scrolled/shrunk window shows the right slice */
  ox = bounds->x0 - event->data.redraw.scroll.x;
  oy = bounds->y0 - event->data.redraw.scroll.y;

  saturn_rnd_seed(task->seed);

  /* loop 1 - the stars: keep points outside the inner disc */
  for (i = 0; i <= task->config.stars_iters; i++)
  {
    x = SATURN_SAMPLE(half, range);
    y = SATURN_SAMPLE(half, range);
    p = (x * x + y * y) / energy_shift;
    if (p > 17)
      saturn_plot(scr, ox, oy, size, x + half, flip - (y + half), task->fg);
  }

  /* loop 2 - the ring: sheared sample with a banded energy gate */
  for (i = 0; i <= task->config.ring_iters; i++)
  {
    int r5, r6, r7, e;

    r5 = SATURN_SAMPLE(half, range);
    r6 = SATURN_SAMPLE(half, range);
    r7 = r5 / 4;
    x  = r6 + r7;
    y  = r6;
    p  = (x * x + y * y) / energy_shift;
    e  = ((r6 + r7) * (r6 + r7) + r5 * r5 + r6 * r6) / energy_shift;
    if (e >= 32 && e < 80 && (r5 < 0 || p > 16))
      saturn_plot(scr, ox, oy, size, x + half, y + half, task->fg);
  }

  /* loop 3 - planet body: filled half-disc offset right */
  for (i = 0; i <= task->config.body_iters; i++)
  {
    int r1, r2, body_r2;

    r1      = SATURN_SAMPLE(half, range);
    r2      = SATURN_SAMPLE(half, range);
    p       = r1 * r1 + r2 * r2;
    body_r2 = SATURN_BODY_R2(half);
    if (p < body_r2)
    {
      x = (int) (sqrt((double) (body_r2 - p)) / 2.0) + half;
      y = flip - (r2 / 2 + half);
      saturn_plot(scr, ox, oy, size, x, y, task->fg);
    }
  }

  return result_OK;
}

static result_t saturn_mouse(saturn_task_t      *task,
                             wuss_mouse_action_t action,
                             wuss_button_t       button,
                             wuss_window_t      *window)
{
  if (action != wuss_MOUSE_DOWN || window != task->window)
    return result_OK;

  if (button & wuss_BUTTON_MENU)
    return wuss_menu_open(task->delegate, &g_saturn_menu,
                          wuss_get_pointer(task->wuss),
                          &task->menu_handle);

  if (button & wuss_BUTTON_SELECT)
  {
    task->seed += 0x9E3779B9UL; /* fresh sketch */
    wuss_window_invalidate_visible(window);
  }

  return result_OK;
}

/* stack items for the size dialogue's layout: a VBOX of label / slider /
 * value-echo / button-row, with fixed-size spacers standing in for the
 * (non-uniform) gaps between them. BTN_ROW is an HBOX with Cancel and
 * Apply side by side, gapped and top-aligned (Apply is taller than
 * Cancel, both start flush with the row's top edge). */
enum
{
  ST_ROOT,

  ST_ROW1,
  ST_LABL1,
  ST_SLDR1,
  ST_VAL1,

  ST_ROW2,
  ST_LABL2,
  ST_SLDR2,
  ST_VAL2,

  ST_ROW3,
  ST_LABL3,
  ST_SLDR3,
  ST_VAL3,

  ST_ROW4,
  ST_LABL4,
  ST_SLDR4,
  ST_VAL4,

  ST_BTNS,
  ST_SPCR,
  ST_CNCL,
  ST_APLY,

  SIZE_STACK__LIMIT
};

/* Leaf main-axis sizes, named so saturn_conf_dialogue_create's hand-computed
 * minimum window size can share them with the table below instead of
 * repeating the numbers as bare literals. */
#define ST_LABEL_W      (5*6) /* enough for "Iters" */
#define ST_LABEL2_W     (4*6) /* enough for "1280" */
#define ST_SLIDER_MIN_W 64
#define ST_CANCEL_W     48
#define ST_APPLY_W      56

static const stack_item_t g_saturn_conf_stack[SIZE_STACK__LIMIT] =
{
  [ST_ROOT]  = STACK_VBOX_EX(-1, 0, wuss_STD_GAP, wuss_STD_INSET, wuss_STD_INSET, wuss_STD_INSET, wuss_STD_INSET),

  [ST_ROW1]  = STACK_HBOX(ST_ROOT, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [ST_LABL1] = STACK_LEAF(ST_ROW1, ST_LABEL_W, 16, stack_ALIGN_CENTRE),
  [ST_SLDR1] = STACK_LEAF_EX(ST_ROW1, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, ST_SLIDER_MIN_W, 0),
  [ST_VAL1]  = STACK_LEAF(ST_ROW1, ST_LABEL2_W, 16, stack_ALIGN_CENTRE),

  [ST_ROW2]  = STACK_HBOX(ST_ROOT, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [ST_LABL2] = STACK_LEAF(ST_ROW2, ST_LABEL_W, 16, stack_ALIGN_CENTRE),
  [ST_SLDR2] = STACK_LEAF_EX(ST_ROW2, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, ST_SLIDER_MIN_W, 0),
  [ST_VAL2]  = STACK_LEAF(ST_ROW2, ST_LABEL2_W, 16, stack_ALIGN_CENTRE),

  [ST_ROW3]  = STACK_HBOX(ST_ROOT, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [ST_LABL3] = STACK_LEAF(ST_ROW3, ST_LABEL_W, 16, stack_ALIGN_CENTRE),
  [ST_SLDR3] = STACK_LEAF_EX(ST_ROW3, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, ST_SLIDER_MIN_W, 0),
  [ST_VAL3]  = STACK_LEAF(ST_ROW3, ST_LABEL2_W, 16, stack_ALIGN_CENTRE),

  [ST_ROW4]  = STACK_HBOX(ST_ROOT, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [ST_LABL4] = STACK_LEAF(ST_ROW4, ST_LABEL_W, 16, stack_ALIGN_CENTRE),
  [ST_SLDR4] = STACK_LEAF_EX(ST_ROW4, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, ST_SLIDER_MIN_W, 0),
  [ST_VAL4]  = STACK_LEAF(ST_ROW4, ST_LABEL2_W, 16, stack_ALIGN_CENTRE),

  [ST_BTNS]  = STACK_HBOX(ST_ROOT, wuss_STD_PRIMARY_BUTTON_HEIGHT, wuss_STD_GAP, stack_ALIGN_END),
  [ST_SPCR]  = STACK_SPACER(ST_BTNS, 1),
  [ST_CNCL]  = STACK_LEAF(ST_BTNS, ST_CANCEL_W, wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
  [ST_APLY]  = STACK_LEAF(ST_BTNS, ST_APPLY_W, wuss_STD_PRIMARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
};

/* Build the size dialogue once: a label, a slider snapped to
 * SATURN_SIZE_STEP, a label echoing the slider's current value, and
 * Cancel/Apply buttons, positioned by stack_solve. Created hidden -- wuss
 * shows and hides it itself, as a menu leaf's borrowed window (see
 * g_saturn_menu_items[SATURN_MENU_SIZE] and wuss_menu_item_t::window), so
 * it must outlive the open menu chain and is never closed here, only
 * hidden. */
static result_t saturn_conf_dialogue_create(saturn_task_t *task)
{
  static const int label_box[SATURN_SIZEDLG_NROWS]   = { ST_LABL1, ST_LABL2, ST_LABL3, ST_LABL4 };
  static const int slider_box[SATURN_SIZEDLG_NROWS]  = { ST_SLDR1, ST_SLDR2, ST_SLDR3, ST_SLDR4 };
  static const int value_box[SATURN_SIZEDLG_NROWS]   = { ST_VAL1, ST_VAL2, ST_VAL3, ST_VAL4 };
  static const int label_icon[SATURN_SIZEDLG_NROWS]  = { SATURN_SIZE_ICON_LABEL1, SATURN_SIZE_ICON_LABEL2, SATURN_SIZE_ICON_LABEL3, SATURN_SIZE_ICON_LABEL4 };
  static const int slider_icon[SATURN_SIZEDLG_NROWS] = { SATURN_SIZE_ICON_SLIDER1, SATURN_SIZE_ICON_SLIDER2, SATURN_SIZE_ICON_SLIDER3, SATURN_SIZE_ICON_SLIDER4 };
  static const int value_icon[SATURN_SIZEDLG_NROWS]  = { SATURN_SIZE_ICON_VALUE1, SATURN_SIZE_ICON_VALUE2, SATURN_SIZE_ICON_VALUE3, SATURN_SIZE_ICON_VALUE4 };

  result_t         rc;
  wuss_icon_spec_t specs[SATURN_SIZE_NICONS];
  wuss_icon_t     *made[SATURN_SIZE_NICONS];
  box_t            boxes[SIZE_STACK__LIMIT];
  box_t            root;
  int              value, row;
  size2d_t         sz;

  /* smallest window the layout can be solved into without any flex item
   * (the sliders) growing past its minimum */
  rc = stack_smallest(g_saturn_conf_stack, NELEMS(g_saturn_conf_stack), &sz);
  if (rc != result_OK)
    return rc;

  rc = wuss_dialogue_create(&task->conf.dialogue, task->delegate, sz,
                            "Configuration", saturn_conf_fillout, task);
  if (rc != result_OK)
    return rc;

  root = (box_t) BOX_POS_SIZE(0, 0, sz.w, sz.h);
  rc = stack_solve(g_saturn_conf_stack, NELEMS(g_saturn_conf_stack), &root, boxes);
  if (rc != result_OK)
    goto exit;

  for (row = 0; row < SATURN_SIZEDLG_NROWS; row++)
  {
    const saturn_sizedlg_rowdesc_t *desc = &g_saturn_sizedlg_rows[row];

    value = *saturn_sizedlg_field(task, row);

    wuss_icon_spec_label(&specs[label_icon[row]], boxes[label_box[row]],
                         desc->label, wuss_COLOUR_BLACK, 1);
    wuss_icon_spec_slider_row(&specs[slider_icon[row]], &specs[value_icon[row]],
                              boxes[slider_box[row]], boxes[value_box[row]],
                              wuss_COLOUR_BLACK, wuss_SLIDER_HORIZONTAL,
                              desc->min, desc->max, value, NULL, desc->step);
  }

  wuss_icon_spec_action(&specs[SATURN_SIZE_ICON_CANCEL], boxes[ST_CNCL], "Cancel", wuss_COLOUR_BLACK, wuss_COLOUR_WINDOW, 0);
  wuss_icon_spec_action(&specs[SATURN_SIZE_ICON_APPLY], boxes[ST_APLY], "Apply", wuss_COLOUR_BLACK, wuss_COLOUR_WINDOW, 1);

  rc = wuss_icon_create_array(wuss_dialogue_window(task->conf.dialogue),
                              specs, SATURN_SIZE_NICONS, made);
  if (rc != result_OK)
    goto exit;

  for (row = 0; row < SATURN_SIZEDLG_NROWS; row++)
    wuss_slider_row_bind(&task->conf.rows[row], made[slider_icon[row]],
                         made[value_icon[row]], NULL,
                         g_saturn_sizedlg_rows[row].min,
                         g_saturn_sizedlg_rows[row].max,
                         g_saturn_sizedlg_rows[row].step);
  task->conf.cancel = made[SATURN_SIZE_ICON_CANCEL];
  task->conf.apply  = made[SATURN_SIZE_ICON_APPLY];

  {
    wuss_dialogue_action_t actions[2];

    actions[0].icon = task->conf.cancel;
    actions[0].fn   = saturn_conf_cancel;
    actions[1].icon = task->conf.apply;
    actions[1].fn   = saturn_conf_apply_action;

    rc = wuss_dialogue_set_actions(task->conf.dialogue, actions,
                                   NELEMS(actions));
    if (rc != result_OK)
      goto exit;
  }

  return result_OK;


exit:
  wuss_dialogue_destroy(task->conf.dialogue); /* not yet a menu leaf: safe to close */
  task->conf.dialogue = NULL;
  return rc;
}

/* Dialogue fillout callback: resync the slider and its echo label to
 * task->config.size, in case Apply (or a config passed to saturn_create)
 * changed it since the dialogue was last shown. Called by
 * wuss_dialogue_handle_pre_show on every reveal, and directly by
 * saturn_conf_cancel to reset the dialogue on an Adjust-Cancel click. */
static result_t saturn_conf_fillout(void *opaque)
{
  result_t       rc;
  saturn_task_t *task;
  int            row, value;

  task = opaque;

  rc = result_OK;
  for (row = 0; row < SATURN_SIZEDLG_NROWS; row++)
  {
    value = *saturn_sizedlg_field(task, row);
    rc = wuss_slider_row_set(wuss_dialogue_window(task->conf.dialogue),
                             &task->conf.rows[row], value);
  }

  return rc;
}

/* Applies the slider's current value to task->window (resize + doc extent),
 * shared by a Select and an Adjust click on Apply. */
static result_t saturn_conf_apply(saturn_task_t *task)
{
  result_t rc;
  int      row, size_value;

  for (row = 0; row < SATURN_SIZEDLG_NROWS; row++)
    *saturn_sizedlg_field(task, row) =
      wuss_icon_get_value(task->conf.rows[row].slider);

  size_value = task->config.size;
  rc = wuss_window_resize(task->window, SIZE2D(size_value, size_value));
  if (rc != result_OK)
    return rc;
  return wuss_window_set_doc(task->window, SIZE2D(size_value, size_value));
}

/* Dialogue action callback for Cancel, split by button per the RISC OS
 * "Adjust doesn't dismiss" convention: Select dismisses the menu chain;
 * Adjust resets the dialogue to task->config.size instead. Dismissing goes
 * through wuss_menu_close rather than touching the (borrowed, reused) window
 * directly -- that is what hides it, same as a click outside the chain
 * would. */
static result_t saturn_conf_cancel(void *opaque, wuss_button_t button)
{
  saturn_task_t *task;

  task = opaque;

  if (button & wuss_BUTTON_SELECT)
  {
    wuss_menu_close(task->menu_handle);
    task->menu_handle = NULL;
    return result_OK;
  }
  if (button & wuss_BUTTON_ADJUST)
    return saturn_conf_fillout(task);
  return result_OK;
}

/* Dialogue action callback for Apply: Select applies and dismisses; Adjust
 * applies but leaves the dialogue open. */
static result_t saturn_conf_apply_action(void *opaque, wuss_button_t button)
{
  saturn_task_t *task;

  task = opaque;

  if (button & wuss_BUTTON_SELECT)
  {
    wuss_menu_close(task->menu_handle);
    task->menu_handle = NULL;
    return saturn_conf_apply(task);
  }
  if (button & wuss_BUTTON_ADJUST)
    return saturn_conf_apply(task);
  return result_OK;
}

/* wuss_EVENT_ICON on the size dialogue: slider drag updates the echo label
 * (snapped to SATURN_SIZE_STEP) live on DOWN/MOVE, handled here directly;
 * a Cancel/Apply click (UP only -- a press that drags off the button before
 * release is not taken as a click) is dispatched through the dialogue's
 * action table. */
static result_t saturn_conf_dialogue_icon(saturn_task_t      *task,
                                          const wuss_event_t *event)
{
  result_t rc;
  int      row;

  for (row = 0; row < SATURN_SIZEDLG_NROWS; row++)
    if (wuss_slider_row_event(wuss_dialogue_window(task->conf.dialogue),
                              &task->conf.rows[row], event, NULL))
      return result_OK;

  if (wuss_dialogue_handle_icon(task->conf.dialogue, event, &rc))
    return rc;

  return result_OK;
}

/* Applies event to *colour if it's a pick from colourmenu, returns whether
 * it was. Shared by saturn_menu_select's fg/bg attempts below. */
static int saturn_menu_select_apply(const wuss_colourmenu_t *colourmenu,
                                    colour_t                *colour,
                                    const colour_t          *palette,
                                    int                      npalette,
                                    const wuss_event_t      *event)
{
  wuss_colour_t picked;
  int           mine;

  picked = wuss_colourmenu_selected(colourmenu, event, &mine);
  if (!mine)
    return 0;

  if (picked < npalette)
    *colour = palette[picked];
  return 1;
}

/* A pick from either colour submenu, resolved against whichever colourmenu
 * it came from. "Configuration" is a wuss_menu_item_t::window leaf, not a leaf pick,
 * so it never reaches here -- see saturn_conf_dialogue_icon and
 * saturn_sizedlg_pre_show. */
static result_t saturn_menu_select(saturn_task_t      *task,
                                   const wuss_event_t *event)
{
  const colour_t *palette;
  int             npalette;

  palette = wuss_get_palette(task->wuss, &npalette);

  if (saturn_menu_select_apply(task->fg_colourmenu, &task->fg, palette,
                               npalette, event) ||
      saturn_menu_select_apply(task->bg_colourmenu, &task->bg, palette,
                               npalette, event))
    wuss_window_invalidate_visible(task->window);

  return result_OK;
}

result_t saturn_handle(wuss_window_t      *window,
                       const wuss_event_t *event,
                       void               *task_data)
{
  saturn_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return saturn_redraw(event, task);

  case wuss_EVENT_MOUSE:
    return saturn_mouse(task, event->data.mouse.action,
                        event->data.mouse.button, window);

  case wuss_EVENT_ICON:
    if (window == wuss_dialogue_window(task->conf.dialogue))
      return saturn_conf_dialogue_icon(task, event);
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (window == wuss_dialogue_window(task->conf.dialogue))
      return wuss_dialogue_handle_pre_show(task->conf.dialogue);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    return saturn_menu_select(task, event);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_QUIT:
    wuss_colourmenu_destroy(task->fg_colourmenu);
    wuss_colourmenu_destroy(task->bg_colourmenu);
    wuss_dialogue_destroy(task->conf.dialogue);
    free(task); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
