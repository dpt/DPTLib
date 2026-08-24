/* image.h -- wuss test - static bitmap image client */

#ifndef CLIENTS_IMAGE_H
#define CLIENTS_IMAGE_H

#ifdef USE_SDL

#include "framebuf/bitmap.h"
#include "wuss/window.h"

/* window's client: a loaded PNG, alpha-tested against the window
 * background (this test screen is paletted, so fully-transparent source
 * pixels are skipped and everything else is drawn at full strength) */
typedef struct image_client
{
  bitmap_t bitmap; /* owned: base freed by the caller when done */
}
image_client_t;

wuss_redraw_fn_t image_redraw;

#endif /* USE_SDL */

#endif /* CLIENTS_IMAGE_H */
