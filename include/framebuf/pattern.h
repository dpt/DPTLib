/* framebuf/pattern.h -- 8x8 repeating fill pattern */

#ifndef FRAMEBUF_PATTERN_H
#define FRAMEBUF_PATTERN_H

#include <stdint.h>

#include "framebuf/colour.h"
#include "geom/point.h"

/**
 * Built-in 8x8 fill patterns. Each is a 1-bit tile: set bits take the
 * foreground colour, clear bits the background. Pass one to
 * `pattern_from_preset` to obtain a `pattern_t`.
 */
typedef enum screen_pattern
{
  /**
   * 8x8 ordered (Bayer) dither, one entry per coverage level 0 (empty) to 64
   * (solid). Index by level as `screen_PATTERN_BAYER0 + level`;
   * `screen_PATTERN_BAYER_LIMIT` is one past the last.
   */
  screen_PATTERN_BAYER0 = 0,
  screen_PATTERN_BAYER_LIMIT = screen_PATTERN_BAYER0 + 65,

  /**
   * A flat fill and a 50% checkerboard are just the ends and midpoint of the
   * Bayer run, so they are aliases rather than separate tiles. `EMPTY` is
   * the inverse of `SOLID`; `GREY50` is its own inverse.
   */
  screen_PATTERN_EMPTY  = screen_PATTERN_BAYER0,
  screen_PATTERN_GREY50 = screen_PATTERN_BAYER0 + 32,
  screen_PATTERN_SOLID  = screen_PATTERN_BAYER0 + 64,

  /* The remaining named tiles follow the Bayer run. */
  screen_PATTERN_HSTRIPE = screen_PATTERN_BAYER_LIMIT,
                                 /**< Horizontal bars. */
  screen_PATTERN_VSTRIPE,        /**< Vertical bars. */
  screen_PATTERN_DIAGONAL,       /**< Diagonal lines. */
  screen_PATTERN_DOTS,           /**< Sparse dots. */
  screen_PATTERN_GRID,           /**< Thin grid lines. */
  screen_PATTERN_CROSSHATCH,     /**< Crossed thin lines. */

  screen_PATTERN_HSTRIPE_INV,    /**< Inverse of HSTRIPE. */
  screen_PATTERN_VSTRIPE_INV,    /**< Inverse of VSTRIPE. */
  screen_PATTERN_DIAGONAL_INV,   /**< Inverse of DIAGONAL. */
  screen_PATTERN_DOTS_INV,       /**< Inverse of DOTS. */
  screen_PATTERN_GRID_INV,       /**< Inverse of GRID. */
  screen_PATTERN_CROSSHATCH_INV, /**< Inverse of CROSSHATCH. */

  screen_PATTERN__LIMIT
                    /**< Count of patterns; not itself a pattern. */
}
screen_pattern_t;

/** Flags for `pattern_t`. */
enum
{
  /**
   * Paint only where a pattern bit is set, leaving clear-bit pixels
   * untouched (a stencil). Without this the whole area is painted, clear
   * bits taking `bg`.
   */
  pattern_FLAG_STENCIL = 1u << 0
};

/**
 * An 8x8 repeating fill pattern and how it paints. Passed to
 * `screen_fill_pattern` and `bitmap_fill_pattern`.
 */
typedef struct pattern
{
  uint8_t  bits[8]; /**< One byte per row, MSB = leftmost pixel. */
  colour_t fg;      /**< Colour for set bits. */
  colour_t bg;      /**< Colour for clear bits, unless a stencil. */
  unsigned flags;   /**< Bitwise OR of `pattern_FLAG_*`, or 0. */
  point_t  origin;  /**< Tile phase: the coordinate mapping to the fill box's
                         top-left corner. Passing a scroll origin keeps the
                         pattern locked to content rather than crawling. */
}
pattern_t;

/**
 * Build a `pattern_t` from a built-in preset and a pair of colours. The
 * result has no flags set and a zero origin; assign `.flags` and `.origin`
 * afterwards if needed.
 *
 * \param[in] preset One of `screen_PATTERN_*`.
 * \param[in] fg     Colour for set bits.
 * \param[in] bg     Colour for clear bits.
 * \return The pattern.
 */
pattern_t pattern_from_preset(screen_pattern_t preset,
                              colour_t         fg,
                              colour_t         bg);

/**
 * Build a stencil `pattern_t` from a caller-supplied 8x8 mask and a single
 * colour. `pattern_FLAG_STENCIL` is set, so clear-bit pixels are left
 * untouched; `.bg` is unused. The origin is zero.
 *
 * \param[in] mask   Eight bytes, one per pattern row, MSB leftmost.
 * \param[in] colour Colour for set bits.
 * \return The pattern.
 */
pattern_t pattern_from_mask(const uint8_t mask[8], colour_t colour);

/**
 * Build an ordered-dither `pattern_t` approximating an arbitrary colour from
 * a palette. Every pair of palette entries is tried; the pair whose Bayer
 * mix lies closest to `target` wins, with the mix ratio choosing the
 * `screen_PATTERN_BAYER*` coverage level. Widely-spaced pairs are penalised
 * so a near solid entry is preferred over, say, a black/white stipple for a
 * grey. An exact palette match comes back as a single-colour tile. Alpha is
 * ignored. The result has no flags set and a zero origin.
 *
 * O(nentries²): fine for occasional fills, not per-pixel use.
 *
 * \param[in] palette  Palette to choose from.
 * \param[in] nentries Number of entries in the palette; at least one.
 * \param[in] target   Colour to approximate.
 * \return The pattern.
 */
pattern_t pattern_from_colour(const colour_t *palette,
                              int             nentries,
                              colour_t        target);

/**
 * The 8x8 ordered (Bayer) dither threshold for screen pixel (`x`, `y`), a
 * value in the range 0 to 63. This is the recursively-built matrix the
 * `screen_PATTERN_BAYER*` tiles are derived from, exposed for callers that
 * dither a continuous value per pixel rather than fill with a fixed-coverage
 * tile (e.g. `screen_copy_bitmap_dithered`). `x` and `y` may be negative.
 *
 * \param[in] x Screen X coordinate.
 * \param[in] y Screen Y coordinate.
 * \return The threshold, 0 to 63.
 */
int pattern_bayer_threshold(int x, int y);

#endif /* FRAMEBUF_PATTERN_H */
