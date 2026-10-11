/* wuss/test/tasks/paint-new.h -- paint's New dialogue */

#ifndef TASKS_PAINT_NEW_H
#define TASKS_PAINT_NEW_H

#ifdef WUSS_APP

#include "base/result.h"
#include "framebuf/colour.h"
#include "geom/size.h"
#include "wuss/wuss.h"

#define PAINTNEW_SIZE_MIN 1
#define PAINTNEW_SIZE_MAX 2048 /* a side, in pixels */

/* what the dialogue asks for */
typedef struct paintnew_params
{
  size2d_t size; /* canvas size, each side PAINTNEW_SIZE_MIN..MAX */
  colour_t bg;   /* canvas colour */
  colour_t fg;   /* the pen's starting colour, one that shows on bg */
}
paintnew_params_t;

/* called when Create is clicked, after the dialogue has been dismissed. A
 * failure is returned to the click's handler */
typedef result_t (paintnew_accept_fn_t)(const paintnew_params_t *params,
                                        void                    *opaque);

typedef struct paintnew paintnew_t;

/* create the dialogue, hidden, on a task of its own; that task is freed, and
 * *slot cleared, when it is destroyed, which happens with its parent task if
 * the caller set one (see wuss_set_task_parent). Width and height start at
 * the usual 640 x 480 on white and then keep what was last asked for */
result_t paintnew_create(paintnew_t          **slot,
                         wuss_t               *wuss,
                         paintnew_accept_fn_t *accept,
                         void                 *opaque);

/* show the dialogue centred on the pointer, as a menu of its own, with the
 * caret in the width field */
result_t paintnew_open(paintnew_t *pn);

#endif /* WUSS_APP */

#endif /* TASKS_PAINT_NEW_H */
