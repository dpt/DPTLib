/* wuss/tasks/common/zxtask.c -- shared host for ZX Spectrum game tasks */

#if defined(WUSS_APP) && (defined(WUSS_CHASEHQ) || defined(WUSS_GREATESCAPE))

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "common.h"
#include "zxtask.h"

/* The Spectrum's screen, and the border drawn around it, in pixels. */
#define ZXTASK_SCREEN_WIDTH  (256)
#define ZXTASK_SCREEN_HEIGHT (192)
#define ZXTASK_BORDER        (32)
#define ZXTASK_WINDOW_WIDTH  (ZXTASK_SCREEN_WIDTH + 2 * ZXTASK_BORDER)
#define ZXTASK_WINDOW_HEIGHT (ZXTASK_SCREEN_HEIGHT + 2 * ZXTASK_BORDER)
#define ZXTASK_MAX_ZOOM      (2)

/* Pacing: a backlog longer than MAX_LAG is dropped rather than caught up,
 * and a wait shorter than MIN_WAIT is left on the deadline for a later,
 * longer one to honour. */
#define ZXTASK_MAX_LAG_SECS  (0.08)
#define ZXTASK_MIN_WAIT_SECS (0.002)

/* Speed, in percent of real time. Maximum runs unpaced. */
#define ZXTASK_SPEED_STEP    (20)
#define ZXTASK_SPEED_NORMAL  (100)
#define ZXTASK_SPEED_MAX     (99999)

/* Kempston joystick bits, as the hardware reads them. */
#define ZXTASK_KEMPSTON_RIGHT (1u << 0)
#define ZXTASK_KEMPSTON_LEFT  (1u << 1)
#define ZXTASK_KEMPSTON_DOWN  (1u << 2)
#define ZXTASK_KEMPSTON_UP    (1u << 3)
#define ZXTASK_KEMPSTON_FIRE  (1u << 4)

/* MENU click pops this menu. */
enum
{
  ZXTASK_MENU_SPEED,
  ZXTASK_MENU_MONO,
  ZXTASK_MENU_ZOOM
};

/* Rows of the Speed submenu, as the desktop front ends have them. */
enum
{
  ZXTASK_SPEED_MENU_PAUSE,
  ZXTASK_SPEED_MENU_NORMAL,
  ZXTASK_SPEED_MENU_MAXIMUM,
  ZXTASK_SPEED_MENU_FASTER,
  ZXTASK_SPEED_MENU_SLOWER
};

static wuss_window_fn_t zxtask_handle;

/* ----------------------------------------------------------------------- */

