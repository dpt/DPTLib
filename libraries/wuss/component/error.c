/* wuss/component/error.c -- error report window */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "geom/box.h"
#include "geom/stack.h"
#include "utils/array.h"

#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "wuss/component/dialogue.h"
#include "wuss/component/error.h"

#include "../core/types.h"

/* ----------------------------------------------------------------------- */

/* the icon pool entry drawn left of the message, and its size in pixels */
#define ERROR_IMAGE_NAME "error"
#define ERROR_IMAGE_SIZE 34

/* icons, in creation order; an absent Cancel or application icon leaves its
 * slot out and the later ones move up */
enum
{
  ERROR_ICON_IMAGE,
  ERROR_ICON_MESSAGE,
  ERROR_ICON_OK,
  ERROR_ICON_CANCEL,
  ERROR_ICON_APP,
  ERROR_ICON__COUNT
};

/* layout items, in creation order: a VBOX of a row above a row of buttons.
 * The first row has the image and the message and, at its right end, the
 * application's icon. The buttons are pushed to the right, with Cancel before
 * OK. A report without Cancel or an application icon leaves those items out:
 * see error_solve. See docs/windowing/wuss-layout.md */
enum
{
  ERROR_ST_ROOT,
  ERROR_ST_BODY,
  ERROR_ST_IMAGE,
  ERROR_ST_MESSAGE,
  ERROR_ST_BODY_SPACER,
  ERROR_ST_APP,
  ERROR_ST_BUTTONS,
  ERROR_ST_SPACER,
  ERROR_ST_CANCEL,
  ERROR_ST_OK,
  ERROR_ST__LIMIT
};

static const stack_item_t error_layout[ERROR_ST__LIMIT] =
{
  [ERROR_ST_ROOT]        = { .kind = stack_KIND_VBOX, .parent = -1,
                             .gap = wuss_STD_GAP, .pad = wuss_STD_INSETS },
  [ERROR_ST_BODY]        = STACK_HBOX(ERROR_ST_ROOT, ERROR_IMAGE_SIZE,
                                      wuss_STD_GAP, stack_ALIGN_CENTRE),
  [ERROR_ST_IMAGE]       = STACK_LEAF(ERROR_ST_BODY, ERROR_IMAGE_SIZE,
                                      ERROR_IMAGE_SIZE, stack_ALIGN_CENTRE),
  [ERROR_ST_MESSAGE]     = STACK_LEAF(ERROR_ST_BODY, 0, wuss_STD_GLYPH_SIZE,
                                      stack_ALIGN_CENTRE),
  [ERROR_ST_BODY_SPACER] = STACK_SPACER(ERROR_ST_BODY, 1),
  [ERROR_ST_APP]         = STACK_LEAF(ERROR_ST_BODY, 0, 0,
                                      stack_ALIGN_CENTRE),
  [ERROR_ST_BUTTONS]     = STACK_HBOX(ERROR_ST_ROOT,
                                      wuss_STD_PRIMARY_BUTTON_HEIGHT,
                                      wuss_STD_GAP, stack_ALIGN_END),
  [ERROR_ST_SPACER]      = STACK_SPACER(ERROR_ST_BUTTONS, 1),
  [ERROR_ST_CANCEL]      = STACK_LEAF(ERROR_ST_BUTTONS, 0,
                                      wuss_STD_SECONDARY_BUTTON_HEIGHT,
                                      stack_ALIGN_CENTRE),
  [ERROR_ST_OK]          = STACK_LEAF(ERROR_ST_BUTTONS, 0,
                                      wuss_STD_PRIMARY_BUTTON_HEIGHT,
                                      stack_ALIGN_CENTRE)
};

/* one queued report; the strings are owned */
typedef struct
{
  char                   *title;
  char                   *app_icon; /* icon pool entry, or NULL */
  char                   *message;
  wuss_error_buttons_t    buttons;
  wuss_error_choice_fn_t *fn;
  void                   *opaque;
}
error_report_t;

/* The singleton. Built on the first report and dropped, with its window, by
 * the task's wuss_EVENT_QUIT when the wuss instance goes. queue[0] is the
 * report on show, if shown. */
static struct
{
  wuss_t          *wuss;
  wuss_alloc_t     alloc;
  wuss_task_t     *task;
  wuss_dialogue_t *dialogue;
  wuss_icon_t     *icons[ERROR_ICON__COUNT];
  int              nicons;
  int              shown;
  error_report_t  *queue;
  int              used;
  int              allocated;
}
g;

