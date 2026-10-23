/* wuss/tasks/orbing/orbing.h -- KPT Spheroid Designer-style lit sphere */

#ifndef TASKS_ORBING_H
#define TASKS_ORBING_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "utils/rng.h"
#include "wuss/component/colourpicker.h"
#include "wuss/component/saveas.h"
#include "wuss/gadget/colourset.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#include "../common/dithermenu.h"

#define ORBING_NLIGHTS 4
#define ORBING_NROWS   5 /* control strip slider rows */
#define ORBING_NCOLOURS 3 /* control strip colour sets */

/* one of the fixed lights: a unit direction from the sphere's centre (x
 * right, y up, z towards the viewer; negative z is behind the sphere), a
 * colour, an intensity in percent (100 = the colour as-is) and the size and
 * sharpness of the highlight it makes */
typedef struct orbing_light
{
  int      on;
  double   x, y, z;
  colour_t colour;
  int      intensity;
  int      size;      /* 0..100: highlight size */
  int      sharpness; /* 0..100: highlight edge hardness */
}
orbing_light_t;

/* window task: a single sphere, shaded per pixel by up to four coloured lights,
 * in the spirit of Kai's Power Tools' Spheroid Designer. The sphere fills the
 * window right of a fixed control strip framed in three groups: Background
 * (its colour set), Sphere (ambient, glow and its colour set) and the current
 * light's (its highlight size and sharpness, its level, an On option and its
 * colour set); the sphere scales with the window. Each lit light shows
 * as a ring marker (dotted when behind the sphere, filled when current). A
 * Select click on a marker makes that light current; anywhere else it moves the
 * current light there (turning it on), and dragging keeps moving it. The arrow
 * keys move it too, a pixel at a time (eight with Shift). Inside the disc the
 * light faces the surface under the pointer; the band just outside wraps it
 * round to the back. Menu > Light (or keys 1-4) picks the current light; O
 * switches it on or off, as its On option does. Each colour set picks from the
 * palette; Light colour... (C) opens a true-colour picker on the current
 * light. Dithering picks how the preview is blitted to a paletted screen.
 * Mutate (M) nudges the sliders, the lit lights' directions and the colours'
 * hues; Randomise (^R) picks every setting afresh, keeping at least one light
 * on; Reset (R) restores the defaults; Save (F3) opens a Save As dialogue
 * that writes the sphere alone, transparent outside its edge. */
typedef struct orbing_task
{
  wuss_t            *wuss;     /* for wuss_get_pointer, opening the menu */
  wuss_task_t       *delegate; /* the task that owns the menu */
  wuss_window_t     *window;
  wuss_menu_handle_t menu_handle; /* live only between open and a pick */
  wuss_menu_item_t   menu_items[7]; /* per-instance, not a shared static */
  wuss_menu_t        menu;
  /* "Light" submenu: one row per light */
  wuss_menu_item_t   light_items[ORBING_NLIGHTS];
  wuss_menu_t        light_menu;
  dithermenu_t       dither;     /* "Dithering" submenu */
  screen_dither_t    dithering;  /* how the preview is blitted */
  int                current;    /* index of the light a drag moves */
  int                dragging;   /* non-zero while a Select drag is live */
  orbing_light_t   lights[ORBING_NLIGHTS];
  colour_t           sphere;     /* surface colour */
  colour_t           background; /* colour outside the sphere */
  int                ambient;    /* 0..100 */
  int                glow;       /* 0..100: rim light in the surface colour */
  wuss_slider_row_t  rows[ORBING_NROWS];
  wuss_icon_t       *light_frame; /* captioned with the current light */
  wuss_icon_t       *on_icon;     /* the current light's On option */
  wuss_colourset_t  *colour_sets[ORBING_NCOLOURS]; /* sphere, background,
                                                     * current light */
  rng_t              rng;        /* for Mutate and Randomise */
  wuss_saveas_t       *saveas;
  wuss_colourpicker_t *picker;      /* Light colour...: any 24-bit colour */
  int                  picker_light; /* index of the light it edits */
}
orbing_task_t;

wuss_window_fn_t orbing_handle;

/* create the orbing window against the given wuss instance; the task block
 * is allocated here, owned by the window, and freed when it closes. if out
 * is non-NULL, the task block is also returned through it */
result_t orbing_create(wuss_t *wuss, orbing_task_t **out);

/* install the icon bar app that opens orbing windows on demand */
result_t orbing_app_create(wuss_t *wuss);

/* free a task block allocated by orbing_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void orbing_destroy(orbing_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_ORBING_H */
