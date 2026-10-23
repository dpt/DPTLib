/* wuss/component/colourpicker.h -- true-colour colour picker dialogue */

/**
 * \file colourpicker.h
 *
 * A wuss component: a dialogue for picking any 24-bit colour, as opposed to
 * wuss_colourmenu's system palette entries. A radio row picks the colour
 * model -- RGB, CMYK, HSL, HSV or OKLab -- and a slider for each of the
 * model's channels sets the colour. Swatches show the colour the dialogue
 * was opened with beside the current one, and a writable field shows the
 * colour as hex, taking "#RRGGBB" or "#RGB" (the '#' optional) typed in.
 *
 * The colour is kept as doubles in the current model (see
 * framebuf/colourspace.h), so a channel is not lost to rounding when another
 * moves, or to a model switch. Values outside the sRGB gamut, which OKLab
 * can reach, are clamped per channel when reported.
 *
 * The dialogue runs on a task of its own (created by
 * wuss_colourpicker_create and freed by wuss_colourpicker_destroy), so
 * callers forward nothing. It is a standalone window, not a menu: it stays
 * up until OK or Cancel. Every change is reported at once through a
 * callback, so the caller can show it live:
 *
 * - Select on OK closes the dialogue, keeping the colour. - Adjust on OK
 * keeps the colour and leaves the dialogue open, the "old" swatch taking the
 * current colour. - Select on Cancel reports the old colour and closes the
 * dialogue. - Adjust on Cancel reports the old colour and leaves the
 * dialogue open.
 *
 * RETURN is OK and ESCAPE is Cancel, both as Select. Each picker remembers
 * its last model across openings.
 *
 * Built only when WUSS_COMPONENTS is defined.
 */

#ifndef WUSS_COMPONENT_COLOURPICKER_H
#define WUSS_COMPONENT_COLOURPICKER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/colour.h"

#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Opaque handle: owns the dialogue, its icons and its task. */
typedef struct wuss_colourpicker wuss_colourpicker_t;

/**
 * Change callback: the picked colour has changed, through a slider, the hex
 * field, or Cancel restoring the old colour. The picker may be destroyed
 * from here.
 *
 * \param[in] colour The colour, opaque.
 * \param[in] opaque As passed to wuss_colourpicker_create.
 */
typedef void (wuss_colourpicker_changed_fn_t)(colour_t  colour,
                                              void     *opaque);

/* ----------------------------------------------------------------------- */

/**
 * Create a colour picker: a hidden window on a task of its own.
 *
 * \param[out] out    Filled with the new handle on success, untouched on
 *                    failure.
 * \param[in]  wuss   Window manager.
 * \param[in]  title  Titlebar caption, or NULL for "Colour"; copied.
 * \param[in]  fn     Called on every change.
 * \param[in]  opaque Passed back to \p fn.
 * \return \ref result_OK, \ref result_OOM, \ref result_NULL_ARG if \p out,
 *         \p wuss or \p fn is NULL, or a wuss_task_create /
 *         wuss_window_create / wuss_icon_create code.
 */
result_t wuss_colourpicker_create(wuss_colourpicker_t           **out,
                                  wuss_t                         *wuss,
                                  const char                     *title,
                                  wuss_colourpicker_changed_fn_t *fn,
                                  void                           *opaque);

/**
 * Free a colour picker, closing its window and task without reporting
 * anything. Safe to pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_colourpicker_destroy(wuss_colourpicker_t *doomed);

/**
 * Show the picker set to \p colour, which becomes the old colour too. A
 * hidden picker opens centred on the pointer; one already shown is reset to
 * \p colour and brought to the front where it is. Opening reports nothing.
 *
 * \param[in] picker Handle.
 * \param[in] colour Colour to start from; its alpha is ignored.
 * \return \ref result_OK, or a wuss_window_set_hidden code.
 */
result_t wuss_colourpicker_open(wuss_colourpicker_t *picker,
                                colour_t             colour);

/**
 * The colour currently picked, opaque.
 *
 * \param[in] picker Handle.
 * \return The colour.
 */
colour_t wuss_colourpicker_get_colour(const wuss_colourpicker_t *picker);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_COLOURPICKER_H */
