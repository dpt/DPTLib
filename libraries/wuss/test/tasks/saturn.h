/* wuss/test/tasks/saturn.h -- Elite loading-screen planet, recreated */

#ifndef TASKS_SATURN_H
#define TASKS_SATURN_H

#ifdef WUSS_APP

/* ponytail: no #ifdef WUSS_COMPONENTS guard -- it is PUBLIC on DPTLib and ON
 * by default, so the wuss app always has the colourmenu component. */
#include "framebuf/colour.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/dialogue.h"
#include "wuss/component/proginfo.h"
#include "wuss/icon-spec.h"
#include "wuss/icon.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* window task: recreates the ringed planet from the loading screen of
 * Elite (Ian Bell and David Braben, Acornsoft, 1984). A cryptic BBC BASIC
 * one-liner stipples it out of three rejection-sampled point clouds - a
 * ring of dots, a sheared streak across it and a filled disc for the
 * planet body. Select re-seeds the RNG for a fresh sketch; the idle
 * handler re-seeds every null event so it churns. The plot is
 * deterministic in the seed. */
/* iteration counts for the three rejection-sampling loops, plus the window's
 * size, in document pixels (the window is always square); saturn_create
 * fills in SATURN_CONFIG_DEFAULT values when the caller passes NULL */
typedef struct saturn_config
{
  int stars_iters; /* loop 1: the ring */
  int ring_iters;  /* loop 2: ring shadow band */
  int body_iters;  /* loop 3: planet body */
  int size;        /* window size (both axes) */
}
saturn_config_t;

#define SATURN_CONFIG_DEFAULT { 477, 1280, 1280, 256 }

/* the "Size..." dialogue's slider: [min,max] and the step it snaps to */
#define SATURN_SIZE_MIN  128
#define SATURN_SIZE_MAX  512
#define SATURN_SIZE_STEP 64

/* the size dialogue's slider rows, in creation/layout order */
enum
{
  SATURN_SIZEDLG_ROW_SIZE = 0,
  SATURN_SIZEDLG_ROW_STARS,
  SATURN_SIZEDLG_ROW_RING,
  SATURN_SIZEDLG_ROW_BODY,

  SATURN_SIZEDLG_NROWS
};

/* the size dialogue: built once, hidden; a menu leaf (see
 * g_saturn_menu_items[SATURN_MENU_SIZE].window) */
typedef struct saturn_conf
{
  wuss_dialogue_t   *dialogue;
  wuss_slider_row_t  rows[SATURN_SIZEDLG_NROWS]; /* size, stars, ring, body */
  wuss_icon_t       *cancel;
  wuss_icon_t       *apply;
}
saturn_conf_t;

typedef struct saturn_task
{
  wuss_t            *wuss;     /* for wuss_get_pointer/wuss_get_palette */
  wuss_window_t     *window;
  wuss_task_t       *delegate; /* the task that owns the menu */
  colour_t           bg, fg;
  unsigned long      seed;     /* RNG state; a Select click bumps it */
  saturn_config_t    config;
  wuss_colourmenu_t *fg_colourmenu, *bg_colourmenu;
  wuss_menu_handle_t menu_handle; /* live only between open and a SELECT pick */
  saturn_conf_t      conf;
  wuss_proginfo_t   *proginfo;
}
saturn_task_t;

wuss_window_fn_t saturn_handle;

/* create the planet window against the given wuss instance, always starting
 * at SATURN_CONFIG_DEFAULT; the task block is allocated here, owned by the
 * window and freed when it closes.
 * if out is non-NULL, the task block is also returned through it */
result_t saturn_create(wuss_t *wuss, saturn_task_t **out);

/* free a task block allocated by saturn_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void saturn_destroy(saturn_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_SATURN_H */
