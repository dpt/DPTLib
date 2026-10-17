/* wuss/task/arrange.c -- hide, show, raise and cycle apps and their windows */

#include <stddef.h>

#include "wuss/task.h"
#include "wuss/window.h"

#include "../impl.h"

/* The task that heads t's app: its parent, or itself. */
static wuss_task_t *task_root(const wuss_task_t *t)
{
  return t->parent != NULL ? t->parent : (wuss_task_t *) t;
}

wuss_task_t *wuss_task_get_root(const wuss_task_t *task)
{
  return task_root(task);
}

/* A window the cycling keys can land on: a visible, focusable regular
 * window. */
static int window_cycles(const wuss_window_t *window)
{
  return window->stack == wuss_STACK_MIDDLE &&
         !(window->flags & wuss_WINDOW_HIDDEN) &&
         (window->flags & wuss_WINDOW_FOCUSABLE);
}

/* The frontmost cycling window of the app headed by root, or of any app when
 * root is NULL, skipping the first skip matches. */
static wuss_window_t *front_window(wuss_t            *wuss,
                                   const wuss_task_t *root,
                                   int                skip)
{
  list_t        *l;
  wuss_window_t *window;

  for (l = wuss->z_order[wuss_STACK_MIDDLE].next; l != NULL; l = l->next)
  {
    window = wuss__window_from_link(l);
    if (!window_cycles(window))
      continue;

    if (root != NULL && task_root(window->task) != root)
      continue;

    if (skip-- == 0)
      return window;
  }

  return NULL;
}

/* The rearmost cycling window of the app headed by root (or of any app when
 * root is NULL), leaving out the app headed by except. */
static wuss_window_t *back_window(wuss_t            *wuss,
                                  const wuss_task_t *root,
                                  const wuss_task_t *except)
{
  list_t        *l;
  wuss_window_t *window;
  wuss_window_t *found;

  found = NULL;
  for (l = wuss->z_order[wuss_STACK_MIDDLE].next; l != NULL; l = l->next)
  {
    window = wuss__window_from_link(l);
    if (!window_cycles(window) || task_root(window->task) == except)
      continue;

    if (root == NULL || task_root(window->task) == root)
      found = window;
  }

  return found;
}

/* Focus the frontmost cycling window if nothing holds the focus. */
static void focus_if_none(wuss_t *wuss)
{
  wuss_window_t *window;

  if (wuss->focus != NULL)
    return;

  window = front_window(wuss, NULL, 0);
  if (window != NULL)
    (void) wuss_set_focus(wuss, window);
}

/* Raise, back to front, every window from node l onward that belongs to the
 * app headed by root, so their order among themselves is kept. */
static void raise_from(list_t *l, const wuss_task_t *root)
{
  list_t        *next;
  wuss_window_t *window;

  if (l == NULL)
    return;

  next = l->next;
  raise_from(next, root);

  window = wuss__window_from_link(l);
  if (window->stack == wuss_STACK_MIDDLE && task_root(window->task) == root)
    wuss_window_restack(window, wuss_ZORDER_FRONT);
}

int wuss_task_front(wuss_t *wuss, wuss_task_t *task)
{
  const wuss_task_t *root;
  wuss_window_t     *window;

  root = task_root(task);
  raise_from(wuss->z_order[wuss_STACK_MIDDLE].next, root);

  window = front_window(wuss, root, 0);
  if (window == NULL)
    return 0;

  (void) wuss_set_focus(wuss, window);

  return 1;
}

int wuss_task_cycle_windows(wuss_t *wuss, wuss_task_t *task, int step)
{
  const wuss_task_t *root;
  wuss_window_t     *window;
  wuss_window_t     *second;

  root   = task_root(task);
  second = front_window(wuss, root, 1);
  if (second == NULL)
    return 0; /* fewer than two windows to cycle */

  if (step > 0)
  {
    window = back_window(wuss, root, NULL);
  }
  else
  {
    wuss_window_restack(front_window(wuss, root, 0), wuss_ZORDER_BACK);
    window = second;
  }

  wuss_window_restack(window, wuss_ZORDER_FRONT);
  (void) wuss_set_focus(wuss, window);

  return 1;
}

