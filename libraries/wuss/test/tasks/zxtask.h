/* wuss/test/tasks/zxtask.h -- shared host for ZX Spectrum game tasks */

#ifndef TASKS_ZXTASK_H
#define TASKS_ZXTASK_H

#if defined(WUSS_APP) && (defined(WUSS_CHASEHQ) || defined(WUSS_GREATESCAPE))

#include <pthread.h>

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "geom/box.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/wuss.h"

/* The wuss side of a ZX Spectrum game port: a window with a border in the
 * game's border colour, a menu with Info, Speed (pause and rate),
 * Monochromatic and Double size, real-time pacing, the arrows and '.' as a
 * Kempston joystick, Shift as CAPS SHIFT and a thread for the game loop.
 * Each port's own ZXSpectrum headers stay in its task file, which supplies
 * a zxtask_ops_t and forwards its zxconfig_t callbacks to the zxtask_*
 * calls below. */

/* areas drawn between idles kept apart before folding together: a frame's
 * HUD digits and playfield fit */
#define ZXTASK_MAX_DAMAGE (8)

typedef struct zxtask zxtask_t;

typedef struct zxtask_ops
{
  const char *title;     /* window, menu and proginfo name */
  const char *purpose;   /* proginfo purpose line */
  const char *name;      /* wuss task name */
  double      clock_hz;  /* T-states per second, for zxtask_sleep */

  /* game thread: run the game until it ends or zxtask_sleep reports a
   * quit */
  void (*run)(zxtask_t *task);

  /* optional: nudge the game thread out of its loop, after the quit flag is
   * set */
  void (*stop)(zxtask_t *task);

  /* free the port's own objects, but not the task block */
  void (*destroy)(zxtask_t *task);

  /* lock the converted screen and describe it in *bm as 256x192 p4, index
   * BRIGHT * 8 + ZX colour, with no palette: the host supplies it.
   * release_screen always follows */
  result_t (*claim_screen)(zxtask_t *task, bitmap_t *bm);
  void     (*release_screen)(zxtask_t *task);

  /* press (down) or release ASCII key c (1..127) on the Spectrum keyboard;
   * c == 0 releases every key. Called with the task locked */
  void (*key)(zxtask_t *task, int c, int down);
}
zxtask_ops_t;

/* Embed as the first member of a port's task block, which must be calloc'd:
 * zxtask_destroy frees the block through it. */
struct zxtask
{
  const zxtask_ops_t  *ops;
  wuss_task_t         *delegate;
  wuss_window_t       *window;
  wuss_menu_item_t     menu_items[4];
  wuss_menu_t          menu;
  wuss_menu_item_t     speed_items[5];
  wuss_menu_t          speed_menu;
  char                 speed_title[16]; /* "Speed 100%" */
  wuss_menu_handle_t   menu_handle; /* live only between open and close */
  wuss_proginfo_desc_t desc;
  colour_t             palette[16]; /* the screen's colours, mono aware */
  unsigned char       *pixels;   /* p4 doubled screen, when zoomed */
  int                  mono;     /* simulate a black and white TV */
  int                  zoom;     /* pixel scale, 1 or 2 */
  pthread_t            thread;
  int                  thread_started;
  pthread_mutex_t      lock;     /* guards kempston, border, damage, quit,
                                  * paused, speed and the port's key state */
  pthread_cond_t       wake;     /* signalled on unpause and quit */
  unsigned int         kempston; /* port 0x1F bits: 000FUDLR */
  int                  border;   /* ZX colour 0..7 */
  box_t                damage[ZXTASK_MAX_DAMAGE]; /* unzoomed window
                                  * areas drawn since the last idle */
  int                  ndamage;
  int                  quit;
  int                  paused;
  int                  speed;    /* percent of real time */
  int                  caps;     /* Shift is down as CAPS SHIFT */
  double               deadline; /* game thread only: pacing deadline,
                                  * seconds */
  int                  deadline_valid;

  /* profiling counters, logged by zxtask_destroy */
  double               started;  /* when the game thread started */
  double               awoke;    /* game thread: when sleep last returned */
  double               busy;     /* game thread: seconds outside sleep */
  long                 sleeps;   /* game thread: zxtask_sleep calls */
  long                 lags;     /* game thread: backlogs dropped */
  long                 draws;    /* under lock: zxtask_drawn calls, several
                                  * per game frame */
  long                 redraws;  /* main thread: redraw events */
  double               redrawn;  /* main thread: seconds redrawing */
};

/* set up a calloc'd task block before the port creates its Spectrum, whose
 * callbacks may then use it */
void zxtask_init(zxtask_t *task, const zxtask_ops_t *ops);

/* open the window and start the game thread. On failure the task block is
 * freed */
result_t zxtask_start(zxtask_t *task, wuss_t *wuss);

/* stop the game thread, then free the port's objects and the task block */
void zxtask_destroy(zxtask_t *task);

/* Game-thread calls, for the port's zxconfig_t callbacks. */

/* the game drew in dirty, a ZX box (bottom-left origin, pixels), or the
 * whole screen when NULL: repaint it on the next idle */
void zxtask_drawn(zxtask_t *task, const box_t *dirty);

/* the game set the border to ZX colour 0..7 */
void zxtask_set_border(zxtask_t *task, int colour);

/* sleep for duration T-states, scaled by the speed, against a running
 * deadline; blocks while paused. returns non-zero once the task is
 * quitting */
int zxtask_sleep(zxtask_t *task, int duration);

/* returns non-zero once the task is quitting */
int zxtask_quitting(zxtask_t *task);

#endif /* WUSS_APP && (WUSS_CHASEHQ || WUSS_GREATESCAPE) */

#endif /* TASKS_ZXTASK_H */
