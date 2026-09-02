/* wuss/test/tasks/porter-duff.h -- animated Porter-Duff compositing task */

#ifndef TASKS_PORTER_DUFF_H
#define TASKS_PORTER_DUFF_H

#ifdef USE_SDL

#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/composite.h"
#include "wuss/window.h"

/* window's task: the two composite demo images blended under a cycling
 * Porter-Duff rule, over an alpha checkerboard. The source image's alpha is
 * ramped up and back down across each rule's turn, so every operator is seen
 * across its full range */
typedef struct porter_duff_task
{
  wuss_window_t   *window;
  bmfont_t        *font;
  bitmap_t         a;               /* owned: pristine source, BGRA */
  bitmap_t         b;               /* owned: pristine destination, BGRA */
  bitmap_t         src;             /* owned: scratch, rebuilt each frame */
  bitmap_t         dst;             /* owned: scratch, rebuilt each frame */
  composite_rule_t rule;
  int              frame;           /* frames elapsed in the current rule */
  int              frames_per_rule;
  colour_t         light;           /* checkerboard */
  colour_t         dark;            /* checkerboard */
  colour_t         fg;              /* rule name label */
  colour_t         bg;              /* rule name label */
}
porter_duff_task_t;

wuss_window_fn_t porter_duff_handle;

/* load the two demo images and create the window against the given wuss
 * instance; resources is the DPTLib repo root, for locating the bundled PNGs */
result_t porter_duff_create(wuss_t             *wuss,
                            const colour_t     *palette,
                            bmfont_t           *font,
                            const char         *resources,
                            porter_duff_task_t *task);


#endif /* USE_SDL */

#endif /* TASKS_PORTER_DUFF_H */
