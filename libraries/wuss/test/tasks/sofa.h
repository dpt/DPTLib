/* wuss/test/tasks/sofa.h -- rotating wireframe sofa task */

#ifndef TASKS_SOFA_H
#define TASKS_SOFA_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "framebuf/colour.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* which model is currently on show */
typedef enum sofa_shape
{
  sofa_SHAPE_SOFA,
  sofa_SHAPE_SHIP,
  sofa_SHAPE_COBRA,
  sofa_SHAPE_TETRAHEDRON,
  sofa_SHAPE_CUBE,
  sofa_SHAPE_OCTAHEDRON,
  sofa_SHAPE_ICOSAHEDRON,
  sofa_SHAPE_DODECAHEDRON,
  sofa_SHAPE__LIMIT
}
sofa_shape_t;

/* a wireframe sofa (seat, backrest, two arms), spaceship or Platonic solid,
 * spinning about its vertical axis; a Select click pauses/resumes the spin,
 * an Adjust click cycles the model */
typedef struct sofa_task
{
  wuss_t             *wuss;   /* borrowed; for wuss_get_pointer on MENU click */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_proginfo_t    *proginfo;
  wuss_menu_item_t    menu_items[1]; /* per-instance: a shared static would
                                       * leak one instance's proginfo window
                                       * pointer into another's menu */
  wuss_menu_t         menu;
  wuss_window_t *window;
  colour_t       bg, line, dot;
  double         angle;
  double         zoom; /* scroll-adjustable */
  bool           spinning;
  sofa_shape_t   shape;
  int            turns; /* completed rotations of the current model */
}
sofa_task_t;

wuss_window_fn_t sofa_handle;

/* create the sofa window against the given wuss instance; if out is
 * non-NULL, the task block is also returned through it */
result_t sofa_create(wuss_t *wuss, sofa_task_t **out);

/* free a task block allocated by sofa_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void sofa_destroy(sofa_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_SOFA_H */
