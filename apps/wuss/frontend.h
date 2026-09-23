/* wuss/frontend.h -- platform backend for the Wuss interactive demo */

#ifndef WUSS_FRONTEND_H
#define WUSS_FRONTEND_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"
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
  wuss_INPUT_KEY            /* .key, .mods: a press or autorepeat */
}
wuss_input_kind_t;

typedef struct wuss_input
{
  wuss_input_kind_t    kind;
  point_t              pos;    /* screen-space pixel coordinates */
  wuss_button_t        button;
  int                  wheel;  /* wheel delta, +ve = up */
  int                  key;    /* Unicode code point or wuss_KEY_* */
  wuss_key_modifiers_t mods;
}
wuss_input_t;

/* Open a drawing surface `width` x `height` pixels.
 *
 * `palette` / `npalette` are the initial system palette: a backend that owns
 * the physical palette (RISC OS 16-colour mode) programmes it here.
 *
 * `depth` is the framebuffer's bits per pixel: 32 for a direct-colour
 * surface (no per-frame conversion), 4 for a paletted one (exercises the
 * nibble-packed blit path). Those are the only values the SDL backend
 * accepts today; a backend with a fixed format (RISC OS 16-colour mode)
 * ignores it.
 *
 * `scale` is the initial integer window zoom (device pixels per screen
 * pixel) for backends with a resizable window; <= 0 means "backend default".
 * The RISC OS backend ignores it.
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
                            int               depth,
                            int               scale,
                            void            **pixels,
                            int              *rowbytes,
                            pixelfmt_t       *fmt,
                            wuss_frontend_t **frontend);

/* Pull the next pending input event. Returns true and fills *event while
 * events remain; returns false when the queue is empty for this frame. */
bool wuss_frontend_poll(wuss_frontend_t *frontend, wuss_input_t *event);

/* Push the current framebuffer contents to the screen and pace the frame. A
 * backend rendering straight into screen memory only waits for vsync here.
 *
 * `dirty`, if non-NULL, bounds the region that actually changed since the
 * last present; a backend that has to convert `bm` (paletted -> display
 * format) may use it to convert only that rect instead of the whole
 * bitmap. NULL means "assume the whole bitmap changed". */
void wuss_frontend_present(wuss_frontend_t *frontend,
                           const bitmap_t  *bm,
                           const box_t     *dirty);

/* Resize the drawing surface to width x height, keeping the current scale
 * and depth. On success, *pixels / *rowbytes describe the new backing storage
 * exactly as wuss_frontend_open's did -- any previous *pixels value is
 * invalid whether or not it happened to be reused. Returns
 * result_NOT_SUPPORTED on a backend with a fixed screen mode (RISC OS),
 * leaving the surface untouched. */
result_t wuss_frontend_resize(wuss_frontend_t *frontend,
                              int              width,
                              int              height,
                              void           **pixels,
                              int             *rowbytes);

/* Push a new system palette to the physical palette, if the backend owns one.
 * Called after the palette task's picker menu changes the system palette.
 * No-op for SDL. */
void wuss_frontend_set_palette(wuss_frontend_t *frontend,
                               const colour_t  *palette,
                               int              npalette);

/* Step the window zoom by `delta` (F2 / Shift-F2), clamped to the backend's
 * range. No-op on a backend without a resizable window (RISC OS). */
void wuss_frontend_zoom(wuss_frontend_t *frontend, int delta);

/* Tear down the surface and free everything wuss_frontend_open allocated. */
void wuss_frontend_close(wuss_frontend_t *frontend);

#endif /* WUSS_APP */

#endif /* WUSS_FRONTEND_H */
