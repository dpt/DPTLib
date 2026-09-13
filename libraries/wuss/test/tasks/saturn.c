/* wuss/test/tasks/saturn.c -- Elite loading-screen planet, recreated */

#ifdef WUSS_APP

#include <math.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
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

#define SATURN_SIZE         256 /* window size is this value squared */

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
 * once the colourmenus exist). */
enum { SATURN_MENU_COLOURS = 0 };
enum { SATURN_COLOURS_MENU_FOREGROUND = 0, SATURN_COLOURS_MENU_BACKGROUND };

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
  { "Colours", wuss_MENU_ITEM_NONE, &g_saturn_colours_menu, NULL, 0 }
};

static wuss_menu_t g_saturn_menu =
{
  "Saturn", g_saturn_menu_items, NELEMS(g_saturn_menu_items)
};

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
  task->fg_colourmenu = NULL;
  task->bg_colourmenu = NULL;
  task->menu_handle   = NULL;

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
                                 SIZE2D(SATURN_SIZE, SATURN_SIZE),
                                 "Saturn",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 SIZE2D(SATURN_SIZE, SATURN_SIZE),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_colourmenu_destroy(task->bg_colourmenu);
    wuss_colourmenu_destroy(task->fg_colourmenu);
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
  }

  return rc;
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
  if (action != wuss_MOUSE_DOWN)
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

/* A pick from either colour submenu: resolve against whichever colourmenu it
 * came from and store into the matching field. */
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
    return saturn_redraw(event, task);

  case wuss_EVENT_MOUSE:
    return saturn_mouse(task, event->data.mouse.action,
                        event->data.mouse.button, window);

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