/* ----------------------------------------------------------------------- */

static int error_text_width(const wuss_t *wuss, const char *text)
{
  bmfont_width_t width;

  bmfont_measure(wuss_get_font(wuss), text, (int) strlen(text), NULL,
                 INT_MAX, NULL, &width);

  return width;
}

static int error_button_width(const wuss_t *wuss,
                              const char   *text,
                              int           border)
{
  return error_text_width(wuss, text) + 2 * error_text_width(wuss, " ") +
         2 * border;
}

static char *error_dup(wuss_alloc_t alloc, const char *text)
{
  char  *copy;
  size_t n;

  n    = strlen(text) + 1;
  copy = alloc.malloc(n);
  if (copy != NULL)
    memcpy(copy, text, n);

  return copy;
}

static void error_report_free(error_report_t *report)
{
  g.alloc.free(report->title);
  g.alloc.free(report->app_icon);
  g.alloc.free(report->message);
}

/* ----------------------------------------------------------------------- */

static result_t error_show_head(void);

/* take the report on show off the queue, then put up the next one, or hide.
 * The callback comes last: it may report again */
static result_t error_answer(wuss_error_choice_t choice)
{
  error_report_t done;

  if (g.used == 0)
    return result_OK;

  done = g.queue[0];
  array_delete_element(g.queue, sizeof(*g.queue), g.used, 0);
  g.used--;
  error_report_free(&done);

  /* a next report that cannot be shown is answered Cancel, rather than left
   * to wedge the queue */
  while (g.used > 0 && error_show_head() != result_OK)
  {
    error_report_t failed;

    failed = g.queue[0];
    array_delete_element(g.queue, sizeof(*g.queue), g.used, 0);
    g.used--;
    error_report_free(&failed);
    if (failed.fn != NULL)
      failed.fn(wuss_ERROR_CANCEL, failed.opaque);
  }
  if (g.used == 0)
  {
    (void) wuss_dialogue_hide(g.dialogue);
    g.shown = 0;
  }

  if (done.fn != NULL)
    done.fn(choice, done.opaque);

  return result_OK;
}

static result_t error_action_ok(void *opaque, wuss_button_t button)
{
  (void) opaque;
  (void) button;

  return error_answer(wuss_ERROR_OK);
}

static result_t error_action_cancel(void *opaque, wuss_button_t button)
{
  (void) opaque;
  (void) button;

  return error_answer(wuss_ERROR_CANCEL);
}

/* ----------------------------------------------------------------------- */

/* the application icon's bitmap, or NULL for none */
static const bitmap_t *error_app_bitmap(const wuss_t         *wuss,
                                        const error_report_t *report)
{
  if (report->app_icon == NULL)
    return NULL;

  return wuss_iconpool_bitmap(wuss, wuss_iconpool_lookup(wuss,
                                                         report->app_icon));
}

/* solve the layout for a report: the window size, and a box for every item
 * (empty for those left out) */
static result_t error_solve(const wuss_t         *wuss,
                            const error_report_t *report,
                            size2d_t             *size,
                            box_t                 boxes[ERROR_ST__LIMIT])
{
  result_t        rc;
  stack_item_t    items[ERROR_ST__LIMIT];
  box_t           solved[ERROR_ST__LIMIT];
  int             map[ERROR_ST__LIMIT];
  const bitmap_t *app;
  int             cancel, n, i;
  stack_item_t    item;
  box_t           root;

  app    = error_app_bitmap(wuss, report);
  cancel = report->buttons == wuss_ERROR_BUTTONS_OK_CANCEL;

  /* leave out what the report does not have, renumbering the parents */
  n = 0;
  for (i = 0; i < ERROR_ST__LIMIT; i++)
  {
    map[i] = -1;
    if ((app == NULL && (i == ERROR_ST_BODY_SPACER || i == ERROR_ST_APP)) ||
        (!cancel && i == ERROR_ST_CANCEL))
      continue;

    item = error_layout[i];
    if (item.parent >= 0)
      item.parent = map[item.parent];
    map[i]   = n;
    items[n] = item;
    n++;
  }

  items[map[ERROR_ST_MESSAGE]].axis_size =
    error_text_width(wuss, report->message);
  items[map[ERROR_ST_OK]].axis_size =
    error_button_width(wuss, "OK", wuss_STD_PRIMARY_BUTTON_BORDER);
  if (cancel)
    items[map[ERROR_ST_CANCEL]].axis_size =
      error_button_width(wuss, "Cancel", wuss_STD_SECONDARY_BUTTON_BORDER);
  if (app != NULL)
  {
    items[map[ERROR_ST_APP]].axis_size  = app->size.w;
    items[map[ERROR_ST_APP]].cross_size = app->size.h;
    items[map[ERROR_ST_BODY]].axis_size = MAX(ERROR_IMAGE_SIZE, app->size.h);
  }

  rc = stack_smallest(items, n, size);
  if (rc != result_OK)
    return rc;

  root = (box_t) BOX_POS_SIZE(0, 0, size->w, size->h);
  rc   = stack_solve(items, n, &root, solved);
  if (rc != result_OK)
    return rc;

  for (i = 0; i < ERROR_ST__LIMIT; i++)
    boxes[i] = map[i] >= 0 ? solved[map[i]] : (box_t) BOX_POS_SIZE(0, 0, 0, 0);

  return result_OK;
}

