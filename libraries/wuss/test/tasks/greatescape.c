/* wuss/test/tasks/greatescape.c -- The Great Escape task */

#if defined(WUSS_APP) && defined(WUSS_GREATESCAPE)

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
#include "base/utils.h"
#include "geom/box.h"

#include "TheGreatEscape/TheGreatEscape.h"
#include "ZXSpectrum/Kempston.h"
#include "ZXSpectrum/Keyboard.h"

#include "wuss/component/proginfo.h"
#include "wuss/menu.h"

#include "greatescape.h"
#include "common.h"

/* The game's timings are in T-states of a 48K Spectrum's Z80. */
#define GREATESCAPE_CLOCK_HZ      (3.5e6)

/* Pacing, as the Chase H.Q. task: a backlog longer than MAX_LAG is dropped
 * rather than caught up, and a wait shorter than MIN_WAIT is left on the
 * deadline for a later, longer one to honour. */
#define GREATESCAPE_MAX_LAG_SECS  (0.08)
#define GREATESCAPE_MIN_WAIT_SECS (0.002)

/* Width of the border drawn around the Spectrum screen, in pixels. */
#define GREATESCAPE_BORDER        (32)

/* MENU click pops this menu. Its Info row is retargeted at the shared
 * proginfo singleton just before each open. */
enum
{
  GREATESCAPE_MENU_INFO = 0
};

/* The game loops inside tge_menu and tge_main, which only return between
 * frames or on a quit, so it gets its own thread. It talks to the task only through the zxconfig_t callbacks below;
 * everything they share with the main thread sits under lock. */
