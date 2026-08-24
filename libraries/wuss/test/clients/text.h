/* text.h -- wuss test - static paragraph client */

#ifndef CLIENTS_TEXT_H
#define CLIENTS_TEXT_H

#ifdef USE_SDL

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/window.h"

/* window B's client: flows a fixed paragraph of placeholder text over its
 * wuss-filled background, one line per bmfont_draw call */
typedef struct text_client
{
  bmfont_t *font;
  colour_t  bg, fg;
}
text_client_t;

wuss_redraw_fn_t text_redraw;

#endif /* USE_SDL */

#endif /* CLIENTS_TEXT_H */
