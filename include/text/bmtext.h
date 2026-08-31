/* bmtext.h -- word-wrap and draw a paragraph in a bitmap font */

/**
 * \file bmtext.h
 *
 * Splits a string into lines that each fit a given pixel width when drawn in a
 * \ref bmfont_t, then draws those lines stacked.
 *
 * Unlike \ref txtfmt (which wraps at character counts, for monospaced text)
 * this measures each candidate line with \ref bmfont_measure, so it wraps
 * proportional fonts correctly.
 *
 * - Breaks at the last space that still fits; hard-breaks a word with no space
 * in it. - Runs of whitespace at a break are swallowed. - Layout is pure: it
 * touches no screen or scroll state.
 */

#ifndef DPTLIB_BMTEXT_H
#define DPTLIB_BMTEXT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"
#include "geom/point.h"

/**
 * One laid-out line: a pointer into the caller's string plus a length. Not
 * NUL-terminated.
 */
typedef struct bmtext_line
{
  const char *str;
  int         len;
}
bmtext_line_t;

/**
 * Break \p string into lines that each fit \p wrap_width pixels in \p font.
 *
 * The line pointers refer into \p string itself, so it must outlive \p lines.
 *
 * \param[in]  font       Bitmap font the lines will be drawn in.
 * \param[in]  string     Text to wrap.
 * \param[in]  stringlen  Length of \p string in bytes.
 * \param[in]  wrap_width Width to wrap to, in pixels.
 * \param[out] lines      Filled with up to \p max lines.
 * \param[in]  max        Capacity of \p lines. Lines past this are dropped.
 *
 * \return Number of lines written to \p lines.
 */
int bmtext_layout(bmfont_t      *font,
                  const char    *string,
                  int            stringlen,
                  int            wrap_width,
                  bmtext_line_t *lines,
                  int            max);

/**
 * Draw \p nlines pre-laid-out \p lines stacked downward from \p origin,
 * advancing by the font height plus \p leading pixels per line.
 *
 * \param[in] font    Bitmap font to draw in.
 * \param[in] scr     Screen to draw on.
 * \param[in] lines   Lines from \ref bmtext_layout.
 * \param[in] nlines  Number of lines.
 * \param[in] fg      Foreground colour.
 * \param[in] bg      Background colour, for glyph blending.
 * \param[in] leading Extra pixels between lines.
 * \param[in] origin  Top-left of the first line, in pixels.
 */
void bmtext_draw(bmfont_t            *font,
                 screen_t            *scr,
                 const bmtext_line_t *lines,
                 int                  nlines,
                 colour_t             fg,
                 colour_t             bg,
                 int                  leading,
                 point_t              origin);

#ifdef __cplusplus
}
#endif

#endif /* DPTLIB_BMTEXT_H */
