/* wuss/test/tasks/sofa.h -- rotating wireframe sofa task */

#ifndef TASKS_SOFA_H
#define TASKS_SOFA_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "framebuf/colour.h"
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

/* create the sofa window against the given wuss instance */
result_t sofa_create(wuss_t*wuss, sofa_task_t*task);

#endif /* WUSS_APP */

#endif /* TASKS_SOFA_H */
