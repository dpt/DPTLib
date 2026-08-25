/* checker.c -- wuss test - checkerboard task */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "checker.h"

result_t checker_create(wuss_t *wuss, const colour_t *palette, checker_task_t *task)
{
  wuss_task_t delegate;
  box_t       box;
  result_t    rc;

  task->black = palette[palette_PICO8_BLACK];
  task->white = palette[palette_PICO8_WHITE];

  delegate = wuss_task_make(checker_redraw, NULL, task, wuss_NO_BACKGROUND); /* checker_redraw paints every pixel itself */
  box      = (box_t) BOX_POS_SIZE(440, 300, 160, 160);

  rc = wuss_window_create(wuss, &box, "Checker 1", wuss_WINDOW_NONE, &delegate, &task->window);
  if (rc != result_OK)
    return rc;

  box = (box_t) BOX_POS_SIZE(440, 10, 160, 160);

  rc = wuss_window_create(wuss, &box, "Checker 2", wuss_WINDOW_NONE, &delegate, &task->window2);
  if (rc != result_OK)
  {
    wuss_window_destroy(task->window);
    return rc;
  }

  return result_OK;
}

void checker_destroy(checker_task_t *task)
{
  wuss_window_destroy(task->window);
  wuss_window_destroy(task->window2);
}

result_t checker_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *task_data)
{
  checker_task_t   *cc;
  int               x, y;

  NOT_USED(window);

  cc = task_data;

  for (y = content->y0; y < content->y1; y++)
    for (x = content->x0; x < content->x1; x++)
      screen_draw_pixel(scr, x, y, ((x + y) & 1) ? cc->black : cc->white);

  return result_OK;
}

#endif /* USE_SDL */
