/* wuss/tasks/greatescape/greatescape.c -- The Great Escape task */

#if defined(WUSS_APP) && defined(WUSS_GREATESCAPE)

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"

#include "TheGreatEscape/TheGreatEscape.h"
#include "ZXSpectrum/Keyboard.h"

#include "greatescape.h"
#include "../taskapp.h"
#include "../zxtask.h"

/* The game's timings are in T-states of a 48K Spectrum's Z80. */
#define GREATESCAPE_CLOCK_HZ (3.5e6)

/* The game loops inside tge_menu and tge_main, which only return between
 * frames or on a quit; zxtask runs it on its own thread. */
struct greatescape_task
{
  zxtask_t      base; /* must be first */
  zxspectrum_t *zx;
  tgestate_t   *game;
  zxkeyset_t    keys; /* under base.lock */
};

/* ----------------------------------------------------------------------- */

/* Game-thread callbacks. */

static void greatescape_draw(const zxbox_t *dirty, void *opaque)
{
  box_t box;

  if (dirty == NULL)
  {
    zxtask_drawn(opaque, NULL);
    return;
  }

  box.x0 = dirty->x0;
  box.y0 = dirty->y0;
  box.x1 = dirty->x1;
  box.y1 = dirty->y1;
  zxtask_drawn(opaque, &box);
}

static void greatescape_stamp(void *opaque)
{
  (void) opaque;
}

static void greatescape_border(int colour, void *opaque)
{
  zxtask_set_border(opaque, colour);
}

static int greatescape_sleep(int duration, void *opaque)
{
  return zxtask_sleep(opaque, duration);
}

static int greatescape_key(uint16_t port, void *opaque)
{
  greatescape_task_t *gt;
  int                 bits;

  gt = opaque;

  pthread_mutex_lock(&gt->base.lock);
  if (port == port_KEMPSTON_JOYSTICK)
    bits = (int) gt->base.kempston;
  else
    bits = zxkeyset_for_port(port, &gt->keys);
  pthread_mutex_unlock(&gt->base.lock);

  return bits;
}

/* ----------------------------------------------------------------------- */

/* zxtask ops. */

static void greatescape_run(zxtask_t *task)
{
  greatescape_task_t *gt;
  int                 proceed;

  gt = (greatescape_task_t *) task;

  tge_setup(gt->game);

  /* the menu returns > 0 to start the game, < 0 once sleep signals a quit */
  do
    proceed = tge_menu(gt->game);
  while (proceed == 0);
  if (proceed < 0)
    return;

  tge_setup2(gt->game);
  while (!zxtask_quitting(task))
    tge_main(gt->game);
}

static void greatescape_free(zxtask_t *task)
{
  greatescape_task_t *gt;

  gt = (greatescape_task_t *) task;

  if (gt->game)
    tge_destroy(gt->game);
  if (gt->zx)
    zxspectrum_destroy(gt->zx);
}

static result_t greatescape_claim_screen(zxtask_t *task, bitmap_t *bm)
{
  uint32_t *pixels;

  /* 4bpp, as the build selects (see CMakeLists.txt) */
  pixels = zxspectrum_claim_screen(((greatescape_task_t *) task)->zx);

  return bitmap_init(bm, SIZE2D(SCREEN_WIDTH, SCREEN_HEIGHT),
                     pixelfmt_p4, SCREEN_WIDTH / 2, NULL, pixels);
}

static void greatescape_release_screen(zxtask_t *task)
{
  zxspectrum_release_screen(((greatescape_task_t *) task)->zx);
}

static void greatescape_set_key(zxtask_t *task, int c, int down)
{
  greatescape_task_t *gt;

  gt = (greatescape_task_t *) task;

  if (c == 0)
    zxkeyset_clear(&gt->keys);
  else if (down)
    zxkeyset_setchar(&gt->keys, c);
  else
    zxkeyset_clearchar(&gt->keys, c);
}

/* ----------------------------------------------------------------------- */

result_t greatescape_create(wuss_t *wuss, greatescape_task_t **out)
{
  static const zxtask_ops_t ops =
  {
    "The Great Escape",
    "Escape the prison camp",
    "greatescape",
    GREATESCAPE_CLOCK_HZ,
    greatescape_run,
    NULL, /* sleep's quit unwinds the game by itself */
    greatescape_free,
    greatescape_claim_screen,
    greatescape_release_screen,
    greatescape_set_key
  };

  result_t            rc;
  greatescape_task_t *task;
  zxconfig_t          config;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  zxtask_init(&task->base, &ops);

  memset(&config, 0, sizeof(config));
  config.width  = SCREEN_WIDTH / 8;
  config.height = SCREEN_HEIGHT / 8;
  config.opaque = task;
  config.draw   = greatescape_draw;
  config.stamp  = greatescape_stamp;
  config.sleep  = greatescape_sleep;
  config.key    = greatescape_key;
  config.border = greatescape_border;

  task->zx = zxspectrum_create(&config);
  if (task->zx == NULL)
  {
    zxtask_destroy(&task->base);
    return result_OOM;
  }

  task->game = tge_create(task->zx);
  if (task->game == NULL)
  {
    zxtask_destroy(&task->base);
    return result_OOM;
  }

  rc = zxtask_start(&task->base, wuss);
  if (rc != result_OK)
    return rc;

  if (out)
    *out = task;

  return result_OK;
}

result_t greatescape_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "The Great Escape",
    "Escape the prison camp",
    (taskapp_create_fn_t) greatescape_create
  };

  return taskapp_install(wuss, &desc);
}

#endif /* WUSS_APP && WUSS_GREATESCAPE */
