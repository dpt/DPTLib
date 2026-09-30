/* wuss/component/saveas.c -- RISC OS-style drag-and-drop Save As dialogue */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "geom/box.h"
#include "io/filetype.h"
#include "io/path.h"

#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/message.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "wuss/component/dialogue.h"
#include "wuss/component/saveas.h"

#include "../core/impl.h"
#include "../icon.h"

/* ----------------------------------------------------------------------- */

#define SAVEAS_ICON_W       34
#define SAVEAS_ICON_H       34
#define SAVEAS_BUTTON_W     64
#define SAVEAS_ROW_H        24
#define SAVEAS_SAVE_GROW    8  /* extra height for wuss_ICON_BORDER_ACTION */

/* The dialogue is square, sized by the two buttons side by side; the
 * writable spans the same width and the rows are spread evenly down it. */
#define SAVEAS_SIDE         (wuss_STD_INSET * 3 + SAVEAS_BUTTON_W * 2)
#define SAVEAS_LEAF_W       (SAVEAS_SIDE - wuss_STD_INSET * 2)
#define SAVEAS_GAP          ((SAVEAS_SIDE - SAVEAS_ICON_H - SAVEAS_ROW_H * 2 - \
                              SAVEAS_SAVE_GROW) / 4)
#define SAVEAS_LEAF_SIZE    DPTLIB_MAXPATH

/* The struct is opaque outside this file, unlike wuss_dialogue/wuss_info --
 * no other component composes with it, so nothing needs its layout. */
struct wuss_saveas
{
  wuss_alloc_t            alloc;   /* copied hooks; wuss_t itself not retained */
  wuss_t                 *wuss;
  wuss_task_t             *task;
  wuss_dialogue_t         *dialogue;
  wuss_icon_t             *drag_icon;
  wuss_icon_t             *leaf_icon;
  wuss_icon_t             *cancel_icon;
  wuss_icon_t             *save_icon;
  filetype_t               filetype;
  wuss_saveas_save_fn_t   *save_fn;
  void                    *opaque;

  /* Transfer state; transfer_active gates a fresh drag from starting a
   * second one and gates a stray reply after a bounce. save_target is the
   * window the DataSave was addressed to -- the DataLoad that follows must
   * go back to the same window, since a wuss_task_t has no single "the"
   * window to derive it from. */
  int                      transfer_active;
  int                      keep_open; /* set from the drag's button: an
                                       * ADJUST drag-save leaves the dialogue
                                       * open for a repeat save, a SELECT one
                                       * closes it once the transfer completes */
  wuss_window_t           *save_target;
  unsigned int             save_ref;
  unsigned int             load_ref;
};

/* ----------------------------------------------------------------------- */

/* Dismiss the dialogue: shown as a menu leaf, close the whole chain (as a
 * RISC OS save box does on completion); standalone, just hide it. */
static void saveas_dismiss(wuss_saveas_t *sa)
{
  if (wuss__menu_contains(sa->wuss, wuss_dialogue_window(sa->dialogue)))
    wuss__menu_abandon(sa->wuss);
  else
    (void) wuss_dialogue_hide(sa->dialogue);
}

static result_t saveas_do_save(wuss_saveas_t *sa)
{
  const char *path;

  path = wuss_icon_get_text(sa->leaf_icon);
  if (!path_is_full(path))
    return result_OK;

  return sa->save_fn(path, sa->opaque);
}

static result_t saveas_action_cancel(void *opaque, wuss_button_t button)
{
  wuss_saveas_t *sa;

  sa = opaque;
  sa->transfer_active = 0;

  /* ADJUST cancels any transfer but leaves the dialogue open, as on RISC OS */
  if (!(button & wuss_BUTTON_ADJUST))
    saveas_dismiss(sa);

  return result_OK;
}

static result_t saveas_action_save(void *opaque, wuss_button_t button)
{
  result_t       rc;
  wuss_saveas_t *sa;

  (void) button;
  sa = opaque;
  rc = saveas_do_save(sa);
  if (rc == result_OK)
    saveas_dismiss(sa);

  return rc;
}

static void saveas_set_actions(wuss_saveas_t *sa)
{
  wuss_dialogue_action_t actions[2];

  actions[0].icon = sa->cancel_icon;
  actions[0].fn   = saveas_action_cancel;
  actions[1].icon = sa->save_icon;
  actions[1].fn   = saveas_action_save;

  (void) wuss_dialogue_set_actions(sa->dialogue, actions, 2);
}

/* ----------------------------------------------------------------------- */

/* Icon-set entry for the file icon: "file_<type>" (RISC OS sprite naming,
 * three lowercase hex digits), else the generic "file_xxx". Returns a
 * wuss_ICON_SET-encoded value, or 0 if neither is loaded. */
static int saveas_file_icon(const wuss_t *wuss, filetype_t filetype)
{
  char name[16];
  int  idx;

  snprintf(name, sizeof(name), "file_%03x", filetype.riscos & 0xFFFu);
  idx = wuss_icons_lookup(wuss, name);
  if (idx < 0)
    idx = wuss_icons_lookup(wuss, "file_xxx");

  return idx >= 0 ? wuss_ICON_SET(idx) : 0;
}

