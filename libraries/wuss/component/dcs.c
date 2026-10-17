/* wuss/component/dcs.c -- Discard/Cancel/Save dialogue */

#include <limits.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "geom/box.h"
#include "geom/stack.h"

#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "wuss/component/dcs.h"
#include "wuss/component/dialogue.h"

#include "../core/types.h"

/* ----------------------------------------------------------------------- */

/* icons, in creation order */
enum
{
  DCS_ICON_MESSAGE,
  DCS_ICON_DISCARD,
  DCS_ICON_CANCEL,
  DCS_ICON_SAVE,
  DCS_ICON__COUNT
};

/* layout items, in creation order: a VBOX of the message above a row of
 * buttons, Discard at the left and Cancel and Save pushed right. The
 * message's width and the buttons' widths are filled in from the font. Save
 * comes last so the Quit variant, which has none, uses a prefix of both this
 * and the icon list. See docs/windowing/wuss-layout.md */
enum
{
  DCS_ST_ROOT,
  DCS_ST_MESSAGE,
  DCS_ST_BUTTONS,
  DCS_ST_DISCARD,
  DCS_ST_SPACER,
  DCS_ST_CANCEL,
  DCS_ST_SAVE,
  DCS_ST__LIMIT
};

static const stack_item_t dcs_layout[DCS_ST__LIMIT] =
{
  [DCS_ST_ROOT]    = { .kind = stack_KIND_VBOX, .parent = -1,
                       .gap = wuss_STD_GAP, .pad = wuss_STD_INSETS },
  [DCS_ST_MESSAGE] = STACK_LEAF(DCS_ST_ROOT, wuss_STD_GLYPH_SIZE, 0,
                                stack_ALIGN_START),
  [DCS_ST_BUTTONS] = STACK_HBOX(DCS_ST_ROOT, wuss_STD_PRIMARY_BUTTON_HEIGHT,
                                wuss_STD_GAP, stack_ALIGN_END),
  [DCS_ST_DISCARD] = STACK_LEAF(DCS_ST_BUTTONS, 0,
                                wuss_STD_SECONDARY_BUTTON_HEIGHT,
                                stack_ALIGN_CENTRE),
  [DCS_ST_SPACER]  = STACK_SPACER(DCS_ST_BUTTONS, 1), /* push Cancel and Save right */
  [DCS_ST_CANCEL]  = STACK_LEAF(DCS_ST_BUTTONS, 0,
                                wuss_STD_SECONDARY_BUTTON_HEIGHT,
                                stack_ALIGN_CENTRE),
  [DCS_ST_SAVE]    = STACK_LEAF(DCS_ST_BUTTONS, 0,
                                wuss_STD_PRIMARY_BUTTON_HEIGHT,
                                stack_ALIGN_CENTRE)
};

struct wuss_dcs
{
  wuss_alloc_t          alloc;       /* copied hooks; wuss_t itself not retained */
  wuss_t               *wuss;
  wuss_task_t          *task;
  wuss_dialogue_t      *dialogue;
  wuss_dcs_choice_fn_t *fn;
  void                 *opaque;
  wuss_menu_handle_t    menu_handle; /* live only while shown */
  int                   pending;     /* shown, and no choice reported yet */
  wuss_dcs_kind_t       kind;
  wuss_icon_t          *icons[DCS_ICON__COUNT];
  int                   nicons;      /* live entries in icons */
};

/* ----------------------------------------------------------------------- */

/* pixel width of text in the desktop font */
static int dcs_text_width(const wuss_t *wuss, const char *text)
{
  bmfont_width_t width;

  bmfont_measure(wuss_get_font(wuss), text, (int) strlen(text), NULL,
                 INT_MAX, NULL, &width);

  return width;
}

/* smallest width of a button labelled text inside a border of the given
 * width: the text plus a space either side */
static int dcs_button_width(const wuss_t *wuss, const char *text, int border)
{
  return dcs_text_width(wuss, text) + 2 * dcs_text_width(wuss, " ") +
         2 * border;
}

/* ----------------------------------------------------------------------- */

/* dismiss the dialogue and report the choice. The callback comes last: it
 * may destroy or reopen the dialogue */
static result_t dcs_choose(wuss_dcs_t *dcs, wuss_dcs_choice_t choice)
{
  if (!dcs->pending)
    return result_OK;

  dcs->pending = 0;
  wuss_menu_close(dcs->menu_handle);
  dcs->menu_handle = NULL;

  dcs->fn(choice, dcs->opaque);

  return result_OK;
}

static result_t dcs_action_discard(void *opaque, wuss_button_t button)
{
  (void) button;

  return dcs_choose(opaque, wuss_DCS_DISCARD);
}