int wuss_cycle_tasks(wuss_t *wuss, int step)
{
  const wuss_task_t *current;
  wuss_window_t     *window;
  list_t            *l;
  int                n;

  window = front_window(wuss, NULL, 0);
  if (window == NULL)
    return 0;

  current = task_root(window->task);

  if (step > 0)
  {
    window = back_window(wuss, NULL, current);
    if (window == NULL)
      return 0; /* only one app */

    return wuss_task_front(wuss, window->task);
  }

  /* previous: send the front app behind the rest, then raise the new front */
  n = 0;
  for (l = wuss->z_order[wuss_STACK_MIDDLE].next; l != NULL; l = l->next)
    if (window_cycles(wuss__window_from_link(l)) &&
        task_root(wuss__window_from_link(l)->task) != current)
      n++;

  if (n == 0)
    return 0;

  n = 0;
  for (l = wuss->z_order[wuss_STACK_MIDDLE].next; l != NULL; l = l->next)
    n += window_cycles(wuss__window_from_link(l)) &&
         task_root(wuss__window_from_link(l)->task) == current;

  /* ponytail: restacking moves a window to the tail, where the walk would
   * meet it again, so the walk stops after n moves */
  for (l = wuss->z_order[wuss_STACK_MIDDLE].next; l != NULL && n > 0; )
  {
    window = wuss__window_from_link(l);
    l      = l->next;
    if (window_cycles(window) && task_root(window->task) == current)
    {
      wuss_window_restack(window, wuss_ZORDER_BACK);
      n--;
    }
  }

  window = front_window(wuss, NULL, 0);

  return wuss_task_front(wuss, window->task);
}

result_t wuss_task_hide_app(wuss_t *wuss, wuss_task_t *task)
{
  result_t rc;

  rc = wuss_task_set_hidden(wuss, task_root(task), 1);
  focus_if_none(wuss);

  return rc;
}

/* Whether the app headed by root has a window the hide keys would act on. */
static int app_has_window(wuss_t *wuss, const wuss_task_t *root)
{
  list_t        *t;
  list_t        *w;
  wuss_task_t   *each;
  wuss_window_t *window;

  for (t = wuss->tasks.next; t != NULL; t = t->next)
  {
    each = wuss__task_from_link(t);
    if (task_root(each) != root)
      continue;

    for (w = each->windows.next; w != NULL; w = w->next)
    {
      window = wuss__window_from_task_link(w);
      if (window->stack != wuss_STACK_BACK && !(window->flags & wuss_WINDOW_HIDDEN))
        return 1;
    }
  }

  return 0;
}

result_t wuss_task_hide_others(wuss_t *wuss, wuss_task_t *task)
{
  result_t           rc;
  result_t           first;
  list_t            *t;
  wuss_task_t       *each;
  const wuss_task_t *keep;

  keep  = task_root(task);
  first = result_OK;
  for (t = wuss->tasks.next; t != NULL; t = t->next)
  {
    each = wuss__task_from_link(t);
    if (task_root(each) == keep || !app_has_window(wuss, task_root(each)))
      continue;

    rc = wuss_task_set_hidden(wuss, each, 1);
    if (first == result_OK)
      first = rc;
  }

  return first;
}

result_t wuss_task_show_all(wuss_t *wuss)
{
  result_t rc;
  result_t first;
  list_t  *t;

  first = result_OK;
  for (t = wuss->tasks.next; t != NULL; t = t->next)
  {
    rc = wuss_task_set_hidden(wuss, wuss__task_from_link(t), 0);
    if (first == result_OK)
      first = rc;
  }

  focus_if_none(wuss);

  return first;
}