/* make the icons for boxes, replacing any already on the window */
static result_t error_build_icons(const error_report_t *report,
                                  const box_t           boxes[ERROR_ST__LIMIT])
{
  result_t               rc;
  wuss_icon_spec_t       specs[ERROR_ICON__COUNT];
  wuss_icon_t           *icons[ERROR_ICON__COUNT];
  wuss_dialogue_action_t actions[2];
  wuss_window_t         *win;
  int                    cancel, n, ok, cancel_icon, i;

  cancel = report->buttons == wuss_ERROR_BUTTONS_OK_CANCEL;
  win    = wuss_dialogue_window(g.dialogue);

  n = 0;
  memset(&specs[n], 0, sizeof(specs[n]));
  specs[n].bbox          = boxes[ERROR_ST_IMAGE];
  specs[n].type          = wuss_ICON_TYPE_BITMAP;
  specs[n].u.bitmap.name = ERROR_IMAGE_NAME;
  n++;

  wuss_icon_spec_label(&specs[n++], boxes[ERROR_ST_MESSAGE], report->message,
                       0);

  ok = n;
  wuss_icon_spec_action(&specs[n++], boxes[ERROR_ST_OK], "OK", 1);

  cancel_icon = ok;
  if (cancel)
  {
    cancel_icon = n;
    wuss_icon_spec_action(&specs[n++], boxes[ERROR_ST_CANCEL], "Cancel", 0);
  }

  if (error_app_bitmap(g.wuss, report) != NULL)
  {
    memset(&specs[n], 0, sizeof(specs[n]));
    specs[n].bbox          = boxes[ERROR_ST_APP];
    specs[n].type          = wuss_ICON_TYPE_BITMAP;
    specs[n].u.bitmap.name = report->app_icon;
    n++;
  }

  rc = wuss_icon_create_array(win, specs, n, icons);
  if (rc != result_OK)
    return rc;

  for (i = 0; i < g.nicons; i++)
    wuss_icon_delete(win, g.icons[i]);
  memcpy(g.icons, icons, sizeof(*icons) * (size_t) n);
  g.nicons = n;

  actions[0].icon = icons[ok];
  actions[0].fn   = error_action_ok;
  if (cancel)
  {
    actions[1].icon = icons[cancel_icon];
    actions[1].fn   = error_action_cancel;
  }

  /* with nothing to cancel, ESCAPE dismisses with OK */
  wuss_dialogue_set_cancel(g.dialogue, icons[cancel_icon]);

  return wuss_dialogue_set_actions(g.dialogue, actions, cancel ? 2 : 1);
}

/* put queue[0] on the window, which is then shown if it was not. A window
 * already up is resized about its top-left, not moved */