static result_t dcs_action_cancel(void *opaque, wuss_button_t button)
{
  (void) button;

  return dcs_choose(opaque, wuss_DCS_CANCEL);
}

static result_t dcs_action_save(void *opaque, wuss_button_t button)
{
  (void) button;

  return dcs_choose(opaque, wuss_DCS_SAVE);
}

/* ----------------------------------------------------------------------- */

/* solve the layout: the window size, and a box for every item */
static result_t dcs_solve(const wuss_t   *wuss,
                          wuss_dcs_kind_t kind,
                          const char     *message,
                          size2d_t       *size,
                          box_t           boxes[DCS_ST__LIMIT])
{
  result_t     rc;
  stack_item_t items[DCS_ST__LIMIT];
  int          n;
  box_t        root;

  memcpy(items, dcs_layout, sizeof(items));
  n = DCS_ST__LIMIT;
  items[DCS_ST_MESSAGE].cross_size = dcs_text_width(wuss, message);
  items[DCS_ST_DISCARD].axis_size  =
    dcs_button_width(wuss, "Discard", wuss_STD_SECONDARY_BUTTON_BORDER);
  items[DCS_ST_CANCEL].axis_size   =
    dcs_button_width(wuss, "Cancel", wuss_STD_SECONDARY_BUTTON_BORDER);
  items[DCS_ST_SAVE].axis_size     =
    dcs_button_width(wuss, "Save", wuss_STD_PRIMARY_BUTTON_BORDER);

  /* without Save, Cancel is the default and so the primary button */
  if (kind == wuss_DCS_KIND_QUIT)
  {
    n = DCS_ST_SAVE;
    items[DCS_ST_CANCEL].cross_size = wuss_STD_PRIMARY_BUTTON_HEIGHT;
    items[DCS_ST_CANCEL].axis_size  =
      dcs_button_width(wuss, "Cancel", wuss_STD_PRIMARY_BUTTON_BORDER);
  }

  rc = stack_smallest(items, n, size);
  if (rc != result_OK)
    return rc;

  root = (box_t) BOX_POS_SIZE(0, 0, size->w, size->h);

  return stack_solve(items, n, &root, boxes);
}

/* make the icons for boxes, replacing any already on the window */
static result_t dcs_build_icons(wuss_dcs_t *dcs,
                                const char *message,
                                const box_t boxes[DCS_ST__LIMIT])
{
  result_t               rc;
  wuss_icon_spec_t       specs[DCS_ICON__COUNT];
  wuss_icon_t           *icons[DCS_ICON__COUNT];
  wuss_dialogue_action_t actions[3];
  int                    quit, n, i;

  quit = dcs->kind == wuss_DCS_KIND_QUIT;
  n    = quit ? DCS_ICON_SAVE : DCS_ICON__COUNT;

  wuss_icon_spec_label(&specs[DCS_ICON_MESSAGE], boxes[DCS_ST_MESSAGE],
                       message, 0);
  wuss_icon_spec_action(&specs[DCS_ICON_DISCARD], boxes[DCS_ST_DISCARD],
                        "Discard", 0);
  wuss_icon_spec_action(&specs[DCS_ICON_CANCEL], boxes[DCS_ST_CANCEL],
                        "Cancel", quit);
  if (!quit)
    wuss_icon_spec_action(&specs[DCS_ICON_SAVE], boxes[DCS_ST_SAVE],
                          "Save", 1);

  rc = wuss_icon_create_array(wuss_dialogue_window(dcs->dialogue), specs, n,
                              icons);
  if (rc != result_OK)
    return rc;

  for (i = 0; i < dcs->nicons; i++)
    wuss_icon_delete(wuss_dialogue_window(dcs->dialogue), dcs->icons[i]);
  memcpy(dcs->icons, icons, sizeof(*icons) * (size_t) n);
  dcs->nicons = n;

  actions[0].icon = icons[DCS_ICON_DISCARD];
  actions[0].fn   = dcs_action_discard;
  actions[1].icon = icons[DCS_ICON_CANCEL];
  actions[1].fn   = dcs_action_cancel;
  if (!quit)
  {
    actions[2].icon = icons[DCS_ICON_SAVE];
    actions[2].fn   = dcs_action_save;
  }

  wuss_dialogue_set_cancel(dcs->dialogue, icons[DCS_ICON_CANCEL]);

  return wuss_dialogue_set_actions(dcs->dialogue, actions, quit ? 2 : 3);
}

/* The dialogue's own task: not autoclose, so it survives its window being
 * dismissed and shown again; wuss_dcs_destroy frees it. */