static result_t saveas_build_icons(wuss_saveas_t *sa, const char *leafname)
{
  result_t         rc;
  wuss_window_t   *window;
  int              save_y;
  wuss_icon_spec_t specs[4];
  wuss_icon_t     *icons[4];

  window = wuss_dialogue_window(sa->dialogue);
  save_y = SAVEAS_GAP * 3 + SAVEAS_ICON_H + SAVEAS_ROW_H;
  memset(specs, 0, sizeof(specs));

  specs[0].bbox = (box_t) BOX_POS_SIZE((SAVEAS_SIDE - SAVEAS_ICON_W) / 2,
                                       SAVEAS_GAP,
                                       SAVEAS_ICON_W, SAVEAS_ICON_H);
  specs[0].type         = wuss_ICON_TYPE_DRAGGABLE;
  specs[0].u.bitmap.set = saveas_file_icon(sa->wuss, sa->filetype);

  wuss_icon_spec_writable(&specs[1],
                          (box_t) BOX_POS_SIZE(wuss_STD_INSET,
                                               SAVEAS_GAP * 2 + SAVEAS_ICON_H,
                                               SAVEAS_LEAF_W, SAVEAS_ROW_H),
                          leafname, SAVEAS_LEAF_SIZE, 0);

  wuss_icon_spec_action(&specs[2],
                        (box_t) BOX_POS_SIZE(wuss_STD_INSET,
                                             save_y + SAVEAS_SAVE_GROW / 2,
                                             SAVEAS_BUTTON_W, SAVEAS_ROW_H),
                        "Cancel", 0);

  wuss_icon_spec_action(&specs[3],
                        (box_t) BOX_POS_SIZE(wuss_STD_INSET * 2 +
                                             SAVEAS_BUTTON_W,
                                             save_y,
                                             SAVEAS_BUTTON_W,
                                             SAVEAS_ROW_H + SAVEAS_SAVE_GROW),
                        "Save", 1);

  rc = wuss_icon_create_array(window, specs, 4, icons);
  if (rc != result_OK)
    return rc;

  sa->drag_icon   = icons[0];
  sa->leaf_icon   = icons[1];
  sa->cancel_icon = icons[2];
  sa->save_icon   = icons[3];

  return result_OK;
}

result_t wuss_saveas_create(wuss_saveas_t        **out,
                            wuss_task_t           *task,
                            const filetype_t      *filetype,
                            const char            *leafname,
                            wuss_saveas_save_fn_t *save_fn,
                            void                  *opaque)
{
  result_t       rc;
  wuss_saveas_t *sa;
  size2d_t       size;

  if (out == NULL || task == NULL || save_fn == NULL)
    return result_NULL_ARG;

  sa = task->wuss->alloc.malloc(sizeof(*sa));
  if (sa == NULL)
    return result_OOM;

  sa->alloc           = task->wuss->alloc;
  sa->wuss            = task->wuss;
  sa->task            = task;
  sa->filetype        = filetype != NULL ? *filetype : filetype_DATA;
  sa->save_fn         = save_fn;
  sa->opaque          = opaque;
  sa->transfer_active = 0;
  sa->save_target     = NULL;

  size = SIZE2D(SAVEAS_SIDE, SAVEAS_SIDE);

  rc = wuss_dialogue_create(&sa->dialogue, task, size, "Save As", NULL, sa);
  if (rc != result_OK)
  {
    sa->alloc.free(sa);
    return rc;
  }

  rc = saveas_build_icons(sa, leafname);
  if (rc != result_OK)
  {
    wuss_dialogue_destroy(sa->dialogue);
    sa->alloc.free(sa);
    return rc;
  }

  saveas_set_actions(sa);

  *out = sa;
  return result_OK;
}

void wuss_saveas_destroy(wuss_saveas_t *doomed)
{
  wuss_alloc_t alloc;

  if (doomed == NULL)
    return;

  alloc = doomed->alloc;
  wuss_dialogue_destroy(doomed->dialogue); /* frees the window and its icons */
  alloc.free(doomed);
}

/* ----------------------------------------------------------------------- */

void wuss_saveas_set_leafname(wuss_saveas_t *saveas, const char *leafname)
{
  if (saveas->transfer_active)
    return;

  (void) wuss_icon_set_text(wuss_dialogue_window(saveas->dialogue),
                            saveas->leaf_icon, leafname);
}

void wuss_saveas_set_filetype(wuss_saveas_t    *saveas,
                              const filetype_t *filetype)
{
  wuss_icon_t *icon;
  int          set;

  saveas->filetype = filetype != NULL ? *filetype : filetype_DATA;

  icon = saveas->drag_icon;
  set  = saveas_file_icon(saveas->wuss, saveas->filetype);
  if (set == icon->spec.u.bitmap.set)
    return;

  icon->spec.u.bitmap.set   = set;
  icon->spec.u.bitmap.image = wuss_icons_bitmap(saveas->wuss, set - 1);
  wuss__icon_invalidate(wuss_dialogue_window(saveas->dialogue), icon);
}

