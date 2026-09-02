/* wuss/frontend.h -- platform backend for the Wuss interactive demo */

#ifndef WUSS_FRONTEND_H
#define WUSS_FRONTEND_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "geom/point.h"
#include "wuss/wuss.h"

/* One backend drives the demo: SDL on the desktop, native VDU/OS_Mouse calls
 * on RISC OS. main.c owns the wuss instance and the main loop; the backend
 * only opens a surface, reports input as normalised wuss_input_t records and
 * gets the framebuffer onto the screen. */

typedef struct wuss_frontend wuss_frontend_t;

/* Normalised input event kinds. The backend translates whatever the platform
 * delivers (SDL events, polled OS_Mouse state, key scans) into these; the
 * loop in main.c acts on them without knowing which backend produced them. */
typedef enum wuss_input_kind
{
  wuss_INPUT_NONE = 0,
  wuss_INPUT_QUIT,          /* window closed, or the quit key */
  wuss_INPUT_MOUSE_MOVE,    /* .pos */
  wuss_INPUT_MOUSE_DOWN,    /* .pos, .button */
  wuss_INPUT_MOUSE_UP,      /* .pos, .button */
  wuss_INPUT_WHEEL,         /* .pos, .wheel */
  wuss_INPUT_REDRAW_ALL,    /* force a full redraw (F1) */
  wuss_INPUT_GARBAGE,       /* corrupt the whole screen for one frame (Shift-F1) */
  wuss_INPUT_PIXEL_STRESS,  /* one-pixel-at-a-time redraw (F3) */
  wuss_INPUT_PALETTE_CYCLE  /* advance to the next system palette (F4) */
}
wuss_input_kind_t;

typedef struct wuss_input
{
  wuss_input_kind_t kind;
  point_t           pos;    /* screen-space pixel coordinates */
  wuss_button_t     button;
  int               wheel;  /* wheel delta, +ve = up */
}
wuss_input_t;

/* Open a drawing surface `width` x `height` pixels.
 *
 * `palette` / `npalette` are the initial system palette: a backend that owns
 * the physical palette (RISC OS 16-colour mode) programmes it here.
 *
 * On return `*pixels` points at storage for width*height pixels at
 * `*rowbytes` stride, and `*fmt` is the pixel format that storage expects.
 * The caller wraps this in a bitmap_t and hands it to wuss. The backend may
 * hand back screen memory directly (no copy on present) or a private buffer
 * (present() blits it). Either way the caller must not free *pixels; call
 * wuss_frontend_close to release it.
 */
result_t wuss_frontend_open(int               width,
                            int               height,
                            const colour_t   *palette,
                            int               npalette,
                            void            **pixels,
                            int              *rowbytes,
                            pixelfmt_t       *fmt,
                            wuss_frontend_t **frontend);

/* Pull the next pending input event. Returns true and fills *event while
 * events remain; returns false when the queue is empty for this frame. */
bool wuss_frontend_poll(wuss_frontend_t *frontend, wuss_input_t *event);

/* Push the current framebuffer contents to the screen and pace the frame. A
 * backend rendering straight into screen memory only waits for vsync here. */
void wuss_frontend_present(wuss_frontend_t *frontend, const bitmap_t *bm);

/* Push a new system palette to the physical palette, if the backend owns one.
 * Called after the demo cycles palettes (F4). No-op for SDL. */
void wuss_frontend_set_palette(wuss_frontend_t *frontend,
                               const colour_t  *palette,
                               int              npalette);

/* Tear down the surface and free everything wuss_frontend_open allocated. */
void wuss_frontend_close(wuss_frontend_t *frontend);

#endif /* WUSS_APP */

#endif /* WUSS_FRONTEND_H */
