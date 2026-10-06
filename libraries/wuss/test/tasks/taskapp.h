/* wuss/test/tasks/taskapp.h -- icon bar app wrapper for the demo tasks */

#ifndef TASKS_TASKAPP_H
#define TASKS_TASKAPP_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "base/result.h"
#include "wuss/message.h"
#include "wuss/wuss.h"

/* Every demo task's X_create shares one shape: (wuss_t *, X_task_t **). The
 * task block it allocates is owned by its window and freed by X_destroy from
 * the task's wuss_EVENT_QUIT handler, so an app only ever calls X_create with
 * NULL for the block, which is type-compatible however the second parameter
 * is spelt. */
typedef result_t (*taskapp_create_fn_t)(wuss_t *wuss, void **out);

/* Load the file at path into an instance create has just returned. Optional
 * in a taskapp_desc: with none, a file dropped on the app's icon is not
 * acknowledged. */
typedef result_t (*taskapp_data_load_fn_t)(void *instance, const char *path);

/* A task's description of its app. Each task keeps one as a static const and
 * passes it to taskapp_install from its X_app_create. */
typedef struct taskapp_desc
{
  const char          *name;    /* icon label, menu title, proginfo name */
  const char          *purpose; /* proginfo "Purpose" row */
  taskapp_create_fn_t  create;  /* opens an instance of the main window */
  taskapp_data_load_fn_t data_load; /* optional: file dropped on the icon */
  bool                 drop_only;   /* SELECT on the icon opens nothing */
}
taskapp_desc_t;

/* Install the app for desc: a task with an icon bar icon (labelled name,
 * drawn with the application glyph) and a menu of Info and Quit. SELECT on
 * the icon calls desc->create for another instance of the main window, each
 * instance a child task of the app's, so no window opens until asked for.
 * A wuss_MESSAGE_DATA_LOAD sent to the icon (a file dropped on it) opens an
 * instance the same way and passes it the path through desc->data_load; it
 * is acknowledged only if that returns result_OK. With desc->drop_only,
 * SELECT does nothing, so a drop is the only way to open an instance.
 * Does nothing, returning result_OK, if desc is already installed. Without
 * the icon bar, calls desc->create at once instead. */
result_t taskapp_install(wuss_t *wuss, const taskapp_desc_t *desc);

/* A DataLoad on an instance's window: load the file it names through load
 * and acknowledge it. A file that will not load is not acknowledged, so the
 * send bounces to the sender. Messages of any other action are ignored. Call
 * it from the instance's wuss_EVENT_MESSAGE handler, after checking the
 * message is for the instance's main window. */
result_t taskapp_window_data_load(wuss_t                *wuss,
                                  wuss_task_t           *delegate,
                                  void                  *instance,
                                  taskapp_data_load_fn_t load,
                                  const wuss_message_t  *msg);

/* Destroy every app whose Quit was picked, with its instances. Call it after
 * wuss_idle, never from inside it: destroying an app takes its child tasks
 * with it, so it cannot happen while wuss_idle walks the task list. */
void taskapp_reap(void);

#endif /* WUSS_APP */

#endif /* TASKS_TASKAPP_H */
