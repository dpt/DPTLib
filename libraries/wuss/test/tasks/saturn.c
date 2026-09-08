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

#define SATURN_SIZE         256   /* window is SATURN_SIZE x SATURN_SIZE */

/* The original works in a -128..127 sample space (BBC BASIC signed byte). */
#define SATURN_HALF         128   /* sample-space centre / bias */
#define SATURN_RANGE        255   /* saturn_rnd() span: 1..255, then -HALF */
#define SATURN_FLIP         255   /* RISC OS bottom-up -> wuss top-down: FLIP - v */
#define SATURN_ENERGY_SHIFT 256   /* (x*x+y*y) energy divisor */

#define SATURN_RING_ITERS   477   /* loop 1: the ring */
#define SATURN_BAND_ITERS   1280  /* loop 2: ring shadow band */
#define SATURN_BODY_ITERS   1280  /* loop 3: planet body */

#define SATURN_BODY_R2      16384 /* planet body: keep if r1*r1+r2*r2 < this */

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

result_t saturn_create(wuss_t *wuss, saturn_task_t *task)
{
  result_t         rc;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task->bg   = colour_rgb(0x00, 0x00, 0x00);
  task->fg   = colour_rgb(0xFF, 0xFF, 0xFF);
  task->seed = 1;

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
  wuss_task_set_autoclose(delegate, 1);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(SATURN_SIZE, SATURN_SIZE),
                                 "Saturn",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                 SIZE2D(SATURN_SIZE, SATURN_SIZE),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */

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
  for (i = 0; i <= SATURN_RING_ITERS; i++)
  {
    x = SATURN_SAMPLE();
    y = SATURN_SAMPLE();
    p = (x * x + y * y) / SATURN_ENERGY_SHIFT;
    if (p > 17)
      saturn_plot(scr, ox, oy, x + SATURN_HALF, SATURN_FLIP - (y + SATURN_HALF),
                  task->fg);
  }

  /* loop 2 - ring shadow band: sheared sample with a banded energy gate */
  for (i = 0; i <= SATURN_BAND_ITERS; i++)
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
  for (i = 0; i <= SATURN_BODY_ITERS; i++)
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

static result_t saturn_idle(saturn_task_t *task)
{
  task->seed += 0x9E3779B9UL; /* churn: a fresh sketch every null event */
  wuss_window_invalidate_all(task->window);

  return result_OK;
}

static result_t saturn_mouse(saturn_task_t      *task,
                             wuss_mouse_action_t action,
                             wuss_button_t       button,
                             wuss_window_t      *window)
{
  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & wuss_BUTTON_SELECT)
  {
    task->seed += 0x9E3779B9UL; /* fresh sketch */
    wuss_window_invalidate_all(window);
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

  case wuss_EVENT_IDLE:
    return saturn_idle(task);

  case wuss_EVENT_QUIT:
    free(task); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
