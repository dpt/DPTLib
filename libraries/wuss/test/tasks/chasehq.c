/* wuss/test/tasks/chasehq.c -- Chase H.Q. task */

#if defined(WUSS_APP) && defined(WUSS_CHASEHQ)

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "ChaseHQ/ChaseHQ.h"
#include "ZXSpectrum/Kempston.h"
#include "ZXSpectrum/Keyboard.h"

#include "wuss/component/proginfo.h"
#include "wuss/menu.h"

#include "chasehq.h"
#include "common.h"

/* Pacing, as the ChaseHQ SDL host: a backlog longer than MAX_LAG is dropped
 * rather than caught up, and a wait shorter than MIN_WAIT is left on the
 * deadline for a later, longer one to honour. */
#define CHASEHQ_MAX_LAG_SECS  (0.08)
#define CHASEHQ_MIN_WAIT_SECS (0.002)

/* Width of the border drawn around the Spectrum screen, in pixels. */
#define CHASEHQ_BORDER        (32)

/* MENU click pops this menu. Its Info row is retargeted at the shared
 * proginfo singleton just before each open. */
enum
{
  CHASEHQ_MENU_INFO = 0
};

/* The game blocks inside chq_start for its whole life, so it gets its own
 * thread. It talks to the task only through the zxconfig_t callbacks below;
 * everything they share with the main thread sits under lock. */
struct chasehq_task
{
  wuss_task_t       *delegate;
  wuss_window_t     *window;
  wuss_menu_item_t   menu_items[1];
  wuss_menu_t        menu;
  wuss_menu_handle_t menu_handle; /* live only between open and close */
  zxspectrum_t      *zx;
  chqstate_t        *game;
  pthread_t          thread;
  int                thread_started;
  pthread_mutex_t    lock;     /* guards keys, kempston, border, dirty and
                                * quit */
  zxkeyset_t         keys;
  zxkempston_t       kempston;
  int                border;   /* ZX colour 0..7 */
  int                dirty;    /* the game drew since the last idle */
  int                quit;
  double             deadline; /* game thread only: pacing deadline, seconds */
  int                deadline_valid;
};

/* ----------------------------------------------------------------------- */