static result_t dcs_task_handle(wuss_window_t      *window,
                                const wuss_event_t *event,
                                void               *task_data)
{
  wuss_dcs_t *dcs;
  result_t    rc;

  (void) window;

  dcs = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    rc = result_OK;
    (void) wuss_dialogue_handle_icon(dcs->dialogue, event, &rc);
    return rc;

  case wuss_EVENT_KEY:
    rc = result_WUSS_KEY_UNCLAIMED;
    (void) wuss_dialogue_handle_key(dcs->dialogue, event, &rc);
    return rc;

  case wuss_EVENT_MENU_CLOSED:
    dcs->menu_handle = NULL; /* wuss closed the chain under us */
    return dcs_choose(dcs, wuss_DCS_CANCEL);

  default:
    return result_OK;
  }
}

result_t wuss_dcs_create(wuss_dcs_t          **out,
                         wuss_t               *wuss,
                         wuss_dcs_kind_t       kind,
                         const char           *title,
                         const char           *message,
                         wuss_dcs_choice_fn_t *fn,
                         void                 *opaque)
{
  result_t         rc;
  wuss_dcs_t      *dcs;
  wuss_task_desc_t desc;
  size2d_t         size;
  box_t            boxes[DCS_ST__LIMIT];

  if (out == NULL || wuss == NULL || message == NULL || fn == NULL)
    return result_NULL_ARG;

  dcs = wuss->alloc.malloc(sizeof(*dcs));
  if (dcs == NULL)
    return result_OOM;

  dcs->alloc       = wuss->alloc;
  dcs->wuss        = wuss;
  dcs->task        = NULL;
  dcs->dialogue    = NULL;
  dcs->fn          = fn;
  dcs->opaque      = opaque;
  dcs->menu_handle = NULL;
  dcs->pending     = 0;
  dcs->kind        = kind;
  dcs->nicons      = 0;

  rc = dcs_solve(wuss, kind, message, &size, boxes);
  if (rc != result_OK)
    goto failure;

  desc.handle    = dcs_task_handle;
  desc.task_data = dcs;
  desc.name      = "dcs";
  rc = wuss_task_create(wuss, &desc, &dcs->task);
  if (rc != result_OK)
    goto failure;

  rc = wuss_dialogue_create(&dcs->dialogue, dcs->task, size, title, NULL,
                            dcs);
  if (rc != result_OK)
    goto failure;

  rc = dcs_build_icons(dcs, message, boxes);
  if (rc != result_OK)
    goto failure;

  *out = dcs;
  return result_OK;

failure:
  wuss_dcs_destroy(dcs);
  return rc;
}

void wuss_dcs_destroy(wuss_dcs_t *doomed)
{
  wuss_alloc_t alloc;

  if (doomed == NULL)
    return;

  alloc = doomed->alloc;

  doomed->pending = 0; /* no choice is reported for a dialogue torn down */
  wuss_menu_close(doomed->menu_handle);
  wuss_dialogue_destroy(doomed->dialogue); /* frees the window and its icons */
  if (doomed->task != NULL)
    wuss_task_destroy(doomed->task);
  alloc.free(doomed);
}

/* ----------------------------------------------------------------------- */

result_t wuss_dcs_set_message(wuss_dcs_t *dcs, const char *message)
{
  result_t rc;
  size2d_t size;
  box_t    boxes[DCS_ST__LIMIT];

  if (dcs == NULL || message == NULL)
    return result_NULL_ARG;

  rc = dcs_solve(dcs->wuss, dcs->kind, message, &size, boxes);
  if (rc != result_OK)
    return rc;

  rc = wuss_window_resize(wuss_dialogue_window(dcs->dialogue), size);
  if (rc != result_OK)
    return rc;

  rc = wuss_window_set_doc(wuss_dialogue_window(dcs->dialogue), size);
  if (rc != result_OK)
    return rc;

  return dcs_build_icons(dcs, message, boxes);
}

/* ----------------------------------------------------------------------- */

result_t wuss_dcs_open(wuss_dcs_t *dcs)
{
  result_t       rc;
  wuss_window_t *win;
  box_t          content;
  point_t        at;

  win = wuss_dialogue_window(dcs->dialogue);
  wuss_window_get_content_bounds(win, &content);

  /* centred on the pointer, where the click that prompted it was */
  at    = wuss_get_pointer(dcs->wuss);
  at.x -= (content.x1 - content.x0) / 2;
  at.y -= (content.y1 - content.y0) / 2;

  /* a reopen of a shown dialogue reports Cancel for the first showing. Do it
   * here, as the chain's own wuss_EVENT_MENU_CLOSED could arrive after the
   * new one is up */
  rc = dcs_choose(dcs, wuss_DCS_CANCEL);
  if (rc != result_OK)
    return rc;

  rc = wuss_menu_open_window(dcs->task, win, at, &dcs->menu_handle);
  if (rc != result_OK)
    return rc;

  dcs->pending = 1;

  return result_OK;
}
