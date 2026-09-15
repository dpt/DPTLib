/* wuss/test/tasks/saturn.h -- Elite loading-screen planet, recreated */

#ifndef TASKS_SATURN_H
#define TASKS_SATURN_H

#ifdef WUSS_APP

/* ponytail: no #ifdef WUSS_COMPONENTS guard -- it is PUBLIC on DPTLib and ON
 * by default, so the wuss app always has the colourmenu component. */
#include "framebuf/colour.h"
#include "wuss/component/colourmenu.h"
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
  int ring_iters; /* loop 2: ring shadow band */
  int body_iters; /* loop 3: planet body */
  int size;       /* window size (both axes); set via the Size... dialogue */
}
saturn_config_t;

#define SATURN_CONFIG_DEFAULT { 477, 1280, 1280, 256 }

/* the "Size..." dialogue's slider: [min,max] and the step it snaps to */
#define SATURN_SIZE_MIN  128
#define SATURN_SIZE_MAX  512
#define SATURN_SIZE_STEP 64

typedef struct saturn_task
{
  wuss_t            *wuss;     /* for wuss_get_pointer/wuss_get_palette */
  wuss_window_t     *window;
  wuss_task_t       *delegate; /* the task that owns the menu */
  colour_t           bg, fg;
  unsigned long      seed; /* RNG state; a Select click bumps it */
  saturn_config_t    config;
  wuss_colourmenu_t *fg_colourmenu, *bg_colourmenu;
  wuss_menu_handle_t menu_handle; /* live only between open and a SELECT pick */
  wuss_window_t     *size_dialogue;   /* built once, hidden; a menu leaf */
  wuss_icon_t       *size_slider;
  wuss_icon_t       *size_value_label;
  wuss_icon_t       *size_slider2;
  wuss_icon_t       *size_value2_label;
  wuss_icon_t       *size_cancel;
  wuss_icon_t       *size_apply;
}
saturn_task_t;

wuss_window_fn_t saturn_handle;

/* create the planet window against the given wuss instance; "task" is a
 * per-instance block owned by the window and freed when it closes. "config"
 * may be NULL for SATURN_CONFIG_DEFAULT. */
result_t saturn_create(wuss_t                *wuss,
                       saturn_task_t         *task,
                       const saturn_config_t *config);

#endif /* WUSS_APP */

#endif /* TASKS_SATURN_H */
