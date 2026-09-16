/* wuss/component/dialogue.c -- interactive dialogue base class */

#include <stddef.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "geom/box.h"

#include "wuss/task.h"
#include "wuss/window.h"

#include "wuss/component/dialogue.h"

#include "../core/impl.h"

/* ----------------------------------------------------------------------- */

/* The window is created hidden, without wuss_WINDOW_CLOSE, for the same
 * reason as wuss/component/info.c: it may be borrowed as a
 * wuss_menu_item_t::window, and the caller (not this component) decides when
 * a Cancel/OK click dismisses it.
 *
 * owns_window is 0 for a dialogue built via wuss_dialogue_create_on_window
 * (e.g. composed with a wuss_info_t, which owns and resizes the window
 * itself): wuss_dialogue_destroy must then leave the window alone, for the
 * other component's own destroy call to close. */
struct wuss_dialogue
{
  wuss_alloc_t                alloc;   /* copied hooks; wuss_t itself not retained */
  wuss_window_t              *window;
  int                         owns_window;
  wuss_dialogue_fillout_fn_t *fillout;
  void                       *opaque;
  wuss_dialogue_action_t      actions[WUSS_DIALOGUE_MAX_ACTIONS];
  int                         nactions;
};

/* ----------------------------------------------------------------------- */

static wuss_dialogue_t *dialogue_alloc(wuss_alloc_t                alloc,
                                       wuss_dialogue_fillout_fn_t *fillout,
                                       void                       *opaque)
{
  wuss_dialogue_t *dialogue;

  dialogue = alloc.malloc(sizeof(*dialogue));
  if (dialogue == NULL)
    return NULL;

  dialogue->alloc    = alloc;
  dialogue->window   = NULL;
  dialogue->fillout  = fillout;
  dialogue->opaque   = opaque;
  dialogue->nactions = 0;

  return dialogue;
}

result_t wuss_dialogue_create(wuss_dialogue_t           **out,
                              wuss_task_t                *task,
                              size2d_t                    size,
                              const char                 *title,
                              wuss_dialogue_fillout_fn_t *fillout,
                              void                       *opaque)
{
  result_t         rc;
  wuss_dialogue_t *dialogue;
  box_t            content;

  if (out == NULL || task == NULL)
    return result_NULL_ARG;

  dialogue = dialogue_alloc(task->wuss->alloc, fillout, opaque);
  if (dialogue == NULL)
    return result_OOM;
  dialogue->owns_window = 1;

  content = (box_t) BOX_POS_SIZE(0, 0, size.w, size.h);
  rc = wuss_window_create(task,
                          &content,
                          title,
                          wuss_WINDOW_HIDDEN | wuss_WINDOW_NO_REDRAW,
                          wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                          size,
                          size,
                          &dialogue->window);
  if (rc != result_OK)
  {
    dialogue->alloc.free(dialogue);
    return rc;
  }

  *out = dialogue;
  return result_OK;
}

result_t wuss_dialogue_create_on_window(wuss_dialogue_t           **out,
                                        wuss_t                     *wuss,
                                        wuss_window_t              *window,
                                        wuss_dialogue_fillout_fn_t *fillout,
                                        void                       *opaque)
{
  wuss_dialogue_t *dialogue;

  if (out == NULL || wuss == NULL || window == NULL)
    return result_NULL_ARG;

  dialogue = dialogue_alloc(wuss->alloc, fillout, opaque);
  if (dialogue == NULL)
    return result_OOM;
  dialogue->owns_window = 0;
  dialogue->window      = window;

  *out = dialogue;
  return result_OK;
}

void wuss_dialogue_destroy(wuss_dialogue_t *doomed)
{
  wuss_alloc_t alloc;

  if (doomed == NULL)
    return;

  alloc = doomed->alloc;
  if (doomed->owns_window)
    wuss_window_close(doomed->window); /* frees the window and its icons */
  alloc.free(doomed);
}

/* ----------------------------------------------------------------------- */

void wuss_dialogue_set_opaque(wuss_dialogue_t *dialogue, void *opaque)
{
  dialogue->opaque = opaque;
}

result_t wuss_dialogue_set_actions(wuss_dialogue_t              *dialogue,
                                   const wuss_dialogue_action_t *actions,
                                   int                           nactions)
{
  if (nactions < 0 || nactions > WUSS_DIALOGUE_MAX_ACTIONS)
    return result_BAD_ARG;

  dialogue->nactions = nactions;
  if (nactions > 0)
    memcpy(dialogue->actions, actions, sizeof(*actions) * (size_t) nactions);

  return result_OK;
}

int wuss_dialogue_handle_icon(wuss_dialogue_t    *dialogue,
                              const wuss_event_t *event,
                              result_t           *out_result)
{
  int i;

  if (event->data.icon.action != wuss_MOUSE_UP)
    return 0;

  for (i = 0; i < dialogue->nactions; i++)
  {
    if (dialogue->actions[i].icon == event->data.icon.icon)
    {
      *out_result = dialogue->actions[i].fn(dialogue->opaque,
                                            event->data.icon.button);
      return 1;
    }
  }

  return 0;
}

result_t wuss_dialogue_handle_pre_show(wuss_dialogue_t *dialogue)
{
  if (dialogue->fillout == NULL)
    return result_OK;

  return dialogue->fillout(dialogue->opaque);
}

result_t wuss_dialogue_hide(wuss_dialogue_t *dialogue)
{
  return wuss_window_set_hidden(dialogue->window, 1);
}

wuss_window_t *wuss_dialogue_window(const wuss_dialogue_t *dialogue)
{
  return dialogue ? dialogue->window : NULL;
}
