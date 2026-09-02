/* wuss/test/tasks/clock.h -- analogue clock task */

#ifndef TASKS_CLOCK_H
#define TASKS_CLOCK_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "framebuf/colour.h"
#include "wuss/window.h"

/* a round-faced analogue clock: a tick-marked bezel with hour, minute and
 * second hands read from the system clock and advanced on every idle tick.
 * A Select click shows or hides the second hand. */
typedef struct clock_task
{
  wuss_window_t *window;
  colour_t       bg, bezel, hand, second_hand;
  bool           show_second;
}
clock_task_t;

wuss_window_fn_t clock_handle;

/* create the clock window against the given wuss instance */
result_t clock_create(wuss_t *wuss, clock_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_CLOCK_H */
