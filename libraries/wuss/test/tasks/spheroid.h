/* wuss/test/tasks/spheroid.h -- KPT Spheroid Designer-style lit sphere */

#ifndef TASKS_SPHEROID_H
#define TASKS_SPHEROID_H

#ifdef WUSS_APP

#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "utils/rng.h"
#include "wuss/component/proginfo.h"
#include "wuss/component/saveas.h"
#include "wuss/gadget/colourset.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#define SPHEROID_NLIGHTS 4
#define SPHEROID_NROWS   5 /* control strip slider rows */
#define SPHEROID_NCOLOURS 3 /* control strip colour sets */

/* one of the fixed lights: a unit direction from the sphere's centre (x
 * right, y up, z towards the viewer; negative z is behind the sphere), a
 * colour, an intensity in percent (100 = the colour as-is) and the size and
 * sharpness of the highlight it makes */
typedef struct spheroid_light
{
  int      on;
  double   x, y, z;
  colour_t colour;
  int      intensity;
  int      size;      /* 0..100: highlight size */
  int      sharpness; /* 0..100: highlight edge hardness */
}
spheroid_light_t;

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
 * palette. Dithering picks how the preview is blitted to a paletted screen.
 * Mutate (M) nudges the sliders, the lit lights' directions and the colours'
 * hues; Randomise (^R) picks every setting afresh, keeping at least one light
 * on; Reset (R) restores the defaults; Save PNG (^S) opens a Save As dialogue
 * that writes the sphere alone, transparent outside its edge. */
typedef struct spheroid_task
{
  wuss_t            *wuss;     /* for wuss_get_pointer, opening the menu */
  wuss_task_t       *delegate; /* the task that owns the menu */
  wuss_window_t     *window;
  wuss_menu_handle_t menu_handle; /* live only between open and a pick */
  wuss_menu_item_t   menu_items[7]; /* per-instance: shared static "Info"
                                     * row would leak one instance's .window
                                     * pointer into another's menu */
  wuss_menu_t        menu;
  /* "Light" submenu: one row per light */
  wuss_menu_item_t   light_items[SPHEROID_NLIGHTS];
  wuss_menu_t        light_menu;
  /* "Dithering" submenu: one row per screen_dither_t */
  wuss_menu_item_t   dither_items[3];
  wuss_menu_t        dither_menu;
  screen_dither_t    dithering;  /* how the preview is blitted */
  int                current;    /* index of the light a drag moves */
  int                dragging;   /* non-zero while a Select drag is live */
  spheroid_light_t   lights[SPHEROID_NLIGHTS];
  colour_t           sphere;     /* surface colour */
  colour_t           background; /* colour outside the sphere */
  int                ambient;    /* 0..100 */
  int                glow;       /* 0..100: rim light in the surface colour */
  wuss_slider_row_t  rows[SPHEROID_NROWS];
  wuss_icon_t       *light_frame; /* captioned with the current light */
  wuss_icon_t       *on_icon;     /* the current light's On option */
  wuss_colourset_t  *colour_sets[SPHEROID_NCOLOURS]; /* sphere, background,
                                                     * current light */
  rng_t              rng;        /* for Mutate and Randomise */
  wuss_saveas_t       *saveas;
}
spheroid_task_t;

wuss_window_fn_t spheroid_handle;

/* create the spheroid window against the given wuss instance; the task block
 * is allocated here, owned by the window, and freed when it closes. if out
 * is non-NULL, the task block is also returned through it */
result_t spheroid_create(wuss_t *wuss, spheroid_task_t **out);

/* free a task block allocated by spheroid_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void spheroid_destroy(spheroid_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_SPHEROID_H */
