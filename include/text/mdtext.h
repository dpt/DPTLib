/* text/mdtext.h -- lay out and draw Markdown or plain text in a bmfont */

/**
 * \file mdtext.h
 *
 * A block of text, plain or Markdown, wrapped to a width and drawn in a
 * bitmap font. Markdown is reduced to plain text plus styled spans by
 * mdspans_parse. Each span is drawn in a fixed tint, and bold spans in the
 * bold face where there is one. Lines break at each newline as well as
 * wrapping at spaces.
 *
 * The caller owns the fonts and sets the public fields directly. The layout
 * is cached and redone only when the text, a font, the spacing, the
 * alignment or the wrap width changes, so mdtext_layout is cheap to call
 * once per redraw.
 */

#ifndef TEXT_MDTEXT_H
#define TEXT_MDTEXT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "geom/point.h"
#include "text/bmtext.h"
#include "text/mdspans.h"

/* ----------------------------------------------------------------------- */

typedef enum mdtext_align
{
  mdtext_ALIGN_LEFT,
  mdtext_ALIGN_CENTRE,
  mdtext_ALIGN_RIGHT
}
mdtext_align_t;

typedef struct mdtext
{
  /* set by the caller; changes take effect at the next mdtext_layout */
  bmfont_t        *font;    /* regular face, borrowed */
  bmfont_t        *bold;    /* bold face, borrowed, or NULL */
  bmfont_spacing_t spacing;
  mdtext_align_t   align;
  int              leading; /* pixels between lines */

  /* private */
  char            *text;
  mdspan_t        *spans;
  int              nspans;
  int              span_cursor;
  bmtext_line_t   *lines;
  int             *dx;
  int              nlines;
  int              laid_wrap;
  bmfont_t        *laid_font;
  bmfont_t        *laid_bold;
  bmfont_spacing_t laid_spacing;
  mdtext_align_t   laid_align;
}
mdtext_t;

/* ----------------------------------------------------------------------- */

/**
 * Replace the text with 'len' bytes of 'src'. With 'markdown' set it is
 * parsed as Markdown, otherwise copied as it is.
 *
 * \return result_OK, or an error from mdspans_parse or result_OOM. On error
 *         the previous text is kept.
 */
result_t mdtext_set(mdtext_t   *mdtext,
                    const char *src,
                    int         len,
                    int         markdown);

/**
 * Bring the layout up to date for 'wrap_width' pixels. mdtext->font must be
 * set.
 */
result_t mdtext_layout(mdtext_t *mdtext, int wrap_width);

/**
 * \return The number of laid-out lines.
 */
int mdtext_nlines(const mdtext_t *mdtext);

/**
 * \return The height of one line in pixels, including the leading.
 */
int mdtext_line_height(const mdtext_t *mdtext);

/**
 * Draw the laid-out lines that cross scr->clip, the first line's top-left at
 * 'origin'. Unstyled text is drawn in 'fg' on 'bg'.
 */
void mdtext_draw(mdtext_t *mdtext,
                 screen_t *scr,
                 point_t   origin,
                 colour_t  fg,
                 colour_t  bg);

/**
 * Free the text and layout. The fonts are left alone. The block may be set
 * again afterwards.
 */
void mdtext_free(mdtext_t *mdtext);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* TEXT_MDTEXT_H */