static double zxtask_now(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Rec. 601 luma, weights summing to 256 */
static unsigned int zxtask_luma(unsigned int r,
                                unsigned int g,
                                unsigned int b)
{
  return (r * 77 + g * 150 + b * 29) >> 8;
}

/* ZX colour 0..7 (bits G R B) with its set channels at level, grey when
 * mono */
static colour_t zxtask_colour(const zxtask_t *task,
                              int             zx,
                              unsigned int    level)
{
  unsigned int r, g, b;

  r = (zx & 2) ? level : 0;
  g = (zx & 4) ? level : 0;
  b = (zx & 1) ? level : 0;
  if (task->mono)
    r = g = b = zxtask_luma(r, g, b);

  return colour_rgb(r, g, b);
}

static void *zxtask_thread(void *opaque)
{
  zxtask_t *task;

  task = opaque;

  task->ops->run(task);

  return NULL;
}

/* ----------------------------------------------------------------------- */

void zxtask_init(zxtask_t *task, const zxtask_ops_t *ops)
{

  task->ops = ops;

  pthread_mutex_init(&task->lock, NULL);
  pthread_cond_init(&task->wake, NULL);
  task->speed = ZXTASK_SPEED_NORMAL;
  task->zoom  = 1;

  WUSS_MENU_ITEM_SHORTCUT(task->speed_items, ZXTASK_SPEED_MENU_PAUSE, "Pause",
                          wuss_MENU_ITEM_NONE, "^P");
  WUSS_MENU_ITEM(task->speed_items, ZXTASK_SPEED_MENU_NORMAL, "100%",
                 wuss_MENU_ITEM_DASHED | wuss_MENU_ITEM_TICKED);
  WUSS_MENU_ITEM(task->speed_items, ZXTASK_SPEED_MENU_MAXIMUM, "Maximum",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM_SHORTCUT(task->speed_items, ZXTASK_SPEED_MENU_FASTER, "Faster",
                          wuss_MENU_ITEM_DASHED, "^]");
  WUSS_MENU_ITEM_SHORTCUT(task->speed_items, ZXTASK_SPEED_MENU_SLOWER, "Slower",
                          wuss_MENU_ITEM_NONE, "^[");
  snprintf(task->speed_title, sizeof(task->speed_title), "Speed %d%%",
           task->speed);
  WUSS_MENU_TITLE(task->speed_menu, task->speed_title, task->speed_items,
                  NELEMS(task->speed_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, ZXTASK_MENU_SPEED, "Speed",
                      wuss_MENU_ITEM_NONE, &task->speed_menu);
  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, ZXTASK_MENU_MONO, "Monochromatic",
                          wuss_MENU_ITEM_NONE, "^M");
  WUSS_MENU_ITEM(task->menu_items, ZXTASK_MENU_ZOOM, "Double size",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->menu, ops->title, task->menu_items,
                  NELEMS(task->menu_items));
}

result_t zxtask_start(zxtask_t *task, wuss_t *wuss)
{
  result_t         rc;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  delegate_desc.handle    = zxtask_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = task->ops->name;
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    zxtask_destroy(task); /* nothing registered yet */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate,
                          SIZE2D(ZXTASK_WINDOW_WIDTH, ZXTASK_WINDOW_HEIGHT),
                          task->ops->title,
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* its QUIT frees the task block */
    return rc;
  }

  /* a game wants the keyboard straight away, not after a click */
  (void) wuss_set_focus(wuss, task->window);

  task->started = zxtask_now();
  if (pthread_create(&task->thread, NULL, zxtask_thread, task) != 0)
  {
    wuss_task_destroy(delegate);
    return result_OOM;
  }
  task->thread_started = 1;

  return result_OK;
}

/* Log the profiling counters, for timing a run such as a scripted 60 s
 * one. */
static void zxtask_report(const zxtask_t *task)
{
  double elapsed;

  elapsed = zxtask_now() - task->started;
  if (elapsed <= 0.0)
    return;

  logf_info("%s: %.1f s: game busy %.2f s (%.1f%%), %ld sleeps, "
            "%ld lag drops, %.1f draw calls/s; %ld redraws, %.3f ms each",
            task->ops->name, elapsed,
            task->busy, 100.0 * task->busy / elapsed,
            task->sleeps, task->lags, task->draws / elapsed,
            task->redraws,
            task->redraws ? 1e3 * task->redrawn / task->redraws : 0.0);
}

void zxtask_destroy(zxtask_t *task)
{
  if (task->thread_started)
  {
    pthread_mutex_lock(&task->lock);
    task->quit = 1;
    pthread_cond_broadcast(&task->wake);
    pthread_mutex_unlock(&task->lock);
    if (task->ops->stop)
      task->ops->stop(task);
    pthread_join(task->thread, NULL);
    zxtask_report(task);
  }

  task->ops->destroy(task);
  pthread_cond_destroy(&task->wake);
  pthread_mutex_destroy(&task->lock);
  free(task->pixels);
  free(task);
}

/* ----------------------------------------------------------------------- */

void zxtask_drawn(zxtask_t *task, const box_t *dirty)
{
  box_t box;

  /* flip to top-left origin, clamped to the screen, then offset into the
   * border */
  if (dirty == NULL)
  {
    box.x0 = 0;
    box.y0 = 0;
    box.x1 = ZXTASK_SCREEN_WIDTH;
    box.y1 = ZXTASK_SCREEN_HEIGHT;
  }
  else
  {
    box.x0 = CLAMP(dirty->x0, 0, ZXTASK_SCREEN_WIDTH);
    box.y0 = ZXTASK_SCREEN_HEIGHT - CLAMP(dirty->y1, 0, ZXTASK_SCREEN_HEIGHT);
    box.x1 = CLAMP(dirty->x1, 0, ZXTASK_SCREEN_WIDTH);
    box.y1 = ZXTASK_SCREEN_HEIGHT - CLAMP(dirty->y0, 0, ZXTASK_SCREEN_HEIGHT);
  }
  box_translated(&box, ZXTASK_BORDER, ZXTASK_BORDER, &box);

  pthread_mutex_lock(&task->lock);
  (void) box_list_add(task->damage, &task->ndamage, ZXTASK_MAX_DAMAGE, &box);
  task->draws++;
  pthread_mutex_unlock(&task->lock);
}

void zxtask_set_border(zxtask_t *task, int colour)
{
  pthread_mutex_lock(&task->lock);
  /* games may set the border every frame; repaint only on a change */
  if (task->border != colour)
  {
    task->border       = colour;
    task->damage[0].x0 = 0;
    task->damage[0].y0 = 0;
    task->damage[0].x1 = ZXTASK_WINDOW_WIDTH;
    task->damage[0].y1 = ZXTASK_WINDOW_HEIGHT;
    task->ndamage      = 1;
  }
  pthread_mutex_unlock(&task->lock);
}

int zxtask_quitting(zxtask_t *task)
{
  int quit;

  pthread_mutex_lock(&task->lock);
  quit = task->quit;
  pthread_mutex_unlock(&task->lock);

  return quit;
}

int zxtask_sleep(zxtask_t *task, int duration)
{
  int    quit;
  int    speed;
  double now;
  double delay;
  struct timespec ts;

  now = zxtask_now();
  if (task->awoke > 0.0)
    task->busy += now - task->awoke;
  task->sleeps++;

  pthread_mutex_lock(&task->lock);
  if (task->paused && !task->quit)
  {
    do
      pthread_cond_wait(&task->wake, &task->lock);
    while (task->paused && !task->quit);
    task->deadline_valid = 0; /* don't race to catch up the pause */
  }
  quit  = task->quit;
  speed = task->speed;
  pthread_mutex_unlock(&task->lock);

  if (quit)
    return 1;

  if (speed >= ZXTASK_SPEED_MAX)
  {
    task->deadline_valid = 0;
    task->awoke          = zxtask_now();
    return 0;
  }

  now = zxtask_now();

  /* advance an absolute deadline so that oversleeps even out */
  if (!task->deadline_valid)
  {
    task->deadline       = now;
    task->deadline_valid = 1;
  }
  task->deadline += duration / task->ops->clock_hz *
                    ZXTASK_SPEED_NORMAL / speed;
  if (task->deadline < now - ZXTASK_MAX_LAG_SECS)
  {
    task->deadline = now - ZXTASK_MAX_LAG_SECS;
    task->lags++;
  }

  delay = task->deadline - now;
  if (delay >= ZXTASK_MIN_WAIT_SECS)
  {
    ts.tv_sec  = (time_t) delay;
    ts.tv_nsec = (long) ((delay - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
  }
  task->awoke = zxtask_now();

  return 0;
}

/* ----------------------------------------------------------------------- */

/* Double the claimed 256x192 p4 screen into task->pixels. ponytail: zoom 2
 * only, generalise if ZXTASK_MAX_ZOOM grows. */
static void zxtask_convert(zxtask_t *task, const bitmap_t *bm)
{
  int                  rowbytes;
  int                  x, y;
  const unsigned char *row;
  unsigned char       *out;
  unsigned int         p;

  rowbytes = ZXTASK_SCREEN_WIDTH; /* 512 pixels at 4bpp */
  for (y = 0; y < ZXTASK_SCREEN_HEIGHT; y++)
  {
    row = (const unsigned char *) bm->base + y * bm->rowbytes;
    out = task->pixels + y * 2 * rowbytes;
    for (x = 0; x < ZXTASK_SCREEN_WIDTH / 2; x++)
    {
      p      = row[x];
      *out++ = (unsigned char) ((p & 0x0F) | (p << 4));
      *out++ = (unsigned char) ((p & 0xF0) | (p >> 4));
    }
    memcpy(out, out - rowbytes, rowbytes);
  }
}

static result_t zxtask_redraw(zxtask_t *task, const wuss_event_t *event)
{
  static const unsigned int level = 0xCD; /* the border has no BRIGHT */

  result_t     rc;
  const box_t *bounds;
  int          x, y;
  int          border;
  bitmap_t     bm;
  int          i;

  bounds = event->data.redraw.bounds;
  x      = bounds->x0 - event->data.redraw.scroll.x;
  y      = bounds->y0 - event->data.redraw.scroll.y;

  pthread_mutex_lock(&task->lock);
  border = task->border;
  pthread_mutex_unlock(&task->lock);

  screen_fill_rect(event->data.redraw.scr, x, y,
                   SIZE2D(ZXTASK_WINDOW_WIDTH * task->zoom,
                          ZXTASK_WINDOW_HEIGHT * task->zoom),
                   zxtask_colour(task, border, level));

  x += ZXTASK_BORDER * task->zoom;
  y += ZXTASK_BORDER * task->zoom;

  rc = task->ops->claim_screen(task, &bm);
  if (rc != result_OK)
  {
    task->ops->release_screen(task);
    return rc;
  }

  /* the screen borrows the task's palette, never freed through bm; bright
   * black is still black */
  for (i = 0; i < 16; i++)
    task->palette[i] = zxtask_colour(task, i & 7, (i & 8) ? 0xFF : level);
  bm.palette = task->palette;

  if (task->zoom == 1)
  {
    rc = screen_copy_bitmap(event->data.redraw.scr, x, y, &bm);
    task->ops->release_screen(task);
    return rc;
  }

  zxtask_convert(task, &bm);
  task->ops->release_screen(task);
  rc = bitmap_init(&bm,
                   SIZE2D(ZXTASK_SCREEN_WIDTH * task->zoom,
                          ZXTASK_SCREEN_HEIGHT * task->zoom),
                   pixelfmt_p4,
                   ZXTASK_SCREEN_WIDTH * task->zoom / 2,
                   NULL,
                   task->pixels);
  if (rc != result_OK)
    return rc;

  bm.palette = task->palette;

  return screen_copy_bitmap(event->data.redraw.scr, x, y, &bm);
}

static result_t zxtask_mouse(zxtask_t           *task,
                             wuss_window_t      *window,
                             const wuss_event_t *event)
{
  /* the proginfo dialogue has no click behaviour of its own */
  if (window != task->window)
    return result_OK;

  if (event->data.mouse.action != wuss_MOUSE_DOWN ||
      (event->data.mouse.button & wuss_BUTTON_MENU) == 0)
    return result_OK;

  return wuss_menu_open_at_pointer(task->delegate, &task->menu,
                                   &task->menu_handle);
}

/* Bring the menu's ticks and the Speed title into line with the task, and
 * the open menu too if there is one. paused and speed are only written on
 * this thread, so are read without the lock. */
static void zxtask_sync_menu(zxtask_t *task)
{
  unsigned int ticked;

  ticked = 0;
  if (task->paused)
    ticked |= 1u << ZXTASK_SPEED_MENU_PAUSE;
  if (task->speed == ZXTASK_SPEED_NORMAL)
    ticked |= 1u << ZXTASK_SPEED_MENU_NORMAL;
  if (task->speed == ZXTASK_SPEED_MAX)
  {
    ticked |= 1u << ZXTASK_SPEED_MENU_MAXIMUM;
    snprintf(task->speed_title, sizeof(task->speed_title), "Speed max");
  }
  else
  {
    snprintf(task->speed_title, sizeof(task->speed_title), "Speed %d%%",
           task->speed);
  }

  wuss_menu_tick_set(&task->speed_menu, ticked);
  wuss_menu_tick_item(&task->menu, ZXTASK_MENU_MONO, task->mono);
  wuss_menu_tick_item(&task->menu, ZXTASK_MENU_ZOOM, task->zoom > 1);
  if (task->menu_handle == NULL)
    return;

  wuss_menu_tick_set_live(task->menu_handle, &task->speed_menu, ticked);
  wuss_menu_set_title_live(task->menu_handle, &task->speed_menu,
                           task->speed_title);
  wuss_menu_tick_item_live(task->menu_handle, &task->menu, ZXTASK_MENU_MONO,
                           task->mono);
  wuss_menu_tick_item_live(task->menu_handle, &task->menu, ZXTASK_MENU_ZOOM,
                           task->zoom > 1);
}

/* Speed rows act as the desktop front ends' Speed menu: Pause toggles, any
 * other row unpauses. */
static void zxtask_set_speed(zxtask_t *task, int index)
{
  pthread_mutex_lock(&task->lock);
  if (index == ZXTASK_SPEED_MENU_PAUSE)
  {
    task->paused = !task->paused;
  }
  else
  {
    task->paused = 0;
    switch (index)
    {
    case ZXTASK_SPEED_MENU_NORMAL:
      task->speed = ZXTASK_SPEED_NORMAL;
      break;
    case ZXTASK_SPEED_MENU_MAXIMUM:
      task->speed = ZXTASK_SPEED_MAX;
      break;
    case ZXTASK_SPEED_MENU_FASTER:
      task->speed += ZXTASK_SPEED_STEP;
      break;
    case ZXTASK_SPEED_MENU_SLOWER:
      task->speed -= ZXTASK_SPEED_STEP;
      break;
    }
    task->speed = CLAMP(task->speed, ZXTASK_SPEED_STEP, ZXTASK_SPEED_MAX);
  }
  pthread_cond_broadcast(&task->wake);
  pthread_mutex_unlock(&task->lock);
}

/* Toggle mono or zoom. Mono is all in the palette. The zoom buffer is made
 * when zoom is first wanted and freed once it is not. */
static result_t zxtask_set_view(zxtask_t *task, int index)
{
  size2d_t size;

  if (index == ZXTASK_MENU_MONO)
  {
    task->mono = !task->mono;
  }
  else if (task->pixels == NULL)
  {
    task->pixels = malloc(ZXTASK_SCREEN_WIDTH * ZXTASK_SCREEN_HEIGHT *
                          ZXTASK_MAX_ZOOM * ZXTASK_MAX_ZOOM / 2);
    if (task->pixels == NULL)
      return result_OOM;

    task->zoom = ZXTASK_MAX_ZOOM;
  }
  else
  {
    free(task->pixels);
    task->pixels = NULL;
    task->zoom   = 1;
  }

  if (task->window == NULL)
    return result_OK;

  if (index == ZXTASK_MENU_ZOOM)
  {
    size = SIZE2D(ZXTASK_WINDOW_WIDTH * task->zoom,
                  ZXTASK_WINDOW_HEIGHT * task->zoom);
    (void) wuss_window_resize(task->window, size);
    (void) wuss_window_set_doc(task->window, size);
  }
  wuss_window_invalidate_visible(task->window);

  return result_OK;
}

static result_t zxtask_select(zxtask_t *task, const wuss_event_t *event)
{
  result_t rc;
  int      index;

  index = event->data.menu_select.index;
  if (event->data.menu_select.menu == &task->speed_menu)
  {
    zxtask_set_speed(task, index);
  }
  else if (event->data.menu_select.menu == &task->menu &&
           (index == ZXTASK_MENU_MONO || index == ZXTASK_MENU_ZOOM))
  {
    rc = zxtask_set_view(task, index);
    if (rc != result_OK)
      return rc;
  }

  zxtask_sync_menu(task);

  return result_OK;
}

/* Shift is CAPS SHIFT, as on the macOS front end. wuss sends no event for
 * a modifier alone, so it follows the modifiers of each key event. */
static void zxtask_set_caps(zxtask_t *task, wuss_key_modifiers_t modifiers)
{
  int caps;

  caps = (modifiers & wuss_KEY_MOD_SHIFT) != 0;
  if (caps == task->caps)
    return;

  task->caps = caps;
  pthread_mutex_lock(&task->lock);
  task->ops->key(task, '\\', caps); /* '\' is the ports' CAPS SHIFT */
  pthread_mutex_unlock(&task->lock);
}

/* Press (down) or release a key. The arrows and '.' are the Kempston
 * joystick, as on the ports' SDL hosts; other ASCII keys go to the
 * Spectrum keyboard. Returns zero if the key has no mapping. */
static int zxtask_set_key(zxtask_t *task, int code, int down)
{
  unsigned int bit;

  switch (code)
  {
  case wuss_KEY_UP:    bit = ZXTASK_KEMPSTON_UP;    break;
  case wuss_KEY_DOWN:  bit = ZXTASK_KEMPSTON_DOWN;  break;
  case wuss_KEY_LEFT:  bit = ZXTASK_KEMPSTON_LEFT;  break;
  case wuss_KEY_RIGHT: bit = ZXTASK_KEMPSTON_RIGHT; break;
  case '.':            bit = ZXTASK_KEMPSTON_FIRE;  break;
  default:             bit = 0;                     break;
  }

  if (bit == 0 && (code <= 0 || code >= 128))
    return 0;

  pthread_mutex_lock(&task->lock);
  if (bit == 0)
    task->ops->key(task, code, down);
  else if (down)
    task->kempston |= bit;
  else
    task->kempston &= ~bit;
  pthread_mutex_unlock(&task->lock);

  return 1;
}

static result_t zxtask_handle(wuss_window_t      *window,
                              const wuss_event_t *event,
                              void               *task_data)
{
  result_t  rc;
  zxtask_t *task;
  double    then;
  box_t     damage[ZXTASK_MAX_DAMAGE];
  int       ndamage;
  int       i;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    then = zxtask_now();
    rc   = zxtask_redraw(task, event);
    task->redrawn += zxtask_now() - then;
    task->redraws++;
    return rc;

  case wuss_EVENT_MOUSE:
    return zxtask_mouse(task, window, event);

  case wuss_EVENT_IDLE:
    pthread_mutex_lock(&task->lock);
    ndamage = task->ndamage;
    memcpy(damage, task->damage, ndamage * sizeof(*damage));
    task->ndamage = 0;
    pthread_mutex_unlock(&task->lock);
    if (task->window != NULL)
    {
      for (i = 0; i < ndamage; i++)
      {
        box_scalelog2(&damage[i], task->zoom - 1); /* zoom is 1 or 2 */
        wuss_window_invalidate(task->window, &damage[i]);
      }
    }
    return result_OK;

  case wuss_EVENT_KEY:
    /* leave the proginfo dialogue's keys and unmatched Ctrl/Alt
     * combinations to the launcher's shortcuts */
    if (window != task->window)
      return result_WUSS_KEY_UNCLAIMED;
    if (!task_key_is_plain(task->delegate, &task->menu, event, &rc))
      return rc;

    zxtask_set_caps(task, event->data.key.modifiers);
    return zxtask_set_key(task, event->data.key.code, 1) ?
           result_OK : result_WUSS_KEY_UNCLAIMED;

  case wuss_EVENT_KEY_UP:
    zxtask_set_caps(task, event->data.key.modifiers);
    (void) zxtask_set_key(task, event->data.key.code, 0);
    return result_OK;

  case wuss_EVENT_LOSE_FOCUS:
    /* releases go to the new focus, so drop everything held now */
    pthread_mutex_lock(&task->lock);
    task->ops->key(task, 0, 0);
    task->kempston = 0;
    pthread_mutex_unlock(&task->lock);
    task->caps = 0;
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    return zxtask_select(task, event);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (event->data.pre_show.handle == NULL)
      return result_OK;

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);

  case wuss_EVENT_CLOSE:
    if (window == task->window)
      task->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    zxtask_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP && (WUSS_CHASEHQ || WUSS_GREATESCAPE) */
