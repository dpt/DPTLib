/* wuss/test/tasks/saturn.c -- Elite loading-screen planet, recreated */

#ifdef WUSS_APP

#include <math.h>
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
 * the term that actually decides which way the ring's shadow band tilts.
 *
 *   loop 1: X%,Y% in -127..127; keep if (X*X+Y*Y)/256 > 17  -> the ring
 *   loop 2: sheared X% = R6+R5/4, Y% = R6; keep if 32<=E<80 and
 *           (R5<0 or (X*X+Y*Y)/256 > 16)                    -> ring shadow band
 *   loop 3: R1,R2 random; keep if R1*R1+R2*R2 < 16384;
 *           x = sqrt(16384-P)/2                             -> planet body
 */

/* the sample space below (and so the sketch itself) is always 256 pixels
 * square, matching the original's -128..127 signed-byte range; the window's
 * on-screen size (task->config.size, set via the Size... dialogue) scales
 * independently, so a larger window just shows more margin around the same
 * sketch and a smaller one clips it. */
#define SATURN_SIZE         256

/* The original works in a -128..127 sample space (BBC BASIC signed byte). */
#define SATURN_HALF         (SATURN_SIZE / 2) /* sample-space centre / bias */
#define SATURN_RANGE        (SATURN_SIZE - 1) /* saturn_rnd() span: 1..255, then -HALF */
#define SATURN_FLIP         (SATURN_SIZE - 1) /* RISC OS bottom-up -> wuss top-down: FLIP - v */
#define SATURN_ENERGY_SHIFT SATURN_SIZE /* (x*x+y*y) energy divisor */

#define SATURN_BODY_R2    16384 /* planet body: keep if r1*r1+r2*r2 < this */

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

/* one signed sample in the original -128..127 space */
#define SATURN_SAMPLE() (saturn_rnd(SATURN_RANGE) - SATURN_HALF)

/* MENU click over the content pops this. "Colours" leads to a submenu with
 * one row per task->fg/task->bg, each of which pops a wuss_colourmenu (see
 * saturn_create -- the two leaf items' submenu pointers are patched in there,
 * once the colourmenus exist). "Size" hover-opens task->size_dialogue, a
 * borrowed window built once in saturn_create and patched into
 * g_saturn_menu_items[SATURN_MENU_SIZE].window there, the same way. */
enum { SATURN_MENU_COLOURS = 0, SATURN_MENU_SIZE };
enum { SATURN_COLOURS_MENU_FOREGROUND = 0, SATURN_COLOURS_MENU_BACKGROUND };

/* the size dialogue's icons, in creation order (see saturn_size_dialogue_create) */
enum { SATURN_SIZE_ICON_LABEL = 0, SATURN_SIZE_ICON_SLIDER,
       SATURN_SIZE_ICON_VALUE, SATURN_SIZE_ICON_CANCEL,
       SATURN_SIZE_ICON_APPLY, SATURN_SIZE_NICONS };

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
  { "Size", wuss_MENU_ITEM_BORROWED_SUBMENU, NULL, NULL, 0 }
};

static wuss_menu_t g_saturn_menu =
{
  "Saturn", g_saturn_menu_items, NELEMS(g_saturn_menu_items)
};

static result_t saturn_size_dialogue_create(saturn_task_t *task);

