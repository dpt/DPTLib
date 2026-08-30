/* checker.h -- wuss test - checkerboard task */

#ifndef TASKS_CHECKER_H
#define TASKS_CHECKER_H

#ifdef USE_SDL

#include "framebuf/colour.h"
#include "wuss/window.h"

/* cycled by a content click, in this order */
typedef enum checker_pattern
{
  checker_PATTERN_CHECKERBOARD,
  checker_PATTERN_HORIZONTAL,
  checker_PATTERN_VERTICAL,
  checker_PATTERN_DIAGONAL,
  checker_PATTERN__COUNT
}
checker_pattern_t;

/* fills the whole content area with a black and white two-tone pattern;
 * each window cycles its own pattern independently on a content click */
typedef struct checker_task
{
  wuss_window_t    *window, *window2;
  colour_t          black, white;
  checker_pattern_t pattern, pattern2;
  int               band, band2; /* pixels per band, scroll-adjustable */
}
checker_task_t;

wuss_event_fn_t checker_handle;

/* create the two checkerboard windows against the given wuss instance */
result_t checker_create(wuss_t         *wuss,
                        const colour_t *palette,
                        checker_task_t *task);


#endif /* USE_SDL */

#endif /* TASKS_CHECKER_H */
