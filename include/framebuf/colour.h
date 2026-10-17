/* framebuf/colour.h -- colour type */

#ifndef FRAMEBUF_COLOUR_H
#define FRAMEBUF_COLOUR_H

#include "framebuf/pixelfmt.h"
#include "utils/rng.h"

typedef struct colour colour_t;

struct colour
{
  pixelfmt_rgba8888_t primary;
};

/**
 * Create a colour from RGB components.
 *
 * \param[in] r Red component.
 * \param[in] g Green component.
 * \param[in] b Blue component.
 * \return Colour.
 */
colour_t colour_rgb(unsigned int r, unsigned int g, unsigned int b);

/**
 * Create an opaque colour with each channel picked at random.
 *
 * \param[in,out] rng Generator state.
 * \return Colour.
 */
colour_t colour_random(rng_t *rng);

/**
 * Create an opaque random colour that stands out from `against`: its luma
 * differs by at least 96 of 255. Falls back to black or white if a few tries
 * find nothing.
 *
 * \param[in,out] rng Generator state.
 * \param[in] against Colour to contrast with.
 * \return Colour.
 */
colour_t colour_random_contrast(rng_t *rng, colour_t against);

/**
 * Create a colour from RGBA components.
 *
 * \param[in] r Red component.
 * \param[in] g Green component.
 * \param[in] b Blue component.
 * \param[in] a Alpha component.
 * \return Colour.
 */
colour_t colour_rgba(unsigned int r,
                     unsigned int g,
                     unsigned int b,
                     unsigned int a);

/**
 * Return colour `c` as a pixel of format `fmt`.
 *
 * \param[in] palette   Palette to use.
 * \param[in] nentries  Number of entries in the palette.
 * \param[in] required  Required colour.
 * \param[in] fmt       Pixel format.
 * \return Pixel.
 */
pixelfmt_any_t colour_to_pixel(const colour_t *palette,
                               int             nentries,
                               colour_t        required,
                               pixelfmt_t      fmt);

/**
 * Return the alpha component of the specified colour.
 *
 * \param[in] c Colour.
 * \return Alpha component.
 */
unsigned int colour_get_alpha(const colour_t *c);

/**
 * Return the RGB components of the specified colour.
 *
 * \param[in]  c Colour.
 * \param[out] r Red component.
 * \param[out] g Green component.
 * \param[out] b Blue component.
 */
void colour_get_rgb(const colour_t *c,
                    unsigned int   *r,
                    unsigned int   *g,
                    unsigned int   *b);

/**
 * Blend two colours, per component and opaque.
 *
 * \param[in] a   Colour returned when num is 0.
 * \param[in] b   Colour returned when num equals den.
 * \param[in] num Numerator of the blend fraction, at most den.
 * \param[in] den Denominator of the blend fraction, non-zero.
 * \return Colour num/den of the way from a to b.
 */
colour_t colour_mix(colour_t     a,
                    colour_t     b,
                    unsigned int num,
                    unsigned int den);

#endif /* FRAMEBUF_COLOUR_H */
