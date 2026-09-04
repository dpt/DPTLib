/* wuss/task/create.c -- register a task on a window manager */

#include <assert.h>
#include <stddef.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "wuss/task.h"

#include "../impl.h"

result_t wuss_task_create(wuss_t                 *wuss,
                          const wuss_task_desc_t *desc,
                          wuss_task_t           **task)
{
  wuss_task_t *t;

  assert(wuss != NULL);
  assert(desc != NULL);
  assert(task != NULL);

  t = wuss__malloc(wuss, sizeof(*t));
  if (t == NULL)
    return result_OOM;

  t->wuss      = wuss;
  t->handle    = desc->handle;
  t->task_data = desc->task_data;
  t->name      = desc->name;
  t->flags     = 0;
  list_init(&t->windows);

  list_add_to_tail(&wuss->tasks, &t->link);

  *task = t;

  return result_OK;
}
