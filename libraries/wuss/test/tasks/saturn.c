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
#include "wuss/component/proginfo.h"
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
 * one row per task->fg/task->bg; both rows share task->colourmenu, retargeted
 * per hover by saturn_pre_submenu_open (wuss_EVENT_PRE_SUBMENU_OPEN) so one
 * instance serves either row. "Configuration" and "Info" hover-open
 * task->size_dialogue / task->proginfo's window, both borrowed windows built
 * once in saturn_create and stored into task->menu_items[...].window
 * there. */
enum { SATURN_MENU_INFO = 0, SATURN_MENU_COLOURS, SATURN_MENU_SIZE };
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

  SATURN_SIZE_ICON_DEFAULT,
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

/* Both rows' .submenu is patched in saturn_create to the one shared
 * task->colourmenu, just to give each row an arrow and a hover target --
 * wuss_EVENT_PRE_SUBMENU_OPEN fires on hover regardless of which menu
 * .submenu names, and saturn_pre_submenu_open retargets the shared
 * colourmenu before handing it back as the menu to actually open. The item
 * tables and wuss_menu_t values live per-instance in saturn_task_t, not as
 * file-scope statics, so that each window's Info/Configuration rows point at
 * their own proginfo/dialogue windows rather than every instance sharing
 * (and overwriting) one global .window pointer. */

static result_t saturn_conf_dialogue_create(saturn_task_t *task);
static result_t saturn_conf_fillout(void *opaque);
static result_t saturn_conf_cancel(void *opaque, wuss_button_t button);
static result_t saturn_conf_apply_action(void *opaque, wuss_button_t button);
static result_t saturn_conf_default_action(void         *opaque,
                                           wuss_button_t button);

result_t saturn_create(wuss_t *wuss, saturn_task_t **out)
{
  static const saturn_config_t default_config = SATURN_CONFIG_DEFAULT;

  result_t         rc;
  saturn_task_t   *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss             = wuss;
  task->bg               = colour_rgb(0x00, 0x00, 0x00);
  task->fg               = colour_rgb(0xFF, 0xFF, 0xFF);
  task->seed             = 1;
  task->config           = default_config;
  task->colourmenu_target = NULL;
  task->menu_handle      = NULL;
  task->conf.dialogue    = NULL;
  memset(task->conf.rows, 0, sizeof(task->conf.rows));
  task->conf.deflt       = NULL;
  task->conf.cancel      = NULL;
  task->conf.apply       = NULL;
  task->proginfo         = NULL;

  /* saturn_redraw paints its own background */
  delegate_desc.handle    = saturn_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "saturn";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  task->delegate = delegate; /* the task the menu opens against */
  wuss_task_set_autoclose(delegate, 1);

  /* shared colourmenu singleton: wuss_EVENT_PRE_SUBMENU_OPEN retitles/
   * retargets it per hover (see saturn_pre_submenu_open), so Foreground and
   * Background don't need their own instance. Both rows' .submenu just need
   * to be non-NULL to draw an arrow and become hoverable; which menu they
   * name doesn't matter since the handler always supplies the menu to
   * open. */
  wuss_colourmenu_set_none(0);
  WUSS_MENU_ITEM_MENU(task->colours_items, SATURN_COLOURS_MENU_FOREGROUND,
                      "Foreground", wuss_MENU_ITEM_PRE_OPEN,
                      wuss_colourmenu_menu(wuss));

  WUSS_MENU_ITEM_MENU(task->colours_items, SATURN_COLOURS_MENU_BACKGROUND,
                      "Background", wuss_MENU_ITEM_PRE_OPEN,
                      wuss_colourmenu_menu(wuss));

  WUSS_MENU_TITLE(task->colours_menu, "Colours", task->colours_items,
                 NELEMS(task->colours_items));

  WUSS_MENU_ITEM(task->menu_items, SATURN_MENU_INFO, "Info",
                wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN);

  WUSS_MENU_ITEM_MENU(task->menu_items, SATURN_MENU_COLOURS, "Colours",
                      wuss_MENU_ITEM_NONE, &task->colours_menu);

  WUSS_MENU_ITEM(task->menu_items, SATURN_MENU_SIZE, "Configuration",
                wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN);

  WUSS_MENU_TITLE(task->menu, "Saturn", task->menu_items,
                 NELEMS(task->menu_items));

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(task->config.size, task->config.size),
                                 "Saturn",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(task->config.size, task->config.size),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    goto fail_delegate;

  rc = saturn_conf_dialogue_create(task);
  if (rc != result_OK)
    goto fail_delegate; /* wuss_task_destroy closes task->window too */

  task->menu_items[SATURN_MENU_SIZE].window =
    wuss_dialogue_window(task->conf.dialogue);

  /* The "Info" menu row's standard dialogue. A create failure is non-fatal
   * -- the task just runs without an Info dialogue (see image.c). */
  {
    static const wuss_proginfo_desc_t desc =
    {
      "Saturn",
      "Elite loading-screen planet, recreated",
      "(c) DPTLib contributors",
      "1.0 (" __DATE__ ")"
    };

    if (wuss_proginfo_create(&task->proginfo, delegate, &desc) != result_OK)
      task->proginfo = NULL;
  }
  task->menu_items[SATURN_MENU_INFO].window =
    wuss_proginfo_window(task->proginfo);

  if (out)
    *out = task;

  return result_OK;

fail_delegate:
  wuss_task_destroy(delegate); /* unregisters; its QUIT frees the task block */
  return rc;
}

