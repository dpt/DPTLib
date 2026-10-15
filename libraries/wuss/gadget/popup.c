/* wuss/gadget/popup.c -- layout and menu placement shared by the pop-up field gadgets */

#include <string.h>

#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"

#include "wuss/icon.h"
#include "wuss/menu.h"

#include "../font/font.h"
#include "../icon.h"
#include "popup.h"

int wuss__popup_arrow_spec(const wuss_t     *wuss,
                           box_t             bbox,
                           const char       *name,
                           const char       *pressed_name,
                           wuss_icon_spec_t *spec)
{
  const bitmap_t *bm;
  int             idx;
  int             w, h;

  idx = wuss_iconpool_lookup(wuss, name);
  bm  = (idx >= 0) ? wuss_iconpool_bitmap(wuss, idx) : NULL;
  if (bm == NULL)
  {
    /* no icon set: a square button the height of the gadget */
    h = bbox.y1 - bbox.y0;
    wuss_icon_spec_action(spec,
                          (box_t) BOX_POS_SIZE(bbox.x1 - h, bbox.y0, h, h),
                          ">", 0);
    return h;
  }

  w = bm->size.w;
  h = bm->size.h;

  memset(spec, 0, sizeof(*spec));
  spec->bbox  = (box_t) BOX_POS_SIZE(bbox.x1 - w,
                                     bbox.y0 + (bbox.y1 - bbox.y0 - h) / 2,
                                     w, h);
  spec->type  = wuss_ICON_TYPE_BITMAP;
  spec->flags = wuss_ICON_FLAGS_INTERACTIVE;
  spec->u.bitmap.name         = name;
  spec->u.bitmap.pressed_name = pressed_name;
  return w;
}

int wuss__popup_layout(const wuss_t    *wuss,
                       box_t            bbox,
                       const char      *title,
                       int              title_width,
                       const char      *arrow_name,
                       const char      *arrow_pressed_name,
                       wuss_icon_spec_t specs[3],
                       box_t           *field)
{
  bmfont_t *font;
  int       w;
  int       nspecs;

  *field     = bbox;
  field->x1 -= wuss__popup_arrow_spec(wuss, bbox, arrow_name,
                                      arrow_pressed_name, &specs[1]) +
               wuss_STD_GAP;
  nspecs     = 2;

  font = wuss->fonts.fonts[0];
  if (title_width >= 0 && title != NULL && title[0] != '\0' && font != NULL)
  {
    w = title_width;
    if (w == 0)
      w = (int) wuss__text_width(font, title) + 2 * WUSS_LABEL_TEXT_PAD;

    wuss_icon_spec_label(&specs[2],
                         (box_t) BOX_POS_SIZE(bbox.x0, bbox.y0, w,
                                              bbox.y1 - bbox.y0),
                         title, wuss_ICON_FLAGS_JUSTIFY_RIGHT);
    field->x0 += w + wuss_STD_GAP;
    nspecs     = 3;
  }

  return nspecs;
}

result_t wuss__popup_open(wuss_window_t      *window,
                          wuss_icon_t        *arrow,
                          const wuss_menu_t  *menu,
                          wuss_menu_handle_t *handle)
{
  box_t   content, bbox, screen;
  point_t scroll;

  wuss_window_get_content_bounds(window, &content);
  wuss_window_get_scroll(window, &scroll);
  wuss_icon_get_bbox(arrow, &bbox);
  wuss__icon_box_to_screen(&content, scroll, &bbox, &screen);

  return wuss_menu_open(window->task, menu, POINT(screen.x1, screen.y0),
                        handle);
}