const char *wuss_saveas_get_path(const wuss_saveas_t *saveas)
{
  return wuss_icon_get_text(saveas->leaf_icon);
}

wuss_window_t *wuss_saveas_window(const wuss_saveas_t *saveas)
{
  return saveas ? wuss_dialogue_window(saveas->dialogue) : NULL;
}

result_t wuss_saveas_open(wuss_saveas_t *saveas)
{
  wuss_window_t *win;
  point_t        at;

  win = wuss_dialogue_window(saveas->dialogue);

  /* centred on the pointer, as a RISC OS Save As opened from a key press */
  at    = wuss_get_pointer(saveas->wuss);
  at.x -= SAVEAS_SIDE / 2;
  at.y -= SAVEAS_SIDE / 2;

  return wuss_menu_open_window(saveas->task, win, at, NULL);
}

/* ----------------------------------------------------------------------- */

static int saveas_handle_drag_end(wuss_saveas_t      *sa,
                                  const wuss_event_t *event)
{
  wuss_data_save_t payload;
  const char      *leafname;

  if (sa->transfer_active)
    return 1; /* a transfer is already running: ignore this drag */
  if (event->data.drag_end.cancelled || event->data.drag_end.drop == NULL)
    return 1;

  leafname = wuss_icon_get_text(sa->leaf_icon);
  memset(&payload, 0, sizeof(payload));
  payload.filetype     = sa->filetype;
  payload.reply_window = wuss_dialogue_window(sa->dialogue);
  strncpy(payload.leafname, leafname, sizeof(payload.leafname) - 1);

  if (wuss_send_recorded(sa->wuss, sa->task, event->data.drag_end.drop,
                         wuss_MESSAGE_DATA_SAVE, &payload, sizeof(payload),
                         0, &sa->save_ref) == result_OK)
  {
    sa->transfer_active = 1;
    sa->save_target     = event->data.drag_end.drop;
    sa->keep_open       = (event->data.drag_end.button &
                           wuss_BUTTON_ADJUST) != 0;
  }

  return 1;
}

/* wuss_EVENT_MESSAGE: a DataSaveAck (path to write to) or DataLoadAck
 * (transfer complete), or a broadcast this dialogue has no interest in. */
static int saveas_handle_message(wuss_saveas_t      *sa,
                                 const wuss_event_t *event)
{
  const wuss_message_t *msg;

  msg = event->data.message;

  if (!sa->transfer_active)
    return 0;

  if (msg->action == wuss_MESSAGE_DATA_SAVE_ACK &&
     msg->your_ref == sa->save_ref)
  {
    char   path[DPTLIB_MAXPATH];
    size_t len;

    len = MIN(msg->size, sizeof(path) - 1);
    strncpy(path, (const char *) msg->data, len);
    path[len] = '\0';

    if (sa->save_fn(path, sa->opaque) != result_OK)
    {
      sa->transfer_active = 0;
      return 1;
    }

    (void) wuss_icon_set_text(wuss_dialogue_window(sa->dialogue),
                              sa->leaf_icon, path);
    (void) wuss_send_recorded(sa->wuss, sa->task, sa->save_target,
                              wuss_MESSAGE_DATA_LOAD, path,
                              strlen(path) + 1, 0, &sa->load_ref);

    return 1;
  }

  if (msg->action == wuss_MESSAGE_DATA_LOAD_ACK &&
     msg->your_ref == sa->load_ref)
  {
    sa->transfer_active = 0;
    if (!sa->keep_open)
      saveas_dismiss(sa);

    return 1;
  }

  return 0;
}

/* wuss_EVENT_MESSAGE_BOUNCED: silently abandon whichever half of the
 * transfer failed to reach its target. */
static int saveas_handle_bounced(wuss_saveas_t      *sa,
                                 const wuss_event_t *event)
{
  const wuss_message_t *msg;

  msg = event->data.message;

  if (!sa->transfer_active)
    return 0;
  if (msg->my_ref != sa->save_ref && msg->my_ref != sa->load_ref)
    return 0;

  sa->transfer_active = 0;

  return 1;
}

int wuss_saveas_handle_event(wuss_saveas_t      *saveas,
                             wuss_window_t      *window,
                             const wuss_event_t *event)
{
  result_t action_result;

  if (saveas == NULL || event == NULL)
    return 0;

  /* wuss_EVENT_MESSAGE_BOUNCED is always task-view (window == NULL); every
   * other kind this component handles is delivered to its own window. */
  if (event->kind != wuss_EVENT_MESSAGE_BOUNCED &&
     window != wuss_dialogue_window(saveas->dialogue))
    return 0;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    return wuss_dialogue_handle_icon(saveas->dialogue, event, &action_result);

  case wuss_EVENT_DRAG_END:
    return saveas_handle_drag_end(saveas, event);

  case wuss_EVENT_MESSAGE:
    return saveas_handle_message(saveas, event);

  case wuss_EVENT_MESSAGE_BOUNCED:
    return saveas_handle_bounced(saveas, event);

  default:
    return 0;
  }
}