result_t saturn_create(wuss_t                *wuss,
                       saturn_task_t         *task,
                       const saturn_config_t *config)
{
  static const saturn_config_t default_config = SATURN_CONFIG_DEFAULT;
  result_t                     rc;
  wuss_task_t                 *delegate;
  wuss_task_desc_t             delegate_desc;

  task->wuss          = wuss;
  task->bg            = colour_rgb(0x00, 0x00, 0x00);
  task->fg            = colour_rgb(0xFF, 0xFF, 0xFF);
  task->seed          = 1;
  task->config        = (config != NULL) ? *config : default_config;
  task->fg_colourmenu     = NULL;
  task->bg_colourmenu     = NULL;
  task->menu_handle       = NULL;
  task->size_dialogue     = NULL;
  task->size_slider       = NULL;
  task->size_value_label  = NULL;
  task->size_cancel       = NULL;
  task->size_apply        = NULL;

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

  rc = wuss_colourmenu_create(&task->fg_colourmenu, wuss, "Foreground");
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  rc = wuss_colourmenu_create(&task->bg_colourmenu, wuss, "Background");
  if (rc != result_OK)
  {
    wuss_colourmenu_destroy(task->fg_colourmenu);
    wuss_task_destroy(delegate);
    return rc;
  }

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
  {
    wuss_colourmenu_destroy(task->bg_colourmenu);
    wuss_colourmenu_destroy(task->fg_colourmenu);
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  rc = saturn_size_dialogue_create(task);
  if (rc != result_OK)
  {
    wuss_colourmenu_destroy(task->bg_colourmenu);
    wuss_colourmenu_destroy(task->fg_colourmenu);
    wuss_task_destroy(delegate); /* closes task->window too; QUIT frees the block */
    return rc;
  }
  g_saturn_menu_items[SATURN_MENU_SIZE].window = task->size_dialogue;

  return result_OK;
}

/* plot one point in window content space, clipped to the window. x,y are
 * already in the window's top-down pixel space (callers apply the RISC OS
 * (255 - ...) flip). */
static void saturn_plot(screen_t *scr,
                        int       ox,
                        int       oy,
                        int       x,
                        int       y,
                        colour_t  c)
{
  if (x < 0 || x >= SATURN_SIZE || y < 0 || y >= SATURN_SIZE)
    return;

  screen_set_pixel(scr, ox + x, oy + y, c);
}

static result_t saturn_redraw(const wuss_event_t *event, saturn_task_t *task)
{
  screen_t    *scr;
  const box_t *content, *bounds;
  int          ox, oy;
  int          i;
  int          x, y, p;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  screen_fill_rect(scr, content->x0, content->y0, box_size(content), task->bg);

  /* plot in doc space (saturn_plot clips to 0..SATURN_SIZE); origin carries
   * the scroll so a scrolled/shrunk window shows the right slice */
  ox = bounds->x0 - event->data.redraw.scroll.x;
  oy = bounds->y0 - event->data.redraw.scroll.y;

  saturn_rnd_seed(task->seed);

  /* loop 1 - the ring: keep points outside the inner disc */
  for (i = 0; i <= task->config.ring_iters; i++)
  {
    x = SATURN_SAMPLE();
    y = SATURN_SAMPLE();
    p = (x * x + y * y) / SATURN_ENERGY_SHIFT;
    if (p > 17)
      saturn_plot(scr, ox, oy, x + SATURN_HALF, SATURN_FLIP - (y + SATURN_HALF),
                  task->fg);
  }

  /* loop 2 - ring shadow band: sheared sample with a banded energy gate */
  for (i = 0; i <= task->config.band_iters; i++)
  {
    int r5, r6, r7, e;

    r5 = SATURN_SAMPLE();
    r6 = SATURN_SAMPLE();
    r7 = r5 / 4;
    x  = r6 + r7;
    y  = r6;
    p  = (x * x + y * y) / SATURN_ENERGY_SHIFT;
    e  = ((r6 + r7) * (r6 + r7) + r5 * r5 + r6 * r6) / SATURN_ENERGY_SHIFT;
    if (e >= 32 && e < 80 && (r5 < 0 || p > 16))
      saturn_plot(scr, ox, oy, x + SATURN_HALF, y + SATURN_HALF, task->fg);
  }

  /* loop 3 - planet body: filled half-disc offset right */
  for (i = 0; i <= task->config.body_iters; i++)
  {
    int r1, r2;

    r1 = SATURN_SAMPLE();
    r2 = SATURN_SAMPLE();
    p  = r1 * r1 + r2 * r2;
    if (p < SATURN_BODY_R2)
    {
      x = (int) (sqrt((double) (SATURN_BODY_R2 - p)) / 2.0) + SATURN_HALF;
      y = SATURN_FLIP - (r2 / 2 + SATURN_HALF);
      saturn_plot(scr, ox, oy, x, y, task->fg);
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

/* nearest multiple of SATURN_SIZE_STEP to v, clamped to
 * [SATURN_SIZE_MIN,SATURN_SIZE_MAX] */
static int saturn_size_snap(int v)
{
  v = ((v - SATURN_SIZE_MIN + SATURN_SIZE_STEP / 2) / SATURN_SIZE_STEP) *
      SATURN_SIZE_STEP + SATURN_SIZE_MIN;

  return CLAMP(v, SATURN_SIZE_MIN, SATURN_SIZE_MAX);
}

/* stack items for the size dialogue's layout: a VBOX of label / slider /
 * value-echo / button-row, with fixed-size spacers standing in for the
 * (non-uniform) gaps between them. BTN_ROW is an HBOX with Cancel and
 * Apply side by side, gapped and top-aligned (Apply is taller than
 * Cancel, both start flush with the row's top edge). */
enum
{
  ST_ROOT,
  
  ST_ROW,
  ST_LABL,
  ST_SLDR,
  ST_VAL,

  ST_BTNS,
  ST_SPCR,
  ST_CNCL,
  ST_APLY,
  
  SIZE_STACK__LIMIT
};

#define G 4
#define wuss_STD_SLIDER_HEIGHT           18
#define wuss_STD_SECONDARY_BUTTON_HEIGHT 26
#define wuss_STD_PRIMARY_BUTTON_HEIGHT   34

static const stack_item_t g_saturn_size_stack[SIZE_STACK__LIMIT] =
{
  [ST_ROOT] = STACK_VBOX_EX(-1, 0, G, G, G, G, G),

  [ST_ROW]  = STACK_HBOX(ST_ROOT, wuss_STD_SLIDER_HEIGHT, G, stack_ALIGN_START),
  [ST_LABL] = STACK_LEAF(ST_ROW, 24, 16, stack_ALIGN_CENTRE),
  [ST_SLDR] = STACK_LEAF_EX(ST_ROW, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, 64, 0),
  [ST_VAL]  = STACK_LEAF(ST_ROW, 24, 16, stack_ALIGN_CENTRE),

  [ST_BTNS] = STACK_HBOX(ST_ROOT, wuss_STD_PRIMARY_BUTTON_HEIGHT, G, stack_ALIGN_END),
  [ST_SPCR] = STACK_SPACER(ST_BTNS, 1),
  [ST_CNCL] = STACK_LEAF(ST_BTNS, 48, wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
  [ST_APLY] = STACK_LEAF(ST_BTNS, 56, wuss_STD_PRIMARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
};

/* Build the size dialogue once: a label, a slider snapped to
 * SATURN_SIZE_STEP, a label echoing the slider's current value, and
 * Cancel/Apply buttons, positioned by stack_solve. Created hidden -- wuss
 * shows and hides it itself, as a menu leaf's borrowed window (see
 * g_saturn_menu_items[SATURN_MENU_SIZE] and wuss_menu_item_t::window), so
 * it must outlive the open menu chain and is never closed here, only
 * hidden. */
static result_t saturn_size_dialogue_create(saturn_task_t *task)
{
  wuss_icon_spec_t  specs[SATURN_SIZE_NICONS];
  wuss_icon_t      *made[SATURN_SIZE_NICONS];
  wuss_icon_spec_t *s;
  box_t             boxes[SIZE_STACK__LIMIT];
  box_t             root;
  char              buf[16];
  result_t          rc;
  int               row_w, btns_w;
  size2d_t          sz;

  /* stack_solve distributes into a box it's given; it can't report a
   * subtree's intrinsic minimum size in one call, since flex/spacer items
   * always consume whatever slack the root provides. So the minimum
   * window size is hand-computed here from the same constants the table
   * uses, rather than measured by solving. */
  row_w  = 24 + G + 64 + G + 24;
  btns_w = 48 + G + 56;
  sz.w   = MAX(row_w, btns_w) + 2 * G;
  sz.h   = wuss_STD_SLIDER_HEIGHT + G + wuss_STD_PRIMARY_BUTTON_HEIGHT + 2 * G;

  rc = wuss_window_create_placed(task->delegate,
                                 sz,
                                 "Size",
                                 wuss_WINDOW_HIDDEN         |
                                 wuss_WINDOW_NO_CLOSE       |
                                 wuss_WINDOW_NO_BACK        |
                                 wuss_WINDOW_NO_TOGGLE_SIZE |
                                 wuss_WINDOW_NO_HSCROLL     |
                                 wuss_WINDOW_NO_VSCROLL     |
                                 wuss_WINDOW_NO_RESIZE      |
                                 wuss_WINDOW_NO_REDRAW,
                                 wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                                 sz,
                                 sz,
                                 &task->size_dialogue);
  if (rc != result_OK)
    return rc;

  root = (box_t) BOX_POS_SIZE(0, 0, sz.w, sz.h);
  rc = stack_solve(g_saturn_size_stack, NELEMS(g_saturn_size_stack), &root, boxes);
  if (rc != result_OK)
  {
    wuss_window_close(task->size_dialogue);
    task->size_dialogue = NULL;
    return rc;
  }

  memset(specs, 0, sizeof(specs));

  s        = &specs[SATURN_SIZE_ICON_LABEL];
  s->bbox  = boxes[ST_LABL];
  s->type  = wuss_ICON_TYPE_LABEL;
  s->text  = "Size";
  s->fg    = wuss_COLOUR_BLACK;
  s->bg    = wuss_NO_BACKGROUND;
  s->flags = wuss_ICON_FLAGS_JUSTIFY_RIGHT;

  s       = &specs[SATURN_SIZE_ICON_SLIDER];
  s->bbox = boxes[ST_SLDR];
  s->type = wuss_ICON_TYPE_SLIDER;
  s->fg   = wuss_COLOUR_BLACK;
  s->bg   = wuss_NO_BACKGROUND;
  s->u.slider.orientation   = wuss_SLIDER_HORIZONTAL;
  s->u.slider.min           = SATURN_SIZE_MIN;
  s->u.slider.max           = SATURN_SIZE_MAX;
  s->u.slider.default_value = saturn_size_snap(task->config.size);

  snprintf(buf, sizeof(buf), "%d", s->u.slider.default_value);
  s        = &specs[SATURN_SIZE_ICON_VALUE];
  s->bbox  = boxes[ST_VAL];
  s->type  = wuss_ICON_TYPE_LABEL;
  s->text  = buf; /* copied by wuss_icon_create_array */
  s->fg    = wuss_COLOUR_BLACK;
  s->bg    = wuss_NO_BACKGROUND;

  s        = &specs[SATURN_SIZE_ICON_CANCEL];
  s->bbox  = boxes[ST_CNCL];
  s->type  = wuss_ICON_TYPE_ACTION;
  s->text  = "Cancel";
  s->fg    = wuss_COLOUR_BLACK;
  s->bg    = wuss_COLOUR_WINDOW;

  s        = &specs[SATURN_SIZE_ICON_APPLY];
  s->bbox  = boxes[ST_APLY];
  s->type  = wuss_ICON_TYPE_ACTION;
  s->text  = "Apply";
  s->fg    = wuss_COLOUR_BLACK;
  s->bg    = wuss_COLOUR_WINDOW;
  s->flags = wuss_ICON_FLAGS_DEFAULT;

  rc = wuss_icon_create_array(task->size_dialogue, specs, SATURN_SIZE_NICONS,
                              made);
  if (rc != result_OK)
  {
    wuss_window_close(task->size_dialogue); /* not yet a menu leaf: safe to close */
    task->size_dialogue = NULL;
    return rc;
  }

  task->size_slider      = made[SATURN_SIZE_ICON_SLIDER];
  task->size_value_label = made[SATURN_SIZE_ICON_VALUE];
  task->size_cancel      = made[SATURN_SIZE_ICON_CANCEL];
  task->size_apply       = made[SATURN_SIZE_ICON_APPLY];

  return result_OK;
}

/* wuss_EVENT_ICON on the size dialogue: slider drag updates the echo label
 * (snapped to SATURN_SIZE_STEP) live on DOWN/MOVE; Cancel/Apply act on a
 * Select UP, so a press that drags off the button before release is not
 * taken as a click, and an Adjust click leaves the dialogue open (RISC OS
 * "Adjust doesn't dismiss" convention). Cancel just dismisses the menu
 * chain; Apply resizes the planet window to the chosen size, stores it as
 * the new default and dismisses. Dismissing goes through wuss_menu_close
 * rather than touching the (borrowed, reused) window directly -- that is
 * what hides it, same as a click outside the chain would. */
static result_t saturn_size_icon(saturn_task_t      *task,
                                 const wuss_event_t *event)
{
  wuss_icon_t *icon;
  int          value;
  char         buf[16];

  icon = event->data.icon.icon;

  if (icon == task->size_slider)
  {
    if (event->data.icon.action != wuss_MOUSE_DOWN &&
        event->data.icon.action != wuss_MOUSE_MOVE)
      return result_OK;

    value = saturn_size_snap(event->data.icon.value);
    wuss_icon_set_value(task->size_dialogue, task->size_slider, value);
    snprintf(buf, sizeof(buf), "%d", value);
    return wuss_icon_set_text(task->size_dialogue, task->size_value_label,
                              buf);
  }

  if (event->data.icon.action != wuss_MOUSE_UP ||
      !(event->data.icon.button & wuss_BUTTON_SELECT))
    return result_OK;

  if (icon == task->size_cancel)
  {
    wuss_menu_close(task->menu_handle);
    task->menu_handle = NULL;
    return result_OK;
  }

  if (icon == task->size_apply)
  {
    value = wuss_icon_get_value(task->size_slider);
    task->config.size = value;
    wuss_menu_close(task->menu_handle);
    task->menu_handle = NULL;
    return wuss_window_resize(task->window, SIZE2D(value, value));
  }

  return result_OK;
}

/* wuss_EVENT_PRE_SHOW on the size dialogue: resync the slider and its echo
 * label to task->config.size, in case Apply (or a config passed to
 * saturn_create) changed it since the dialogue was last shown. Always
 * allows the show. */
static result_t saturn_size_pre_show(saturn_task_t *task)
{
  int  value;
  char buf[16];

  value = saturn_size_snap(task->config.size);
  wuss_icon_set_value(task->size_dialogue, task->size_slider, value);
  snprintf(buf, sizeof(buf), "%d", value);

  return wuss_icon_set_text(task->size_dialogue, task->size_value_label, buf);
}

/* A pick from either colour submenu, resolved against whichever colourmenu
 * it came from. "Size" is a wuss_menu_item_t::window leaf, not a leaf pick,
 * so it never reaches here -- see saturn_size_icon and saturn_size_pre_show. */
static result_t saturn_menu_select(saturn_task_t      *task,
                                   const wuss_event_t *event)
{
  const colour_t *palette;
  wuss_colour_t   picked;
  int             npalette, mine;

  palette = wuss_get_palette(task->wuss, &npalette);

  picked = wuss_colourmenu_selected(task->fg_colourmenu, event, &mine);
  if (mine)
  {
    if (picked < npalette)
      task->fg = palette[picked];
    wuss_window_invalidate_visible(task->window);
    return result_OK;
  }

  picked = wuss_colourmenu_selected(task->bg_colourmenu, event, &mine);
  if (mine)
  {
    if (picked < npalette)
      task->bg = palette[picked];
    wuss_window_invalidate_visible(task->window);
    return result_OK;
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
    /* the size dialogue has no content of its own -- just its icons, which
     * wuss already drew */
    if (window == task->size_dialogue)
      return result_OK;
    return saturn_redraw(event, task);

  case wuss_EVENT_MOUSE:
    return saturn_mouse(task, event->data.mouse.action,
                        event->data.mouse.button, window);

  case wuss_EVENT_ICON:
    if (window == task->size_dialogue)
      return saturn_size_icon(task, event);
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (window == task->size_dialogue)
      return saturn_size_pre_show(task);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    return saturn_menu_select(task, event);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_QUIT:
    wuss_colourmenu_destroy(task->fg_colourmenu);
    wuss_colourmenu_destroy(task->bg_colourmenu);
    free(task); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
