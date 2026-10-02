/* wuss/test/tasks/chasehq.c -- Chase H.Q. task */

#if defined(WUSS_APP) && defined(WUSS_CHASEHQ)

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"

#include "ChaseHQ/ChaseHQ.h"
#include "ZXSpectrum/Keyboard.h"

#include "chasehq.h"
#include "zxtask.h"

/* The game blocks inside chq_start for its whole life; zxtask runs it on its
 * own thread. */
struct chasehq_task
{
  zxtask_t      base; /* must be first */
  zxspectrum_t *zx;
  chqstate_t   *game;
  zxkeyset_t    keys; /* under base.lock */
};

/* ----------------------------------------------------------------------- */

/* Game-thread callbacks. */

static void chasehq_draw(const zxbox_t *dirty, void *opaque)
{
  (void) dirty;

  zxtask_drawn(opaque);
}

static void chasehq_stamp(void *opaque)
{
  (void) opaque;
}

static void chasehq_border(int colour, void *opaque)
{
  zxtask_set_border(opaque, colour);
}

static int chasehq_sleep(int duration, void *opaque)
{
  return zxtask_sleep(opaque, duration);
}

static int chasehq_key(uint16_t port, void *opaque)
{
  chasehq_task_t *ct;
  int             bits;

  ct = opaque;

  pthread_mutex_lock(&ct->base.lock);
  if (port == port_KEMPSTON_JOYSTICK)
    bits = (int) ct->base.kempston;
  else
    bits = zxkeyset_for_port(port, &ct->keys);
  pthread_mutex_unlock(&ct->base.lock);

  return bits;
}

/* ----------------------------------------------------------------------- */

/* zxtask ops. */

static void chasehq_run(zxtask_t *task)
{
  chq_start(((chasehq_task_t *) task)->game, 1 /* 128K */);
}

static void chasehq_stop(zxtask_t *task)
{
  chq_stop(((chasehq_task_t *) task)->game);
}

static void chasehq_free(zxtask_t *task)
{
  chasehq_task_t *ct;

  ct = (chasehq_task_t *) task;

  if (ct->game)
    chq_destroy(ct->game);
  if (ct->zx)
    zxspectrum_destroy(ct->zx);
}

static result_t chasehq_claim_screen(zxtask_t *task, bitmap_t *bm)
{
  const zx_frame_t *frame;

  frame = zxspectrum_claim_screen(((chasehq_task_t *) task)->zx);

  return bitmap_init(bm, SIZE2D(frame->width, frame->height),
                     pixelfmt_rgbx8888, frame->stride, NULL, frame->pixels);
}

static void chasehq_release_screen(zxtask_t *task)
{
  zxspectrum_release_screen(((chasehq_task_t *) task)->zx);
}

static void chasehq_set_key(zxtask_t *task, int c, int down)
{
  chasehq_task_t *ct;

  ct = (chasehq_task_t *) task;

  if (c == 0)
    zxkeyset_clear(&ct->keys);
  else if (down)
    zxkeyset_setchar(&ct->keys, c);
  else
    zxkeyset_clearchar(&ct->keys, c);
}

/* ----------------------------------------------------------------------- */

result_t chasehq_create(wuss_t *wuss, chasehq_task_t **out)
{
  static const zxtask_ops_t ops =
  {
    "Chase H.Q.",
    "ZX Spectrum Chase H.Q., natively ported",
    "chasehq",
    Z80_CLOCK_128K,
    chasehq_run,
    chasehq_stop,
    chasehq_free,
    chasehq_claim_screen,
    chasehq_release_screen,
    chasehq_set_key
  };

  result_t        rc;
  chasehq_task_t *task;
  zxconfig_t      config;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  zxtask_init(&task->base, &ops);

  memset(&config, 0, sizeof(config));
  config.width        = SCREEN_WIDTH / 8;
  config.height       = SCREEN_HEIGHT / 8;
  config.opaque       = task;
  config.draw         = chasehq_draw;
  config.stamp        = chasehq_stamp;
  config.sleep        = chasehq_sleep;
  config.key          = chasehq_key;
  config.border       = chasehq_border;
  config.pixel_format = ZX_PIXEL_ABGR8888; /* R in the low byte */

  task->zx = zxspectrum_create(&config);
  if (task->zx == NULL)
  {
    zxtask_destroy(&task->base);
    return result_OOM;
  }

  task->game = chq_create(task->zx);
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

#endif /* WUSS_APP && WUSS_CHASEHQ */
