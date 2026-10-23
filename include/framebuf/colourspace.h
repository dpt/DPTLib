/* framebuf/colourspace.h -- colour model conversions */

/**
 * \file colourspace.h
 *
 * Conversions between sRGB and the CMYK, HSL, HSV and OKLab colour models.
 * Every model converts to and from \ref colourspace_rgb_t, an sRGB triple of
 * doubles; there are no direct conversions between other models, so convert
 * through RGB.
 *
 * RGB values are not clamped on the way through: a model may yield sRGB
 * outside 0..1 (OKLab especially), and converting that back gives the
 * original values. Only \ref colourspace_to_colour clamps.
 *
 * CMYK is the naive device-independent formula (K = 1 - max(R, G, B)), with
 * no ink or profile modelling. OKLab is Björn Ottosson's, computed from
 * linear sRGB.
 */

#ifndef FRAMEBUF_COLOURSPACE_H
#define FRAMEBUF_COLOURSPACE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/colour.h"

/* ----------------------------------------------------------------------- */

/** sRGB (gamma-encoded). Each channel is nominally 0..1. */
typedef struct colourspace_rgb
{
  double r, g, b;
}
colourspace_rgb_t;

/** CMYK. Each channel is 0..1. */
typedef struct colourspace_cmyk
{
  double c, m, y, k;
}
colourspace_cmyk_t;

/** HSL. Hue is in degrees, 0..360; saturation and lightness are 0..1. */
typedef struct colourspace_hsl
{
  double h, s, l;
}
colourspace_hsl_t;

/** HSV. Hue is in degrees, 0..360; saturation and value are 0..1. */
typedef struct colourspace_hsv
{
  double h, s, v;
}
colourspace_hsv_t;

/** OKLab. L is 0..1; a and b are roughly -0.4..0.4 for sRGB colours. */
typedef struct colourspace_oklab
{
  double l, a, b;
}
colourspace_oklab_t;

/* ----------------------------------------------------------------------- */

/**
 * Convert a colour to sRGB doubles. Alpha is discarded.
 *
 * \param[in]  c   Colour.
 * \param[out] rgb sRGB, each channel 0..1.
 */
void colourspace_from_colour(colour_t c, colourspace_rgb_t *rgb);

/**
 * Convert sRGB doubles to an opaque colour, clamping each channel to 0..1
 * and rounding to the nearest level.
 *
 * \param[in] rgb sRGB.
 * \return Colour.
 */
colour_t colourspace_to_colour(const colourspace_rgb_t *rgb);

/**
 * Parse a hex colour: "RRGGBB" or "RGB", with an optional leading '#'. Case
 * is ignored. "RGB" expands each digit, so "#f80" is "#ff8800".
 *
 * \param[in]  s Text to parse.
 * \param[out] c Colour, opaque. Unchanged on failure.
 * \return \ref result_OK, or \ref result_BAD_ARG if `s` is not a hex colour.
 */
result_t colourspace_parse_hex(const char *s, colour_t *c);

/* ----------------------------------------------------------------------- */

/**
 * Convert sRGB to CMYK. Inputs outside 0..1 give CMYK outside 0..1.
 *
 * \param[in]  rgb  sRGB.
 * \param[out] cmyk CMYK. Black gives C = M = Y = 0, K = 1.
 */
void colourspace_rgb_to_cmyk(const colourspace_rgb_t *rgb,
                             colourspace_cmyk_t      *cmyk);

/**
 * Convert CMYK to sRGB.
 *
 * \param[in]  cmyk CMYK.
 * \param[out] rgb  sRGB.
 */
void colourspace_cmyk_to_rgb(const colourspace_cmyk_t *cmyk,
                             colourspace_rgb_t        *rgb);

/**
 * Convert sRGB to HSL.
 *
 * \param[in]  rgb sRGB.
 * \param[out] hsl HSL. Greys give hue and saturation 0.
 */
void colourspace_rgb_to_hsl(const colourspace_rgb_t *rgb,
                            colourspace_hsl_t       *hsl);

/**
 * Convert HSL to sRGB. Hue wraps, so 360 is 0.
 *
 * \param[in]  hsl HSL.
 * \param[out] rgb sRGB.
 */
void colourspace_hsl_to_rgb(const colourspace_hsl_t *hsl,
                            colourspace_rgb_t       *rgb);

/**
 * Convert sRGB to HSV.
 *
 * \param[in]  rgb sRGB.
 * \param[out] hsv HSV. Greys give hue and saturation 0.
 */
void colourspace_rgb_to_hsv(const colourspace_rgb_t *rgb,
                            colourspace_hsv_t       *hsv);

/**
 * Convert HSV to sRGB. Hue wraps, so 360 is 0.
 *
 * \param[in]  hsv HSV.
 * \param[out] rgb sRGB.
 */
void colourspace_hsv_to_rgb(const colourspace_hsv_t *hsv,
                            colourspace_rgb_t       *rgb);

/**
 * Convert sRGB to OKLab.
 *
 * \param[in]  rgb   sRGB.
 * \param[out] oklab OKLab.
 */
void colourspace_rgb_to_oklab(const colourspace_rgb_t *rgb,
                              colourspace_oklab_t     *oklab);

/**
 * Convert OKLab to sRGB. The result may lie outside 0..1 if the colour is
 * outside the sRGB gamut.
 *
 * \param[in]  oklab OKLab.
 * \param[out] rgb   sRGB.
 */
void colourspace_oklab_to_rgb(const colourspace_oklab_t *oklab,
                              colourspace_rgb_t         *rgb);

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUF_COLOURSPACE_H */
