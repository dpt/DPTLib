/* wuss/task/set-hidden.c -- hide or show all of a task's windows */

#include <stddef.h>

#include "wuss/task.h"
#include "wuss/window.h"

#include "../impl.h"

static result_t task_windows_set_hidden(wuss_task_t *task, int hidden)
{
  result_t       rc;
  result_t       first;
  list_t        *w;
  wuss_window_t *window;

  first = result_OK;
  for (w = task->windows.next; w != NULL; w = w->next)
  {
    window = wuss__window_from_task_link(w);

    /* the back stack holds the icon bar and the like, never hidden with
     * the app */
    if (window->stack == wuss_STACK_BACK)
      continue;

    if (hidden)
    {
      if (window->flags & wuss_WINDOW_HIDDEN)
        continue;

      window->state |= wuss_WINDOW_STATE_TASK_HIDDEN;
      rc = wuss_window_set_hidden(window, 1);
    }
    else
    {
      if (!(window->state & wuss_WINDOW_STATE_TASK_HIDDEN))
        continue;

      window->state &= (wuss_window_state_t) ~wuss_WINDOW_STATE_TASK_HIDDEN;
      rc = wuss_window_set_hidden(window, 0);
    }

    if (first == result_OK)
      first = rc;
  }

  return first;
}

result_t wuss_task_set_hidden(wuss_t *wuss, wuss_task_t *task, int hidden)
{
  result_t     rc;
  result_t     first;
  list_t      *t;
  wuss_task_t *each;

  first = result_OK;
  for (t = wuss->tasks.next; t != NULL; t = t->next)
  {
    each = wuss__task_from_link(t);
    if (each != task && each->parent != task)
      continue;

    rc = task_windows_set_hidden(each, hidden);
    if (first == result_OK)
      first = rc;
  }

  return first;
}

int wuss_task_has_windows(wuss_t *wuss, wuss_task_t *task)
{
  list_t        *t;
  list_t        *w;
  wuss_task_t   *each;
  wuss_window_t *window;

  for (t = wuss->tasks.next; t != NULL; t = t->next)
  {
    each = wuss__task_from_link(t);
    if (each != task && each->parent != task)
      continue;

    for (w = each->windows.next; w != NULL; w = w->next)
    {
      window = wuss__window_from_task_link(w);
      if (!(window->flags & wuss_WINDOW_HIDDEN) ||
          (window->state & wuss_WINDOW_STATE_TASK_HIDDEN))
        return 1;
    }
  }

  return 0;
}

int wuss_task_is_hidden(wuss_t *wuss, wuss_task_t *task)
{
  list_t        *t;
  list_t        *w;
  wuss_task_t   *each;
  wuss_window_t *window;
  int            hidden;

  hidden = 0;
  for (t = wuss->tasks.next; t != NULL; t = t->next)
  {
    each = wuss__task_from_link(t);
    if (each != task && each->parent != task)
      continue;

    for (w = each->windows.next; w != NULL; w = w->next)
    {
      window = wuss__window_from_task_link(w);
      if (window->stack == wuss_STACK_BACK)
        continue;

      if (!(window->flags & wuss_WINDOW_HIDDEN))
        return 0;
      if (window->state & wuss_WINDOW_STATE_TASK_HIDDEN)
        hidden = 1;
    }
  }

  return hidden;
}