static double chasehq_now(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* ----------------------------------------------------------------------- */

/* Game-thread callbacks. */

static void chasehq_draw(const zxbox_t *dirty, void *opaque)
{
  chasehq_task_t *ct;

  (void) dirty;

  ct = opaque;

  pthread_mutex_lock(&ct->lock);
  ct->dirty = 1;
  pthread_mutex_unlock(&ct->lock);
}

static void chasehq_stamp(void *opaque)
{
  (void) opaque;
}

static void chasehq_border(int colour, void *opaque)
{
  chasehq_task_t *ct;

  ct = opaque;

  pthread_mutex_lock(&ct->lock);
  ct->border = colour;
  ct->dirty  = 1;
  pthread_mutex_unlock(&ct->lock);
}

static int chasehq_sleep(int duration, void *opaque)
{
  chasehq_task_t *ct;
  int             quit;
  double          now;
  double          delay;
  struct timespec ts;

  ct = opaque;

  pthread_mutex_lock(&ct->lock);
  quit = ct->quit;
  pthread_mutex_unlock(&ct->lock);
  if (quit)
    return 1;

  now = chasehq_now();

  /* advance an absolute deadline so that oversleeps even out */
  if (!ct->deadline_valid)
  {
    ct->deadline       = now;
    ct->deadline_valid = 1;
  }
  ct->deadline += duration / (double) Z80_CLOCK_128K;
  if (ct->deadline < now - CHASEHQ_MAX_LAG_SECS)
    ct->deadline = now - CHASEHQ_MAX_LAG_SECS;

  delay = ct->deadline - now;
  if (delay >= CHASEHQ_MIN_WAIT_SECS)
  {
    ts.tv_sec  = (time_t) delay;
    ts.tv_nsec = (long) ((delay - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
  }

  return 0;
}

static int chasehq_key(uint16_t port, void *opaque)
{
  chasehq_task_t *ct;
  int             bits;

  ct = opaque;

  pthread_mutex_lock(&ct->lock);
  if (port == port_KEMPSTON_JOYSTICK)
    bits = (int) ct->kempston;
  else
    bits = zxkeyset_for_port(port, &ct->keys);
  pthread_mutex_unlock(&ct->lock);

  return bits;
}

static void *chasehq_thread(void *opaque)
{
  chasehq_task_t *ct;

  ct = opaque;

  chq_start(ct->game, 1 /* 128K */);

  return NULL;
}

/* ----------------------------------------------------------------------- */

result_t chasehq_create(wuss_t *wuss, chasehq_task_t **out)
{
  result_t         rc;
  chasehq_task_t  *task;
  zxconfig_t       config;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  pthread_mutex_init(&task->lock, NULL);

  WUSS_MENU_ITEM_WINDOW(task->menu_items, CHASEHQ_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL);
  WUSS_MENU_TITLE(task->menu, "Chase H.Q.", task->menu_items,
                  (int) NELEMS(task->menu_items));

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
    chasehq_destroy(task);
    return result_OOM;
  }

  task->game = chq_create(task->zx);
  if (task->game == NULL)
  {
    chasehq_destroy(task);
    return result_OOM;
  }

  delegate_desc.handle    = chasehq_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "chasehq";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    chasehq_destroy(task); /* nothing registered yet */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate,
                          SIZE2D(SCREEN_WIDTH + 2 * CHASEHQ_BORDER,
                                 SCREEN_HEIGHT + 2 * CHASEHQ_BORDER),
                          "Chase H.Q.",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* its QUIT frees the task block */
    return rc;
  }

  if (pthread_create(&task->thread, NULL, chasehq_thread, task) != 0)
  {
    wuss_task_destroy(delegate);
    return result_OOM;
  }
  task->thread_started = 1;

  if (out)
    *out = task;

  return result_OK;
}

void chasehq_destroy(chasehq_task_t *task)
{
  if (task->thread_started)
  {
    pthread_mutex_lock(&task->lock);
    task->quit = 1;
    pthread_mutex_unlock(&task->lock);
    chq_stop(task->game);
    pthread_join(task->thread, NULL);
  }

  if (task->game)
    chq_destroy(task->game);
  if (task->zx)
    zxspectrum_destroy(task->zx);
  pthread_mutex_destroy(&task->lock);
  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t chasehq_redraw(chasehq_task_t *ct, const wuss_event_t *event)
{
  static const unsigned int level = 0xCD; /* the border has no BRIGHT */

  result_t          rc;
  const box_t      *bounds;
  int               x, y;
  int               border;
  const zx_frame_t *frame;
  bitmap_t          bm;

  bounds = event->data.redraw.bounds;
  x      = bounds->x0 - event->data.redraw.scroll.x;
  y      = bounds->y0 - event->data.redraw.scroll.y;

  pthread_mutex_lock(&ct->lock);
  border = ct->border;
  pthread_mutex_unlock(&ct->lock);

  /* ZX colour bits are G R B */
  screen_fill_rect(event->data.redraw.scr, x, y,
                   SIZE2D(SCREEN_WIDTH + 2 * CHASEHQ_BORDER,
                          SCREEN_HEIGHT + 2 * CHASEHQ_BORDER),
                   colour_rgb((border & 2) ? level : 0,
                              (border & 4) ? level : 0,
                              (border & 1) ? level : 0));

  frame = zxspectrum_claim_screen(ct->zx);
  rc = bitmap_init(&bm, SIZE2D(frame->width, frame->height),
                   pixelfmt_rgbx8888, frame->stride, NULL, frame->pixels);
  if (rc == result_OK)
    rc = screen_copy_bitmap(event->data.redraw.scr,
                            x + CHASEHQ_BORDER,
                            y + CHASEHQ_BORDER,
                            &bm);
  zxspectrum_release_screen(ct->zx);

  return rc;
}

static result_t chasehq_mouse(chasehq_task_t     *ct,
                              wuss_window_t      *window,
                              const wuss_event_t *event)
{
  /* static: proginfo only copies the fields when it is next shown */
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("Chase H.Q.", "ZX Spectrum Chase H.Q., natively ported");

  /* the proginfo dialogue has no click behaviour of its own */
  if (window != ct->window)
    return result_OK;

  if (event->data.mouse.action != wuss_MOUSE_DOWN ||
      (event->data.mouse.button & wuss_BUTTON_MENU) == 0)
    return result_OK;

  wuss_proginfo_set_desc(&desc);
  ct->menu_items[CHASEHQ_MENU_INFO].window = wuss_proginfo_window(ct->delegate);

  return wuss_menu_open_at_pointer(ct->delegate, &ct->menu, &ct->menu_handle);
}

/* Press (down) or release a key. The arrows and '.' are the Kempston
 * joystick, as on the ChaseHQ SDL host; other ASCII keys go to the
 * Spectrum keyboard. Returns zero if the key has no mapping. */
static int chasehq_set_key(chasehq_task_t *ct, int code, int down)
{
  zxjoystick_t joy;

  switch (code)
  {
  case wuss_KEY_UP:    joy = zxjoystick_UP;    break;
  case wuss_KEY_DOWN:  joy = zxjoystick_DOWN;  break;
  case wuss_KEY_LEFT:  joy = zxjoystick_LEFT;  break;
  case wuss_KEY_RIGHT: joy = zxjoystick_RIGHT; break;
  case '.':            joy = zxjoystick_FIRE;  break;
  default:             joy = zxjoystick_UNKNOWN; break;
  }

  if (joy == zxjoystick_UNKNOWN && (code <= 0 || code >= 128))
    return 0;

  pthread_mutex_lock(&ct->lock);
  if (joy != zxjoystick_UNKNOWN)
    zxkempston_assign(&ct->kempston, joy, down);
  else if (down)
    zxkeyset_setchar(&ct->keys, code);
  else
    zxkeyset_clearchar(&ct->keys, code);
  pthread_mutex_unlock(&ct->lock);

  return 1;
}

result_t chasehq_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  result_t        rc;
  chasehq_task_t *ct;
  int             dirty;

  ct = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return chasehq_redraw(ct, event);

  case wuss_EVENT_MOUSE:
    return chasehq_mouse(ct, window, event);

  case wuss_EVENT_IDLE:
    pthread_mutex_lock(&ct->lock);
    dirty     = ct->dirty;
    ct->dirty = 0;
    pthread_mutex_unlock(&ct->lock);
    /* ponytail: whole-window invalidate; the draw callback's dirty box
     * could narrow it if redraw cost ever shows */
    if (dirty && ct->window != NULL)
      wuss_window_invalidate_visible(ct->window);
    return result_OK;

  case wuss_EVENT_KEY:
    /* leave the proginfo dialogue's keys and Ctrl/Alt combinations to the
     * launcher's shortcuts */
    if (window != ct->window)
      return result_WUSS_KEY_UNCLAIMED;
    if (event->data.key.modifiers & (wuss_KEY_MOD_CTRL | wuss_KEY_MOD_ALT))
      return result_WUSS_KEY_UNCLAIMED;

    return chasehq_set_key(ct, event->data.key.code, 1) ?
           result_OK : result_WUSS_KEY_UNCLAIMED;

  case wuss_EVENT_KEY_UP:
    (void) chasehq_set_key(ct, event->data.key.code, 0);
    return result_OK;

  case wuss_EVENT_LOSE_FOCUS:
    /* releases go to the new focus, so drop everything held now */
    pthread_mutex_lock(&ct->lock);
    zxkeyset_clear(&ct->keys);
    ct->kempston = 0;
    pthread_mutex_unlock(&ct->lock);
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    ct->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (window == ct->menu_items[CHASEHQ_MENU_INFO].window)
    {
      rc = wuss_proginfo_handle_pre_show();
      if (rc != result_OK)
        return rc;
    }
    if (event->data.pre_show.handle == NULL)
      return result_OK;

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);

  case wuss_EVENT_CLOSE:
    if (window == ct->window)
      ct->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    chasehq_destroy(ct);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP && WUSS_CHASEHQ */