struct greatescape_task
{
  wuss_task_t       *delegate;
  wuss_window_t     *window;
  wuss_menu_item_t   menu_items[1];
  wuss_menu_t        menu;
  wuss_menu_handle_t menu_handle; /* live only between open and close */
  zxspectrum_t      *zx;
  tgestate_t        *game;
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

static double greatescape_now(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* ----------------------------------------------------------------------- */

/* Game-thread callbacks. */

static int greatescape_quitting(greatescape_task_t *gt)
{
  int quit;

  pthread_mutex_lock(&gt->lock);
  quit = gt->quit;
  pthread_mutex_unlock(&gt->lock);

  return quit;
}

static void greatescape_draw(const zxbox_t *dirty, void *opaque)
{
  greatescape_task_t *gt;

  NOT_USED(dirty);

  gt = opaque;

  pthread_mutex_lock(&gt->lock);
  gt->dirty = 1;
  pthread_mutex_unlock(&gt->lock);
}

static void greatescape_stamp(void *opaque)
{
  NOT_USED(opaque);
}

static void greatescape_border(int colour, void *opaque)
{
  greatescape_task_t *gt;

  gt = opaque;

  pthread_mutex_lock(&gt->lock);
  gt->border = colour;
  gt->dirty  = 1;
  pthread_mutex_unlock(&gt->lock);
}

static int greatescape_sleep(int duration, void *opaque)
{
  greatescape_task_t *gt;
  double              now;
  double              delay;
  struct timespec ts;

  gt = opaque;

  if (greatescape_quitting(gt))
    return 1;

  now = greatescape_now();

  /* advance an absolute deadline so that oversleeps even out */
  if (!gt->deadline_valid)
  {
    gt->deadline       = now;
    gt->deadline_valid = 1;
  }
  gt->deadline += duration / GREATESCAPE_CLOCK_HZ;
  if (gt->deadline < now - GREATESCAPE_MAX_LAG_SECS)
    gt->deadline = now - GREATESCAPE_MAX_LAG_SECS;

  delay = gt->deadline - now;
  if (delay >= GREATESCAPE_MIN_WAIT_SECS)
  {
    ts.tv_sec  = (time_t) delay;
    ts.tv_nsec = (long) ((delay - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
  }

  return 0;
}

static int greatescape_key(uint16_t port, void *opaque)
{
  greatescape_task_t *gt;
  int                 bits;

  gt = opaque;

  pthread_mutex_lock(&gt->lock);
  if (port == port_KEMPSTON_JOYSTICK)
    bits = (int) gt->kempston;
  else
    bits = zxkeyset_for_port(port, &gt->keys);
  pthread_mutex_unlock(&gt->lock);

  return bits;
}

static void *greatescape_thread(void *opaque)
{
  greatescape_task_t *gt;
  int                 proceed;

  gt = opaque;

  tge_setup(gt->game);

  /* the menu returns > 0 to start the game, < 0 once sleep signals a quit */
  do
    proceed = tge_menu(gt->game);
  while (proceed == 0);
  if (proceed < 0)
    return NULL;

  tge_setup2(gt->game);
  while (!greatescape_quitting(gt))
    tge_main(gt->game);

  return NULL;
}

/* ----------------------------------------------------------------------- */

result_t greatescape_create(wuss_t *wuss, greatescape_task_t **out)
{
  result_t            rc;
  greatescape_task_t *task;
  zxconfig_t          config;
  wuss_task_t        *delegate;
  wuss_task_desc_t    delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  pthread_mutex_init(&task->lock, NULL);

  WUSS_MENU_ITEM_WINDOW(task->menu_items, GREATESCAPE_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL);
  WUSS_MENU_TITLE(task->menu, "The Great Escape", task->menu_items,
                  (int) NELEMS(task->menu_items));

  memset(&config, 0, sizeof(config));
  config.width        = SCREEN_WIDTH / 8;
  config.height       = SCREEN_HEIGHT / 8;
  config.opaque       = task;
  config.draw         = greatescape_draw;
  config.stamp        = greatescape_stamp;
  config.sleep        = greatescape_sleep;
  config.key          = greatescape_key;
  config.border       = greatescape_border;

  task->zx = zxspectrum_create(&config);
  if (task->zx == NULL)
  {
    greatescape_destroy(task);
    return result_OOM;
  }

  task->game = tge_create(task->zx);
  if (task->game == NULL)
  {
    greatescape_destroy(task);
    return result_OOM;
  }

  delegate_desc.handle    = greatescape_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "greatescape";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    greatescape_destroy(task); /* nothing registered yet */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate,
                          SIZE2D(SCREEN_WIDTH + 2 * GREATESCAPE_BORDER,
                                 SCREEN_HEIGHT + 2 * GREATESCAPE_BORDER),
                          "The Great Escape",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* its QUIT frees the task block */
    return rc;
  }

  if (pthread_create(&task->thread, NULL, greatescape_thread, task) != 0)
  {
    wuss_task_destroy(delegate);
    return result_OOM;
  }
  task->thread_started = 1;

  if (out)
    *out = task;

  return result_OK;
}

void greatescape_destroy(greatescape_task_t *task)
{
  if (task->thread_started)
  {
    pthread_mutex_lock(&task->lock);
    task->quit = 1;
    pthread_mutex_unlock(&task->lock);
    pthread_join(task->thread, NULL);
  }

  if (task->game)
    tge_destroy(task->game);
  if (task->zx)
    zxspectrum_destroy(task->zx);
  pthread_mutex_destroy(&task->lock);
  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t greatescape_redraw(greatescape_task_t *gt,
                                   const wuss_event_t *event)
{
  static const unsigned int level = 0xCD; /* the border has no BRIGHT */

  result_t        rc;
  const box_t    *bounds;
  int             x, y;
  int             border;
  const uint32_t *pixels;
  bitmap_t        bm;

  bounds = event->data.redraw.bounds;
  x      = bounds->x0 - event->data.redraw.scroll.x;
  y      = bounds->y0 - event->data.redraw.scroll.y;

  pthread_mutex_lock(&gt->lock);
  border = gt->border;
  pthread_mutex_unlock(&gt->lock);

  /* ZX colour bits are G R B */
  screen_fill_rect(event->data.redraw.scr, x, y,
                   SIZE2D(SCREEN_WIDTH + 2 * GREATESCAPE_BORDER,
                          SCREEN_HEIGHT + 2 * GREATESCAPE_BORDER),
                   colour_rgb((border & 2) ? level : 0,
                              (border & 4) ? level : 0,
                              (border & 1) ? level : 0));

  /* the converted screen is 0x00BBGGRR words: R in the low byte */
  pixels = zxspectrum_claim_screen(gt->zx);
  rc = bitmap_init(&bm, SIZE2D(SCREEN_WIDTH, SCREEN_HEIGHT),
                   pixelfmt_rgbx8888, SCREEN_WIDTH * 4, NULL,
                   (void *) pixels);
  if (rc == result_OK)
    rc = screen_copy_bitmap(event->data.redraw.scr,
                            x + GREATESCAPE_BORDER,
                            y + GREATESCAPE_BORDER,
                            &bm);
  zxspectrum_release_screen(gt->zx);

  return rc;
}

static result_t greatescape_mouse(greatescape_task_t *gt,
                                  wuss_window_t      *window,
                                  const wuss_event_t *event)
{
  /* static: proginfo only copies the fields when it is next shown */
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("The Great Escape", "ZX Spectrum The Great Escape, recreated in C");

  /* the proginfo dialogue has no click behaviour of its own */
  if (window != gt->window)
    return result_OK;

  if (event->data.mouse.action != wuss_MOUSE_DOWN ||
      (event->data.mouse.button & wuss_BUTTON_MENU) == 0)
    return result_OK;

  wuss_proginfo_set_desc(&desc);
  gt->menu_items[GREATESCAPE_MENU_INFO].window = wuss_proginfo_window(gt->delegate);

  return wuss_menu_open_at_pointer(gt->delegate, &gt->menu, &gt->menu_handle);
}

/* Press (down) or release a key. The arrows and '.' are the Kempston
 * joystick, as on the game's SDL host; other ASCII keys go to the
 * Spectrum keyboard. Returns zero if the key has no mapping. */
static int greatescape_set_key(greatescape_task_t *gt, int code, int down)
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

  pthread_mutex_lock(&gt->lock);
  if (joy != zxjoystick_UNKNOWN)
    zxkempston_assign(&gt->kempston, joy, down);
  else if (down)
    zxkeyset_setchar(&gt->keys, code);
  else
    zxkeyset_clearchar(&gt->keys, code);
  pthread_mutex_unlock(&gt->lock);

  return 1;
}

result_t greatescape_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  result_t            rc;
  greatescape_task_t *gt;
  int                 dirty;

  gt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return greatescape_redraw(gt, event);

  case wuss_EVENT_MOUSE:
    return greatescape_mouse(gt, window, event);

  case wuss_EVENT_IDLE:
    pthread_mutex_lock(&gt->lock);
    dirty     = gt->dirty;
    gt->dirty = 0;
    pthread_mutex_unlock(&gt->lock);
    /* ponytail: whole-window invalidate; the draw callback's dirty box
     * could narrow it if redraw cost ever shows */
    if (dirty && gt->window != NULL)
      wuss_window_invalidate_visible(gt->window);
    return result_OK;

  case wuss_EVENT_KEY:
    /* leave the proginfo dialogue's keys and Ctrl/Alt combinations to the
     * launcher's shortcuts */
    if (window != gt->window)
      return result_WUSS_KEY_UNCLAIMED;
    if (event->data.key.modifiers & (wuss_KEY_MOD_CTRL | wuss_KEY_MOD_ALT))
      return result_WUSS_KEY_UNCLAIMED;

    return greatescape_set_key(gt, event->data.key.code, 1) ?
           result_OK : result_WUSS_KEY_UNCLAIMED;

  case wuss_EVENT_KEY_UP:
    (void) greatescape_set_key(gt, event->data.key.code, 0);
    return result_OK;

  case wuss_EVENT_LOSE_FOCUS:
    /* releases go to the new focus, so drop everything held now */
    pthread_mutex_lock(&gt->lock);
    zxkeyset_clear(&gt->keys);
    gt->kempston = 0;
    pthread_mutex_unlock(&gt->lock);
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    gt->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (window == gt->menu_items[GREATESCAPE_MENU_INFO].window)
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
    if (window == gt->window)
      gt->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    greatescape_destroy(gt);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP && WUSS_GREATESCAPE */
