/* wuss/tasks/fern.h -- parameterised Barnsley fern */

#ifndef TASKS_FERN_H
#define TASKS_FERN_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "utils/rng.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/dialogue.h"
#include "wuss/component/saveas.h"
#include "wuss/icon-spec.h"
#include "wuss/icon.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* window task: plots Michael Barnsley's iterated-function-system fern.
 * Menu > Configuration sets the window size, the number of points plotted
 * and the curl (a percentage scaling the small shear terms of the main frond
 * transform: 100 is Barnsley's fern, 0 a straight stem, higher bends it
 * over). Select re-seeds for a fresh fern and Adjust steps back; Left/Right
 * step back and forward once the window has the input focus, and Space toggles
 * Menu > Animate, which re-seeds every null event. Menu > Colours sets the
 * foreground and background. Menu > Randomise (^R) picks a fresh seed,
 * curl and colours; Menu > Reset (R) restores the starting configuration,
 * colours and seed. Menu > Save PNG writes the window's content
 * out. The plot is deterministic in the seed. */
typedef struct fern_config
{
  int size;   /* window size (both axes) */
  int points; /* points plotted */
  int curl;   /* percent, 100 = Barnsley's coefficients */
}
fern_config_t;

#define FERN_CONFIG_DEFAULT { 256, 20000, 100 }

#define FERN_SIZE_MIN  128
#define FERN_SIZE_MAX  512
#define FERN_SIZE_STEP 64

enum
{
  FERN_ROW_SIZE = 0,
  FERN_ROW_POINTS,
  FERN_ROW_CURL,

  FERN_NROWS
};

typedef struct fern_conf
{
  wuss_dialogue_t  *dialogue;
  wuss_slider_row_t rows[FERN_NROWS];
  wuss_icon_t      *deflt;
  wuss_icon_t      *cancel;
  wuss_icon_t      *apply;
}
fern_conf_t;

typedef struct fern_task
{
  wuss_t            *wuss;
  wuss_window_t     *window;
  wuss_task_t       *delegate; /* the task that owns the menu */
  wuss_saveas_t     *saveas;
  fern_config_t      config;
  colour_t           bg, fg;
  unsigned long      seed;        /* RNG state; Select/Adjust step it */
  wuss_menu_handle_t menu_handle; /* live only between open and a pick */
  fern_conf_t        conf;
  wuss_colourlist_t  colours; /* per-instance, see saturn.h */
  wuss_menu_item_t   menu_items[6];
  wuss_menu_t        menu;
  int                animate;     /* Menu > Animate: re-seed on every idle */
  rng_t              rng;         /* Randomise */
}
fern_task_t;

wuss_window_fn_t fern_handle;

/* create the fern window, always starting at FERN_CONFIG_DEFAULT; the task
 * block is owned by the window and freed when it closes. if out is non-NULL,
 * the task block is also returned through it */
result_t fern_create(wuss_t *wuss, fern_task_t **out);

/* install the icon bar app that opens fern windows on demand */
result_t fern_app_create(wuss_t *wuss);

/* free a task block allocated by fern_create; normally called by the
 * window's wuss_EVENT_QUIT handler */
void fern_destroy(fern_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_FERN_H */