static result_t error_show_head(void)
{
  result_t              rc;
  const error_report_t *report;
  wuss_window_t        *win;
  size2d_t              size, scr;
  box_t                 boxes[ERROR_ST__LIMIT];

  report = &g.queue[0];
  win    = wuss_dialogue_window(g.dialogue);

  rc = error_solve(g.wuss, report, &size, boxes);
  if (rc != result_OK)
    return rc;

  wuss_window_set_title(win, report->title);

  rc = wuss_window_resize(win, size);
  if (rc != result_OK)
    return rc;

  rc = wuss_window_set_doc(win, size);
  if (rc != result_OK)
    return rc;

  rc = error_build_icons(report, boxes);
  if (rc != result_OK)
    return rc;

  if (!g.shown)
  {
    scr = wuss_get_screen_size(g.wuss);
    wuss_window_move(win, POINT((scr.w - size.w) / 2, (scr.h - size.h) / 2));

    rc = wuss_window_set_hidden(win, 0);
    if (rc != result_OK)
      return rc;

    g.shown = 1;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

static result_t error_task_handle(wuss_window_t      *window,
                                  const wuss_event_t *event,
                                  void               *task_data)
{
  result_t rc;
  int      i;

  (void) window;
  (void) task_data;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    rc = result_OK;
    (void) wuss_dialogue_handle_icon(g.dialogue, event, &rc);
    return rc;

  case wuss_EVENT_KEY:
    rc = result_WUSS_KEY_UNCLAIMED;
    (void) wuss_dialogue_handle_key(g.dialogue, event, &rc);
    return rc;

  case wuss_EVENT_QUIT:
    /* the wuss instance is going: reports still waiting are not answered */
    for (i = 0; i < g.used; i++)
      error_report_free(&g.queue[i]);
    free(g.queue);
    wuss_dialogue_destroy(g.dialogue);
    memset(&g, 0, sizeof(g));
    return result_OK;

  default:
    return result_OK;
  }
}

/* build the window and its task, empty and hidden */
static result_t error_create(wuss_t *wuss)
{
  result_t         rc;
  wuss_task_desc_t desc;
  size2d_t         size;

  g.wuss  = wuss;
  g.alloc = wuss->alloc;

  desc.handle    = error_task_handle;
  desc.task_data = NULL;
  desc.name      = "error";
  rc = wuss_task_create(wuss, &desc, &g.task);
  if (rc != result_OK)
    return rc;

  size = SIZE2D(1, 1);
  rc = wuss_dialogue_create(&g.dialogue, g.task, size, "Error", NULL, NULL);
  if (rc != result_OK)
    wuss_task_destroy(g.task);

  return rc;
}

result_t wuss_error_report(wuss_t                 *wuss,
                           const char             *app_icon,
                           const char             *title,
                           const char             *message,
                           wuss_error_buttons_t    buttons,
                           wuss_error_choice_fn_t *fn,
                           void                   *opaque)
{
  result_t       rc;
  error_report_t report;
  int            created;

  if (wuss == NULL || message == NULL)
    return result_NULL_ARG;

  created = 0;
  if (g.wuss == NULL)
  {
    rc = error_create(wuss);
    if (rc != result_OK)
    {
      memset(&g, 0, sizeof(g));
      return rc;
    }
    created = 1;
  }
  else if (g.wuss != wuss)
  {
    return result_BAD_ARG;
  }

  report.title   = error_dup(g.alloc, title != NULL ? title : "Error");
  report.message = error_dup(g.alloc, message);
  report.app_icon = app_icon != NULL ? error_dup(g.alloc, app_icon) : NULL;
  report.buttons = buttons;
  report.fn      = fn;
  report.opaque  = opaque;
  rc = result_OOM;
  if (report.title == NULL || report.message == NULL ||
      (app_icon != NULL && report.app_icon == NULL))
    goto failure;

  if (array_grow((void **) &g.queue, sizeof(*g.queue), g.used, &g.allocated,
                 1, 4))
    goto failure;

  g.queue[g.used++] = report;

  /* something already on show leaves this one waiting */
  if (g.used == 1)
  {
    rc = error_show_head();
    if (rc != result_OK)
    {
      g.used--;
      goto failure;
    }
  }

  /* a queued report is no less urgent: bring the window up for it too */
  wuss_window_restack(wuss_dialogue_window(g.dialogue), wuss_ZORDER_FRONT);

  return result_OK;

failure:
  error_report_free(&report);
  if (created && g.used == 0)
  {
    wuss_task_destroy(g.task); /* its QUIT drops the rest of g */
  }

  return rc;
}

int wuss_error_pending(const wuss_t *wuss)
{
  if (wuss == NULL || g.wuss != wuss)
    return 0;

  return g.used;
}