void saturn_destroy(saturn_task_t *task)
{
  /* task->conf.dialogue's window is borrowed into task->menu_items as a
   * submenu leaf; if the chain is still open at QUIT (wuss_destroy sweeps
   * tasks before closing any leftover chain -- see its comment) that leaf's
   * node->window would dangle once wuss_dialogue_destroy below frees it.
   * Close our own chain first, same as wuss_destroy expects every task to
   * do for whatever it still holds. */
  if (task->menu_handle != NULL)
  {
    wuss_menu_close(task->menu_handle);
    task->menu_handle = NULL;
  }

  wuss_dialogue_destroy(task->conf.dialogue);
  wuss_proginfo_destroy(task->proginfo);
  free(task); /* task_data was calloc'd per instance by the spawner */
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
  int          stars_p, ring_p, e_lo, e_hi;
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

  /* p's range grows linearly with size (p_max = size/2), so the original's
   * fixed energy-gate constants (17, 16, 32, 80 at size==256) must scale
   * with size too, or the exclusion disc shrinks relative to half as size
   * grows and the stars/ring creep into the planet body. */
  stars_p = 17 * size / SATURN_SIZE_DEFAULT;
  ring_p  = 16 * size / SATURN_SIZE_DEFAULT;
  e_lo    = 32 * size / SATURN_SIZE_DEFAULT;
  e_hi    = 80 * size / SATURN_SIZE_DEFAULT;

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
    if (p > stars_p)
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
    if (e >= e_lo && e < e_hi && (r5 < 0 || p > ring_p))
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
    return wuss_menu_open(task->delegate, &task->menu,
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
  ST_DFLT,
  ST_CNCL,
  ST_APLY,

  SIZE_STACK__LIMIT
};

/* Leaf main-axis sizes, named so saturn_conf_dialogue_create's hand-computed
 * minimum window size can share them with the table below instead of
 * repeating the numbers as bare literals. */
#define ST_LABEL_W          (5*6) /* enough for "Iters" */
#define ST_LABEL2_W         (4*6) /* enough for "9999" */
#define ST_SLIDER_MIN_W     (64)
#define ST_ACTION_WIDTH(W)  ((W)*6+2*4)
#define ST_DEFAULT_WIDTH(W) ((W)*6+2*6)
#define ST_DEFAULT_W        ST_ACTION_WIDTH(9)
#define ST_CANCEL_W         ST_ACTION_WIDTH(9)
#define ST_APPLY_W          ST_DEFAULT_WIDTH(9)

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
  [ST_DFLT]  = STACK_LEAF(ST_BTNS, ST_DEFAULT_W, wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
  [ST_CNCL]  = STACK_LEAF(ST_BTNS, ST_CANCEL_W, wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
  [ST_APLY]  = STACK_LEAF(ST_BTNS, ST_APPLY_W, wuss_STD_PRIMARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
};

/* Build the size dialogue once: a label, a slider snapped to
 * SATURN_SIZE_STEP, a label echoing the slider's current value, and
 * Cancel/Apply buttons, positioned by stack_solve. Created hidden -- wuss
 * shows and hides it itself, as a menu leaf's borrowed window (see
 * task->menu_items[SATURN_MENU_SIZE] and wuss_menu_item_t::window), so
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
  size2d_t         min_sz;

  /* smallest window the layout can be solved into without any flex item
   * (the sliders) growing past its minimum */
  rc = stack_smallest(g_saturn_conf_stack, NELEMS(g_saturn_conf_stack), &min_sz);
  if (rc != result_OK)
    return rc;

  rc = wuss_dialogue_create(&task->conf.dialogue, task->delegate, min_sz,
                            "Configuration", saturn_conf_fillout, task);
  if (rc != result_OK)
    return rc;

  root = (box_t) BOX_POS_SIZE(0, 0, min_sz.w, min_sz.h);
  rc = stack_solve(g_saturn_conf_stack, NELEMS(g_saturn_conf_stack), &root, boxes);
  if (rc != result_OK)
    goto exit;

  for (row = 0; row < SATURN_SIZEDLG_NROWS; row++)
  {
    const saturn_sizedlg_rowdesc_t *desc = &g_saturn_sizedlg_rows[row];

    value = *saturn_sizedlg_field(task, row);

    wuss_icon_spec_label(&specs[label_icon[row]], boxes[label_box[row]],
                         desc->label, 1);
    wuss_icon_spec_slider_row(&specs[slider_icon[row]], &specs[value_icon[row]],
                              boxes[slider_box[row]], boxes[value_box[row]],
                              wuss_SLIDER_HORIZONTAL,
                              desc->min, desc->max, value, NULL, desc->step);
  }

  wuss_icon_spec_action(&specs[SATURN_SIZE_ICON_DEFAULT], boxes[ST_DFLT], "Default", 0);
  wuss_icon_spec_action(&specs[SATURN_SIZE_ICON_CANCEL], boxes[ST_CNCL], "Cancel", 0);
  wuss_icon_spec_action(&specs[SATURN_SIZE_ICON_APPLY], boxes[ST_APLY], "Apply", 1);

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
  task->conf.deflt  = made[SATURN_SIZE_ICON_DEFAULT];
  task->conf.cancel = made[SATURN_SIZE_ICON_CANCEL];
  task->conf.apply  = made[SATURN_SIZE_ICON_APPLY];

  {
    wuss_dialogue_action_t actions[3];

    actions[0].icon = task->conf.deflt;
    actions[0].fn   = saturn_conf_default_action;
    actions[1].icon = task->conf.cancel;
    actions[1].fn   = saturn_conf_cancel;
    actions[2].icon = task->conf.apply;
    actions[2].fn   = saturn_conf_apply_action;

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

/* Dialogue action callback for Default: resets task->config to
 * SATURN_CONFIG_DEFAULT, refills the sliders/echo labels to match, and
 * applies it (window resize + doc extent) -- Select then dismisses the
 * menu chain same as Apply's Select; Adjust leaves the dialogue open,
 * showing the reset values. */
static result_t saturn_conf_default_action(void         *opaque,
                                           wuss_button_t button)
{
  static const saturn_config_t default_config = SATURN_CONFIG_DEFAULT;

  result_t       rc;
  saturn_task_t *task;

  task = opaque;

  task->config = default_config;
  rc = saturn_conf_fillout(task);
  if (rc != result_OK)
    return rc;

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

/* Every submenu leaf fires wuss_EVENT_PRE_SUBMENU_OPEN, not just the
 * Foreground/Background rows -- "Colours" itself is one (task->colours_menu
 * never changes, so it just opens unchanged). Only the Foreground/Background
 * level needs to retitle/retarget the shared task->colourmenu before opening
 * it. */
static result_t saturn_pre_submenu_open(saturn_task_t      *task,
                                        const wuss_event_t *event)
{
  wuss_menu_handle_t handle;
  int                index;

  handle = event->data.pre_submenu_open.handle;
  index  = event->data.pre_submenu_open.index;

  if (wuss_menu_handle_menu(handle) != &task->colours_menu)
    return wuss_menu_open_submenu_now(handle, index,
                                      task->menu_items[index].submenu);

  if (index == SATURN_COLOURS_MENU_FOREGROUND)
  {
    task->colourmenu_target = &task->fg;
    wuss_colourmenu_set_title("Foreground");
  }
  else
  {
    task->colourmenu_target = &task->bg;
    wuss_colourmenu_set_title("Background");
  }

  return wuss_menu_open_submenu_now(handle, index,
                                    wuss_colourmenu_menu(task->wuss));
}

/* A pick from the shared colour submenu, applied to whichever field it was
 * last retargeted at (task->colourmenu_target, set by
 * saturn_pre_submenu_open). "Configuration" is a wuss_menu_item_t::window
 * leaf, not a leaf pick, so it never reaches here -- see
 * saturn_conf_dialogue_icon and saturn_sizedlg_pre_show. */
static result_t saturn_menu_select(saturn_task_t      *task,
                                   const wuss_event_t *event)
{
  const colour_t *palette;
  int             npalette;
  wuss_colour_t   picked;
  int             mine;

  if (task->colourmenu_target == NULL)
    return result_OK;

  picked = wuss_colourmenu_selected(event, &mine);
  if (!mine)
    return result_OK;

  palette = wuss_get_palette(task->wuss, &npalette);
  if (picked < npalette)
  {
    *task->colourmenu_target = palette[picked];
    wuss_window_invalidate_visible(task->window);
  }

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
  {
    result_t rc;

    if (window == wuss_dialogue_window(task->conf.dialogue))
      rc = wuss_dialogue_handle_pre_show(task->conf.dialogue);
    else if (window == wuss_proginfo_window(task->proginfo))
      rc = wuss_proginfo_handle_pre_show(task->proginfo);
    else
      rc = result_OK;
    if (rc != result_OK)
      return rc;

    if (event->data.pre_show.handle == NULL)
      return result_OK; /* plain window reveal, not a flagged menu leaf:
                          * already proceeding by default */

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    return saturn_pre_submenu_open(task, event);

  case wuss_EVENT_MENU_SELECT:
    return saturn_menu_select(task, event);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_QUIT:
    saturn_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
